#include "demos/demo_uart3_log.h"

#include "components/delay.h"
#include "components/uart3.h"

void demo_uart3_log(void)
{
    unsigned tick = 0;

    uart3_init();
    uart3_puts("Hello, World! USART3 PB10\n");

    while (1) {
        delay_ms(1000);
        tick++;
        uart3_puts("tick ");
        uart3_putu(tick);
        uart3_puts("\n");
    }
}
