#include "demos/demo_servo.h"

/*
 * 当前跑舵机演示（firmware-code-serve-demo）。
 * 串口 tick 在 demo_uart3_log()，换成那个调用即可切回去。
 */
void main(void)
{
    demo_servo();
}
