#ifndef ROBOT_SPI_H
#define ROBOT_SPI_H

/*
 * Wi-Fi 经 ECW SPI 主机下发命令，STM32 分发。不返回。
 * PB3 SCK、PB4 MISO、PB5 MOSI，片选 PD7。
 */
void robot_spi_run(void);

#endif
