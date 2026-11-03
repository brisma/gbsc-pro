/* Host tests of the console text for info / update / log events (make test) */
#include <stdio.h>
#include <string.h>
#include "console_fmt.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(void) {
    char b[512];
    ConInfo i = {"3.0.0", 0x25D00871u, 125, 1234, 2, 0, 77, 0xC3D4u, 5, 1, 1, 1000, 6, 3};
    conFmtInfo(&i, b, sizeof(b));
    CHECK(strstr(b, "fw 3.0.0 build 25D00871 up 125s\r\n") != NULL);
    CHECK(strstr(b, "link rx 1234 bad 2 dropped 0 applied 77 hash C3D4\r\n") != NULL);
    CHECK(strstr(b, "input S-Video lock 1\r\n") != NULL);
    CHECK(strstr(b, "usb up irq/s 1000 isr-max 6us dropped 3\r\n") != NULL);
    i.usbUp = 0; i.input = 1;
    conFmtInfo(&i, b, sizeof(b));
    CHECK(strstr(b, "input RGBs") != NULL && strstr(b, "usb off\r\n") != NULL);

    ConUpd u = {2, 0, 1024, 40720, 5, 5};
    conFmtUpdate(&u, b, sizeof(b));
    CHECK(!strcmp(b, "update READY 1024/40720 erased 5/5\r\n"));
    u.state = 5; u.err = 3;
    conFmtUpdate(&u, b, sizeof(b));
    CHECK(strstr(b, "update ERROR") != NULL && strstr(b, "error crc") != NULL);

    LinkEvent f = {LINK_EV_FRAME, 0x01, 7, 40, 0, 0}, s = {LINK_EV_STATE, 0x01, 0, 0, 0x21, 0};
    conFmtEvent(&f, b, sizeof(b));
    CHECK(!strcmp(b, "rx cmd 01 seq 7 len 40"));
    conFmtEvent(&s, b, sizeof(b));
    CHECK(!strcmp(b, "state applied steps 21"));
    s.failed = 0x20;                                  /* BCSH writes not acknowledged: retried with the next STATE */
    conFmtEvent(&s, b, sizeof(b));
    CHECK(!strcmp(b, "state applied steps 21 i2c failed 20"));
    printf("console_fmt: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
