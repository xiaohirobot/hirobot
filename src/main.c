#include "main_robot.h"
#include "tests/demo_can_slave.h"
#include "tests/demo_servo.h"
#include "tests/demo_spi.h"
#include "tests/demo_uart3_add.h"
#include "tests/demo_uart3_cmd.h"
#include "tests/demo_uart3_log.h"
#include "tests/demo_ws2812_light.h"

/* Wi-Fi -> ECW SPI master -> STM32 command dispatcher. */
extern void robot_spi_run(void);

/*
 * 现在跑 CAN 从机。换成别的演示时，只改下面这一行调用。
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
    demo_can_slave();
}
