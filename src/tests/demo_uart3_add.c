#include "tests/demo_uart3_add.h"

#include "components/uart3.h"

#define LINE_MAX 32

static int is_space(char c)
{
    return c == ' ' || c == '\t';
}

static int parse_u(const char **p, unsigned *out)
{
    const char *s = *p;
    unsigned value = 0;
    int digits = 0;

    while (is_space(*s)) {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        unsigned digit = (unsigned)(*s - '0');

        if (value > (~0U / 10U) ||
            (value == (~0U / 10U) && digit > (~0U % 10U))) {
            return 0;
        }
        value = value * 10U + digit;
        digits++;
        s++;
    }
    if (digits == 0) {
        return 0;
    }
    *out = value;
    *p = s;
    return 1;
}

static int parse_sum(const char *line, unsigned *sum)
{
    const char *p = line;
    unsigned a;
    unsigned b;

    if (!parse_u(&p, &a)) {
        return 0;
    }
    while (is_space(*p)) {
        p++;
    }
    if (*p != ',') {
        return 0;
    }
    p++;
    if (!parse_u(&p, &b)) {
        return 0;
    }
    while (is_space(*p)) {
        p++;
    }
    if (*p != '\0') {
        return 0;
    }
    if (a > ~0U - b) {
        return 0;
    }
    *sum = a + b;
    return 1;
}

void demo_uart3_add(void)
{
    char line[LINE_MAX];
    unsigned len = 0;

    uart3_init();
    uart3_puts("add: send a,b\n");

    while (1) {
        char c = (char)uart3_getc();
        unsigned sum;

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
            if (parse_sum(line, &sum)) {
                uart3_putu(sum);
                uart3_puts("\n");
            } else {
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
