#include "components/ws2812_light.h"

/*
 * 一格 4 个 BSRR 字：先拉高，再按该位决定第二格，后两格拉低。
 * 0 码高电平一格，1 码高电平两格。整板同色，GRB，高位先发。
 */

#define WS2812_LIGHT_CELLS       8192u
#define WS2812_LIGHT_GUARD_CELLS 1024u
#define WS2812_LIGHT_CELL_TICKS  5u

#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB2ENR (*(volatile uint32_t *)0x40023844UL)

#define GPIOD_MODER   (*(volatile uint32_t *)0x40020C00UL)
#define GPIOD_OSPEEDR (*(volatile uint32_t *)0x40020C08UL)
#define GPIOD_BSRR    (*(volatile uint32_t *)0x40020C18UL)

#define DMA2_LISR    (*(volatile uint32_t *)0x40026400UL)
#define DMA2_LIFCR   (*(volatile uint32_t *)0x40026408UL)
#define DMA2_S1_CR   (*(volatile uint32_t *)0x40026428UL)
#define DMA2_S1_NDTR (*(volatile uint32_t *)0x4002642CUL)
#define DMA2_S1_PAR  (*(volatile uint32_t *)0x40026430UL)
#define DMA2_S1_M0AR (*(volatile uint32_t *)0x40026434UL)
#define DMA2_S1_FCR  (*(volatile uint32_t *)0x4002643CUL)

#define TIM8_CR1  (*(volatile uint32_t *)0x40010400UL)
#define TIM8_CR2  (*(volatile uint32_t *)0x40010404UL)
#define TIM8_DIER (*(volatile uint32_t *)0x4001040CUL)
#define TIM8_SR   (*(volatile uint32_t *)0x40010410UL)
#define TIM8_CNT  (*(volatile uint32_t *)0x40010424UL)
#define TIM8_PSC  (*(volatile uint32_t *)0x40010428UL)
#define TIM8_ARR  (*(volatile uint32_t *)0x4001042CUL)

#define NVIC_ISER1 (*(volatile uint32_t *)0xE000E104UL)
#define NVIC_ICPR1 (*(volatile uint32_t *)0xE000E284UL)
#define NVIC_IPR57 (*(volatile uint8_t *)0xE000E439UL)

#define DMA_S1_IRQ_BIT (1u << 25)
#define DMA_S1_TCIF    (1u << 11)
#define DMA_S1_HTIF    (1u << 10)
#define DMA_S1_TEIF    (1u << 9)
#define DMA_S1_DMEIF   (1u << 8)
#define DMA_S1_FEIF    (1u << 6)
#define DMA_S1_FLAGS   (DMA_S1_TCIF | DMA_S1_HTIF | DMA_S1_TEIF | DMA_S1_DMEIF | DMA_S1_FEIF)

#define DMA_CR_EN   (1u << 0)
#define DMA_CR_TEIE (1u << 2)
#define DMA_CR_TCIE (1u << 4)
#define DMA_CR_DIR  (1u << 6)
#define DMA_CR_CIRC (1u << 8)
#define DMA_CR_MINC (1u << 10)

#define DMA_FCR_FTH_HALF (1u << 0)
#define DMA_FCR_DMDIS    (1u << 2)

#define TIM_CR1_CEN  (1u << 0)
#define TIM_CR1_ARPE (1u << 7)
#define TIM_DIER_UDE (1u << 8)

#define PD9_BIT  (1u << 9)
#define PD9_LOW  (1u << 25)

static uint32_t g_words[WS2812_LIGHT_CELLS] __attribute__((aligned(4)));
static uint8_t g_r;
static uint8_t g_g;
static uint8_t g_b;
static uint8_t g_running;

volatile uint32_t g_ws2812_light_complete_count;
volatile uint32_t g_ws2812_light_error_count;

static void ws2812_light_encode(void)
{
    const uint32_t high = PD9_BIT;
    uint32_t *p = g_words;
    uint32_t i;
    unsigned px;
    unsigned ch;
    unsigned bit;

    for (i = 0; i < WS2812_LIGHT_CELLS; i++) {
        p[i] = PD9_LOW;
    }
    p += WS2812_LIGHT_GUARD_CELLS;
    for (px = 0; px < WS2812_LIGHT_PIXELS; px++) {
        const uint8_t grb[3] = {g_g, g_r, g_b};

        for (ch = 0; ch < 3u; ch++) {
            uint8_t value = grb[ch];

            for (bit = 0; bit < 8u; bit++) {
                *p++ = high;
                *p++ = (value & 0x80u) != 0u ? high : PD9_LOW;
                *p++ = PD9_LOW;
                *p++ = PD9_LOW;
                value = (uint8_t)(value << 1);
            }
        }
    }
}

static void ws2812_light_timer_start(void)
{
    TIM8_CR1 = 0;
    TIM8_CR2 = 0;
    TIM8_PSC = 0;
    TIM8_ARR = WS2812_LIGHT_CELL_TICKS - 1u;
    TIM8_CNT = 0;
    TIM8_SR = 0;
    TIM8_DIER = TIM_DIER_UDE;
    TIM8_CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

static Ws2812LightStatus ws2812_light_start(void)
{
    DMA2_S1_CR &= ~DMA_CR_EN;
    while ((DMA2_S1_CR & DMA_CR_EN) != 0u) {
    }
    DMA2_LIFCR = DMA_S1_FLAGS;
    DMA2_S1_PAR = (uint32_t)&GPIOD_BSRR;
    DMA2_S1_M0AR = (uint32_t)g_words;
    DMA2_S1_NDTR = WS2812_LIGHT_CELLS;
    /* 存储器到外设必须走 FIFO，半满阈值。 */
    DMA2_S1_FCR = DMA_FCR_DMDIS | DMA_FCR_FTH_HALF;
    DMA2_S1_CR = (7u << 25) | (2u << 13) | (2u << 11) | DMA_CR_MINC |
                 DMA_CR_CIRC | DMA_CR_DIR | DMA_CR_TCIE | DMA_CR_TEIE;
    NVIC_ICPR1 = DMA_S1_IRQ_BIT;
    DMA2_S1_CR |= DMA_CR_EN;
    if ((DMA2_S1_CR & DMA_CR_EN) == 0u) {
        return WS2812_LIGHT_FAULT;
    }
    ws2812_light_timer_start();
    g_running = 1u;
    return WS2812_LIGHT_OK;
}

void DMA2_Stream1_IRQHandler(void)
{
    uint32_t flags = DMA2_LISR;

    if ((flags & DMA_S1_TCIF) != 0u) {
        DMA2_LIFCR = DMA_S1_TCIF;
        g_ws2812_light_complete_count++;
    }
    if ((flags & DMA_S1_TEIF) != 0u) {
        DMA2_LIFCR = DMA_S1_TEIF;
        g_ws2812_light_error_count++;
    }
}

void ws2812_light_init(void)
{
    RCC_AHB1ENR |= (1u << 3) | (1u << 22); /* GPIODEN、DMA2EN */
    RCC_APB2ENR |= 1u << 1;                /* TIM8EN */
    (void)RCC_AHB1ENR;
    (void)RCC_APB2ENR;

    GPIOD_BSRR = PD9_LOW;
    GPIOD_MODER &= ~(3u << (9u * 2u));
    GPIOD_MODER |= 1u << (9u * 2u);
    GPIOD_OSPEEDR |= 3u << (9u * 2u);

    NVIC_IPR57 = (uint8_t)(5u << 4);
    NVIC_ICPR1 = DMA_S1_IRQ_BIT;
    NVIC_ISER1 = DMA_S1_IRQ_BIT;
    (void)NVIC_ISER1;
}

void ws2812_light_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    g_r = r;
    g_g = g;
    g_b = b;
}

void ws2812_light_stop(void)
{
    DMA2_S1_CR &= ~DMA_CR_EN;
    while ((DMA2_S1_CR & DMA_CR_EN) != 0u) {
    }
    TIM8_CR1 &= ~TIM_CR1_CEN;
    TIM8_DIER = 0;
    TIM8_SR = 0;
    GPIOD_BSRR = PD9_LOW;
    g_running = 0u;
}

Ws2812LightStatus ws2812_light_show(void)
{
    if (g_running != 0u) {
        ws2812_light_stop();
    }
    ws2812_light_encode();
    return ws2812_light_start();
}

uint32_t ws2812_light_error_count(void)
{
    return g_ws2812_light_error_count;
}
