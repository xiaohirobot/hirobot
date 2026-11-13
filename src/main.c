#include "demos/demo_servo_link.h"

/*
 * 现在跑 SPI 从机舵机。换成别的演示时，改这里的调用。
 * 舵机扫动演示是 demo_servo()。
 * 串口 tick 演示是 demo_uart3_log()。
 * SPI 链路测试是 demo_spi()，和舵机同一套从机。
 */
void main(void)
{
    demo_servo_link();
}
