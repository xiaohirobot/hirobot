#include "components/servo.h"

#include <stdbool.h>

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
#define TIM2_CCR4  (*(volatile uint32_t *)0x40000040UL)

#define TIM_CR1_CEN         (1U << 0)
#define TIM_EGR_UG          (1U << 0)
#define TIM_CCER_CC3E       (1U << 8)
#define TIM_CCER_CC4E       (1U << 12)
#define TIM_CCMR2_OC3M_PWM1 (6U << 4)
#define TIM_CCMR2_OC4M_PWM1 (6U << 12)

#define TIM2_CCR3_ADDR 0x4000003CUL
#define TIM2_CCR4_ADDR 0x40000040UL

typedef struct {
    uint32_t ccr_addr;
    uint32_t enable_bit;
    bool initialized;
    bool faulted;
    uint32_t actual_pulse_us;
} Servo;

static Servo g_servo[SERVO_CH_COUNT];
static bool g_timer_ready;

static const uint32_t k_ccr_addr[SERVO_CH_COUNT] = {
    TIM2_CCR3_ADDR,
    TIM2_CCR4_ADDR,
};
static const uint32_t k_enable_bit[SERVO_CH_COUNT] = {
    TIM_CCER_CC3E,
    TIM_CCER_CC4E,
};

static void servo_pwm_stop(Servo *device)
{
    if (device->ccr_addr == 0u) {
        return;
    }
    TIM2_CCER &= ~device->enable_bit;
    *(volatile uint32_t *)device->ccr_addr = 0;
    device->actual_pulse_us = 0;
}

static void tim2_init(void)
{
    RCC_AHB1ENR |= 1U << 0; /* GPIOAEN */
    RCC_APB1ENR |= 1U << 0; /* TIM2EN */
    (void)RCC_AHB1ENR;
    (void)RCC_APB1ENR;

    /* PA2、PA3 = AF1（TIM2_CH3、TIM2_CH4）。 */
    GPIOA_MODER &= ~((3U << (2U * 2U)) | (3U << (3U * 2U)));
    GPIOA_MODER |= (2U << (2U * 2U)) | (2U << (3U * 2U));
    GPIOA_AFRL &= ~((0xFU << (2U * 4U)) | (0xFU << (3U * 4U)));
    GPIOA_AFRL |= (1U << (2U * 4U)) | (1U << (3U * 4U));

    TIM2_CR1 = 0;
    TIM2_PSC = 16U - 1U; /* 16 MHz / 16 = 1 MHz */
    TIM2_ARR = SERVO_FRAME_US - 1U;
    TIM2_CCR3 = 0;
    TIM2_CCR4 = 0;
    TIM2_CCMR2 = TIM_CCMR2_OC3M_PWM1 | TIM_CCMR2_OC4M_PWM1;
    TIM2_CCER &= ~(TIM_CCER_CC3E | TIM_CCER_CC4E);
    TIM2_EGR = TIM_EGR_UG;
    TIM2_CR1 = TIM_CR1_CEN;
}

ServoStatus servo_init(void)
{
    unsigned i;

    if (g_timer_ready) {
        return SERVO_INVALID_STATE;
    }

    tim2_init();
    for (i = 0; i < SERVO_CH_COUNT; i++) {
        g_servo[i].ccr_addr = k_ccr_addr[i];
        g_servo[i].enable_bit = k_enable_bit[i];
        g_servo[i].initialized = true;
        g_servo[i].faulted = false;
        g_servo[i].actual_pulse_us = 0;
    }
    g_timer_ready = true;
    return SERVO_OK;
}

ServoStatus servo_set_pulse_us(unsigned channel, uint16_t pulse_us)
{
    Servo *device;
    uint32_t duty;
    uint32_t actual;

    if (channel >= SERVO_CH_COUNT) {
        return SERVO_INVALID_ARG;
    }
    device = &g_servo[channel];
    if (!device->initialized) {
        return SERVO_NOT_READY;
    }
    if (device->faulted) {
        return SERVO_INVALID_STATE;
    }
    if (pulse_us < SERVO_MIN_PULSE_US || pulse_us > SERVO_MAX_PULSE_US) {
        device->faulted = true;
        servo_pwm_stop(device);
        return SERVO_INVALID_ARG;
    }

    /* 先换成万分比占空，再四舍五入回 us。 */
    duty = (pulse_us * 10000u + SERVO_FRAME_US / 2u) / SERVO_FRAME_US;
    if (duty == 0u || duty >= 10000u) {
        device->faulted = true;
        servo_pwm_stop(device);
        return SERVO_UNSUPPORTED;
    }
    actual = (duty * SERVO_FRAME_US + 5000u) / 10000u;
    if (actual < SERVO_MIN_PULSE_US || actual > SERVO_MAX_PULSE_US) {
        device->faulted = true;
        servo_pwm_stop(device);
        return SERVO_UNSUPPORTED;
    }

    *(volatile uint32_t *)device->ccr_addr = actual;
    TIM2_CCER |= device->enable_bit;
    device->actual_pulse_us = actual;
    return SERVO_OK;
}

void servo_stop(unsigned channel)
{
    if (channel >= SERVO_CH_COUNT) {
        return;
    }
    servo_pwm_stop(&g_servo[channel]);
}

void servo_stop_all(void)
{
    unsigned i;

    for (i = 0; i < SERVO_CH_COUNT; i++) {
        servo_stop(i);
    }
}

uint16_t servo_actual_pulse_us(unsigned channel)
{
    if (channel >= SERVO_CH_COUNT) {
        return 0;
    }
    return (uint16_t)g_servo[channel].actual_pulse_us;
}
