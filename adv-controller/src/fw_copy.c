/**
 * Self-contained RAM routine that replaces the running firmware with the staged image.
 * Runs with interrupts off from RAM (.ramfunc): it calls nothing and touches only EFM/WDT/SCB registers (it
 * unlocks the EFM registers and enables programming itself: they stay locked at run time), the
 * staging area (read in READONLY mode), the app area and its own RAM buffer. Checked after linking by
 * tools/check-ramfunc.py. The flash cannot be read while it programs, so each 8 KB sector is first copied
 * into RAM, then the app sector is erased and programmed from RAM.
 * Order: app sector 0 (vectors) is erased first, so an interrupted copy leaves 0xFFFFFFFF at 0x10000 ("no
 * app" for the bootloader), and programmed last, only when every other sector verified (else it stays erased: the
 * bootloader's update path takes over instead of a jump into a corrupt image).
 */
#include "hc32_ll.h"
#include "fw_copy.h"

#define APP_BASE      0x00010000UL
#define STAGING_BASE  0x00040000UL
#define SECTOR_BYTES  0x2000UL
#define MAX_IMAGE     0x30000UL
#define FSR_ERRORS    (EFM_FSR_PEWERR | EFM_FSR_PEPRTERR | EFM_FSR_PGSZERR | EFM_FSR_PGMISMTCH | EFM_FSR_COLERR)

static uint32_t sSectorBuf[SECTOR_BYTES / 4U];

#define RAMFN __attribute__((section(".ramfunc"), long_call, noinline, used, optimize("no-tree-loop-distribute-patterns")))
#define INLINE static inline __attribute__((always_inline))

INLINE void setMode(uint32_t mode)
{
    CM_EFM->FWMC = (CM_EFM->FWMC & ~EFM_FWMC_PEMOD) | mode;
}

INLINE void waitReady(void)
{
    while ((CM_EFM->FSR & EFM_FSR_RDY) == 0U) {}
}

INLINE void feedDog(void)
{
    CM_WDT->RR = 0x0123UL;
    CM_WDT->RR = 0x3210UL;
}

INLINE void eraseSector(uint32_t addr)
{
    CM_EFM->FSCLR = FSR_ERRORS | EFM_FSR_OPTEND;
    setMode(EFM_MD_ERASE_SECTOR);
    *(volatile uint32_t*)addr = 0UL;
    waitReady();
    CM_EFM->FSCLR = EFM_FSR_OPTEND;
    setMode(EFM_MD_READONLY);
    waitReady();
}

/* Programs `words` words from the RAM buffer; returns the FSR error bits */
INLINE uint32_t programFromBuffer(uint32_t addr, uint32_t words)
{
    volatile uint32_t* dst = (volatile uint32_t*)addr;
    CM_EFM->FSCLR = FSR_ERRORS | EFM_FSR_OPTEND;
    setMode(EFM_MD_PGM_SEQ);
    for (uint32_t i = 0; i < words; i++) {
        dst[i] = sSectorBuf[i];
        while ((CM_EFM->FSR & EFM_FSR_OPTEND) == 0U) {}
        CM_EFM->FSCLR = EFM_FSR_OPTEND;
    }
    setMode(EFM_MD_READONLY);
    waitReady();
    return CM_EFM->FSR & FSR_ERRORS;
}

/* READONLY mode: the programmed words read back equal to the RAM buffer (PGM_SEQ does not compare) */
INLINE uint32_t verifyFromBuffer(uint32_t addr, uint32_t words)
{
    const volatile uint32_t* dst = (const volatile uint32_t*)addr;
    for (uint32_t i = 0; i < words; i++) {
        if (dst[i] != sSectorBuf[i]) return 1U;
    }
    return 0U;
}

/* Staging sector -> RAM buffer (READONLY mode: the flash can be read) */
INLINE uint32_t loadSector(uint32_t sector, uint32_t size)
{
    uint32_t start = sector * SECTOR_BYTES;
    uint32_t bytes = size - start < SECTOR_BYTES ? size - start : SECTOR_BYTES;
    uint32_t words = (bytes + 3U) / 4U;
    const volatile uint32_t* src = (const volatile uint32_t*)(STAGING_BASE + start);
    for (uint32_t i = 0; i < words; i++) sSectorBuf[i] = src[i];
    return words;
}

/* Returns 0 when the sector verified, after at most one retry of the whole sector */
INLINE uint32_t copySector(uint32_t sector, uint32_t size, int erase)
{
    uint32_t words = loadSector(sector, size);
    uint32_t addr = APP_BASE + sector * SECTOR_BYTES;
    uint32_t bad = 0U;
    if (erase) eraseSector(addr);
    if (programFromBuffer(addr, words) != 0U || verifyFromBuffer(addr, words) != 0U) {   /* one retry: whole sector */
        eraseSector(addr);
        bad = programFromBuffer(addr, words) != 0U || verifyFromBuffer(addr, words) != 0U ? 1U : 0U;
    }
    feedDog();
    return bad;
}

RAMFN void hc32CopyAndReset(uint32_t size)
{
    __disable_irq();
    CM_EFM->FAPRT = 0x0123UL;                     /* the EFM registers are locked outside the update (main.c): */
    CM_EFM->FAPRT = 0x3210UL;                     /* unlock them and enable programming here, inline */
    CM_EFM->FWMC |= EFM_FWMC_PEMODE;
    if (size > MAX_IMAGE) size = MAX_IMAGE;
    uint32_t sectors = (size + SECTOR_BYTES - 1U) / SECTOR_BYTES;
    CM_EFM->FRMC &= ~EFM_FRMC_CACHE;
    waitReady();
    eraseSector(APP_BASE);                        /* vectors first: an interrupted copy is "no app" */
    feedDog();
    uint32_t bad = 0U;
    for (uint32_t s = sectors; s-- > 1U;) bad |= copySector(s, size, 1);
    if (bad == 0U && copySector(0U, size, 0) != 0U) eraseSector(APP_BASE);   /* vectors last, only on a good copy */
    __DSB();
    SCB->AIRCR = (0x5FAUL << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk;
    __DSB();
    for (;;) {}
}
