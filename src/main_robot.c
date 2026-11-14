#include "main_robot.h"

#include "components/servo.h"
#include "components/spi1_slave.h"
#include "components/uart3.h"

#include <stdint.h>

/*
 * 舵机接在从机回调上。帧收发在 components/spi1_slave。
 * 命令 0 只把上一份应答再装出去。命令 1 才改脉宽。
 * 回调要快，主机两拍之间只留了几毫秒。
 */

static uint8_t g_status = SPI1_ACK_OK;
static uint8_t g_channel;
static uint16_t g_pulse = 1500u;

static void on_frame(const uint8_t *rx)
{
    uint8_t seq = rx[SPI1_OFF_SEQ];
    uint8_t len = rx[SPI1_OFF_LEN];
    uint8_t cmd = rx[SPI1_OFF_CMD];
    uint8_t channel;
    uint16_t pulse;

    if (cmd == SPI1_CMD_QUERY) {
        spi1_slave_set_reply(seq, g_status, g_channel, g_pulse);
        return;
    }
    if (cmd != SPI1_CMD_SET || len < 4u) {
        g_status = SPI1_ACK_REJECT;
        spi1_slave_set_reply(seq, g_status, g_channel, g_pulse);
        return;
    }
    channel = rx[SPI1_OFF_CH];
    pulse = (uint16_t)rx[SPI1_OFF_PULSE] |
            (uint16_t)((uint16_t)rx[SPI1_OFF_PULSE + 1u] << 8);
    if (channel >= SERVO_CH_COUNT || pulse < SERVO_MIN_PULSE_US ||
        pulse > SERVO_MAX_PULSE_US ||
        servo_set_pulse_us(channel, pulse) != SERVO_OK) {
        g_status = SPI1_ACK_REJECT;
        spi1_slave_set_reply(seq, g_status, channel, g_pulse);
        return;
    }
    g_status = SPI1_ACK_OK;
    g_channel = channel;
    g_pulse = servo_actual_pulse_us(channel);
    spi1_slave_set_reply(seq, g_status, g_channel, g_pulse);
}

void main_robot(void)
{
    unsigned int channel;

    uart3_init();
    uart3_puts("servo link PA2+PA3, spi slave\n");
    if (servo_init() != SERVO_OK) {
        uart3_puts("servo init failed\n");
        while (1) {
        }
    }
    for (channel = 0; channel < SERVO_CH_COUNT; channel++) {
        if (servo_set_pulse_us(channel, 1500u) != SERVO_OK) {
            uart3_puts("servo center failed\n");
            while (1) {
            }
        }
    }

    spi1_slave_init();
    spi1_slave_set_reply(0, SPI1_ACK_OK, 0, 1500u);
    uart3_puts("center 1500 us, waiting cs\n");
    spi1_slave_run(on_frame);
}
