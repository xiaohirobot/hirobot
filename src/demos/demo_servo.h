#ifndef DEMO_SERVO_H
#define DEMO_SERVO_H

/*
 * 两路舵机演示。
 * PA2 = TIM2_CH3，PA3 = TIM2_CH4，50 Hz，脉宽在 500~2500 us 间往复。不返回。
 */
void demo_servo(void);

#endif
