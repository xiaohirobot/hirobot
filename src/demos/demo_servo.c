#include "demos/demo_servo.h"

#include "uart3.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * MG995 经转接板接在 CN1 PWM1（PA2，TIM2_CH3，AF1）。
 * 本工程时钟保持复位后的 HSI 16 MHz，APB1 预分频为 1，TIM2 时钟也是 16 MHz。
 * 预分频 16 得到 1 MHz，1 个计数 = 1 us，和串口 BRR 用的是同一套时钟。
 *
 * 生命周期沿用原 demo：init 只配置、输出保持停止；脉宽超界或启动失败会锁存故障并关掉输出。
 */

#define SERVO_MIN_PULSE_US  500u
#define SERVO_MAX_PULSE_US 2500u
#define SERVO_CENTER_US    1500u
#define SERVO_FRAME_US    20000u
#define DEMO_STEP_US         10u
#define DEMO_TICK_MS         20u

#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB1ENR (*(volatile uint32_t *)0x40023840UL)

#define GPIOA_MODER (*(volatile uint32_t *)0x40020000UL)
#define GPIOA_AFRL  (*(volatile uint32_t *)0x40020020UL)

#define TIM2_CR1   (*(volatile uint32_t *)0x40000000UL)
#define TIM2_EGR   (*(volatile uint32_t *)0x40000014UL)
#define TIM2_CCMR2 (*(volatile uint32_t *)0x4000001CUL)
#define TIM2_CCER  (*(volatile uint32_t *)0x40000020UL)
#define TIM2_PSC   (*(volatile uint32_t *)0x40000028UL)
#define TIM2_ARR   (*(volatile uint32_t *)0x4000002CUL)
#define TIM2_CCR3  (*(volatile uint32_t *)0x4000003CUL)

#define TIM_CR1_CEN   (1U << 0)
#define TIM_EGR_UG    (1U << 0)
#define TIM_CCER_CC3E (1U << 8)
#define TIM_CCMR2_OC3M_PWM1 (6U << 4) /* PWM 模式 1：CNT < CCR 时输出高 */

#define SYSTICK_CTRL      (*(volatile uint32_t *)0xE000E010UL)
#define SYSTICK_LOAD      (*(volatile uint32_t *)0xE000E014UL)
#define SYSTICK_VAL       (*(volatile uint32_t *)0xE000E018UL)
#define SYSTICK_COUNTFLAG (1U << 16)

typedef enum {
    SERVO_OK = 0,
    SERVO_INVALID_ARG = 1,
    SERVO_INVALID_STATE = 2,
    SERVO_NOT_READY = 3,
    SERVO_UNSUPPORTED = 4,
} ServoStatus;

typedef struct {
    uint32_t frame_us;
    uint16_t min_pulse_us;
    uint16_t max_pulse_us;
    bool initialized;
    bool faulted;
    uint32_t actual_pulse_us;
    ServoStatus cleanup_status;
} Servo;

static void delay_ms(unsigned ms)
{
    while (ms > 0U) {
        SYSTICK_LOAD = 16000U - 1U;
        SYSTICK_VAL = 0;
        SYSTICK_CTRL = (1U << 2) | (1U << 0);
        while ((SYSTICK_CTRL & SYSTICK_COUNTFLAG) == 0) {
        }
        SYSTICK_CTRL = 0;
        ms--;
    }
}

static ServoStatus servo_pwm_stop(Servo *device)
{
    (void)device;
    TIM2_CCER &= ~TIM_CCER_CC3E;
    TIM2_CCR3 = 0;
    return SERVO_OK;
}

/* 配好 50 Hz，通道先不使能，满足 init 时输出停止。 */
static ServoStatus servo_init(Servo *device, uint32_t frame_us,
                              uint16_t min_pulse_us, uint16_t max_pulse_us)
{
    if (!device || !frame_us || !min_pulse_us || min_pulse_us > max_pulse_us ||
        max_pulse_us >= frame_us) {
        return SERVO_INVALID_ARG;
    }
    if (device->initialized || device->faulted) {
        return SERVO_INVALID_STATE;
    }

    RCC_AHB1ENR |= 1U << 0; /* GPIOAEN */
    RCC_APB1ENR |= 1U << 0; /* TIM2EN */
    (void)RCC_AHB1ENR;
    (void)RCC_APB1ENR;

    /* PA2 = AF1（TIM2_CH3）。 */
    GPIOA_MODER &= ~(3U << (2U * 2U));
    GPIOA_MODER |= 2U << (2U * 2U);
    GPIOA_AFRL &= ~(0xFU << (2U * 4U));
    GPIOA_AFRL |= 1U << (2U * 4U);

    TIM2_CR1 = 0;
    TIM2_PSC = 16U - 1U;          /* 16 MHz / 16 = 1 MHz */
    TIM2_ARR = frame_us - 1U;     /* 20000 计数 = 20 ms */
    TIM2_CCR3 = 0;
    TIM2_CCMR2 = TIM_CCMR2_OC3M_PWM1;
    TIM2_CCER &= ~TIM_CCER_CC3E;
    TIM2_EGR = TIM_EGR_UG;
    TIM2_CR1 = TIM_CR1_CEN;

    device->frame_us = frame_us;
    device->min_pulse_us = min_pulse_us;
    device->max_pulse_us = max_pulse_us;
    device->initialized = true;
    device->actual_pulse_us = 0;
    return SERVO_OK;
}

static ServoStatus servo_set_pulse_us(Servo *device, uint16_t pulse_us)
{
    if (!device) {
        return SERVO_INVALID_ARG;
    }
    if (!device->initialized) {
        return SERVO_NOT_READY;
    }
    if (device->faulted) {
        return SERVO_INVALID_STATE;
    }
    if (pulse_us < device->min_pulse_us || pulse_us > device->max_pulse_us) {
        device->faulted = true;
        device->cleanup_status = servo_pwm_stop(device);
        return SERVO_INVALID_ARG;
    }
    if (pulse_us >= device->frame_us) {
        return SERVO_UNSUPPORTED;
    }

    TIM2_CCR3 = pulse_us;
    TIM2_CCER |= TIM_CCER_CC3E;
    device->actual_pulse_us = pulse_us;
    return SERVO_OK;
}

static ServoStatus servo_release(Servo *device)
{
    ServoStatus status;

    if (!device) {
        return SERVO_INVALID_ARG;
    }
    if (!device->initialized && !device->faulted) {
        return SERVO_NOT_READY;
    }

    status = servo_pwm_stop(device);
    device->cleanup_status = status;
    if (status != SERVO_OK) {
        device->faulted = true;
        return status;
    }
    device->actual_pulse_us = 0;
    return SERVO_OK;
}

static void servo_hold_stopped(Servo *device)
{
    (void)servo_release(device);
    uart3_puts("servo fault, output stopped\n");
    while (1) {
    }
}

void demo_servo(void)
{
    Servo servo = {0};
    uint32_t pulse = SERVO_CENTER_US;
    int direction = 1;
    unsigned log_div = 0;

    uart3_init();
    uart3_puts("servo PA2 TIM2_CH3 50Hz\n");

    if (servo_init(&servo, SERVO_FRAME_US, SERVO_MIN_PULSE_US,
                   SERVO_MAX_PULSE_US) != SERVO_OK ||
        servo_set_pulse_us(&servo, SERVO_CENTER_US) != SERVO_OK) {
        servo_hold_stopped(&servo);
    }

    uart3_puts("center ");
    uart3_putu(SERVO_CENTER_US);
    uart3_puts(" us\n");

    while (1) {
        delay_ms(DEMO_TICK_MS);
        if (servo.faulted) {
            servo_hold_stopped(&servo);
        }

        pulse = (uint32_t)((int32_t)pulse + direction * (int32_t)DEMO_STEP_US);
        if (pulse >= SERVO_MAX_PULSE_US) {
            pulse = SERVO_MAX_PULSE_US;
            direction = -1;
        } else if (pulse <= SERVO_MIN_PULSE_US) {
            pulse = SERVO_MIN_PULSE_US;
            direction = 1;
        }

        if (servo_set_pulse_us(&servo, (uint16_t)pulse) != SERVO_OK) {
            if (servo.faulted) {
                servo_hold_stopped(&servo);
            }
            continue;
        }

        /* 每 500 ms 打一行，避免 50 Hz 把串口刷满。 */
        log_div++;
        if (log_div >= 25U) {
            log_div = 0;
            uart3_puts("pulse ");
            uart3_putu(pulse);
            uart3_puts(" us\n");
        }
    }
}
