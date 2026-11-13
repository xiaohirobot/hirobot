#include "demos/demo_servo.h"

/*
 * 现在跑舵机扫动演示。换成别的演示时，改这里的调用。
 * 舵机、串口、延时和 SPI 在 src/components。
 * 串口 tick 演示是 demo_uart3_log()。
 * ECW6700 SPI 主机演示是 demo_spi()。
 */
void main(void)
{
    demo_servo();
}
