#ifndef UART3_H
#define UART3_H

/* USART3：PB10 = TX，PB11 = RX，115200 8N1。板载 CH340 接在这里。 */

void uart3_init(void);
/* 阻塞读一字节。 */
int uart3_getc(void);
void uart3_putc(char c);
void uart3_puts(const char *s);
void uart3_putu(unsigned value);

#endif
