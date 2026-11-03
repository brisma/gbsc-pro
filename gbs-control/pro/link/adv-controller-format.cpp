#include "adv-controller-format.h"
#include <stdio.h>
#include <string.h>
#include "../ui/menu-model.h"
#include "../api/api-json.h"
#ifdef ARDUINO
#include <pgmspace.h>   // format strings in flash: ~600 bytes of RAM
#else
#define snprintf_P snprintf
#define PSTR(s) (s)
#endif

void advControllerMenuText(const AdvControllerInfo& h, char* out, size_t n) {
    switch (h.status) {
        case ADV_CONTROLLER_CHECKING: snprintf(out, n, "Checking..."); return;
        case ADV_CONTROLLER_NO_REPLY: snprintf(out, n, "No reply"); return;
        case ADV_CONTROLLER_INCOMPATIBLE: snprintf(out, n, "Incompatible"); return;
        default:
            snprintf(out, n, "%s", h.info.fw);
            menuSanitize(out);
            return;
    }
}

static const char* const kStandardNames[8] = {"NTSC_M", "NTSC_443", "PAL_M", "PAL_60", "PAL", "SECAM", "PAL_N",
                                              "SECAM_525"};   // AD_RESULT order; the API keeps these names

static int signalJson(const AdvSignal& g, char* out, size_t n) {
    char std[16], hz[8], enc[8];
    if (g.standard < 8) snprintf(std, sizeof(std), "\"%s\"", kStandardNames[g.standard]); else strcpy(std, "null");
    if (g.hz) snprintf(hz, sizeof(hz), "%u", (unsigned)g.hz); else strcpy(hz, "null");
    static const char* const kEncoder[3] = {"null", "\"525p\"", "\"625p\""};
    strcpy(enc, g.encoder < 3 ? kEncoder[g.encoder] : "null");
#define B(f) ((g.flags & (f)) ? "true" : "false")
    return snprintf_P(out, n,
                    PSTR("{\"gen\":%u,\"powered\":%s,\"present\":%s,\"lock\":%s,\"fsc\":%s,\"freerun\":%s,"
                    "\"interlaced\":%s,\"colorkill\":%s,\"nonstd\":%s,\"standard\":%s,\"hz\":%s,\"vidsel\":\"0x%02X\",\"encoder\":%s}"),
                    (unsigned)g.gen, B(ADV_SIG_POWERED), B(ADV_SIG_PRESENT), B(ADV_SIG_LOCK), B(ADV_SIG_FSC),
                    B(ADV_SIG_FREERUN), B(ADV_SIG_INTERLACED), B(ADV_SIG_COLORKILL), B(ADV_SIG_NONSTD), std, hz, (unsigned)g.vidSel, enc);
#undef B
}

int8_t advControllerStandard(const AdvControllerInfo& h, const AdvSignal* sig, bool signalFresh) {
    if (sig && signalFresh && h.status == ADV_CONTROLLER_OK)
        return (sig->flags & ADV_SIG_LOCK) && sig->standard < 8 ? (int8_t)sig->standard : -1;
    return h.haveInfo && (h.info.flags & ADV_INFO_LOCK) ? (int8_t)(h.info.standard & 7) : -1;
}

bool advControllerStatusJson(const AdvControllerInfo& h, bool updateAvailable, const char* update, char* out, size_t n,
                    const AdvSignal* sig) {
    static const char* const kStatus[] = {"checking", "ok", "no_reply", "incompatible"};
    const char* st = kStatus[h.status <= ADV_CONTROLLER_INCOMPATIBLE ? h.status : 0];
    int w;
    if (!h.haveInfo || h.status == ADV_CONTROLLER_NO_REPLY || h.status == ADV_CONTROLLER_CHECKING) {
        bool idle = !update || strcmp(update, "idle") == 0;
        if (idle) w = snprintf(out, n, "{\"status\":\"%s\"}", st);
        else w = snprintf(out, n, "{\"status\":\"%s\",\"update\":\"%s\"}", st, update);   // e.g. during the copy
    } else {
        char ver[32];
        jsonEscape(h.info.fw, ver, sizeof(ver));
        char caps[40] = "";                           // "signal","reg_read","listen": what the image answers
        size_t k = 0;
        static const struct { uint8_t bit; const char* name; } kCaps[] = {
            {ADV_INFO_CAP_SIGNAL, "signal"}, {ADV_INFO_CAP_REG_READ, "reg_read"}, {ADV_INFO_CAP_LISTEN, "listen"}};
        for (const auto& c : kCaps)
            if ((h.info.caps & c.bit) && k < sizeof(caps))
                k += (size_t)snprintf(caps + k, sizeof(caps) - k, "%s\"%s\"", k ? "," : "", c.name);
        w = snprintf_P(out, n,
                     PSTR("{\"status\":\"%s\",\"version\":\"%s\",\"build\":\"%08lX\",\"lock\":%u,\"standard\":%u,"
                     "\"uptime\":%u,\"bad_frames\":%u,\"not_applied\":%s,\"update_available\":%s,\"update\":\"%s\","
                     "\"caps\":[%s]}"),
                     st, ver, (unsigned long)h.info.buildId, (h.info.flags & ADV_INFO_LOCK) ? 1u : 0u,
                     (unsigned)h.info.standard, (unsigned)h.info.uptimeS, (unsigned)h.info.badFrames,
                     h.notApplied ? "true" : "false", updateAvailable ? "true" : "false", update ? update : "idle", caps);
        if (sig && w > 1 && (size_t)w < n) {         // reopen the object: ,"signal":{...}}
            w--;
            int a = snprintf(out + w, n - w, ",\"signal\":");
            if (a < 0) return false;
            w += a;
            if ((size_t)w >= n) return false;
            int b = signalJson(*sig, out + w, n - w);
            if (b < 0) return false;
            w += b;
            if ((size_t)w >= n) return false;
            int c = snprintf(out + w, n - w, "}");
            if (c < 0) return false;
            w += c;
        }
    }
    return w >= 0 && (size_t)w < n;
}

size_t advRegReadJson(const AdvRegRead& r, uint8_t announcedGen, char* out, size_t n) {
    static const char* const kState[] = {"idle", "pending", "done", "error", "unsupported"};
    int w;
    if (announcedGen != r.gen) w = snprintf(out, n, "{\"state\":\"pending\",\"gen\":%u}", (unsigned)announcedGen);
    else {
        w = snprintf(out, n, "{\"state\":\"%s\",\"dev\":%u,\"map\":%u,\"reg\":%u,\"values\":[",
                     kState[r.state <= ADV_REG_UNSUPPORTED ? r.state : 0], (unsigned)r.dev, (unsigned)r.map, (unsigned)r.reg);
        uint8_t k = r.state == ADV_REG_DONE ? r.count : 0;
        for (uint8_t i = 0; i < k && w >= 0 && (size_t)w < n; i++)
            w += snprintf(out + w, n - w, i ? ",%u" : "%u", (unsigned)r.v[i]);
        if (w >= 0 && (size_t)w < n) w += snprintf(out + w, n - w, "],\"gen\":%u}", (unsigned)r.gen);
    }
    if (w < 0 || (size_t)w >= n) { if (n) out[0] = '\0'; return 0; }
    return (size_t)w;
}
