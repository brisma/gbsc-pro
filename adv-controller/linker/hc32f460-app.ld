/******************************************************************************
 * Copyright (C) 2022, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 */
/*****************************************************************************/
/*  File        hc32f460-app.ld (project copy of the vendor HC32F460xE.ld)    */
/*  Link v3: app window 0x10000-0x3FFFF (192 KB), staging 0x40000-0x6FFFF      */
/*  reserved for HC32 updates, firmware header at app offset 0x400             */
/*  Abstract    Linker script for HC32F460 Device with                       */
/*              512KByte FLASH, 192KByte RAM                                 */
/*  Version     V1.0                                                         */
/*  Date        2022-03-31                                                   */
/*****************************************************************************/

/* Custom defines, according to section 7.7 of the user manual.
   Take OTP sector 0 for example. */
__OTP_DATA_START = 0x03000C00;
__OTP_DATA_SIZE  = 64;
__OTP_LOCK_START = 0x03000FC0;
__OTP_LOCK_SIZE  = 4;

/* Use contiguous memory regions for simple. */
/* NOTE: Application starts at 0x10000 (64KB offset) - bootloader occupies 0x00000-0x0FFFF */
MEMORY
{
    FLASH       (rx): ORIGIN = 0x00010000, LENGTH = 192K   /* staging 0x40000-0x6FFFF: not linked */
    OTP_DATA    (rx): ORIGIN = __OTP_DATA_START, LENGTH = __OTP_DATA_SIZE
    OTP_LOCK    (rx): ORIGIN = __OTP_LOCK_START, LENGTH = __OTP_LOCK_SIZE
    RAM        (rwx): ORIGIN = 0x1FFF8000, LENGTH = 188K
    RET_RAM    (rwx): ORIGIN = 0x200F0000, LENGTH = 4K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .vectors :
    {
        . = ALIGN(4);
        KEEP(*(.vectors))
        . = ALIGN(4);
    } >FLASH

    /* ICG section removed - it's in the bootloader area at 0x400 */

    /* Firmware header right after the vector table (0x280 bytes), at a fixed app offset (link v3) */
    .fw_header ORIGIN(FLASH) + 0x400 :
    {
        KEEP(*(.fw_header))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)

        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } >FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } >FLASH

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } >FLASH
    __exidx_end = .;

    .preinit_array :
    {
        . = ALIGN(4);
        /* preinit data */
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .init_array :
    {
        . = ALIGN(4);
        /* init data */
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .fini_array :
    {
        . = ALIGN(4);
        /* finit data */
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN (__fini_array_end = .);
        . = ALIGN(4);
    } >FLASH

    __etext = ALIGN(4);

    .otp_data_sec :
    {
        KEEP(*(.otp_data_sec))
    } >OTP_DATA

    .otp_lock_sec :
    {
        KEEP(*(.otp_lock_sec))
    } >OTP_LOCK

    .data : AT (__etext)
    {
        . = ALIGN(4);
        __data_start__ = .;
        *(vtable)
        *(.data)
        *(.data*)
        . = ALIGN(4);
        *(.ramfunc)
        *(.ramfunc*)
        . = ALIGN(4);
        __data_end__ = .;
    } >RAM

    __etext_ret_ram = __etext + ALIGN (SIZEOF(.data), 4);
    .ret_ram_data : AT (__etext_ret_ram)
    {
        . = ALIGN(4);
        __data_start_ret_ram__ = .;
        *(.ret_ram_data)
        *(.ret_ram_data*)
        . = ALIGN(4);
        __data_end_ret_ram__ = .;
    } >RET_RAM

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    .ret_ram_bss :
    {
        . = ALIGN(4);
        __bss_start_ret_ram__ = .;
        *(.ret_ram_bss)
        *(.ret_ram_bss*)
        . = ALIGN(4);
        __bss_end_ret_ram__ = .;
    } >RET_RAM

    .heap_stack (COPY) :
    {
        . = ALIGN(8);
        __end__ = .;
        PROVIDE(end = .);
        PROVIDE(_end = .);
        *(.heap*)
        . = ALIGN(8);
        __HeapLimit = .;

        __StackLimit = .;
        *(.stack*)
        . = ALIGN(8);
        __StackTop = .;
    } >RAM

    /DISCARD/ :
    {
        libc.a (*)
        libm.a (*)
        libgcc.a (*)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }

    PROVIDE(_stack = __StackTop);
    PROVIDE(_Min_Heap_Size = __HeapLimit - __HeapBase);
    PROVIDE(_Min_Stack_Size = __StackTop - __StackLimit);

    __RamEnd = ORIGIN(RAM) + LENGTH(RAM);
    ASSERT(__StackTop <= __RamEnd, "region RAM overflowed with stack")
}
