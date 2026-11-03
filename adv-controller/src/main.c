#include "main.h"
#include "debug_uart.h"
#include "console_hc32.h"
#include "usb_console.h"

extern volatile uint32_t g_u32_ms;          /* BSP: free-running ms */
#include "link.h"
#include "fault_handler.h"

#define LL_PERIPH_SEL (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_EFM | LL_PERIPH_SRAM)

extern uint16_t g_u16_sys_timer;
extern uint16_t g_u16_mis_timer;
extern uint16_t g_u16_osd_timer;

static void GPIO_Init_Output(uint8_t port, uint16_t pin, stc_gpio_init_t *cfg)
{
    (void)GPIO_Init(port, pin, cfg);
}

static void GPIO_InitPins(void)
{
    stc_gpio_init_t cfg;

    // Output pins (PB01, PB00, PA03)
    (void)GPIO_StructInit(&cfg);
    cfg.u16PullUp   = PIN_PU_ON;
    cfg.u16PinDir   = PIN_DIR_OUT;
    cfg.u16PinAttr  = PIN_ATTR_DIGITAL;
    cfg.u16PinState = PIN_STAT_RST;
    GPIO_Init_Output(GPIO_PORT_B, GPIO_PIN_01, &cfg);
    GPIO_Init_Output(GPIO_PORT_B, GPIO_PIN_00, &cfg);
    GPIO_Init_Output(GPIO_PORT_A, GPIO_PIN_03, &cfg);

    // Input pin (PB05)
    cfg.u16PinDir   = PIN_DIR_IN;
    cfg.u16PinState = PIN_STAT_SET;
    GPIO_Init_Output(GPIO_PORT_B, GPIO_PIN_05, &cfg);

    // Analog switch outputs (high drive)
    cfg.u16PullUp        = PIN_PU_OFF;
    cfg.u16PinDir        = PIN_DIR_OUT;
    cfg.u16PinState      = PIN_STAT_RST;
    cfg.u16PinDrv        = PIN_HIGH_DRV;
    cfg.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    GPIO_Init_Output(GPIO_PORT_ASW, GPIO_PIN_ASW1, &cfg);
    GPIO_Init_Output(GPIO_PORT_ASW, GPIO_PIN_ASW2, &cfg);
    GPIO_Init_Output(GPIO_PORT_ASW, GPIO_PIN_ASW3, &cfg);
    GPIO_Init_Output(GPIO_PORT_ASW, GPIO_PIN_ASW4, &cfg);

    // Link v3: no settings in flash. Safe default until the ESP's first STATE (about 2 s): the RGBs path with
    // the sync stripper on, AV disconnected, ADV7280 held powered down (PB00/PB01/PA03 low above)
    asw_01 = 0; asw_02 = 0; asw_03 = 0; asw_04 = 1; AVsw = 0;
    ASW_SetSwitches(asw_01, asw_02, asw_03, asw_04);
    ASW_SetAVConnect(AVsw);
}

// Older firmware kept its settings in a record at 0x7C000 (magic 0xBC): erase it once so it can never be mistaken later
static void EraseLegacySettings(void)
{
    const uint32_t addr = EFM_BASE + FLASH_LEAF_ADDR(USER);
    if (*(volatile const uint8_t*)addr == 0xBC) {
        (void)EFM_SectorErase(addr);
    }
}

int main(void)
{
    LL_PERIPH_WE(LL_PERIPH_SEL);
    __enable_irq();
    EFM_FWMC_Cmd(ENABLE);
    BSP_CLK_Init();
    BSP_LED_Init();
    GPIO_InitPins();
    EraseLegacySettings();
    EFM_FWMC_Cmd(DISABLE);                      /* flash program/erase locked: only the firmware update opens it */
    LL_PERIPH_WP(LL_PERIPH_EFM);

#if (LL_TMR0_ENABLE == DDL_ON)
    TMR02_A_Config();
#endif
#if (LL_PRINT_ENABLE == DDL_ON)
    DDL_PrintfInit(BSP_PRINTF_DEVICE, BSP_PRINTF_BAUDRATE, BSP_PRINTF_Preinit);
#endif

    /* USB console first (its init waits ~70 ms for the core): before the watchdog and the link */
    const ConsolePort *usbPort = USBCON_Init();

    I2C_DriverInit();
    TMR0_Start(CM_TMR0_2, TMR0_CHA);
    UART_DMA_Init();
    DebugUart_Init();

    printf("\r\n[SYSTEM] ADV Controller ready\r\n");

    WDT_Config();

    LINK_Init();
    CONSOLE_HC32_Init(usbPort);                 /* after this, no printf: console output is non-blocking */

    for (;;)
    {
        LINK_Poll();   /* frames from the ESP: STATE diff applied at once */

        if (g_u16_sys_timer >= SYS_TIMEOUT_50MS)
        {
            WDT_Refresh();
            ADV_DetectLoop();
            g_u16_sys_timer = 0;
        }

        if (g_u16_mis_timer >= SYS_TIMEOUT_100MS)
        {
            LED_Tick();
            g_u16_mis_timer = 0;
        }

        USBCON_Poll(g_u32_ms);
        CONSOLE_HC32_Poll(g_u32_ms);         /* after LINK_Poll: bounded work per pass */

        if (g_u16_osd_timer >= SYS_TIMEOUT_500MS)
        {
            err_flag = 1;
            g_u16_osd_timer = 0;
        }

        /* Rest until the next interrupt (the 10 us TMR0 tick at the latest; UART RX keeps running on DMA, USB
         * wakes it): without it the loop spins at 100 % CPU and heats the chip */
        __WFI();
    }
}
