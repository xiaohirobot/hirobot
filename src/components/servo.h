#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

/*
 * 两路 MG995 的 PWM 驱动。demo 和以后的整机逻辑都走这里。
 *   SERVO_CHANNEL_1：PA2，TIM2_CH3，AF1
 *   SERVO_CHANNEL_2：PA3，TIM2_CH4，AF1
 * 时钟是复位后的 HSI 16 MHz，TIM2 预分频 16，1 个计数 = 1 us，帧周期 20 ms。
 * init 只配置定时器，输出保持关闭。脉宽超界或取整失败会锁存该路故障并关掉输出。
 */

#define SERVO_CH_COUNT     2u
#define SERVO_CHANNEL_1    0u
#define SERVO_CHANNEL_2    1u
#define SERVO_FRAME_US     20000u
#define SERVO_MIN_PULSE_US 500u
#define SERVO_MAX_PULSE_US 2500u

typedef enum {
    SERVO_OK = 0,
    SERVO_INVALID_ARG = 1,
    SERVO_INVALID_STATE = 2,
    SERVO_NOT_READY = 3,
    SERVO_UNSUPPORTED = 4,
} ServoStatus;

ServoStatus servo_init(void);
ServoStatus servo_set_pulse_us(unsigned channel, uint16_t pulse_us);
void servo_stop(unsigned channel);
void servo_stop_all(void);
uint16_t servo_actual_pulse_us(unsigned channel);

#endif
