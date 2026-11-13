#include "demos/demo_uart3_log.h"

#include "uart3.h"

#include <stdint.h>

#define SYSTICK_CTRL      (*(volatile uint32_t *)0xE000E010UL)
#define SYSTICK_LOAD      (*(volatile uint32_t *)0xE000E014UL)
#define SYSTICK_VAL       (*(volatile uint32_t *)0xE000E018UL)
#define SYSTICK_COUNTFLAG (1U << 16)

/* HSI 16 MHz，轮询 COUNTFLAG，不打开 SysTick 中断。 */
static void delay_1s(void)
{
    SYSTICK_LOAD = 16000000U - 1U;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = (1U << 2) | (1U << 0);
    while ((SYSTICK_CTRL & SYSTICK_COUNTFLAG) == 0) {
    }
    SYSTICK_CTRL = 0;
}

void demo_uart3_log(void)
{
    unsigned tick = 0;

    uart3_init();
    uart3_puts("Hello, World! USART3 PB10\n");

    while (1) {
        delay_1s();
        tick++;
        uart3_puts("tick ");
        uart3_putu(tick);
        uart3_puts("\n");
    }
}
