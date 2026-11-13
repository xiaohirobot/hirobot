#include "components/spi1.h"

/*
 * 寄存器按 RM0090 的 SPI1 / GPIOB / GPIOD。
 * 主机软件 NSS：SSM=1 且 SSI=1，避免 MODF 把 SPE 清掉。
 */

#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB2ENR (*(volatile uint32_t *)0x40023844UL)

#define GPIOB_MODER   (*(volatile uint32_t *)0x40020400UL)
#define GPIOB_OSPEEDR (*(volatile uint32_t *)0x40020408UL)
#define GPIOB_PUPDR   (*(volatile uint32_t *)0x4002040CUL)
#define GPIOB_AFRL    (*(volatile uint32_t *)0x40020420UL)

#define GPIOD_MODER   (*(volatile uint32_t *)0x40020C00UL)
#define GPIOD_OSPEEDR (*(volatile uint32_t *)0x40020C08UL)
#define GPIOD_BSRR    (*(volatile uint32_t *)0x40020C18UL)

#define SPI1_CR1 (*(volatile uint32_t *)0x40013000UL)
#define SPI1_CR2 (*(volatile uint32_t *)0x40013004UL)
#define SPI1_SR  (*(volatile uint32_t *)0x40013008UL)
#define SPI1_DR  (*(volatile uint32_t *)0x4001300CUL)

#define SYSTICK_CTRL      (*(volatile uint32_t *)0xE000E010UL)
#define SYSTICK_LOAD      (*(volatile uint32_t *)0xE000E014UL)
#define SYSTICK_VAL       (*(volatile uint32_t *)0xE000E018UL)
#define SYSTICK_COUNTFLAG (1U << 16)

#define SPI_SR_RXNE (1U << 0)
#define SPI_SR_TXE  (1U << 1)
#define SPI_SR_MODF (1U << 5)
#define SPI_SR_OVR  (1U << 6)
#define SPI_SR_BSY  (1U << 7)

#define SPI_CR1_MSTR (1U << 2)
#define SPI_CR1_BR_DIV8 (2U << 3) /* 16 MHz / 8 = 2 MHz */
#define SPI_CR1_SPE  (1U << 6)
#define SPI_CR1_SSI  (1U << 8)
#define SPI_CR1_SSM  (1U << 9)

#define CS_PIN 7U

/* 16 MHz 下 24 位计数器大约 1048 ms，留一点余量。 */
#define SPI1_TIMEOUT_MAX_MS 1000U

static void cs_high(void)
{
    GPIOD_BSRR = 1U << CS_PIN;
}

static void cs_low(void)
{
    GPIOD_BSRR = 1U << (CS_PIN + 16U);
}

static void spi1_clear_fault(void)
{
    (void)SPI1_DR;
    (void)SPI1_SR;
}

/* 停掉可能还在移位的一字节，清掉 OVR/MODF，再重新打开，供下一帧使用。 */
static void spi1_abort(void)
{
    SPI1_CR1 &= ~SPI_CR1_SPE;
    spi1_clear_fault();
    cs_high();
    SPI1_CR1 |= SPI_CR1_SPE;
}

static void systick_start(unsigned timeout_ms)
{
    SYSTICK_CTRL = 0;
    SYSTICK_LOAD = 16000U * timeout_ms - 1U;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = (1U << 2) | (1U << 0);
}

static unsigned systick_stop_elapsed(unsigned timeout_ms)
{
    uint32_t load = SYSTICK_LOAD;
    uint32_t val = SYSTICK_VAL;
    uint32_t ctrl = SYSTICK_CTRL;

    SYSTICK_CTRL = 0;
    if ((ctrl & SYSTICK_COUNTFLAG) != 0U || val > load) {
        return timeout_ms;
    }
    return (unsigned)((load - val) / 16000U);
}

static int wait_sr(uint32_t mask)
{
    while ((SPI1_SR & mask) == 0U) {
        if ((SYSTICK_CTRL & SYSTICK_COUNTFLAG) != 0U) {
            return SPI1_TIMEOUT;
        }
        if ((SPI1_SR & (SPI_SR_MODF | SPI_SR_OVR)) != 0U) {
            return SPI1_ERROR;
        }
    }
    return SPI1_OK;
}

int spi1_init(void)
{
    RCC_AHB1ENR |= (1U << 1) | (1U << 3); /* GPIOBEN、GPIODEN */
    RCC_APB2ENR |= 1U << 12;              /* SPI1EN */
    (void)RCC_AHB1ENR;
    (void)RCC_APB2ENR;

    /* PB3/PB4/PB5 = AF5，无上下拉，最高速度。 */
    GPIOB_MODER &= ~((3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U)));
    GPIOB_MODER |= (2U << (3U * 2U)) | (2U << (4U * 2U)) | (2U << (5U * 2U));
    GPIOB_PUPDR &= ~((3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U)));
    GPIOB_OSPEEDR |= (3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U));
    GPIOB_AFRL &= ~((0xFU << (3U * 4U)) | (0xFU << (4U * 4U)) | (0xFU << (5U * 4U)));
    GPIOB_AFRL |= (5U << (3U * 4U)) | (5U << (4U * 4U)) | (5U << (5U * 4U));

    /* PD7 推挽输出，空闲高。复位后 OTYPER 已是推挽。 */
    GPIOD_MODER &= ~(3U << (CS_PIN * 2U));
    GPIOD_MODER |= 1U << (CS_PIN * 2U);
    GPIOD_OSPEEDR &= ~(3U << (CS_PIN * 2U));
    GPIOD_OSPEEDR |= 2U << (CS_PIN * 2U);
    cs_high();

    SPI1_CR1 = 0;
    SPI1_CR2 = 0;
    SPI1_CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_DIV8;
    spi1_clear_fault();
    SPI1_CR1 |= SPI_CR1_SPE;
    return SPI1_OK;
}

int spi1_transceive(const uint8_t *tx, uint8_t *rx, unsigned len,
                    unsigned timeout_ms, unsigned *elapsed_ms)
{
    unsigned i;
    unsigned elapsed;
    int status = SPI1_OK;

    if (len == 0U) {
        if (elapsed_ms != 0) {
            *elapsed_ms = 0;
        }
        return SPI1_OK;
    }
    if (timeout_ms == 0U || timeout_ms > SPI1_TIMEOUT_MAX_MS) {
        return SPI1_ERROR;
    }

    systick_start(timeout_ms);
    cs_low();

    for (i = 0; i < len; i++) {
        status = wait_sr(SPI_SR_TXE);
        if (status != SPI1_OK) {
            break;
        }
        SPI1_DR = tx[i];
        status = wait_sr(SPI_SR_RXNE);
        if (status != SPI1_OK) {
            break;
        }
        rx[i] = (uint8_t)SPI1_DR;
    }

    if (status == SPI1_OK) {
        while ((SPI1_SR & SPI_SR_BSY) != 0U) {
            if ((SYSTICK_CTRL & SYSTICK_COUNTFLAG) != 0U) {
                status = SPI1_TIMEOUT;
                break;
            }
            if ((SPI1_SR & (SPI_SR_MODF | SPI_SR_OVR)) != 0U) {
                status = SPI1_ERROR;
                break;
            }
        }
    }

    elapsed = systick_stop_elapsed(timeout_ms);
    if (status != SPI1_OK) {
        spi1_abort();
        if (status == SPI1_TIMEOUT) {
            elapsed = timeout_ms;
        }
    } else {
        cs_high();
    }

    if (elapsed_ms != 0) {
        *elapsed_ms = elapsed;
    }
    return status;
}
