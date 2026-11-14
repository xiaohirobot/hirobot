#include "tests/demo_can_slave.h"

#include "components/can1.h"
#include "components/servo_angle.h"
#include "tests/demo_uart3_cmd.h"

#include <stdint.h>

static void store_i32(uint8_t *p, int32_t value)
{
    uint32_t bits = (uint32_t)value;

    p[0] = (uint8_t)bits;
    p[1] = (uint8_t)(bits >> 8);
    p[2] = (uint8_t)(bits >> 16);
    p[3] = (uint8_t)(bits >> 24);
}

static int32_t load_i32(const uint8_t *p)
{
    uint32_t bits = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    return (int32_t)bits;
}

static int add_i(int32_t a, int32_t b, int32_t *sum)
{
    if ((b > 0 && a > INT32_MAX - b) || (b < 0 && a < INT32_MIN - b)) {
        return 0;
    }
    *sum = a + b;
    return 1;
}

static int g_servo_ready;

static void servo_fail(void)
{
    (void)can1_send((uint16_t)(DEMO_MODE_SERVO_REMOTE + 1), 0, 0);
    servo_stop_all();
    while (1) {
    }
}

static void on_add(const uint8_t *data)
{
    int32_t sum;
    uint8_t reply[4];

    if (!add_i(load_i32(data), load_i32(data + 4), &sum)) {
        (void)can1_send((uint16_t)(DEMO_MODE_ADD_REMOTE + 1), 0, 0);
        return;
    }
    store_i32(reply, sum);
    (void)can1_send((uint16_t)(DEMO_MODE_ADD_REMOTE + 1), reply, 4);
}

static void on_servo(const uint8_t *data)
{
    int32_t delta1 = load_i32(data);
    int32_t delta2 = load_i32(data + 4);
    uint8_t reply[8];

    if (servo_angle_move(delta1, delta2) != SERVO_OK) {
        servo_fail();
    }
    store_i32(reply, servo_angle_deg(SERVO_CHANNEL_1));
    store_i32(reply + 4, servo_angle_deg(SERVO_CHANNEL_2));
    (void)can1_send((uint16_t)(DEMO_MODE_SERVO_REMOTE + 1), reply, 8);
}

void demo_can_slave(void)
{
    if (can1_init() != 0) {
        while (1) {
        }
    }
    g_servo_ready = (servo_angle_init() == SERVO_OK);
    if (!g_servo_ready) {
        servo_stop_all();
    }

    while (1) {
        uint16_t id = 0;
        uint8_t data[8];
        int dlc = can1_recv(&id, data, 0);

        if (dlc != 8) {
            continue;
        }
        if (id == DEMO_MODE_ADD_REMOTE) {
            on_add(data);
        } else if (id == DEMO_MODE_SERVO_REMOTE) {
            if (!g_servo_ready) {
                (void)can1_send((uint16_t)(DEMO_MODE_SERVO_REMOTE + 1), 0, 0);
                continue;
            }
            on_servo(data);
        }
    }
}
