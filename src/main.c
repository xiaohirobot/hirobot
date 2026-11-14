#include "components/robot_spi.h"

/*
 * 现在跑 Wi-Fi SPI 命令分发。换成别的演示时，改这个 include 和下面的调用。
 * Wi-Fi SPI 命令分发是 robot_spi_run()。
 * 灯带整板同色是 ws2812_light()。
 * 舵机扫动演示是 demo_servo()。
 * 串口 tick 演示是 demo_uart3_log()。
 * 串口加法是 demo_uart3_add()，发 a,b 回和。
 * 串口命令是 demo_uart3_cmd()，主机。发 mode,a,b。
 * CAN 从机是 demo_can_slave()。下载前改成这个调用，再烧一次。
 * SPI 链路测试是 demo_spi()，和舵机同一套从机。
 * SPI 从机舵机是 main_robot()。
 */
void main(void)
{
    robot_spi_run();
}
