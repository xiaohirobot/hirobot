#ifndef WS2812_LIGHT_H
#define WS2812_LIGHT_H

#include <stdint.h>

/*
 * 8x8 WS2812B 整板同色。数据脚 PD9（CN16 / DI0），推挽，空闲低。
 * TIM8 更新事件 → DMA2 Stream1 通道 7 → GPIOD BSRR，循环整帧。
 * 时钟仍是复位后的 HSI 16 MHz。TIM8 不分频，5 个计数一格（312.5 ns），
 * 一位四格，约 800 kbps。帧两边各 1024 格低电平，复位间隙约 320 us。
 * 改色只记下 RGB；show 会先停流再编码再发送。
 */

#define WS2812_LIGHT_PIXELS 64u

typedef enum {
    WS2812_LIGHT_OK = 0,
    WS2812_LIGHT_FAULT = 1,
} Ws2812LightStatus;

void ws2812_light_init(void);
void ws2812_light_set_color(uint8_t r, uint8_t g, uint8_t b);
Ws2812LightStatus ws2812_light_show(void);
void ws2812_light_stop(void);
uint32_t ws2812_light_error_count(void);

#endif
