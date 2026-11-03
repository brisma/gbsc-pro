/* Host tests of the console core (make test): line editing, legacy register byte format, word commands,
 * incremental dumps, per-port output, log broadcast with rate limit */
#include <stdio.h>
#include <string.h>
#include "console.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* Fake ports: output captured, connected flag and free space settable */
static char gOut[2][4096];
static size_t gOutN[2];
static int gConn[2] = {1, 1};
static size_t gSpace[2] = {512, 512};
static void wr(int p, const char* s, size_t n) { memcpy(gOut[p] + gOutN[p], s, n); gOutN[p] += n; gOut[p][gOutN[p]] = 0; }
static void w0(const char* s, size_t n) { wr(0, s, n); }
static void w1(const char* s, size_t n) { wr(1, s, n); }
static int c0(void) { return gConn[0]; }
static int c1(void) { return gConn[1]; }
static size_t s0(void) { return gSpace[0]; }
static size_t s1(void) { return gSpace[1]; }

/* Fake I2C: two devices, register 0x0E selects the map of 0x42 */
static uint8_t gRegs[256][4][256];
static uint8_t gMap[256];
static int gMapLeftNonZero;          /* set when a Poll step returns with 0x0E != 0 */
static int gFailDev = -1;
static int i2cRead(uint8_t dev, uint8_t reg, uint8_t* val) {
    if (dev == gFailDev) return -1;
    *val = gRegs[dev][gMap[dev] / 0x20 & 3][reg];
    return 0;
}
static int i2cWrite(uint8_t dev, uint8_t reg, uint8_t val) {
    if (dev == gFailDev) return -1;
    if (reg == 0x0E) gMap[dev] = val; else gRegs[dev][gMap[dev] / 0x20 & 3][reg] = val;
    return 0;
}

static void reset(void) {
    memset(gOut, 0, sizeof(gOut)); gOutN[0] = gOutN[1] = 0;
    gConn[0] = gConn[1] = 1; gSpace[0] = gSpace[1] = 512;
    memset(gMap, 0, sizeof(gMap)); gFailDev = -1;
    static const ConsolePort p0 = {w0, c0, s0}, p1 = {w1, c1, s1};
    CONSOLE_Init(&p0, &p1, i2cRead, i2cWrite);
    gOutN[0] = gOutN[1] = 0; gOut[0][0] = gOut[1][0] = 0;
}
static void type(int port, const char* s) { while (*s) CONSOLE_Feed((uint8_t)port, *s++); }
static void pollN(int n, uint32_t now) {
    for (int i = 0; i < n; i++) { CONSOLE_Poll(now); if (gMap[0x42]) gMapLeftNonZero = 1; }
}

static void test_echo_and_legacy_read(void) {
    reset();
    gRegs[0x42][0][0x10] = 0xAB;
    type(0, "r 42 10\r");
    CHECK(!strcmp(gOut[0], "r 42 10\r\nAB\r\n>"));
    CHECK(gOutN[1] == 0);                                      /* reply only to the issuing port */
}

static void test_crlf_and_empty_lines(void) {
    reset();
    type(0, "\r\n");
    CHECK(!strcmp(gOut[0], "\r\n>\r\n>"));                     /* legacy: CR and LF each end a line */
}

static void test_backspace_and_unknown(void) {
    reset();
    type(0, "x\b");
    CHECK(!strcmp(gOut[0], "x\b \b"));
    reset();
    type(0, "zz 1\r");
    CHECK(!strcmp(gOut[0], "zz 1\r\n?\r\n>"));
}

static void test_write_and_map_write(void) {
    reset();
    type(0, "w 42 20 5A\r");
    CHECK(strstr(gOut[0], "OK\r\n>") != NULL);
    CHECK(gRegs[0x42][0][0x20] == 0x5A);
    reset();
    type(0, "wm 42 20 31 07\r");
    CHECK(strstr(gOut[0], "OK\r\n>") != NULL);
    CHECK(gRegs[0x42][1][0x31] == 0x07 && gMap[0x42] == 0);
    reset();
    gFailDev = 0x42;
    type(0, "r 42 10\r");
    CHECK(strstr(gOut[0], "ERR\r\n>") != NULL);
}

static void test_map_read(void) {
    reset();
    gRegs[0x42][2][0x05] = 0x3C;
    type(0, "rm 42 40 05\r");
    CHECK(strstr(gOut[0], "3C\r\n>") != NULL && gMap[0x42] == 0);
}

/* Dumps run one register per Poll, keep the legacy format and print ">" only at the end */
static void test_incremental_dump_format(void) {
    reset();
    for (int i = 0; i < 18; i++) gRegs[0x42][0][i] = (uint8_t)i;
    type(0, "d 42 00 12\r");
    CHECK(strchr(gOut[0], '>') == NULL);                        /* not yet */
    pollN(40, 0);
    CHECK(strstr(gOut[0], "\r\n00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n10 11 \r\n>") != NULL);
}

static void test_map_dump_never_leaves_the_map_selected(void) {
    reset();
    gMapLeftNonZero = 0;
    for (int i = 0; i < 4; i++) gRegs[0x42][1][i] = (uint8_t)(0xA0 + i);
    type(0, "dm 42 20 00 04\r");
    CHECK(gMap[0x42] == 0);
    pollN(10, 0);
    CHECK(!gMapLeftNonZero);
    CHECK(strstr(gOut[0], "A0 A1 A2 A3 \r\n>") != NULL);
    reset();
    gFailDev = 0x42;
    type(0, "dm 42 20 00 04\r");
    CHECK(strstr(gOut[0], "ERR\r\n>") != NULL);                 /* the map switch fails at the start */
}

static void test_dump_waits_for_room(void) {
    reset();
    gSpace[0] = 2;                                              /* a full ring: no step */
    type(0, "d 42 00 04\r");
    size_t before = gOutN[0];
    pollN(5, 0);
    CHECK(gOutN[0] == before);
    gSpace[0] = 512;
    pollN(10, 0);
    CHECK(strstr(gOut[0], "\r\n>") != NULL);
}

static void test_two_ports_are_independent(void) {
    reset();
    gRegs[0x42][0][1] = 0x11; gRegs[0x42][0][2] = 0x22;
    type(0, "r 42 ");
    type(1, "r 42 02\r");
    type(0, "01\r");
    CHECK(strstr(gOut[1], "22\r\n>") != NULL);
    CHECK(strstr(gOut[0], "11\r\n>") != NULL);
}

static void test_help_word_command(void) {
    reset();
    type(1, "help\r");
    CHECK(strstr(gOut[1], "rm AA MM RR") != NULL && strstr(gOut[1], "log on|off") != NULL);
    CHECK(gOutN[0] == 0);
}

static int gCustom;
static void customCmd(uint8_t port, const char* args) { gCustom++; CONSOLE_Printf(port, "custom[%s]\r\n", args); }

static void test_registered_word_command(void) {
    reset();
    CONSOLE_AddCommand("info", customCmd);
    type(0, "info now\r");
    CHECK(gCustom == 1 && strstr(gOut[0], "custom[now]\r\n>") != NULL);
}

static void test_log_broadcast_and_rate_limit(void) {
    reset();
    type(1, "log on\r");
    gOutN[1] = 0; gOut[1][0] = 0;
    CONSOLE_Poll(1000);
    for (int i = 0; i < 25; i++) CONSOLE_Log("ev %d", i);
    CHECK(gOutN[0] == 0);                                       /* port 0 did not ask for the log */
    CHECK(strstr(gOut[1], "ev 19\r\n") != NULL && strstr(gOut[1], "ev 20\r\n") == NULL);
    CONSOLE_Poll(2001);                                         /* next second: the drop count is reported */
    CHECK(strstr(gOut[1], "dropped 5") != NULL);
    gConn[1] = 0;
    size_t n = gOutN[1];
    CONSOLE_Log("hidden");
    CHECK(gOutN[1] == n);                                       /* a port that is not connected gets nothing */
}

static void test_overflow_truncates_the_line(void) {
    reset();
    char line[100]; memset(line, 'a', sizeof(line) - 1); line[99] = 0;
    type(0, line);
    type(0, "\r");
    CHECK(strstr(gOut[0], "?\r\n>") != NULL);                    /* no crash, unknown command */
}

int main(void) {
    test_echo_and_legacy_read();
    test_crlf_and_empty_lines();
    test_backspace_and_unknown();
    test_write_and_map_write();
    test_map_read();
    test_incremental_dump_format();
    test_map_dump_never_leaves_the_map_selected();
    test_dump_waits_for_room();
    test_two_ports_are_independent();
    test_help_word_command();
    test_registered_word_command();
    test_log_broadcast_and_rate_limit();
    test_overflow_truncates_the_line();
    printf("console: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
