#include "tests/demo_spi.h"

#include "components/spi1_slave.h"
#include "components/uart3.h"

#include <stdint.h>

/*
 * 和 main_robot 同一套从机，只应答、不驱动舵机。
 * SET 先记下通道和脉宽；查询拍再打印。打印放在查询里，
 * 避免挡住 SET 之后马上要预装的下一帧。
 */

static uint8_t g_status = SPI1_ACK_OK;
static uint8_t g_channel;
static uint16_t g_pulse;

static void on_frame(const uint8_t *rx)
{
    uint8_t seq = rx[SPI1_OFF_SEQ];
    uint8_t len = rx[SPI1_OFF_LEN];
    uint8_t cmd = rx[SPI1_OFF_CMD];

    if (cmd == SPI1_CMD_SET && len >= 4u) {
        g_status = SPI1_ACK_OK;
        g_channel = rx[SPI1_OFF_CH];
        g_pulse = (uint16_t)rx[SPI1_OFF_PULSE] |
                  (uint16_t)((uint16_t)rx[SPI1_OFF_PULSE + 1u] << 8);
        spi1_slave_set_reply(seq, g_status, g_channel, g_pulse);
        return;
    }
    if (cmd == SPI1_CMD_QUERY) {
        spi1_slave_set_reply(seq, g_status, g_channel, g_pulse);
        uart3_puts("ch=");
        uart3_putu(g_channel);
        uart3_puts(" pulse=");
        uart3_putu(g_pulse);
        uart3_puts("\n");
        return;
    }
    g_status = SPI1_ACK_REJECT;
    spi1_slave_set_reply(seq, g_status, g_channel, g_pulse);
}

void demo_spi(void)
{
    uart3_init();
    uart3_puts("spi slave, same frame as servo link\n");
    spi1_slave_init();
    spi1_slave_set_reply(0, SPI1_ACK_OK, 0, 0);
    spi1_slave_run(on_frame);
}
