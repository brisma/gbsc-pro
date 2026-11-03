/* Console core: see console.h. The register commands keep the byte format the old ADV Manager parsed (echo, "\r\n"
 * per CR or LF, "OK"/"ERR"/hex, "?" for unknown, dump "%02X" + ' ' or '\n'). */
#include "console.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LINE_SIZE   64
#define MAP_REG     0x0E
#define LOG_PER_SEC 20
#define MAX_CMDS    8

typedef struct {
    char line[LINE_SIZE];
    uint8_t len;
    uint8_t log;                         /* "log on" */
    /* incremental dump */
    uint8_t dumping, dev, map, reg, left, done;
    /* log rate limit */
    uint8_t sent;
    uint16_t dropped;
} PortState;

static const ConsolePort* sPorts[CONSOLE_PORTS];
static PortState sSt[CONSOLE_PORTS];
static ConsoleI2cRead sRd;
static ConsoleI2cWrite sWr;
static struct { const char* name; ConsoleCmd fn; } sCmds[MAX_CMDS];
static uint8_t sCmdN;
static uint32_t sWindowMs;

void CONSOLE_Write(uint8_t port, const char* s) {
    if (port < CONSOLE_PORTS && sPorts[port]) sPorts[port]->write(s, strlen(s));
}

void CONSOLE_Printf(uint8_t port, const char* fmt, ...) {
    char b[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    CONSOLE_Write(port, b);
}

void CONSOLE_Log(const char* fmt, ...) {
    char b[120];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof(b) - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    strcat(b, "\r\n");
    for (uint8_t p = 0; p < CONSOLE_PORTS; p++) {
        if (!sPorts[p] || !sSt[p].log || !sPorts[p]->connected()) continue;
        if (sSt[p].sent >= LOG_PER_SEC) { sSt[p].dropped++; continue; }
        sSt[p].sent++;
        CONSOLE_Write(p, b);
    }
}

void CONSOLE_Banner(uint8_t port) {
    static const char* const kLines[] = {
        "\r\n================ ADV CLI ================\r\n",
        "r  AA RR        - Read register\r\n",
        "w  AA RR VV     - Write register\r\n",
        "d  AA RR QQ     - Dump registers\r\n",
        "-----------------------------------------\r\n",
        "rm AA MM RR     - Read from map\r\n",
        "wm AA MM RR VV  - Write to map\r\n",
        "dm AA MM RR QQ  - Dump from map\r\n",
        "MM: 00=Main 20=Int/VDP 40=Map2 80=VPP\r\n",
        "-----------------------------------------\r\n",
        "info | update | log on|off | help\r\n",
        "=========================================\r\n",
    };
    for (size_t i = 0; i < sizeof(kLines) / sizeof(kLines[0]); i++) CONSOLE_Write(port, kLines[i]);
}

static uint8_t hexNibble(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
}

static int switchMap(uint8_t dev, uint8_t map) { return sWr(dev, MAP_REG, map); }

/* One register of a pending dump: a map dump selects, reads and deselects in the same step */
static void dumpStep(uint8_t p) {
    PortState* s = &sSt[p];
    uint8_t val = 0;
    int ok;
    if (s->map) {
        ok = switchMap(s->dev, s->map) == 0 && sRd(s->dev, s->reg, &val) == 0;
        switchMap(s->dev, 0x00);
    } else {
        ok = sRd(s->dev, s->reg, &val) == 0;
    }
    s->done++;
    if (ok) CONSOLE_Printf(p, "%02X%c", val, (s->done % 16) ? ' ' : '\n');
    s->reg++;
    if (--s->left == 0) {
        s->dumping = 0;
        CONSOLE_Write(p, "\r\n>");
    }
}

/* Returns 1 when the prompt is printed later (a dump started) */
static int legacy(uint8_t p, char* cmd) {
    uint8_t a[8], n = 0;
    size_t len = strlen(cmd);
    char* q = cmd + (len > 1 && cmd[1] == 'm' ? 3 : 2);
    if (q > cmd + len) q = cmd + len;
    while (*q && n < 8) {
        while (*q == ' ') q++;
        if (!*q) break;
        a[n++] = (uint8_t)((hexNibble(q[0]) << 4) | hexNibble(q[1] ? q[1] : '0'));
        if (!q[1]) break;
        q += 2;
    }
    char c0 = cmd[0], c1 = len > 1 ? cmd[1] : 0;
    uint8_t v = 0;
    if (c0 == 'r' && c1 != 'm' && n == 2) {
        if (sRd(a[0], a[1], &v) == 0) CONSOLE_Printf(p, "%02X\r\n", v); else CONSOLE_Write(p, "ERR\r\n");
    } else if (c0 == 'r' && c1 == 'm' && n == 3) {
        if (switchMap(a[0], a[1]) != 0 || sRd(a[0], a[2], &v) != 0) { CONSOLE_Write(p, "ERR\r\n"); switchMap(a[0], 0); }
        else { switchMap(a[0], 0); CONSOLE_Printf(p, "%02X\r\n", v); }
    } else if (c0 == 'w' && c1 != 'm' && n == 3) {
        CONSOLE_Write(p, sWr(a[0], a[1], a[2]) == 0 ? "OK\r\n" : "ERR\r\n");
    } else if (c0 == 'w' && c1 == 'm' && n == 4) {
        if (switchMap(a[0], a[1]) != 0 || sWr(a[0], a[2], a[3]) != 0) { switchMap(a[0], 0); CONSOLE_Write(p, "ERR\r\n"); }
        else CONSOLE_Write(p, switchMap(a[0], 0) == 0 ? "OK\r\n" : "ERR\r\n");
    } else if (c0 == 'd' && ((c1 != 'm' && n == 3) || (c1 == 'm' && n == 4))) {
        PortState* s = &sSt[p];
        uint8_t map = c1 == 'm' ? a[1] : 0;
        if (c1 == 'm') {                                   /* ERR when the map switch fails (legacy register format) */
            if (switchMap(a[0], map) != 0) { CONSOLE_Write(p, "ERR\r\n"); return 0; }
            switchMap(a[0], 0);
        }
        uint8_t count = a[n - 1];
        if (!count) { CONSOLE_Write(p, "\r\n"); return 0; }
        s->dev = a[0]; s->map = map; s->reg = a[n - 2]; s->left = count; s->done = 0; s->dumping = 1;
        return 1;
    } else {
        CONSOLE_Write(p, "?\r\n");
    }
    return 0;
}

static void execute(uint8_t p, char* cmd) {
    if (!strcmp(cmd, "help")) { CONSOLE_Banner(p); return; }
    if (!strcmp(cmd, "log on")) { sSt[p].log = 1; CONSOLE_Write(p, "OK\r\n"); return; }
    if (!strcmp(cmd, "log off")) { sSt[p].log = 0; CONSOLE_Write(p, "OK\r\n"); return; }
    for (uint8_t i = 0; i < sCmdN; i++) {
        size_t k = strlen(sCmds[i].name);
        if (!strncmp(cmd, sCmds[i].name, k) && (cmd[k] == 0 || cmd[k] == ' ')) {
            const char* args = cmd + k;
            while (*args == ' ') args++;
            sCmds[i].fn(p, args);
            return;
        }
    }
    if (legacy(p, cmd)) return;                           /* the dump prints "\r\n>" when it ends */
}

void CONSOLE_Feed(uint8_t p, char c) {
    if (p >= CONSOLE_PORTS || !sPorts[p]) return;
    PortState* s = &sSt[p];
    if (s->dumping) return;                               /* input ignored while a dump runs */
    if (c == '\r' || c == '\n') {
        CONSOLE_Write(p, "\r\n");
        if (s->len > 0) {
            s->line[s->len] = 0;
            execute(p, s->line);
        }
        if (!s->dumping) CONSOLE_Write(p, ">");          /* a dump prints its own "\r\n>" at the end */
        s->len = 0;
    } else if (c == 8 && s->len > 0) {
        s->len--;
        CONSOLE_Write(p, "\b \b");
    } else if (c >= 32 && s->len < LINE_SIZE - 1) {
        s->line[s->len++] = c;
        char e[2] = {c, 0};
        CONSOLE_Write(p, e);
    }
}

void CONSOLE_Poll(uint32_t nowMs) {
    if (nowMs - sWindowMs >= 1000) {
        sWindowMs = nowMs;
        for (uint8_t p = 0; p < CONSOLE_PORTS; p++) {
            if (sSt[p].dropped && sPorts[p] && sPorts[p]->connected())
                CONSOLE_Printf(p, "... dropped %u log lines\r\n", (unsigned)sSt[p].dropped);
            sSt[p].dropped = 0;
            sSt[p].sent = 0;
        }
    }
    for (uint8_t p = 0; p < CONSOLE_PORTS; p++) {
        if (sSt[p].dumping && sPorts[p] && sPorts[p]->space() >= 4) dumpStep(p);
    }
}

void CONSOLE_AddCommand(const char* name, ConsoleCmd fn) {
    if (sCmdN < MAX_CMDS) { sCmds[sCmdN].name = name; sCmds[sCmdN].fn = fn; sCmdN++; }
}

void CONSOLE_Init(const ConsolePort* uart, const ConsolePort* usb, ConsoleI2cRead rd, ConsoleI2cWrite wr) {
    sPorts[0] = uart;
    sPorts[1] = usb;
    sRd = rd;
    sWr = wr;
    memset(sSt, 0, sizeof(sSt));
    sCmdN = 0;
    sWindowMs = 0;
}
