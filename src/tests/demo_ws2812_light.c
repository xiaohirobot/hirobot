#include "tests/demo_ws2812_light.h"

#include "components/delay.h"
#include "components/uart3.h"
#include "components/ws2812_light.h"

#include <stdint.h>

#define DEMO_STEP_MS  500u
#define DEMO_MAX_RETRY 3u

static const uint8_t k_palette[][3] = {
    {255, 0, 0},   {255, 96, 0},  {255, 255, 0}, {0, 255, 0},
    {0, 255, 255}, {0, 96, 255},  {128, 0, 255}, {255, 0, 192},
};

#define DEMO_PALETTE_LEN (sizeof(k_palette) / sizeof(k_palette[0]))

static void demo_fail(void)
{
    ws2812_light_stop();
    uart3_puts("ws2812 light fault, output stopped\n");
    while (1) {
    }
}

void ws2812_light(void)
{
    unsigned index = 0;
    unsigned retries = 0;

    uart3_init();
    uart3_puts("ws2812 light PD9, 8x8\n");
    ws2812_light_init();
    ws2812_light_set_color(k_palette[0][0], k_palette[0][1], k_palette[0][2]);
    if (ws2812_light_show() != WS2812_LIGHT_OK) {
        demo_fail();
    }
    uart3_puts("color 0\n");

    while (1) {
        if (ws2812_light_error_count() > 0u && retries < DEMO_MAX_RETRY) {
            retries++;
            uart3_puts("ws2812 light retry ");
            uart3_putu(retries);
            uart3_puts("\n");
            if (ws2812_light_show() != WS2812_LIGHT_OK) {
                demo_fail();
            }
            continue;
        }
        if (ws2812_light_error_count() > 0u) {
            demo_fail();
        }

        delay_ms(DEMO_STEP_MS);
        index = (index + 1u) % DEMO_PALETTE_LEN;
        ws2812_light_set_color(k_palette[index][0], k_palette[index][1],
                               k_palette[index][2]);
        if (ws2812_light_show() != WS2812_LIGHT_OK) {
            demo_fail();
        }
        uart3_puts("color ");
        uart3_putu(index);
        uart3_puts("\n");
    }
}
