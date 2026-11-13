#include "demos/demo_servo.h"

#include "components/delay.h"
#include "components/servo.h"
#include "components/uart3.h"

#include <stdint.h>

/*
 * 当前的扫动演示。整机逻辑换上来之后，这个文件可以整份替换。
 * 上电先到中位，然后每 20 ms 按 25 us 在驱动允许的脉宽范围内往复。
 * 两路使用同一条脉宽。驱动报错后关掉两路并停住。
 */

#define SERVO_CENTER_US     1500u
#define SERVO_SWEEP_STEP_US   25u
#define DEMO_TICK_MS          20u

static void demo_fail(void)
{
    servo_stop_all();
    uart3_puts("servo fault, output stopped\n");
    while (1) {
    }
}

void demo_servo(void)
{
    uint32_t pulse = SERVO_CENTER_US;
    int direction = 1;
    unsigned channel;
    unsigned log_div = 0;

    uart3_init();
    uart3_puts("servo PA2 CH3 + PA3 CH4 50Hz\n");

    if (servo_init() != SERVO_OK) {
        demo_fail();
    }
    for (channel = 0; channel < SERVO_CH_COUNT; channel++) {
        if (servo_set_pulse_us(channel, SERVO_CENTER_US) != SERVO_OK) {
            demo_fail();
        }
    }

    uart3_puts("center ");
    uart3_putu(SERVO_CENTER_US);
    uart3_puts(" us\n");

    while (1) {
        delay_ms(DEMO_TICK_MS);

        for (channel = 0; channel < SERVO_CH_COUNT; channel++) {
            if (servo_set_pulse_us(channel, (uint16_t)pulse) != SERVO_OK) {
                demo_fail();
            }
        }

        if (direction > 0) {
            if (pulse + SERVO_SWEEP_STEP_US <= SERVO_MAX_PULSE_US) {
                pulse += SERVO_SWEEP_STEP_US;
            } else {
                direction = -1;
            }
        } else if (pulse >= SERVO_MIN_PULSE_US + SERVO_SWEEP_STEP_US) {
            pulse -= SERVO_SWEEP_STEP_US;
        } else {
            direction = 1;
        }

        log_div++;
        if (log_div >= 25U) {
            log_div = 0;
            uart3_puts("pulse ");
            uart3_putu(servo_actual_pulse_us(SERVO_CHANNEL_1));
            uart3_puts(" us\n");
        }
    }
}
