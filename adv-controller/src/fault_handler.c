/* Fault handlers (LED colour, then reset) and the watchdog that resets a hung firmware */

#include "fault_handler.h"
#include "hc32_ll_wdt.h"
#include "ev_hc32f460_lqfp100_v2_bsp.h"

void WDT_Config(void)
{
    stc_wdt_init_t stcWdtInit;

    /* PCLK3 = 50 MHz (BSP_CLK_Init: MPLL 200 MHz, PCLK3 / 4): 16384 / (50 MHz / 8192) ≈ 2.68 s, enough margin
     * for the blocking steps (ADV_Init + settle, a sector erase) while catching hangs */
    stcWdtInit.u32CountPeriod   = WDT_CNT_PERIOD16384;
    stcWdtInit.u32ClockDiv      = WDT_CLK_DIV8192;
    stcWdtInit.u32RefreshRange  = WDT_RANGE_0TO100PCT;  /* Allow refresh anytime */
    stcWdtInit.u32LPMCount      = WDT_LPM_CNT_CONTINUE; /* Keep counting in sleep */
    stcWdtInit.u32ExceptionType = WDT_EXP_TYPE_RST;     /* Reset on timeout */

    (void)WDT_Init(&stcWdtInit);

    /* First refresh starts the watchdog */
    WDT_FeedDog();

    printf("[WDT] Watchdog enabled (timeout ~2.7s)\n");
}

void WDT_Refresh(void)
{
    WDT_FeedDog();
}

/* These override the weak Default_Handler of startup_hc32f460.S: LED colour, a visible pause, then reset */

void HardFault_Handler(void)
{
    __disable_irq();

    /* HardFault: red */
    BSP_LED_Off(LED_ALL);
    BSP_LED_On(LED_RED);

    for (volatile uint32_t i = 0; i < 2000000; i++) {
        __NOP();
    }

    NVIC_SystemReset();

    for (;;) {
        __NOP();
    }
}

void MemManage_Handler(void)
{
    __disable_irq();
    BSP_LED_Off(LED_ALL);
    BSP_LED_On(LED_RED);
    BSP_LED_On(LED_GREEN);  /* Yellow = MemManage */

    for (volatile uint32_t i = 0; i < 2000000; i++) {
        __NOP();
    }

    NVIC_SystemReset();

    for (;;) {
        __NOP();
    }
}

void BusFault_Handler(void)
{
    __disable_irq();
    BSP_LED_Off(LED_ALL);
    BSP_LED_On(LED_RED);
    BSP_LED_On(LED_BLUE);  /* Magenta = BusFault */

    for (volatile uint32_t i = 0; i < 2000000; i++) {
        __NOP();
    }

    NVIC_SystemReset();

    for (;;) {
        __NOP();
    }
}

void UsageFault_Handler(void)
{
    __disable_irq();
    BSP_LED_Off(LED_ALL);
    BSP_LED_On(LED_GREEN);
    BSP_LED_On(LED_BLUE);  /* Cyan = UsageFault */

    for (volatile uint32_t i = 0; i < 2000000; i++) {
        __NOP();
    }

    NVIC_SystemReset();

    for (;;) {
        __NOP();
    }
}
