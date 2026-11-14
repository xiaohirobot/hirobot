#include "tests/demo_uart3_add.h"

/*
 * 现在跑串口加法。换成别的演示时，改这里的调用。
 * 灯带整板同色是 ws2812_light()。
 * 舵机扫动演示是 demo_servo()。
 * 串口 tick 演示是 demo_uart3_log()。
 * 串口加法是 demo_uart3_add()，发 a,b 回和。
 * SPI 链路测试是 demo_spi()，和舵机同一套从机。
 * SPI 从机舵机是 main_robot()。
 */
void main(void)
{
    demo_uart3_add();
}
