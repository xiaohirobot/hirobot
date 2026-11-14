#ifndef SERVO_ANGLE_H
#define SERVO_ANGLE_H

#include "components/servo.h"

#include <stdint.h>

/*
 * 在脉宽驱动之上记两路角度。
 * 0° 是 1500 us，±90° 对应 500–2500 us。
 * move 是相对转角，超界先夹到 ±90° 再写脉宽。
 */

ServoStatus servo_angle_init(void);
ServoStatus servo_angle_move(int32_t delta1, int32_t delta2);
int32_t servo_angle_deg(unsigned channel);

#endif
