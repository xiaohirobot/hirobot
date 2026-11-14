#include "tests/demo_uart3_cmd.h"

#include "components/servo_angle.h"
#include "components/uart3.h"

#include <stdint.h>

/* 一行三个有符号整数：mode,a,b。这里只解析并按 mode 分发。 */

#define LINE_MAX 48

static int is_space(char c)
{
    return c == ' ' || c == '\t';
}

static void demo_fail(void)
{
    servo_stop_all();
    uart3_puts("servo fault, output stopped\n");
    while (1) {
    }
}

static void put_i(int32_t value)
{
    if (value < 0) {
        uart3_putc('-');
        if (value == INT32_MIN) {
            uart3_puts("2147483648");
            return;
        }
        value = -value;
    }
    uart3_putu((unsigned)value);
}

static int parse_i(const char **p, int32_t *out)
{
    const char *s = *p;
    int neg = 0;
    uint32_t value = 0;
    int digits = 0;
    uint32_t limit;

    while (is_space(*s)) {
        s++;
    }
    if (*s == '+' || *s == '-') {
        neg = (*s == '-');
        s++;
    }
    limit = neg ? 2147483648u : 2147483647u;
    while (*s >= '0' && *s <= '9') {
        uint32_t digit = (uint32_t)(*s - '0');

        if (value > limit / 10u ||
            (value == limit / 10u && digit > limit % 10u)) {
            return 0;
        }
        value = value * 10u + digit;
        digits++;
        s++;
    }
    if (digits == 0) {
        return 0;
    }
    if (neg) {
        *out = (value == 2147483648u) ? INT32_MIN : -(int32_t)value;
    } else {
        *out = (int32_t)value;
    }
    *p = s;
    return 1;
}

static int expect_comma(const char **p)
{
    const char *s = *p;

    while (is_space(*s)) {
        s++;
    }
    if (*s != ',') {
        return 0;
    }
    *p = s + 1;
    return 1;
}

static int parse_cmd(const char *line, int32_t *mode, int32_t *a, int32_t *b)
{
    const char *p = line;

    if (!parse_i(&p, mode) || !expect_comma(&p) || !parse_i(&p, a) ||
        !expect_comma(&p) || !parse_i(&p, b)) {
        return 0;
    }
    while (is_space(*p)) {
        p++;
    }
    return *p == '\0';
}

static int add_i(int32_t a, int32_t b, int32_t *sum)
{
    if ((b > 0 && a > INT32_MAX - b) || (b < 0 && a < INT32_MIN - b)) {
        return 0;
    }
    *sum = a + b;
    return 1;
}

static int handle(int32_t mode, int32_t a, int32_t b)
{
    int32_t sum;

    if (mode == 1) {
        if (!add_i(a, b, &sum)) {
            return 0;
        }
        put_i(sum);
        uart3_puts("\n");
        return 1;
    }
    if (mode == 10) {
        if (servo_angle_move(a, b) != SERVO_OK) {
            demo_fail();
        }
        put_i(servo_angle_deg(SERVO_CHANNEL_1));
        uart3_putc(',');
        put_i(servo_angle_deg(SERVO_CHANNEL_2));
        uart3_puts("\n");
        return 1;
    }
    return 0;
}

void demo_uart3_cmd(void)
{
    char line[LINE_MAX];
    unsigned len = 0;

    uart3_init();
    uart3_puts("cmd: 1,a,b or 10,d1,d2\n");

    if (servo_angle_init() != SERVO_OK) {
        demo_fail();
    }

    while (1) {
        char c = (char)uart3_getc();
        int32_t mode;
        int32_t a;
        int32_t b;

        if (c == '\r' || c == '\n') {
            if (len == 0) {
                continue;
            }
            if (len >= LINE_MAX) {
                len = 0;
                uart3_puts("err\n");
                continue;
            }
            line[len] = '\0';
            len = 0;
            if (!parse_cmd(line, &mode, &a, &b) || !handle(mode, a, b)) {
                uart3_puts("err\n");
            }
            continue;
        }
        if (len >= LINE_MAX - 1U) {
            len = LINE_MAX;
            continue;
        }
        line[len++] = c;
    }
}
