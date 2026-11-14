#ifndef ROBOT_I2C_H
#define ROBOT_I2C_H
/* I2C1 slave 0x32 (7-bit), PB6 SCL / PB7 SDA, AF4 open drain.
 * Poll frequently from main; no IRQ, no blocking waits. 16 MHz APB1. */
void robot_i2c_init(void);
void robot_i2c_poll(void);
#endif
