#include <stdint.h>

/*
 * STM32F407VET6 串口 Hello World。
 * 时钟：复位后的内部 HSI 16 MHz。
 * 串口：USART1，PA9 = TX，PA10 = RX，115200 8N1。
 * 16 MHz / 115200 时 BRR = 0x8B。
 */

#define RCC_AHB1ENR  (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB2ENR  (*(volatile uint32_t *)0x40023844UL)

#define GPIOA_MODER  (*(volatile uint32_t *)0x40020000UL)
#define GPIOA_AFRH   (*(volatile uint32_t *)0x40020024UL)

#define USART1_SR    (*(volatile uint32_t *)0x40011000UL)
#define USART1_DR    (*(volatile uint32_t *)0x40011004UL)
#define USART1_BRR   (*(volatile uint32_t *)0x40011008UL)
#define USART1_CR1   (*(volatile uint32_t *)0x4001100CUL)

#define USART_SR_TXE (1U << 7)
#define USART_CR1_UE (1U << 13)
#define USART_CR1_TE (1U << 3)

#define SYSTICK_CTRL (*(volatile uint32_t *)0xE000E010UL)
#define SYSTICK_LOAD (*(volatile uint32_t *)0xE000E014UL)
#define SYSTICK_VAL  (*(volatile uint32_t *)0xE000E018UL)
#define SYSTICK_COUNTFLAG (1U << 16)

static void uart_putc(char c)
{
    while ((USART1_SR & USART_SR_TXE) == 0) {
    }
    USART1_DR = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s != '\0') {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}

static void uart_putu(unsigned value)
{
    char buf[10];
    unsigned i = 0;

    if (value == 0) {
        uart_putc('0');
        return;
    }

    while (value > 0) {
        buf[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

static void uart_init(void)
{
    RCC_AHB1ENR |= 1U << 0; /* GPIOAEN */
    RCC_APB2ENR |= 1U << 4; /* USART1EN */
    (void)RCC_AHB1ENR;
    (void)RCC_APB2ENR;

    /* PA9、PA10 设为复用功能 AF7（USART1）。 */
    GPIOA_MODER &= ~((3U << (9U * 2U)) | (3U << (10U * 2U)));
    GPIOA_MODER |= (2U << (9U * 2U)) | (2U << (10U * 2U));
    GPIOA_AFRH &= ~((0xFU << 4) | (0xFU << 8));
    GPIOA_AFRH |= (7U << 4) | (7U << 8);

    USART1_BRR = 0x8B;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void delay_1s(void)
{
    SYSTICK_LOAD = 16000000U - 1U;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = (1U << 2) | (1U << 0); /* 处理器时钟，开始计数 */
    while ((SYSTICK_CTRL & SYSTICK_COUNTFLAG) == 0) {
    }
    SYSTICK_CTRL = 0;
}

void main(void)
{
    unsigned tick = 0;

    uart_init();
    uart_puts("Hello, World!\n");

    while (1) {
        delay_1s();
        tick++;
        uart_puts("tick ");
        uart_putu(tick);
        uart_puts("\n");
    }
}
