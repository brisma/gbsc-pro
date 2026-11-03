/* Console core (DDL-free, host-tested): per-port line editing, the legacy register commands
 * (byte-compatible output), word commands, incremental dumps paced by the port's free space, and a log
 * broadcast with a rate limit. Ports: 0 = USART3, 1 = USB CDC. */
#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>
#include <stdint.h>

#define CONSOLE_PORTS 2

typedef struct {
    void (*write)(const char* s, size_t n);   /* never blocks (the port's ring drops when full) */
    int (*connected)(void);                   /* a reader is there (USB: DTR set and not suspended) */
    size_t (*space)(void);                    /* free bytes in the port's output ring */
} ConsolePort;

typedef int (*ConsoleI2cRead)(uint8_t dev, uint8_t reg, uint8_t* val);   /* 0 = ok */
typedef int (*ConsoleI2cWrite)(uint8_t dev, uint8_t reg, uint8_t val);
typedef void (*ConsoleCmd)(uint8_t port, const char* args);

void CONSOLE_Init(const ConsolePort* uart, const ConsolePort* usb, ConsoleI2cRead rd, ConsoleI2cWrite wr);
void CONSOLE_AddCommand(const char* name, ConsoleCmd fn);   /* word commands, matched before the legacy ones */
void CONSOLE_Feed(uint8_t port, char c);
void CONSOLE_Poll(uint32_t nowMs);                          /* dump steps, rate-limit windows */
void CONSOLE_Write(uint8_t port, const char* s);
void CONSOLE_Printf(uint8_t port, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void CONSOLE_Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));   /* ports with "log on" */
void CONSOLE_Banner(uint8_t port);

#endif
