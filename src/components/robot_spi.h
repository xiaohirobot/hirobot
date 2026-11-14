#ifndef ROBOT_SPI_H
#define ROBOT_SPI_H

/*
 * ECW 经 SPI 或 I2C 下发同一套命令，两接口在非阻塞主循环中轮询。不返回。
 * PB3 SCK、PB4 MISO、PB5 MOSI，片选 PD7；I2C1 PB8 SCL、PB9 SDA，7位地址 0x32。
 */
void robot_spi_run(void);

#endif
