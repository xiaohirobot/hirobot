#ifndef DEMO_SERVO_LINK_H
#define DEMO_SERVO_LINK_H

/*
 * SPI 从机舵机。不返回。
 * 上电两路先到 1500 us，再收 ECW6700 主机的 32 字节帧，CRC 正确才改脉宽。
 * PB3 SCK、PB5 MOSI、PB4 MISO，片选 PD7 只做帧同步（硬件 NSS 不在这只脚）。
 * 扫动演示仍是 demo_servo()，SPI 主机演示仍是 demo_spi()。
 */
void demo_servo_link(void);

#endif
