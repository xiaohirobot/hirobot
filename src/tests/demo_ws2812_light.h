#ifndef DEMO_WS2812_LIGHT_H
#define DEMO_WS2812_LIGHT_H

/*
 * 8x8 灯带变色。不返回。
 * PD9 推整板同色，每 500 ms 换一档：红、橙、黄、绿、青、蓝、紫、粉。
 * DMA 传输出错后停流重发，最多 3 次，仍失败就停住。
 */
void ws2812_light(void);

#endif
