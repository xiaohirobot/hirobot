#include "demos/demo_spi.h"

#include "components/delay.h"
#include "components/spi1.h"
#include "components/uart3.h"

#include <stdint.h>

/*
 * P_SPI_FRAME，一次事务 CS 全程为低，主机时钟走完整帧，从机在同一拍里把应答移出来。
 *   请求：{ magic=0xA5, seq, len, payload[16] }，其余字节填 0xFF
 *   应答：{ magic=0x5A, seq, len, payload[16], crc16-lo, crc16-hi }
 * CRC-16/CCITT（初值 0xFFFF，多项式 0x1021）覆盖应答的前 19 字节。
 *
 * 时钟保持 HSI 16 MHz，和串口、舵机一致。SCK 因此是 2 MHz，
 * 原来 HAL 版本在 168 MHz 下是 2.625 MHz。Mode 0 没变。
 *
 * 错误注入默认关闭。编译时加 -DMOCE_TEST_FAULT_INJECTION=N，
 * 前 N 次事务走超时路径，用来看重试和丢帧。
 */

#ifndef SPI_POLL_PERIOD_MS
#define SPI_POLL_PERIOD_MS 1000u
#endif
#ifndef SPI_MAX_RETRIES
#define SPI_MAX_RETRIES 3u
#endif
#ifndef SPI_TIMEOUT_MS
#define SPI_TIMEOUT_MS 50u
#endif

#ifndef MOCE_TEST_FAULT_INJECTION
#define MOCE_TEST_FAULT_INJECTION 0
#endif

#define SPI_PAYLOAD_MAX 16u
#define REQ_HDR         3u
#define RSP_TAIL        2u
#define SPI_FRAME_SIZE  (REQ_HDR + SPI_PAYLOAD_MAX + RSP_TAIL)
#define REQ_MAGIC       0xA5u
#define RSP_MAGIC       0x5Au

#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830UL)
#define GPIOE_MODER (*(volatile uint32_t *)0x40021000UL)
#define GPIOE_ODR   (*(volatile uint32_t *)0x40021014UL)
#define GPIOE_BSRR  (*(volatile uint32_t *)0x40021018UL)
#define LED_PIN     7U

static uint8_t g_req_frame[SPI_FRAME_SIZE];
static uint8_t g_rsp_frame[SPI_FRAME_SIZE];
static uint8_t g_req_pending;
static uint8_t g_req_retries;
static uint8_t g_req_seq;
static uint32_t g_dropped;

#if MOCE_TEST_FAULT_INJECTION
static uint32_t g_injected_faults_left = MOCE_TEST_FAULT_INJECTION;
#endif

static void led_init(void)
{
    RCC_AHB1ENR |= 1U << 4; /* GPIOEEN */
    (void)RCC_AHB1ENR;
    GPIOE_MODER &= ~(3U << (LED_PIN * 2U));
    GPIOE_MODER |= 1U << (LED_PIN * 2U);
    GPIOE_BSRR = 1U << (LED_PIN + 16U);
}

static void led_toggle(void)
{
    GPIOE_ODR ^= 1U << LED_PIN;
}

static uint16_t crc16_ccitt(const uint8_t *data, unsigned len)
{
    uint16_t crc = 0xFFFFu;
    unsigned i;

    for (i = 0; i < len; i++) {
        unsigned bit;

        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (bit = 0; bit < 8u; bit++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static void build_request(const uint8_t *payload, uint8_t len)
{
    unsigned i;

    if (len > SPI_PAYLOAD_MAX) {
        len = SPI_PAYLOAD_MAX;
    }
    for (i = 0; i < SPI_FRAME_SIZE; i++) {
        g_req_frame[i] = 0xFFu;
    }
    g_req_frame[0] = REQ_MAGIC;
    g_req_frame[1] = ++g_req_seq;
    g_req_frame[2] = len;
    for (i = 0; i < len; i++) {
        g_req_frame[REQ_HDR + i] = payload[i];
    }
    g_req_retries = 0;
    g_req_pending = 1u;
}

/* 成功返回 0，并写出 payload。失败返回 -1 magic、-2 seq、-3 长度、-4 CRC。 */
static int parse_response(uint8_t *app_out, uint8_t *app_len)
{
    uint16_t crc_calc;
    uint16_t crc_recv;
    unsigned i;
    uint8_t len = g_rsp_frame[2];

    if (g_rsp_frame[0] != RSP_MAGIC) {
        return -1;
    }
    if (g_rsp_frame[1] != g_req_frame[1]) {
        return -2;
    }
    if (len > SPI_PAYLOAD_MAX) {
        return -3;
    }
    crc_calc = crc16_ccitt(g_rsp_frame, REQ_HDR + SPI_PAYLOAD_MAX);
    crc_recv = (uint16_t)g_rsp_frame[REQ_HDR + SPI_PAYLOAD_MAX] |
               (uint16_t)((uint16_t)g_rsp_frame[REQ_HDR + SPI_PAYLOAD_MAX + 1u] << 8);
    if (crc_calc != crc_recv) {
        return -4;
    }
    *app_len = len;
    for (i = 0; i < len; i++) {
        app_out[i] = g_rsp_frame[REQ_HDR + i];
    }
    return 0;
}

static int do_transaction(void)
{
    uint8_t app_payload[SPI_PAYLOAD_MAX];
    uint8_t app_len = 0;
    unsigned elapsed_ms = 0;
    unsigned i;
    int status;
    int parsed;

    for (i = 0; i < SPI_FRAME_SIZE; i++) {
        g_rsp_frame[i] = 0;
    }

#if MOCE_TEST_FAULT_INJECTION
    if (g_injected_faults_left > 0u) {
        g_injected_faults_left--;
        status = SPI1_TIMEOUT;
        elapsed_ms = SPI_TIMEOUT_MS;
    } else
#endif
    {
        status = spi1_transceive(g_req_frame, g_rsp_frame, SPI_FRAME_SIZE,
                                 SPI_TIMEOUT_MS, &elapsed_ms);
    }

    if (status == SPI1_TIMEOUT) {
        uart3_puts("[SPI] timeout seq=");
        uart3_putu(g_req_frame[1]);
        uart3_puts(" elapsed=");
        uart3_putu(elapsed_ms);
        uart3_puts(" ms\n");
        return -1;
    }
    if (status != SPI1_OK) {
        uart3_puts("[SPI] error seq=");
        uart3_putu(g_req_frame[1]);
        uart3_puts("\n");
        return -2;
    }

    parsed = parse_response(app_payload, &app_len);
    if (parsed != 0) {
        uart3_puts("[SPI] parse fail seq=");
        uart3_putu(g_req_frame[1]);
        uart3_puts(" code=");
        uart3_putu((unsigned)(-parsed));
        uart3_puts("\n");
        return -3;
    }

    uart3_puts("[SPI] ok seq=");
    uart3_putu(g_req_frame[1]);
    uart3_puts(" len=");
    uart3_putu(app_len);
    uart3_puts(" data0=");
    uart3_putu(app_len == 0u ? 0u : app_payload[0]);
    uart3_puts("\n");
    return 0;
}

void demo_spi(void)
{
    uint32_t req_counter = 0;

    uart3_init();
    led_init();
    if (spi1_init() != SPI1_OK) {
        uart3_puts("[APP] spi init fail\n");
        while (1) {
        }
    }

    uart3_puts("[APP] spi master up, poll=");
    uart3_putu(SPI_POLL_PERIOD_MS);
    uart3_puts(" ms\n");

    while (1) {
        int rc;

        if (!g_req_pending) {
            uint8_t payload[4];

            /* 轮询周期到了才组新帧。重试帧不等这一拍。 */
            delay_ms(SPI_POLL_PERIOD_MS);
            led_toggle();
            req_counter++;
            payload[0] = (uint8_t)(req_counter & 0xFFu);
            payload[1] = (uint8_t)((req_counter >> 8) & 0xFFu);
            payload[2] = 0;
            payload[3] = 0;
            build_request(payload, 4u);
        }

        rc = do_transaction();
        if (rc == 0) {
            g_req_pending = 0;
        } else if (g_req_retries < SPI_MAX_RETRIES) {
            g_req_retries++;
            uart3_puts("[SPI] requeued seq=");
            uart3_putu(g_req_frame[1]);
            uart3_puts(" retry=");
            uart3_putu(g_req_retries);
            uart3_puts("\n");
        } else {
            g_dropped++;
            g_req_pending = 0;
            uart3_puts("[SPI] retries exhausted seq=");
            uart3_putu(g_req_frame[1]);
            uart3_puts(" dropped=");
            uart3_putu(g_dropped);
            uart3_puts("\n");
        }
    }
}
