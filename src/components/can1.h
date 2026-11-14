#ifndef CAN1_H
#define CAN1_H

#include <stdint.h>

/*
 * CAN1：PD0 = RX，PD1 = TX，AF9。500 kbit/s。
 * 时钟是复位后的 APB1 HSI 16 MHz。只发标准数据帧。
 * send / recv 借用 SysTick 做超时，返回前关掉，不要和 delay_ms 嵌套。
 */

#define CAN1_TIMEOUT_MS 50u

/* 0 成功，-1 参数、超时或这帧没被应答。 */
int can1_init(void);
int can1_send(uint16_t id, const uint8_t *data, unsigned len);

/*
 * 取一帧标准数据帧。data 至少 8 字节。
 * timeout_ms 为 0 则一直等；超过 1000 按 1000 计。
 * 返回 DLC；超时返回 -1。
 */
int can1_recv(uint16_t *id, uint8_t *data, unsigned timeout_ms);

/* 在同一段超时里丢掉其他 ID，直到等到 want。 */
int can1_recv_id(uint16_t want, uint8_t *data, unsigned timeout_ms);

#endif
