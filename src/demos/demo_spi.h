#ifndef DEMO_SPI_H
#define DEMO_SPI_H

/*
 * SPI 从机链路测试。不返回。
 * 和 demo_servo_link() 共用 components/spi1_slave，32 字节帧，ECW6700 做主机。
 * 不驱动舵机。收到 SET 后在查询拍把通道和脉宽打到 USART3。
 * PB3 SCK、PB4 MISO、PB5 MOSI，片选 PD7。
 */
void demo_spi(void);

#endif
