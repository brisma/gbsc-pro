# XHSC USB device core (vendored, unmodified)

Source: upstream `RetroScaler/gbsc-pro`, commit `6910d4f` ("V1.3"), directory
`GBSC-Pro-Source code/usart_uart_dma - （IapApp）/midwares/hc32/usb/`:

- `usb_device_lib/device_core/*` → `device_core/`
- `usb_lib.h` → `usb_lib.h`

These are the XHSC 2022 USB device core files. They are byte-identical to upstream; do not edit them.
The CDC class is not taken from here: `src/usb_cdc.c` is our fork (the vendored `usb_dev_cdc_class.c` is not
compiled). Board glue lives in `src/usb_bsp.c` (clock started before `usb_dev_init`, see `src/usb_guard.c`),
descriptors in `src/usb_dev_desc.c`.
