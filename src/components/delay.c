#include "components/delay.h"

#include <stdint.h>

#define SYSTICK_CTRL      (*(volatile uint32_t *)0xE000E010UL)
#define SYSTICK_LOAD      (*(volatile uint32_t *)0xE000E014UL)
#define SYSTICK_VAL       (*(volatile uint32_t *)0xE000E018UL)
#define SYSTICK_COUNTFLAG (1U << 16)

/* 24 位计数器一次最多约 1 秒。更长的时间拆成多段。 */
#define DELAY_CHUNK_MS 1000u

static void delay_once(unsigned ms)
{
    SYSTICK_LOAD = 16000U * ms - 1U;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = (1U << 2) | (1U << 0);
    while ((SYSTICK_CTRL & SYSTICK_COUNTFLAG) == 0) {
    }
    SYSTICK_CTRL = 0;
}

void delay_ms(unsigned ms)
{
    while (ms > 0U) {
        unsigned step = ms;

        if (step > DELAY_CHUNK_MS) {
            step = DELAY_CHUNK_MS;
        }
        delay_once(step);
        ms -= step;
    }
}
