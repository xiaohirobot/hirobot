#include "components/robot_i2c.h"
#include "components/robot_command.h"
#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_AHB1ENR REG(0x40023830UL)
#define RCC_APB1ENR REG(0x40023840UL)
#define RCC_APB1RSTR REG(0x40023820UL)
#define GPIOB_MODER REG(0x40020400UL)
#define GPIOB_OTYPER REG(0x40020404UL)
#define GPIOB_OSPEEDR REG(0x40020408UL)
#define GPIOB_PUPDR REG(0x4002040CUL)
#define GPIOB_AFRL REG(0x40020420UL)
#define CR1 REG(0x40005400UL)
#define CR2 REG(0x40005404UL)
#define OAR1 REG(0x40005408UL)
#define OAR2 REG(0x4000540CUL)
#define DR REG(0x40005410UL)
#define SR1 REG(0x40005414UL)
#define SR2 REG(0x40005418UL)
#define CCR REG(0x4000541CUL)
#define TRISE REG(0x40005420UL)
#define CYCLES REG(0xE0001004UL)
#define ADDR (1u << 1)
#define STOPF (1u << 4)
#define RXNE (1u << 6)
#define TXE (1u << 7)
#define AF (1u << 10)
#define ERRORS ((1u << 8) | (1u << 9) | (1u << 11) | (1u << 14))
#define PE_ACK ((1u << 0) | (1u << 10))
static RobotEndpoint endpoint;
static uint8_t rx[RW_SIZE], tx[RW_SIZE];
static unsigned count;
static int active, transmitting;
static uint32_t started;
static void reset_bus(void)
{
    RCC_APB1RSTR |= 1u << 21;
    (void)RCC_APB1RSTR;
    RCC_APB1RSTR &= ~(1u << 21);
    (void)RCC_APB1RSTR;
    CR2 = 16; /* APB1 MHz; polling, clock stretching enabled. */
    OAR1 = (1u << 14) | (0x32u << 1);
    OAR2 = 0;
    CCR = 80; TRISE = 17; /* Standard mode timing, 16 MHz / 100 kHz. */
    CR1 = PE_ACK;
    active = 0; transmitting = 0; count = 0;
}
void robot_i2c_init(void)
{
    RCC_AHB1ENR |= 1u << 1;
    RCC_APB1ENR |= 1u << 21;
    (void)RCC_AHB1ENR; (void)RCC_APB1ENR;
    GPIOB_MODER = (GPIOB_MODER & ~((3u << 12) | (3u << 14))) | (2u << 12) | (2u << 14);
    GPIOB_OTYPER |= (1u << 6) | (1u << 7);
    GPIOB_OSPEEDR |= (3u << 12) | (3u << 14);
    /* External pull-ups to 3.3 V are required; internal pulls keep unused pins idle. */
    GPIOB_PUPDR = (GPIOB_PUPDR & ~((3u << 12) | (3u << 14))) | (1u << 12) | (1u << 14);
    GPIOB_AFRL = (GPIOB_AFRL & ~((15u << 24) | (15u << 28))) | (4u << 24) | (4u << 28);
    robot_endpoint_init(&endpoint);
    reset_bus();
}
static void receive_byte(void)
{
    uint8_t byte = (uint8_t)DR;
    if (count < RW_SIZE) rx[count] = byte;
    if (count <= RW_SIZE) count++; /* Saturate at 33; overlong frames never execute. */
}
void robot_i2c_poll(void)
{
    uint32_t status = SR1;
    if (status & ERRORS) { reset_bus(); return; }
    if (status & AF) {
        /* Master NACK ends a read. Reset also discards any prefetched TX byte.
         * Master uses STOP and waits before a new request; no repeated START API. */
        reset_bus(); return;
    }
    if (active && (uint32_t)(CYCLES - started) > 1600000u) {
        reset_bus(); return; /* 100 ms: discard incomplete writes, release stretch. */
    }
    if (status & ADDR) {
        (void)SR1;
        uint32_t direction = SR2; /* RM0090: SR1 then SR2 clears ADDR. */
        transmitting = (direction & (1u << 2)) != 0;
        active = 1; count = 0; started = CYCLES;
        if (transmitting) memcpy(tx, endpoint.reply, RW_SIZE);
        status = SR1;
    }
    if (active && !transmitting) {
        /* Up to two bytes may be buffered when STOPF arrives. Drain before commit. */
        for (unsigned i = 0; i < 2 && (SR1 & RXNE); i++) receive_byte();
    }
    if (SR1 & STOPF) {
        (void)SR1; CR1 = PE_ACK; /* RM0090: read SR1 then write CR1. */
        if (active && !transmitting && count == RW_SIZE) robot_command(&endpoint, rx);
        active = 0;
        return;
    }
    if (active && transmitting && (SR1 & TXE)) {
        DR = count < RW_SIZE ? tx[count++] : 0;
    }
}
