#ifndef DEMO_SPI_H
#define DEMO_SPI_H

/*
 * ECW6700 的 SPI 主机演示。不返回。
 * SPI1：PB3 SCK、PB4 MISO、PB5 MOSI，片选 PD7，心跳灯 PE7。
 * 日志走 USART3 PB10，115200 8N1。
 * 每秒发一帧，全双工收回应答，校验 magic / seq / CRC16-CCITT。
 * 失败最多重试 3 次，耗尽后丢掉该帧并继续。
 */
void demo_spi(void);

#endif
