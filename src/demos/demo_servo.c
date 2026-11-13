#include "demos/demo_servo.h"

#include "uart3.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * 两路 MG995，经转接板 CN1。
 *   PWM1：PA2，TIM2_CH3，AF1。新固件 main.c 写明的就是这一路。
 *   PWM2：PA3，TIM2_CH4，AF1。同一组 TIM2 上相邻的第二路。
 * 时钟仍是复位后的 HSI 16 MHz。APB1 预分频为 1，TIM2 也是 16 MHz。
 * 预分频 16 得到 1 MHz，1 个计数 = 1 us。
 *
 * 扫动跟新固件：先输出中位，之后每 20 ms 按 25 us 在 500~2500 us 内往复，
 * 脉宽先按万分比取整再写入 CCR。超界或取整失败会关掉两路输出并停住。
 */

#define SERVO_MIN_PULSE_US   500u
#define SERVO_MAX_PULSE_US  2500u
#define SERVO_CENTER_US     1500u
#define SERVO_FRAME_US     20000u
#define SERVO_SWEEP_STEP_US   25u
#define DEMO_TICK_MS          20u
#define SERVO_COUNT            2u

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
#define TIM_CCMR2_OC3M_PWM1 (6U << 4)  /* CH3：CNT < CCR 时为高 */
#define TIM_CCMR2_OC4M_PWM1 (6U << 12) /* CH4 */

#define TIM2_CCR3_ADDR 0x4000003CUL
#define TIM2_CCR4_ADDR 0x40000040UL

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
    uint32_t ccr_addr;
    uint32_t enable_bit;
    uint32_t frame_us;
    uint16_t min_pulse_us;
    uint16_t max_pulse_us;
    bool initialized;
    bool faulted;
    uint32_t actual_pulse_us;
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

static void servo_pwm_stop(Servo *device)
{
    if (device->ccr_addr == 0u) {
        return;
    }
    TIM2_CCER &= ~device->enable_bit;
    *(volatile uint32_t *)device->ccr_addr = 0;
}

static void servo_stop_all(Servo *devices, unsigned count)
{
    unsigned i;

    for (i = 0; i < count; i++) {
        servo_pwm_stop(&devices[i]);
        devices[i].actual_pulse_us = 0;
    }
}

/* 两路通道一次配好，输出先保持关闭。 */
static void tim2_init(uint32_t frame_us)
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
    TIM2_PSC = 16U - 1U;
    TIM2_ARR = frame_us - 1U;
    TIM2_CCR3 = 0;
    TIM2_CCR4 = 0;
    TIM2_CCMR2 = TIM_CCMR2_OC3M_PWM1 | TIM_CCMR2_OC4M_PWM1;
    TIM2_CCER &= ~(TIM_CCER_CC3E | TIM_CCER_CC4E);
    TIM2_EGR = TIM_EGR_UG;
    TIM2_CR1 = TIM_CR1_CEN;
}

static ServoStatus servo_init(Servo *device, uint32_t ccr_addr, uint32_t enable_bit,
                              uint32_t frame_us, uint16_t min_pulse_us,
                              uint16_t max_pulse_us)
{
    if (!device || !frame_us || !min_pulse_us || min_pulse_us > max_pulse_us ||
        max_pulse_us >= frame_us) {
        return SERVO_INVALID_ARG;
    }
    if (device->initialized || device->faulted) {
        return SERVO_INVALID_STATE;
    }

    device->ccr_addr = ccr_addr;
    device->enable_bit = enable_bit;
    device->frame_us = frame_us;
    device->min_pulse_us = min_pulse_us;
    device->max_pulse_us = max_pulse_us;
    device->initialized = true;
    device->actual_pulse_us = 0;
    return SERVO_OK;
}

/* 与新固件相同：先换成万分比占空，再四舍五入回 us。 */
static ServoStatus servo_set_pulse_us(Servo *device, uint16_t pulse_us)
{
    uint32_t duty;
    uint32_t actual;

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
        return SERVO_INVALID_ARG;
    }

    duty = (pulse_us * 10000u + device->frame_us / 2u) / device->frame_us;
    if (duty == 0u || duty >= 10000u) {
        return SERVO_UNSUPPORTED;
    }
    actual = (duty * device->frame_us + 5000u) / 10000u;
    if (actual < device->min_pulse_us || actual > device->max_pulse_us) {
        return SERVO_UNSUPPORTED;
    }

    *(volatile uint32_t *)device->ccr_addr = actual;
    TIM2_CCER |= device->enable_bit;
    device->actual_pulse_us = actual;
    return SERVO_OK;
}

static void servo_hold_stopped(Servo *devices, unsigned count)
{
    unsigned i;

    for (i = 0; i < count; i++) {
        devices[i].faulted = true;
    }
    servo_stop_all(devices, count);
    uart3_puts("servo fault, output stopped\n");
    while (1) {
    }
}

void demo_servo(void)
{
    Servo servos[SERVO_COUNT];
    static const uint32_t ccr_addr[SERVO_COUNT] = {
        TIM2_CCR3_ADDR,
        TIM2_CCR4_ADDR,
    };
    static const uint32_t enable_bit[SERVO_COUNT] = {
        TIM_CCER_CC3E,
        TIM_CCER_CC4E,
    };
    uint32_t pulse = SERVO_CENTER_US;
    int direction = 1;
    unsigned i;
    unsigned log_div = 0;

    uart3_init();
    uart3_puts("servo PA2 CH3 + PA3 CH4 50Hz\n");

    tim2_init(SERVO_FRAME_US);
    for (i = 0; i < SERVO_COUNT; i++) {
        servos[i].ccr_addr = 0;
        servos[i].enable_bit = 0;
        servos[i].initialized = false;
        servos[i].faulted = false;
        if (servo_init(&servos[i], ccr_addr[i], enable_bit[i], SERVO_FRAME_US,
                       SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US) != SERVO_OK ||
            servo_set_pulse_us(&servos[i], SERVO_CENTER_US) != SERVO_OK) {
            servo_hold_stopped(servos, SERVO_COUNT);
        }
    }

    uart3_puts("center ");
    uart3_putu(SERVO_CENTER_US);
    uart3_puts(" us\n");

    while (1) {
        delay_ms(DEMO_TICK_MS);

        for (i = 0; i < SERVO_COUNT; i++) {
            if (servos[i].faulted ||
                servo_set_pulse_us(&servos[i], (uint16_t)pulse) != SERVO_OK) {
                servo_hold_stopped(servos, SERVO_COUNT);
            }
        }

        if (direction > 0) {
            if (pulse + SERVO_SWEEP_STEP_US <= servos[0].max_pulse_us) {
                pulse += SERVO_SWEEP_STEP_US;
            } else {
                direction = -1;
            }
        } else if (pulse >= (uint32_t)servos[0].min_pulse_us + SERVO_SWEEP_STEP_US) {
            pulse -= SERVO_SWEEP_STEP_US;
        } else {
            direction = 1;
        }

        log_div++;
        if (log_div >= 25U) {
            log_div = 0;
            uart3_puts("pulse ");
            uart3_putu(servos[0].actual_pulse_us);
            uart3_puts(" us\n");
        }
    }
}
