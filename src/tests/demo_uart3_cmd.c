#include "tests/demo_uart3_cmd.h"

#include "components/can1.h"
#include "components/servo_angle.h"
#include "components/uart3.h"

#include <stdint.h>

/* 一行三个有符号整数：mode,a,b。这里只解析并按 mode 分发。 */

#define LINE_MAX 48

static int g_can_ready;

static void store_i32(uint8_t *p, int32_t value)
{
    uint32_t bits = (uint32_t)value;

    p[0] = (uint8_t)bits;
    p[1] = (uint8_t)(bits >> 8);
    p[2] = (uint8_t)(bits >> 16);
    p[3] = (uint8_t)(bits >> 24);
}

static int32_t load_i32(const uint8_t *p)
{
    uint32_t bits = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    return (int32_t)bits;
}

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

static int handle_remote(int32_t mode, int32_t a, int32_t b)
{
    uint8_t tx[8];
    uint8_t rx[8];
    int dlc;

    if (!g_can_ready) {
        uart3_puts("err tx\n");
        return 1;
    }
    store_i32(tx, a);
    store_i32(tx + 4, b);
    if (can1_send((uint16_t)mode, tx, 8) != 0) {
        uart3_puts("err tx\n");
        return 1;
    }
    dlc = can1_recv_id((uint16_t)(mode + 1), rx, CAN1_TIMEOUT_MS);
    if (mode == DEMO_MODE_ADD_REMOTE) {
        if (dlc != 4) {
            uart3_puts("err rx\n");
            return 1;
        }
        put_i(load_i32(rx));
        uart3_puts("\n");
        return 1;
    }
    if (dlc != 8) {
        uart3_puts("err rx\n");
        return 1;
    }
    put_i(load_i32(rx));
    uart3_putc(',');
    put_i(load_i32(rx + 4));
    uart3_puts("\n");
    return 1;
}

static int handle(int32_t mode, int32_t a, int32_t b)
{
    int32_t sum;

    if (mode == DEMO_MODE_ADD_LOCAL) {
        if (!add_i(a, b, &sum)) {
            return 0;
        }
        put_i(sum);
        uart3_puts("\n");
        return 1;
    }
    if (mode == DEMO_MODE_SERVO_LOCAL) {
        if (servo_angle_move(a, b) != SERVO_OK) {
            demo_fail();
        }
        put_i(servo_angle_deg(SERVO_CHANNEL_1));
        uart3_putc(',');
        put_i(servo_angle_deg(SERVO_CHANNEL_2));
        uart3_puts("\n");
        return 1;
    }
    if (mode == DEMO_MODE_ADD_REMOTE || mode == DEMO_MODE_SERVO_REMOTE) {
        return handle_remote(mode, a, b);
    }
    return 0;
}

void demo_uart3_cmd(void)
{
    char line[LINE_MAX];
    unsigned len = 0;

    uart3_init();
    uart3_puts("cmd: 1,a,b or 2,a,b or 10,d1,d2 or 11,d1,d2\n");
    g_can_ready = (can1_init() == 0);
    if (!g_can_ready) {
        uart3_puts("can init failed\n");
    }

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
