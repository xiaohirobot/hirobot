#ifndef DEMO_UART3_CMD_H
#define DEMO_UART3_CMD_H

/*
 * 串口一行 mode,a,b，回复在 USART3。主机跑 demo_uart3_cmd()。
 *   1,a,b      本机求 a+b，回和
 *   2,a,b      从机求 a+b，回和。例：2,2,4 回 6
 *   10,d1,d2   本机两路舵机相对转角，回两个角度
 *   11,d1,d2   从机两路舵机相对转角，回两个角度。例：11,5,10
 * 从机另烧 demo_can_slave()。CAN1 500 kbit/s，PD0 = RX，PD1 = TX。
 * 主机发送标准帧，ID = mode，8 字节小端 a、b。
 * 从机应答 ID = mode + 1。mode 2 的 DLC 为 4（和）；mode 11 的 DLC 为 8（两个角度）。
 * 发送失败回 err tx。帧已发出但没有合法应答回 err rx。
 */

#define DEMO_MODE_ADD_LOCAL    1
#define DEMO_MODE_ADD_REMOTE   2
#define DEMO_MODE_SERVO_LOCAL  10
#define DEMO_MODE_SERVO_REMOTE 11

/* 不返回。 */
void demo_uart3_cmd(void);

#endif
