/* USB board support: the clock comes from the UPLL (XTAL 8 MHz /2 x84 /7 = 48 MHz on PLLXP), independent of
 * the 200 MHz system clock; PA11 (DM) / PA12 (DP) analog; VBUS pin unused (sensing forced in usb_console.c).
 * Adapted from the XHSC usb_dev_cdc example (2022 API); no BSP clock/LED/printf calls. */
#include "usb_bsp.h"
#include "usb_dev_int.h"
#include "hc32_ll.h"

usb_core_instance g_usbDev;
static int sClockOk;
static volatile uint32_t sIrqs, sNonSof, sIrqMax;

#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004UL)
#define DEMCR      (*(volatile uint32_t *)0xE000EDFCUL)

static void USB_IRQ_Handler(void)
{
    uint32_t t0 = DWT_CYCCNT;
    if ((CM_USBFS->GINTSTS & CM_USBFS->GINTMSK) & ~USBFS_GINTSTS_SOF) sNonSof++;   /* the storm guard skips SOF */
    usb_isr_handler(&g_usbDev);
    uint32_t dt = DWT_CYCCNT - t0;
    sIrqs++;
    if (dt > sIrqMax) sIrqMax = dt;
}

/* Called by USBCON_Init before usb_dev_init (the core's soft reset needs the clock): UPLL, USB clock, FCG */
int usb_bsp_clock_start(void)
{
    stc_clock_pllx_init_t upll;
    (void)CLK_PLLxStructInit(&upll);
    upll.u8PLLState = CLK_PLLX_ON;
    upll.PLLCFGR = 0UL;
    upll.PLLCFGR_f.PLLM = 2UL - 1UL;
    upll.PLLCFGR_f.PLLN = 84UL - 1UL;
    upll.PLLCFGR_f.PLLR = 7UL - 1UL;
    upll.PLLCFGR_f.PLLQ = 7UL - 1UL;
    upll.PLLCFGR_f.PLLP = 7UL - 1UL;
    sClockOk = 0;
    if (CLK_PLLxInit(&upll) == LL_OK) {
        for (uint32_t i = 0; i < 2000UL; i++) {   /* ~20 ms at most */
            if (CLK_GetStableStatus(CLK_STB_FLAG_PLLX) == SET) { sClockOk = 1; break; }
            DDL_DelayUS(10UL);
        }
    }
    if (!sClockOk) return 0;
    CLK_SetUSBClockSrc(CLK_USBCLK_PLLXP);
    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USBFS, ENABLE);
    return 1;
}

/* Called by usb_dev_init (only after usb_bsp_clock_start succeeded and the core went idle): pins, ISR timing */
void usb_bsp_init(usb_core_instance *pdev)
{
    (void)pdev;
    DEMCR |= (1UL << 24);                      /* TRCENA: cycle counter for the ISR timing */
    DWT_CYCCNT = 0UL;
    DWT_CTRL |= 1UL;

    stc_gpio_init_t pin;
    (void)GPIO_StructInit(&pin);
    pin.u16PinAttr = PIN_ATTR_ANALOG;
    (void)GPIO_Init(GPIO_PORT_A, GPIO_PIN_11, &pin);   /* DM */
    (void)GPIO_Init(GPIO_PORT_A, GPIO_PIN_12, &pin);   /* DP */
}

void usb_bsp_nvicconfig(void)
{
    stc_irq_signin_config_t irq;
    irq.enIRQn = INT030_IRQn;
    irq.enIntSrc = INT_SRC_USBFS_GLB;
    irq.pfnCallback = &USB_IRQ_Handler;
    (void)INTC_IrqSignIn(&irq);
    NVIC_ClearPendingIRQ(irq.enIRQn);
    NVIC_SetPriority(irq.enIRQn, DDL_IRQ_PRIO_15);
    NVIC_EnableIRQ(irq.enIRQn);
}

void usb_udelay(const uint32_t usec) { DDL_DelayUS(usec); }
void usb_mdelay(const uint32_t msec) { DDL_DelayMS(msec); }
uint32_t usb_bsp_irq_count(void) { return sIrqs; }
uint32_t usb_bsp_irq_non_sof(void) { return sNonSof; }
uint32_t usb_bsp_irq_max_cycles(void) { return sIrqMax; }
