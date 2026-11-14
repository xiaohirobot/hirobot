#include "components/can1.h"

/*
 * 16 MHz / (预分频 4 × (1 + BS1 5 + BS2 2)) = 500 kbit/s。
 * 采样点在 (1+5)/8 = 75%。只开了不自动重发，对端不在时发送会结束。
 */

#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB1ENR (*(volatile uint32_t *)0x40023840UL)

#define GPIOD_MODER (*(volatile uint32_t *)0x40020C00UL)
#define GPIOD_AFRL  (*(volatile uint32_t *)0x40020C20UL)

#define CAN1_MCR   (*(volatile uint32_t *)0x40006400UL)
#define CAN1_MSR   (*(volatile uint32_t *)0x40006404UL)
#define CAN1_TSR   (*(volatile uint32_t *)0x40006408UL)
#define CAN1_RF0R  (*(volatile uint32_t *)0x4000640CUL)
#define CAN1_BTR   (*(volatile uint32_t *)0x4000641CUL)
#define CAN1_TI0R  (*(volatile uint32_t *)0x40006580UL)
#define CAN1_TDT0R (*(volatile uint32_t *)0x40006584UL)
#define CAN1_TDL0R (*(volatile uint32_t *)0x40006588UL)
#define CAN1_TDH0R (*(volatile uint32_t *)0x4000658CUL)
#define CAN1_RI0R  (*(volatile uint32_t *)0x400065B0UL)
#define CAN1_RDT0R (*(volatile uint32_t *)0x400065B4UL)
#define CAN1_RDL0R (*(volatile uint32_t *)0x400065B8UL)
#define CAN1_RDH0R (*(volatile uint32_t *)0x400065BCUL)
#define CAN1_FMR   (*(volatile uint32_t *)0x40006600UL)
#define CAN1_FM1R  (*(volatile uint32_t *)0x40006604UL)
#define CAN1_FS1R  (*(volatile uint32_t *)0x4000660CUL)
#define CAN1_FFA1R (*(volatile uint32_t *)0x40006614UL)
#define CAN1_FA1R  (*(volatile uint32_t *)0x4000661CUL)
#define CAN1_F0R1  (*(volatile uint32_t *)0x40006640UL)
#define CAN1_F0R2  (*(volatile uint32_t *)0x40006644UL)

#define CAN_MCR_INRQ (1U << 0)
#define CAN_MCR_NART (1U << 4)
#define CAN_MCR_ABOM (1U << 6)
#define CAN_MSR_INAK (1U << 0)
#define CAN_MSR_SLAK (1U << 1)
#define CAN_TSR_RQCP0 (1U << 0)
#define CAN_TSR_TXOK0 (1U << 1)
#define CAN_TSR_ABRQ0 (1U << 7)
#define CAN_TSR_TME0  (1U << 26)
#define CAN_RF0R_RFOM0 (1U << 5)
#define CAN_RF0R_FOVR0 (1U << 4)
#define CAN_FMR_FINIT (1U << 0)
#define CAN_TI0R_TXRQ (1U << 0)

/* 预分频 4，BS1 = 5，BS2 = 2，SJW = 1。 */
#define CAN_BTR_500K  ((3U << 0) | (4U << 16) | (1U << 20))

#define SYSTICK_CTRL      (*(volatile uint32_t *)0xE000E010UL)
#define SYSTICK_LOAD      (*(volatile uint32_t *)0xE000E014UL)
#define SYSTICK_VAL       (*(volatile uint32_t *)0xE000E018UL)
#define SYSTICK_COUNTFLAG (1U << 16)

static int wait_bit(volatile uint32_t *reg, uint32_t mask, int level)
{
    unsigned spins = 1000000u;

    while (spins-- > 0u) {
        int has = ((*reg & mask) != 0);

        if (has == level) {
            return 0;
        }
    }
    return -1;
}

static void systick_start(unsigned ms)
{
    SYSTICK_LOAD = 16000U * ms - 1U;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = (1U << 2) | (1U << 0);
}

static void systick_stop(void)
{
    SYSTICK_CTRL = 0;
}

static int systick_elapsed(void)
{
    return (SYSTICK_CTRL & SYSTICK_COUNTFLAG) != 0;
}

static int wait_tme(void)
{
    systick_start(CAN1_TIMEOUT_MS);
    while ((CAN1_TSR & CAN_TSR_TME0) == 0) {
        if (systick_elapsed()) {
            systick_stop();
            return -1;
        }
    }
    systick_stop();
    return 0;
}

/* 空邮箱返回 -1，扩展帧或远程帧丢掉并返回 -2，否则返回 DLC。 */
static int fifo_pop(uint16_t *id, uint8_t *data)
{
    uint32_t pending = CAN1_RF0R;
    uint32_t rir;
    uint32_t low;
    uint32_t high;
    unsigned dlc;
    unsigned i;

    if ((pending & 3u) == 0u) {
        return -1;
    }

    rir = CAN1_RI0R;
    dlc = CAN1_RDT0R & 0xFu;
    low = CAN1_RDL0R;
    high = CAN1_RDH0R;
    CAN1_RF0R = CAN_RF0R_RFOM0 | (pending & CAN_RF0R_FOVR0);

    if ((rir & ((1U << 1) | (1U << 2))) != 0u) {
        return -2;
    }
    if (dlc > 8u) {
        dlc = 8u;
    }

    *id = (uint16_t)((rir >> 21) & 0x7FFu);
    for (i = 0; i < 8u; i++) {
        data[i] = 0;
    }
    for (i = 0; i < dlc; i++) {
        uint32_t word = (i < 4u) ? low : high;

        data[i] = (uint8_t)(word >> (8u * (i & 3u)));
    }
    return (int)dlc;
}

int can1_init(void)
{
    RCC_AHB1ENR |= 1U << 3;  /* GPIODEN */
    RCC_APB1ENR |= 1U << 25; /* CAN1EN */
    (void)RCC_AHB1ENR;
    (void)RCC_APB1ENR;

    /* PD0、PD1 = AF9（CAN1_RX、CAN1_TX）。只改这两脚。 */
    GPIOD_MODER &= ~((3U << 0) | (3U << 2));
    GPIOD_MODER |= (2U << 0) | (2U << 2);
    GPIOD_AFRL &= ~((0xFU << 0) | (0xFU << 4));
    GPIOD_AFRL |= (9U << 0) | (9U << 4);

    /* 清 SLEEP 再进初始化。INAK 起来之后才能写位定时。 */
    CAN1_MCR = CAN_MCR_INRQ;
    if (wait_bit(&CAN1_MSR, CAN_MSR_SLAK, 0) != 0 ||
        wait_bit(&CAN1_MSR, CAN_MSR_INAK, 1) != 0) {
        return -1;
    }
    CAN1_MCR = CAN_MCR_INRQ | CAN_MCR_NART | CAN_MCR_ABOM;

    CAN1_BTR = CAN_BTR_500K;

    CAN1_FMR |= CAN_FMR_FINIT;
    CAN1_FA1R &= ~1u;
    CAN1_FS1R |= 1u;  /* 32 位。 */
    CAN1_FM1R &= ~1u; /* 掩码方式。 */
    CAN1_FFA1R &= ~1u;
    CAN1_F0R1 = 0;
    CAN1_F0R2 = 0; /* 掩码为 0，标准帧都收进 FIFO0。 */
    CAN1_FA1R |= 1u;
    CAN1_FMR &= ~CAN_FMR_FINIT;

    CAN1_MCR = CAN_MCR_NART | CAN_MCR_ABOM;
    if (wait_bit(&CAN1_MSR, CAN_MSR_INAK, 0) != 0) {
        return -1;
    }
    return 0;
}

int can1_send(uint16_t id, const uint8_t *data, unsigned len)
{
    uint32_t low = 0;
    uint32_t high = 0;
    unsigned i;

    if (id > 0x7FFu || len > 8u || (len > 0u && data == 0)) {
        return -1;
    }
    if ((CAN1_TSR & CAN_TSR_TME0) == 0u) {
        CAN1_TSR = CAN_TSR_ABRQ0;
        if (wait_tme() != 0) {
            return -1;
        }
    }

    for (i = 0; i < len; i++) {
        if (i < 4u) {
            low |= (uint32_t)data[i] << (8u * i);
        } else {
            high |= (uint32_t)data[i] << (8u * (i - 4u));
        }
    }

    CAN1_TDL0R = low;
    CAN1_TDH0R = high;
    CAN1_TDT0R = len;
    CAN1_TSR = CAN_TSR_RQCP0;
    CAN1_TI0R = ((uint32_t)id << 21) | CAN_TI0R_TXRQ;

    systick_start(CAN1_TIMEOUT_MS);
    while ((CAN1_TSR & CAN_TSR_RQCP0) == 0u) {
        if (systick_elapsed()) {
            CAN1_TSR = CAN_TSR_ABRQ0;
            systick_stop();
            return -1;
        }
    }
    systick_stop();

    if ((CAN1_TSR & CAN_TSR_TXOK0) == 0u) {
        CAN1_TSR = CAN_TSR_RQCP0;
        return -1;
    }
    CAN1_TSR = CAN_TSR_RQCP0;
    return 0;
}

int can1_recv(uint16_t *id, uint8_t *data, unsigned timeout_ms)
{
    int armed = 0;

    if (id == 0 || data == 0) {
        return -1;
    }
    if (timeout_ms > 1000u) {
        timeout_ms = 1000u;
    }
    if (timeout_ms > 0u) {
        systick_start(timeout_ms);
        armed = 1;
    }

    for (;;) {
        int dlc = fifo_pop(id, data);

        if (dlc >= 0) {
            if (armed) {
                systick_stop();
            }
            return dlc;
        }
        if (dlc == -1 && armed && systick_elapsed()) {
            systick_stop();
            return -1;
        }
    }
}

int can1_recv_id(uint16_t want, uint8_t *data, unsigned timeout_ms)
{
    int armed = 0;

    if (data == 0) {
        return -1;
    }
    if (timeout_ms > 1000u) {
        timeout_ms = 1000u;
    }
    if (timeout_ms > 0u) {
        systick_start(timeout_ms);
        armed = 1;
    }

    for (;;) {
        uint16_t id = 0;
        int dlc = fifo_pop(&id, data);

        if (dlc >= 0 && id == want) {
            if (armed) {
                systick_stop();
            }
            return dlc;
        }
        if (dlc == -1 && armed && systick_elapsed()) {
            systick_stop();
            return -1;
        }
    }
}
