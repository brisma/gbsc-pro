#include "console_fmt.h"
#include <stdio.h>

static const char* inputName(uint8_t in) {
    static const char* const k[7] = {"-", "RGBs", "RGsB", "VGA", "YPbPr", "S-Video", "Composite"};
    return in < 7 ? k[in] : "?";
}

void conFmtInfo(const ConInfo* i, char* out, size_t n) {
    int w = snprintf(out, n, "fw %s build %08lX up %lus\r\nlink rx %u bad %u dropped %u applied %u hash %04X\r\ninput %s lock %u\r\n",
                     i->fw, (unsigned long)i->build, (unsigned long)i->uptimeS, (unsigned)i->rx, (unsigned)i->bad,
                     (unsigned)i->dropped, (unsigned)i->applied, (unsigned)i->hash, inputName(i->input), (unsigned)i->lock);
    if (w < 0 || (size_t)w >= n) return;
    if (i->usbUp)
        snprintf(out + w, n - (size_t)w, "usb up irq/s %lu isr-max %luus dropped %lu\r\n", (unsigned long)i->irqRate,
                 (unsigned long)i->isrUs, (unsigned long)i->usbDropped);
    else
        snprintf(out + w, n - (size_t)w, "usb off\r\n");
}

void conFmtUpdate(const ConUpd* u, char* out, size_t n) {
    static const char* const kState[6] = {"IDLE", "ERASING", "READY", "RECEIVING", "VERIFYING", "ERROR"};
    static const char* const kErr[5] = {"", "program", "erase", "crc", "header"};
    const char* st = u->state < 6 ? kState[u->state] : "?";
    int w = snprintf(out, n, "update %s %lu/%lu erased %u/%u", st, (unsigned long)u->next, (unsigned long)u->size,
                     (unsigned)u->erased, (unsigned)u->sectors);
    if (w < 0 || (size_t)w >= n) return;
    if (u->err && u->err < 5) w += snprintf(out + w, n - (size_t)w, " error %s", kErr[u->err]);
    if (w >= 0 && (size_t)w < n) snprintf(out + w, n - (size_t)w, "\r\n");
}

void conFmtEvent(const LinkEvent* e, char* out, size_t n) {
    if (e->kind == LINK_EV_STATE && e->failed)
        snprintf(out, n, "state applied steps %02X i2c failed %02X", (unsigned)e->steps, (unsigned)e->failed);
    else if (e->kind == LINK_EV_STATE) snprintf(out, n, "state applied steps %02X", (unsigned)e->steps);
    else snprintf(out, n, "rx cmd %02X seq %u len %u", (unsigned)e->cmd, (unsigned)e->seq, (unsigned)e->len);
}
