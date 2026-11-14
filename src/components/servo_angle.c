#include "components/servo_angle.h"

#define ANGLE_MIN       (-90)
#define ANGLE_MAX       90
#define SERVO_CENTER_US 1500
#define US_SPAN         2000
#define DEG_SPAN        180

static int32_t g_angle[SERVO_CH_COUNT];

static int32_t clamp_add(int32_t cur, int32_t delta)
{
    if (delta >= 0) {
        if (delta > ANGLE_MAX - cur) {
            return ANGLE_MAX;
        }
    } else if (delta < ANGLE_MIN - cur) {
        return ANGLE_MIN;
    }
    return cur + delta;
}

/* pulse = 1500 + angle * 2000 / 180，四舍五入。angle 已在 ±90 内。 */
static uint16_t angle_to_pulse(int32_t angle)
{
    int32_t mag = angle < 0 ? -angle : angle;
    int32_t rounded = (mag * US_SPAN + DEG_SPAN / 2) / DEG_SPAN;

    if (angle < 0) {
        rounded = -rounded;
    }
    return (uint16_t)(SERVO_CENTER_US + rounded);
}

ServoStatus servo_angle_init(void)
{
    unsigned channel;
    ServoStatus status;

    status = servo_init();
    if (status != SERVO_OK) {
        return status;
    }
    for (channel = 0; channel < SERVO_CH_COUNT; channel++) {
        g_angle[channel] = 0;
        status = servo_set_pulse_us(channel, SERVO_CENTER_US);
        if (status != SERVO_OK) {
            return status;
        }
    }
    return SERVO_OK;
}

ServoStatus servo_angle_move(int32_t delta1, int32_t delta2)
{
    int32_t next[SERVO_CH_COUNT];
    unsigned channel;
    ServoStatus status;

    next[SERVO_CHANNEL_1] = clamp_add(g_angle[SERVO_CHANNEL_1], delta1);
    next[SERVO_CHANNEL_2] = clamp_add(g_angle[SERVO_CHANNEL_2], delta2);
    for (channel = 0; channel < SERVO_CH_COUNT; channel++) {
        status = servo_set_pulse_us(channel, angle_to_pulse(next[channel]));
        if (status != SERVO_OK) {
            return status;
        }
    }
    g_angle[SERVO_CHANNEL_1] = next[SERVO_CHANNEL_1];
    g_angle[SERVO_CHANNEL_2] = next[SERVO_CHANNEL_2];
    return SERVO_OK;
}

int32_t servo_angle_deg(unsigned channel)
{
    if (channel >= SERVO_CH_COUNT) {
        return 0;
    }
    return g_angle[channel];
}
