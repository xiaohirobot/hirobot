#ifndef SPI1_SLAVE_H
#define SPI1_SLAVE_H

#include <stdint.h>

/*
 * SPI1 从机，Mode 0。ECW6700 做主机。
 *   SCK  PB3  AF5
 *   MISO PB4  AF5
 *   MOSI PB5  AF5
 *   CS   PD7  输入上拉，只用来对齐帧。硬件 NSS 在 PA4，不在这只脚。
 * 32 字节，MSB：A5 5A | seq | len | payload | CRC-8（多项式 0x07，覆盖前 31 字节）。
 * 这一帧 MISO 上是上一帧装好的应答。等待片选不用 SysTick。
 */

#define SPI1_FRAME_LEN  32u
#define SPI1_OFF_SEQ    2u
#define SPI1_OFF_LEN    3u
#define SPI1_OFF_CMD    4u
#define SPI1_OFF_CH     5u
#define SPI1_OFF_PULSE  6u
#define SPI1_CMD_QUERY  0u
#define SPI1_CMD_SET    1u
#define SPI1_ACK_OK     0u
#define SPI1_ACK_REJECT 1u

typedef void (*spi1_frame_fn)(const uint8_t *rx);

/* 打开 SPI1。片选空闲为高。应答要另外用 spi1_slave_set_reply() 装上。 */
void spi1_slave_init(void);

/* 装上下一帧要移出的应答。可在回调里调用。 */
void spi1_slave_set_reply(uint8_t seq, uint8_t status, uint8_t channel, uint16_t pulse);

/*
 * 不返回。CRC 正确且长度至少 1 才调用 on_frame。
 * 回调期间 rx 有效。坏帧丢掉，上一份应答保持不动。
 * on_frame 要在片选再次落下前返回，里面不要打长日志。
 */
void spi1_slave_run(spi1_frame_fn on_frame);

#endif
