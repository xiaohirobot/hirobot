#include <stdint.h>
#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB2ENR (*(volatile uint32_t *)0x40023844UL)

#define GPIOB_MODER   (*(volatile uint32_t *)0x40020400UL)
#define GPIOB_OSPEEDR (*(volatile uint32_t *)0x40020408UL)
#define GPIOB_PUPDR   (*(volatile uint32_t *)0x4002040CUL)
#define GPIOB_AFRL    (*(volatile uint32_t *)0x40020420UL)

#define GPIOD_MODER (*(volatile uint32_t *)0x40020C00UL)
#define GPIOD_PUPDR (*(volatile uint32_t *)0x40020C0CUL)
#define GPIOD_IDR   (*(volatile uint32_t *)0x40020C10UL)

#define SPI1_CR1 (*(volatile uint32_t *)0x40013000UL)
#define SPI1_CR2 (*(volatile uint32_t *)0x40013004UL)
#define SPI1_SR  (*(volatile uint32_t *)0x40013008UL)
#define SPI1_DR  (*(volatile uint32_t *)0x4001300CUL)

#define SPI_SR_RXNE (1U << 0)
#define SPI_SR_TXE  (1U << 1)
#define SPI_SR_OVR  (1U << 6)
#define SPI_SR_BSY  (1U << 7)

#define SPI_CR1_SPE (1U << 6)
#define SPI_CR1_SSM (1U << 9)

#define CS_PIN 7U
#define MAGIC0 0xA5u
#define MAGIC1 0x5Au


#include "components/robot_wire.h"
#include "components/servo_angle.h"
#include "components/can1.h"
#include "components/uart3.h"
static uint8_t tx[32],rx[32],last[32];
static int have_last,can_ready,servo_ready;
#define DEMCR (*(volatile uint32_t *)0xE000EDFCUL)
#define DWT_CTRL (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004UL)
static int cs_high(void) { return (GPIOD_IDR & (1u<<7))!=0; }
static void spi_flush(void) {
    SPI1_CR1 &= ~SPI_CR1_SPE;
    (void)SPI1_DR;(void)SPI1_SR;
    SPI1_CR1 |= SPI_CR1_SPE;
}
static void dispatch(void) {
    if(!rw_valid(rx) || rx[3]!=RW_SUBMIT) return;
    if(have_last && rw_get(last+4)==rw_get(rx+4)) {
        /* A reused ID with different arguments must never look successful. */
        if(memcmp(last,rx,32)!=0) {
            rw_make(tx,RW_REPLY,rw_get(rx+4),rx[9],0,0);
            tx[8]=RW_INVALID;rw_finish(tx);
        }
        return;
    }
    memcpy(last,rx,32);have_last=1;
    int32_t a=(int32_t)rw_get(rx+10),b=(int32_t)rw_get(rx+14),x=0,y=0;
    uint8_t mode=rx[9],status=RW_OK;
    if(mode==1) {
        if((b>0 && a>INT32_MAX-b)||(b<0 && a<INT32_MIN-b)) status=RW_INVALID;
        else x=a+b;
    } else if(mode==10) {
        if(!servo_ready) servo_ready=servo_angle_init()==SERVO_OK?1:-1;
        if(servo_ready<0 || servo_angle_move(a,b)!=SERVO_OK) {servo_stop_all();status=RW_SERVO;}
        else {x=servo_angle_deg(0);y=servo_angle_deg(1);}
    } else if(mode==2 || mode==11) {
        uint8_t data[8],reply[8];
        rw_put(data,(uint32_t)a);rw_put(data+4,(uint32_t)b);
        if(!can_ready) can_ready=can1_init()==0?1:-1;
        if(can_ready<0 || can1_send(mode,data,8)!=0) status=RW_REMOTE;
        else {
            int n=can1_recv_id(mode+1,reply,CAN1_TIMEOUT_MS);
            if(n!=(mode==2?4:8)) status=RW_REMOTE;
            else {x=(int32_t)rw_get(reply);if(n==8)y=(int32_t)rw_get(reply+4);}
        }
    } else status=RW_INVALID;
    rw_make(tx,RW_REPLY,rw_get(rx+4),mode,x,y);tx[8]=status;rw_finish(tx);
}
static void init_pins(void)
{
    RCC_AHB1ENR |= (1U << 1) | (1U << 3); /* GPIOBEN、GPIODEN */
    RCC_APB2ENR |= 1U << 12;              /* SPI1EN */
    (void)RCC_AHB1ENR;
    (void)RCC_APB2ENR;

    /* PB3 SCK、PB4 MISO、PB5 MOSI = AF5。 */
    GPIOB_MODER &= ~((3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U)));
    GPIOB_MODER |= (2U << (3U * 2U)) | (2U << (4U * 2U)) | (2U << (5U * 2U));
    GPIOB_PUPDR &= ~((3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U)));
    GPIOB_OSPEEDR |= (3U << (3U * 2U)) | (3U << (4U * 2U)) | (3U << (5U * 2U));
    GPIOB_AFRL &= ~((0xFU << (3U * 4U)) | (0xFU << (4U * 4U)) | (0xFU << (5U * 4U)));
    GPIOB_AFRL |= (5U << (3U * 4U)) | (5U << (4U * 4U)) | (5U << (5U * 4U));

    /* PD7 输入上拉。主机片选空闲为高。 */
    GPIOD_MODER &= ~(3U << (CS_PIN * 2U));
    GPIOD_PUPDR &= ~(3U << (CS_PIN * 2U));
    GPIOD_PUPDR |= 1U << (CS_PIN * 2U);

    SPI1_CR1 = 0;
    SPI1_CR2 = 0;
    SPI1_CR1 = SPI_CR1_SSM;
    spi_flush();
}


void robot_spi_run(void) {
    uart3_init();uart3_puts("SPI command bridge: modes 1,2,10,11; PD7 CS\n");
    DEMCR|=1u<<24;DWT_CYCCNT=0;DWT_CTRL|=1;
    init_pins();
    rw_make(tx,RW_REPLY,0,0,0,0);tx[8]=RW_BUSY;rw_finish(tx);
    for(;;) {
        /* Drop a stuck-low transaction; resume only after CS is released. */
        if(!cs_high()) continue;
        spi_flush();SPI1_DR=tx[0];
        while(cs_high()) { }
        uint32_t start=DWT_CYCCNT;
        unsigned n=0;int bad=0;
        while(!cs_high()) {
            if((uint32_t)(DWT_CYCCNT-start)>32000000u) {bad=1;break;}
            if(SPI1_SR & SPI_SR_OVR) {bad=1;break;}
            if(SPI1_SR & SPI_SR_RXNE) {
                uint8_t value=(uint8_t)SPI1_DR;
                if(n>=32) {bad=1;break;}
                rx[n++]=value;
                if(n<32) SPI1_DR=tx[n];
            }
        }
        /* Business executes only after the complete CS transaction ends.
         * Master guarantees >=200ms idle; no clocks during blocking CAN work. */
        if(!bad && n==32 && cs_high()) dispatch();
    }
}
