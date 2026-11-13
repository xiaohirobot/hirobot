#include "demos/demo_servo_link.h"

#include "components/servo.h"
#include "components/uart3.h"

#include <stdint.h>

/*
 * ECW6700 做主机。本机是从机，片选下降沿重新对齐一帧。
 * 32 字节，mode 0，MSB：
 *   [0]=0xA5 [1]=0x5A [2]=seq [3]=len [4..27]=payload [31]=CRC-8。
 * SET 负载：命令 1、通道 0/1、脉宽低字节、脉宽高字节。
 * 命令 0 只取走上一帧已经装好的应答，不改舵机。
 * 这一帧 MISO 上移出去的是上一帧的应答，所以处理完才装下一帧。
 * 等待片选不用 SysTick，delay_ms 也占用它。
 */

#define FRAME_LEN      32u
#define PAYLOAD_LEN    4u
#define MAGIC0         0xA5u
#define MAGIC1         0x5Au
#define SPI_CMD_QUERY  0u
#define SPI_CMD_SET    1u
#define SPI_ACK_OK     0u
#define SPI_ACK_REJECT 1u

#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB2ENR (*(volatile uint32_t *)0x40023844UL)

#define GPIOB_MODER   (*(volatile uint32_t *)0x40020400UL)
#define GPIOB_OSPEEDR (*(volatile uint32_t *)0x40020408UL)
#define GPIOB_PUPDR   (*(volatile uint32_t *)0x4002040CUL)
#define GPIOB_AFRL    (*(volatile uint32_t *)0x40020420UL)

#define GPIOD_MODER (*(volatile uint32_t *)0x40020C00UL)
#define GPIOD_PUPDR (*(volatile uint32_t *)0x40020C0CUL)
#define GPIOD_IDR   (*(volatile uint32_t *)0x40020C10UL)

#define SPI1_CR1 (*(volatile uint32_t *)0x40013000UL)
#define SPI1_CR2 (*(volatile uint32_t *)0x40013004UL)
#define SPI1_SR  (*(volatile uint32_t *)0x40013008UL)
#define SPI1_DR  (*(volatile uint32_t *)0x4001300CUL)

#define SPI_SR_RXNE (1U << 0)
#define SPI_SR_TXE  (1U << 1)
#define SPI_SR_OVR  (1U << 6)
#define SPI_SR_BSY  (1U << 7)

#define SPI_CR1_SPE (1U << 6)
#define SPI_CR1_SSM (1U << 9)

#define CS_PIN 7U

static uint8_t g_tx[FRAME_LEN];
static uint8_t g_rx[FRAME_LEN];
static uint8_t g_last_status;
static uint8_t g_last_ch;
static uint16_t g_last_pulse;

static uint8_t crc8(const uint8_t *data, unsigned int len)
{
    uint8_t crc = 0;
    unsigned int i;

    for (i = 0; i < len; i++) {
        unsigned int bit;

        crc ^= data[i];
        for (bit = 0; bit < 8u; bit++) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

static int cs_high(void)
{
    return (GPIOD_IDR & (1U << CS_PIN)) != 0U;
}

static void build_reply(uint8_t seq, uint8_t status, uint8_t channel, uint16_t pulse)
{
    unsigned int i;

    for (i = 0; i < FRAME_LEN; i++) {
        g_tx[i] = 0;
    }
    g_tx[0] = MAGIC0;
    g_tx[1] = MAGIC1;
    g_tx[2] = seq;
    g_tx[3] = PAYLOAD_LEN;
    g_tx[4] = status;
    g_tx[5] = channel;
    g_tx[6] = (uint8_t)(pulse & 0xffu);
    g_tx[7] = (uint8_t)((pulse >> 8) & 0xffu);
    g_tx[31] = crc8(g_tx, FRAME_LEN - 1u);
    g_last_status = status;
    g_last_ch = channel;
    g_last_pulse = pulse;
}

static void spi_flush(void)
{
    SPI1_CR1 &= ~SPI_CR1_SPE;
    (void)SPI1_DR;
    (void)SPI1_SR;
    SPI1_CR1 |= SPI_CR1_SPE;
}

static void preload_first(void)
{
    unsigned int spins = 0;

    while ((SPI1_SR & SPI_SR_TXE) == 0U && spins < 100000u) {
        spins++;
    }
    if ((SPI1_SR & SPI_SR_TXE) == 0U) {
        spi_flush();
    }
    SPI1_DR = g_tx[0];
}

static void spi_slave_init(void)
{
    RCC_AHB1ENR |= (1U << 1) | (1U << 3); /* GPIOBEN、GPIODEN */
    RCC_APB2ENR |= 1U << 12;              /* SPI1EN */
    (void)RCC_AHB1ENR;
    (void)RCC_APB2ENR;

    /* PB3 SCK、PB4 MISO、PB5 MOSI = AF5。 */
    GPIOB_MODER &= ~((3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U)));
    GPIOB_MODER |= (2U << (3U * 2U)) | (2U << (4U * 2U)) | (2U << (5U * 2U));
    GPIOB_PUPDR &= ~((3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U)));
    GPIOB_OSPEEDR |= (3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U));
    GPIOB_AFRL &= ~((0xFU << (3U * 4U)) | (0xFU << (4U * 4U)) | (0xFU << (5U * 4U)));
    GPIOB_AFRL |= (5U << (3U * 4U)) | (5U << (4U * 4U)) | (5U << (5U * 4U));

    /* PD7 输入上拉。主机片选空闲为高。 */
    GPIOD_MODER &= ~(3U << (CS_PIN * 2U));
    GPIOD_PUPDR &= ~(3U << (CS_PIN * 2U));
    GPIOD_PUPDR |= 1U << (CS_PIN * 2U);

    /*
     * 从机，软件 NSS 且内部保持选中。PD7 不是 SPI1 的硬件 NSS，
     * 帧边界用片选脚自己数 32 字节。
     */
    SPI1_CR1 = 0;
    SPI1_CR2 = 0;
    SPI1_CR1 = SPI_CR1_SSM;
    spi_flush();
}

static int frame_ok(void)
{
    if (g_rx[0] != MAGIC0 || g_rx[1] != MAGIC1 || g_rx[3] > 24u) {
        return 0;
    }
    if (crc8(g_rx, FRAME_LEN - 1u) != g_rx[31]) {
        return 0;
    }
    return 1;
}

static void handle_frame(void)
{
    uint8_t seq = g_rx[2];
    uint8_t len = g_rx[3];
    uint8_t cmd;
    uint8_t channel;
    uint16_t pulse;

    if (!frame_ok() || len < 1u) {
        return;
    }
    cmd = g_rx[4];
    if (cmd == SPI_CMD_QUERY) {
        build_reply(seq, g_last_status, g_last_ch, g_last_pulse);
        return;
    }
    if (cmd != SPI_CMD_SET || len < PAYLOAD_LEN) {
        build_reply(seq, SPI_ACK_REJECT, 0, g_last_pulse);
        return;
    }
    channel = g_rx[5];
    pulse = (uint16_t)g_rx[6] | (uint16_t)((uint16_t)g_rx[7] << 8);
    if (channel >= SERVO_CH_COUNT || pulse < SERVO_MIN_PULSE_US || pulse > SERVO_MAX_PULSE_US) {
        build_reply(seq, SPI_ACK_REJECT, channel, g_last_pulse);
        return;
    }
    if (servo_set_pulse_us(channel, pulse) != SERVO_OK) {
        build_reply(seq, SPI_ACK_REJECT, channel, g_last_pulse);
        return;
    }
    build_reply(seq, SPI_ACK_OK, channel, servo_actual_pulse_us(channel));
}

static unsigned int read_frame(void)
{
    unsigned int n = 0;

    while (n < FRAME_LEN) {
        if ((SPI1_SR & SPI_SR_RXNE) != 0U) {
            g_rx[n] = (uint8_t)SPI1_DR;
            n++;
            if (n < FRAME_LEN) {
                SPI1_DR = g_tx[n];
            }
        } else if (cs_high()) {
            break;
        }
    }
    return n;
}

void demo_servo_link(void)
{
    unsigned int channel;
    int was_high = 1;

    uart3_init();
    uart3_puts("servo link PA2+PA3, spi slave\n");
    if (servo_init() != SERVO_OK) {
        uart3_puts("servo init failed\n");
        while (1) {
        }
    }
    for (channel = 0; channel < SERVO_CH_COUNT; channel++) {
        if (servo_set_pulse_us(channel, 1500u) != SERVO_OK) {
            uart3_puts("servo center failed\n");
            while (1) {
            }
        }
    }

    build_reply(0, SPI_ACK_OK, 0, 1500u);
    spi_slave_init();
    preload_first();
    uart3_puts("center 1500 us, waiting cs\n");

    for (;;) {
        int high = cs_high();
        unsigned int spins;

        if (was_high && !high) {
            if ((SPI1_SR & SPI_SR_OVR) == 0U && read_frame() == FRAME_LEN) {
                handle_frame();
            }
            while (!cs_high()) {
            }
            spins = 0;
            while ((SPI1_SR & SPI_SR_BSY) != 0U && spins < 100000u) {
                spins++;
            }
            if ((SPI1_SR & (SPI_SR_OVR | SPI_SR_RXNE)) != 0U) {
                spi_flush();
            }
            preload_first();
        }
        was_high = high;
    }
}
