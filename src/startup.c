#include <stdint.h>

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _estack;

void Reset_Handler(void);
void Default_Handler(void);
void main(void);
void DMA2_Stream1_IRQHandler(void);

__attribute__((section(".isr_vector"), used))
const void *const g_pfnVectors[] = {
    (void *)&_estack,
    Reset_Handler,
    Default_Handler, /* NMI */
    Default_Handler, /* HardFault */
    Default_Handler, /* MemManage */
    Default_Handler, /* BusFault */
    Default_Handler, /* UsageFault */
    0, 0, 0, 0,
    Default_Handler, /* SVCall */
    Default_Handler, /* DebugMon */
    0,
    Default_Handler, /* PendSV */
    Default_Handler, /* SysTick */
    /* IRQ 0..56 用默认处理。IRQ 57（向量下标 73）是 DMA2 Stream1，灯带用。 */
    [16 ... 72] = Default_Handler,
    [73] = DMA2_Stream1_IRQHandler,
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {
        *dst++ = *src++;
    }

    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    main();
    while (1) {
    }
}

void Default_Handler(void)
{
    while (1) {
    }
}
