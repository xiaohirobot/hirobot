#include <stdint.h>
#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB2ENR (*(volatile uint32_t *)0x40023844UL)
#define RCC_APB2RSTR (*(volatile uint32_t *)0x40023824UL)

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
#define SPI_CR1_SSI (1U << 8)
#define SPI_CR1_SSM (1U << 9)

#define CS_PIN 7U
#define MAGIC0 0xA5u
#define MAGIC1 0x5Au


#include "components/robot_spi.h"
#include "components/robot_wire.h"
#include "components/robot_command.h"
#include "components/robot_i2c.h"
#include "components/uart3.h"
static RobotEndpoint endpoint;
#define tx endpoint.reply
static uint8_t rx[RW_SIZE];
#define DEMCR (*(volatile uint32_t *)0xE000EDFCUL)
#define DWT_CTRL (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004UL)
static int cs_high(void) { return (GPIOD_IDR & (1u<<7))!=0; }
static void spi_end_frame(void) {
    /* PD7 is a GPIO, not hardware NSS. Explicitly deselect and disable SPI
     * while CS is high so master startup/pinmux clocks cannot shift data. */
    SPI1_CR1 = SPI_CR1_SSM | SPI_CR1_SSI;
}
static void spi_begin_frame(void) {
    /* SPE toggling/DR reads do not provide a complete peripheral reset.
     * Reset on every CS assertion, including after short/overrun frames.
     * The master waits one RTOS tick after CS low before its first clock. */
    RCC_APB2RSTR |= 1U << 12;
    (void)RCC_APB2RSTR;
    RCC_APB2RSTR &= ~(1U << 12);
    (void)RCC_APB2RSTR;
    SPI1_CR2 = 0;
    SPI1_CR1 = SPI_CR1_SSM | SPI_CR1_SPE;
    SPI1_DR = tx[0];
}
static void init_pins(void)
{
    RCC_AHB1ENR |= (1U << 1) | (1U << 3); /* GPIOBEN、GPIODEN */
    RCC_APB2ENR |= 1U << 12;              /* SPI1EN */
    (void)RCC_AHB1ENR;
    (void)RCC_APB2ENR;

    /* PB3 SCK、PB4 MISO、PB5 MOSI = AF5。 */
    GPIOB_MODER &= ~((3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U)));
    GPIOB_MODER |= (2U << (3U * 2U)) | (2U << (4U * 2U)) | (2U << (5U * 2U));
    GPIOB_PUPDR &= ~((3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U)));
    GPIOB_PUPDR |= 2U << (3U * 2U); /* Mode 0 SCK idle low when master resets. */
    GPIOB_OSPEEDR |= (3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U));
    GPIOB_AFRL &= ~((0xFU << (3U * 4U)) | (0xFU << (4U * 4U)) | (0xFU << (5U * 4U)));
    GPIOB_AFRL |= (5U << (3U * 4U)) | (5U << (4U * 4U)) | (5U << (5U * 4U));

    /* PD7 输入上拉。主机片选空闲为高。 */
    GPIOD_MODER &= ~(3U << (CS_PIN * 2U));
    GPIOD_PUPDR &= ~(3U << (CS_PIN * 2U));
    GPIOD_PUPDR |= 1U << (CS_PIN * 2U);

    SPI1_CR1 = 0;
    SPI1_CR2 = 0;
    spi_end_frame();
}


static void spi_poll(void)
{
    static int active, wait_release = 1, bad;
    static unsigned count;
    static uint32_t started;
    if (wait_release) {
        if (cs_high()) wait_release = 0;
        return;
    }
    if (!active) {
        if (cs_high()) return;
        spi_begin_frame();
        active = 1; bad = 0; count = 0; started = DWT_CYCCNT;
    }
    if ((uint32_t)(DWT_CYCCNT - started) > 32000000u || (SPI1_SR & SPI_SR_OVR)) {
        spi_end_frame(); active = 0; wait_release = 1; return;
    }
    if (SPI1_SR & SPI_SR_RXNE) {
        uint8_t value = (uint8_t)SPI1_DR;
        if (count >= RW_SIZE) bad = 1;
        else {
            rx[count++] = value;
            if (count < RW_SIZE) SPI1_DR = tx[count];
        }
    }
    if (cs_high()) {
        spi_end_frame(); active = 0;
        if (!bad && count == RW_SIZE) robot_command(&endpoint, rx);
    }
}
void robot_spi_run(void)
{
    uart3_init();
    uart3_puts("Robot dual link v3.1: SPI PD7 CS + I2C1 PB8/PB9 addr 0x32\n");
    DEMCR |= 1u << 24; DWT_CYCCNT = 0; DWT_CTRL |= 1;
    robot_endpoint_init(&endpoint);
    init_pins();
    robot_i2c_init();
    for (;;) {
        spi_poll();
        robot_i2c_poll();
    }
}
