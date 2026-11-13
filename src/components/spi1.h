#ifndef SPI1_H
#define SPI1_H

#include <stdint.h>

/*
 * SPI1 主机，Mode 0（CPOL=0，CPHA=0），软件片选。
 *   SCK  PB3  AF5
 *   MISO PB4  AF5
 *   MOSI PB5  AF5
 *   CS   PD7  推挽输出，空闲高
 * 时钟沿用复位后的 HSI 16 MHz，APB2 也是 16 MHz，分频 8，SCK 约 2 MHz。
 * 一次传输期间 CS 保持为低。计时占用 SysTick，不能和 delay_ms 重入。
 * 单次超时不超过 1000 ms（SysTick 24 位，16 MHz 下大约只能记 1 秒）。
 */

#define SPI1_OK       0
#define SPI1_TIMEOUT -1
#define SPI1_ERROR   -2

/* 打开 SPI1 和片选。片选空闲为高。返回 SPI1_OK。 */
int spi1_init(void);

/*
 * 全双工交换 len 个字节。CS 在整段交换期间为低，无论成败都会拉高。
 * 返回 SPI1_OK、SPI1_TIMEOUT 或 SPI1_ERROR。
 * elapsed_ms 非空时写入本次耗时；超时写成 timeout_ms。
 */
int spi1_transceive(const uint8_t *tx, uint8_t *rx, unsigned len,
                    unsigned timeout_ms, unsigned *elapsed_ms);

#endif
