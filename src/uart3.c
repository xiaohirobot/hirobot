#include "uart3.h"

#include <stdint.h>

/*
 * 时钟：复位后的内部 HSI 16 MHz。
 * USART3 在 APB1，复位后同样是 16 MHz。
 * 16 MHz / 115200 时 BRR = 0x8B。
 */

#define RCC_AHB1ENR  (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB1ENR  (*(volatile uint32_t *)0x40023840UL)

#define GPIOB_MODER  (*(volatile uint32_t *)0x40020400UL)
#define GPIOB_AFRH   (*(volatile uint32_t *)0x40020424UL)

#define USART3_SR    (*(volatile uint32_t *)0x40004800UL)
#define USART3_DR    (*(volatile uint32_t *)0x40004804UL)
#define USART3_BRR   (*(volatile uint32_t *)0x40004808UL)
#define USART3_CR1   (*(volatile uint32_t *)0x4000480CUL)

#define USART_SR_TXE (1U << 7)
#define USART_CR1_UE (1U << 13)
#define USART_CR1_TE (1U << 3)

void uart3_init(void)
{
    RCC_AHB1ENR |= 1U << 1;  /* GPIOBEN */
    RCC_APB1ENR |= 1U << 18; /* USART3EN */
    (void)RCC_AHB1ENR;
    (void)RCC_APB1ENR;

    /* PB10、PB11 设为复用功能 AF7（USART3）。 */
    GPIOB_MODER &= ~((3U << (10U * 2U)) | (3U << (11U * 2U)));
    GPIOB_MODER |= (2U << (10U * 2U)) | (2U << (11U * 2U));
    GPIOB_AFRH &= ~((0xFU << 8) | (0xFU << 12));
    GPIOB_AFRH |= (7U << 8) | (7U << 12);

    USART3_CR1 = 0;
    USART3_BRR = 0x8B;
    USART3_CR1 = USART_CR1_UE | USART_CR1_TE;
}

void uart3_putc(char c)
{
    while ((USART3_SR & USART_SR_TXE) == 0) {
    }
    USART3_DR = (uint8_t)c;
}

void uart3_puts(const char *s)
{
    while (*s != '\0') {
        if (*s == '\n') {
            uart3_putc('\r');
        }
        uart3_putc(*s++);
    }
}

void uart3_putu(unsigned value)
{
    char buf[10];
    unsigned i = 0;

    if (value == 0) {
        uart3_putc('0');
        return;
    }

    while (value > 0) {
        buf[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (i > 0) {
        uart3_putc(buf[--i]);
    }
}
