#include "components/spi1_slave.h"

/*
 * 从机软件 NSS，内部保持选中。PD7 下降沿把接收计数清零，收满 32 字节再校验。
 */

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
#define MAGIC0 0xA5u
#define MAGIC1 0x5Au

static uint8_t g_tx[SPI1_FRAME_LEN];
static uint8_t g_rx[SPI1_FRAME_LEN];

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

static int frame_ok(void)
{
    if (g_rx[0] != MAGIC0 || g_rx[1] != MAGIC1 || g_rx[SPI1_OFF_LEN] > 24u) {
        return 0;
    }
    if (g_rx[SPI1_OFF_LEN] < 1u) {
        return 0;
    }
    if (crc8(g_rx, SPI1_FRAME_LEN - 1u) != g_rx[31]) {
        return 0;
    }
    return 1;
}

static unsigned int read_frame(void)
{
    unsigned int n = 0;

    while (n < SPI1_FRAME_LEN) {
        if ((SPI1_SR & SPI_SR_RXNE) != 0U) {
            g_rx[n] = (uint8_t)SPI1_DR;
            n++;
            if (n < SPI1_FRAME_LEN) {
                SPI1_DR = g_tx[n];
            }
        } else if (cs_high()) {
            break;
        }
    }
    return n;
}

void spi1_slave_set_reply(uint8_t seq, uint8_t status, uint8_t channel, uint16_t pulse)
{
    unsigned int i;

    for (i = 0; i < SPI1_FRAME_LEN; i++) {
        g_tx[i] = 0;
    }
    g_tx[0] = MAGIC0;
    g_tx[1] = MAGIC1;
    g_tx[SPI1_OFF_SEQ] = seq;
    g_tx[SPI1_OFF_LEN] = 4u;
    g_tx[SPI1_OFF_CMD] = status;
    g_tx[SPI1_OFF_CH] = channel;
    g_tx[SPI1_OFF_PULSE] = (uint8_t)(pulse & 0xffu);
    g_tx[SPI1_OFF_PULSE + 1u] = (uint8_t)((pulse >> 8) & 0xffu);
    g_tx[31] = crc8(g_tx, SPI1_FRAME_LEN - 1u);
}

void spi1_slave_init(void)
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

    SPI1_CR1 = 0;
    SPI1_CR2 = 0;
    SPI1_CR1 = SPI_CR1_SSM;
    spi_flush();
}

void spi1_slave_run(spi1_frame_fn on_frame)
{
    int was_high = 1;

    preload_first();
    for (;;) {
        int high = cs_high();
        unsigned int spins;

        if (was_high && !high) {
            if ((SPI1_SR & SPI_SR_OVR) == 0U && read_frame() == SPI1_FRAME_LEN &&
                frame_ok() && on_frame != 0) {
                on_frame(g_rx);
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
