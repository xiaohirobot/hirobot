#include "demos/demo_servo.h"

/*
 * 现在跑舵机扫动演示。换成整机逻辑时，改这里的调用。
 * 舵机、串口和延时在 src/components。串口 tick 演示是 demo_uart3_log()。
 */
void main(void)
{
    demo_servo();
}
