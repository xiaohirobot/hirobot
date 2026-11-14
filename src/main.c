#include "tests/demo_ws2812_light.h"

/*
 * 现在跑灯带变色。换成别的演示时，改这里的调用。
 * 灯带整板同色是 ws2812_light()。
 * 舵机扫动演示是 demo_servo()。
 * 串口 tick 演示是 demo_uart3_log()。
 * SPI 链路测试是 demo_spi()，和舵机同一套从机。
 * SPI 从机舵机是 main_robot()。
 */
void main(void)
{
    ws2812_light();
}
