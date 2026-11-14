#ifndef MAIN_ROBOT_H
#define MAIN_ROBOT_H

/*
 * SPI 从机舵机。不返回。
 * 上电两路先到 1500 us，再经 components/spi1_slave 收 ECW6700 的 32 字节帧。
 * 链路测试、不带动舵机的是 demo_spi()。扫动演示仍是 demo_servo()。
 */
void main_robot(void);

#endif
