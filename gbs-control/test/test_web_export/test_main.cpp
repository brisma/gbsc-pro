// Web app tooling: the real registry,
// actions, routes, menu tree, predicates and validators exported as JSON for the simulator, the page model and the
// parity check. With WEB_EXPORT_DIR set the files are written there (tools/gen.mjs); otherwise every export is only
// built and checked in memory.
#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "host-fs.h"
#include <map>
#include <set>
#include <string>
#include <vector>
#include "platform-caps.h"
#include "settings-registry.h"
#include "settings-store.h"
#include "settings-request.h"
#include "settings-bridge.h"
#include "api-actions.h"
#include "api-route-match.h"
#include "menu-types.h"
#include "menu-tree.h"
#include "menu-model.h"
#include "menu-strings.h"
#include "menu-catalog.h"
#include "menu-fake.h"
#include "log-ring.h"

#ifndef WEB_DIR
#define WEB_DIR "public"
#endif

void setUp(void) { platformSetCaps(CAPS_DEFAULT); }
void tearDown(void) { platformSetCaps(CAPS_DEFAULT); }

// ---- enum names (a new enum value without a name fails the build) ----
static const char* const kPredNames[] = {"none", "devmode", "svav", "sv", "av", "sv_wy", "av_ntsc", "av_pal", "rgb_sync",
    "rgbs", "ace_on", "scanlines_ok", "profile_active", "update_available", "adv_controller_update", "vga", "i2p_on",
    "deint_ok", "ftl_ok", "ftl_on", "wifi_ap", "sog", "tv_osd", "update_manual"};
static_assert(sizeof(kPredNames) / sizeof(kPredNames[0]) == P_COUNT, "a MenuPred has no name");
static const char* const kFnNames[] = {"none", "open_info", "decoder_reset", "geometry_reset", "wifi_reset", "check_update",
    "profile_save_active", "profile_load_list", "profile_manage_list", "termination", "edit_ota", "edit_hosts",
    "profile_name_list", "geometry_keep", "adv_controller_skip", "geometry_undo", "picture_reset", "quick_profile",
    "tv_osd", "check_channel_stable", "check_channel_beta", "install_manual", "profile_discard"};
static_assert(sizeof(kFnNames) / sizeof(kFnNames[0]) == FN_COUNT, "a MenuFn has no name");
static const char* const kLiveNames[] = {"none", "adc_gain", "position", "size", "borders", "htotal", "pll_div", "sdram",
    "osr", "adc_filter", "debug_view", "freeze", "if_offset", "sog", "out_rate", "syncwatcher", "osd_always_on",
    "wifi_rssi", "osd_test", "print_info", "ir_log", "sync_polarity", "ota", "border_pos"};
static_assert(sizeof(kLiveNames) / sizeof(kLiveNames[0]) == LV_COUNT, "a MenuLive has no name");
static const char* const kTextNames[] = {"none", "wifi_mode", "wifi_ssid", "wifi_ip", "update_version", "update_result",
    "fw_gbs", "fw_adv", "ota_password", "extra_hosts", "profile_active", "wifi_pass", "wifi_host", "source_name",
    "adv_controller_new", "profile_label", "profile_save", "fw_channel", "update_offer", "update_confirm",
    "manual_version", "manual_offer", "manual_confirm"};
static_assert(sizeof(kTextNames) / sizeof(kTextNames[0]) == LT_COUNT, "a MenuText has no name");
static const char* const kTypeNames[] = {"sub", "setting", "choice", "stepper", "action", "toggle_fn", "live", "adjust2d",
                                         "header"};
static const char* const kFlagNames[] = {"revert", "restart_note", "confirm", "fn", "commit", "percent", "advanced"};
static const char* const kChoiceNames[] = {"filter", "scanlines", "resolution"};
static const char* const kAdjustNames[] = {"move", "size", "borders", "border_move"};
static const char* const kChoiceFlagNames[] = {"advanced", "original", "alias", "sid1_only"};

// ---- JSON writing ----
static void sinkStr(const char* t, void* c) { ((std::string*)c)->append(t); }

static std::string q(const char* s) {
    std::string o = "\"";
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        char b[8];
        if (*p == '"' || *p == '\\') { o += '\\'; o += (char)*p; }
        else if (*p < 0x20 || *p >= 0x7F) { snprintf(b, sizeof(b), "\\u%04x", *p); o += b; }
        else o += (char)*p;
    }
    return o + "\"";
}
static std::string q(const std::string& s) { return q(s.c_str()); }
static std::string num(long v) { char b[24]; snprintf(b, sizeof(b), "%ld", v); return b; }

static std::string menuText(uint16_t id) { char b[96]; menuStrCopy(id, b, sizeof(b)); return b; }
static std::string settingKey(uint8_t sid) { SettingDef d; settingsDefAt(sid, d); char k[48]; settingsKeyCopy(d, k, sizeof(k)); return k; }
static std::string actionName(uint8_t aid) { ActionDef d; actionsDefAt(aid, d); return d.name; }

// ---- minimal JSON reader: validity check and the vectors files ----
struct JVal {
    enum Kind { NUL, BOOL, NUM, STR, ARR, OBJ } kind = NUL;
    bool b = false; double n = 0; std::string s;
    std::vector<JVal> a; std::vector<std::pair<std::string, JVal>> o;
    const JVal* get(const char* k) const { for (auto& kv : o) if (kv.first == k) return &kv.second; return nullptr; }
};
struct JParser {
    const char* p; bool ok;
    explicit JParser(const char* text) : p(text), ok(true) {}
    void ws() { while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++; }
    bool lit(const char* w) { size_t n = strlen(w); if (strncmp(p, w, n)) return false; p += n; return true; }
    std::string str() {
        std::string o; if (*p != '"') { ok = false; return o; } p++;
        while (*p && *p != '"') {
            if (*p == '\\') {
                p++;
                if (*p == 'u') { unsigned v = 0; for (int i = 1; i <= 4; i++) { char c = p[i]; v = v * 16 + (c <= '9' ? c - '0' : (c | 32) - 'a' + 10); } o += (char)v; p += 5; continue; }
                o += *p == 'n' ? '\n' : *p == 't' ? '\t' : *p == 'r' ? '\r' : *p; p++; continue;
            }
            if ((unsigned char)*p < 0x20) { ok = false; return o; }
            o += *p++;
        }
        if (*p != '"') { ok = false; return o; } p++; return o;
    }
    JVal val() {
        JVal v; ws();
        if (*p == '{') {
            v.kind = JVal::OBJ; p++; ws();
            if (*p == '}') { p++; return v; }
            for (;;) { ws(); std::string k = str(); ws(); if (*p != ':') { ok = false; return v; } p++;
                v.o.push_back({k, val()}); ws(); if (*p == ',') { p++; continue; } if (*p == '}') { p++; return v; } ok = false; return v; }
        }
        if (*p == '[') {
            v.kind = JVal::ARR; p++; ws();
            if (*p == ']') { p++; return v; }
            for (;;) { v.a.push_back(val()); ws(); if (*p == ',') { p++; continue; } if (*p == ']') { p++; return v; } ok = false; return v; }
        }
        if (*p == '"') { v.kind = JVal::STR; v.s = str(); return v; }
        if (lit("true")) { v.kind = JVal::BOOL; v.b = true; return v; }
        if (lit("false")) { v.kind = JVal::BOOL; return v; }
        if (lit("null")) return v;
        char* e; v.n = strtod(p, &e); if (e == p) { ok = false; return v; } v.kind = JVal::NUM; p = e; return v;
    }
};
static bool parseJson(const std::string& text, JVal& out) {
    JParser j(text.c_str()); out = j.val(); j.ws(); return j.ok && *j.p == '\0';
}
static std::string readFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb"); if (!f) return "";
    std::string s; char b[4096]; size_t n; while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n); fclose(f); return s;
}

// ---- boards ----
struct Board { const char* name; uint8_t caps; };
static const Board kBoards[] = {{"pro", CAPS_DEFAULT}, {"standard", (uint8_t)(CAP_ORIGINAL | CAP_LED)}};
static uint8_t boardCaps(const std::string& b) { return b == "standard" ? kBoards[1].caps : kBoards[0].caps; }

// ---- schema, actions, values, registry, routes ----
static std::string exportSchema(uint8_t caps, bool dev) { platformSetCaps(caps); std::string s; settingsSchemaJson(dev, sinkStr, &s); return s; }
static std::string exportActions(uint8_t caps, bool dev) { platformSetCaps(caps); std::string s; actionsListJson(dev, sinkStr, &s); return s; }
static std::string exportValues(uint8_t caps) {
    platformSetCaps(caps);
    userOptions u; memset(&u, 0, sizeof(u)); settingsApplyDefaults(u);
    std::string s; settingsValuesJson(u, true, sinkStr, &s); return s;
}

// Every action whatever the board and devmode: flags as names, params as actionsListJson describes them
static std::string exportActionsAll() {
    static const char* const kActFlags[] = {"dev", "confirm", "hex", "bytes", "opt_bytes", "regs", "text", "opt_text"};
    static const char* const kKinds[] = {"serial", "user", "func"};
    platformSetCaps(0xFF);
    std::string listed; actionsListJson(true, sinkStr, &listed);
    JVal list; parseJson(listed, list);
    std::string s = "[";
    for (uint8_t i = 0; i < actionsCount(); i++) {
        ActionDef d; actionsDefAt(i, d);
        std::string name = d.name;
        s += std::string(i ? "," : "") + "{\"name\":" + q(name) + ",\"caps\":" + num(d.caps) + ",\"kind\":\"" + kKinds[d.kind] + "\",\"flags\":[";
        bool first = true;
        for (uint8_t b = 0; b < 8; b++)
            if (d.flags & (1 << b)) { s += (first ? "\"" : ",\"") + std::string(kActFlags[b]) + "\""; first = false; }
        s += "],\"params\":";
        std::string params = "[]";
        for (size_t k = 0; k < list.a.size(); k++) {
            const JVal& e = list.a[k];
            if (e.get("name")->s != name || !e.get("params")) continue;
            // re-serialise the listed params (flat objects of strings and numbers, "values" arrays of strings)
            params = "[";
            for (size_t m = 0; m < e.get("params")->a.size(); m++) {
                const JVal& pv = e.get("params")->a[m];
                params += std::string(m ? "," : "") + "{";
                for (size_t f = 0; f < pv.o.size(); f++) {
                    params += std::string(f ? "," : "") + q(pv.o[f].first) + ":";
                    const JVal& v = pv.o[f].second;
                    if (v.kind == JVal::STR) params += q(v.s);
                    else if (v.kind == JVal::NUM) params += num((long)v.n);
                    else if (v.kind == JVal::ARR) {
                        params += "[";
                        for (size_t z = 0; z < v.a.size(); z++) params += std::string(z ? "," : "") + q(v.a[z].s);
                        params += "]";
                    } else params += "null";
                }
                params += "}";
            }
            params += "]";
        }
        s += params + "}";
    }
    platformSetCaps(CAPS_DEFAULT);
    return s + "]";
}

// "null" | "\"toggle\"" | "[v,...]": a value map fires for each listed value even when unchanged (no echo then)
static std::string settingBridge(const SettingDef& d) {
    SettingsBridgeHit h;
    std::string vals;
    for (uint32_t v = d.min; v <= d.max && v <= 64; v++)
        if (settingsBridgeLookup(d, (uint16_t)v, (uint16_t)v, h)) vals += std::string(vals.empty() ? "" : ",") + num(v);
    if (!vals.empty()) return "[" + vals + "]";
    if (d.max > d.min && settingsBridgeLookup(d, d.min, (uint16_t)(d.min + 1), h)) return "\"toggle\"";
    return "null";
}

static bool settingBridged(const SettingDef& d) {
    SettingsBridgeHit h;
    for (uint32_t v = d.min; v <= d.max && v <= 64; v++)
        if (settingsBridgeLookup(d, (uint16_t)(v == d.min ? d.min + 1 : d.min), (uint16_t)v, h)) return true;
    return false;
}

static std::string exportRegistry() {
    platformSetCaps(0xFF);   // every option, whatever the board
    static const char* const kTypes[] = {"bool", "u8", "u16", "enum"};
    std::string s = "{\"boolWords\":{\"true\":[\"true\",\"on\"],\"false\":[\"false\",\"off\"]},\"settings\":[";
    for (uint8_t i = 0; i < settingsCount(); i++) {
        SettingDef d; settingsDefAt(i, d);
        char k[48]; settingsKeyCopy(d, k, sizeof(k));
        if (i) s += ",";
        s += "{\"key\":" + q(k) + ",\"type\":\"" + kTypes[(int)d.type] + "\",\"scope\":\"" +
             (d.scope == SettingScope::Device ? "device" : "profile") + "\",\"min\":" + num(d.min) + ",\"max\":" + num(d.max) +
             ",\"def\":" + num(d.def) + ",\"dev\":" + (settingsIsDevKey(d) ? "true" : "false") + ",\"caps\":" + num(settingsCaps(d)) +
             ",\"ro\":" + ((d.flags & SETTING_FLAG_DEVICE_ONLY) ? "true" : "false") + ",\"secret\":false,\"bridged\":" +
             (settingBridged(d) ? "true" : "false") + ",\"bridge\":" + settingBridge(d) + ",\"options\":[";
        if (d.type == SettingType::Enum) {
            char name[40]; uint16_t v; uint8_t oc;
            for (uint8_t o = 0; settingsEnumAtEx(d, o, name, sizeof(name), v, oc); o++) {
                if (o) s += ",";
                s += "{\"n\":" + q(name) + ",\"v\":" + num(v) + ",\"caps\":" + num(oc) + ",\"advanced\":" +
                     (settingsOptionAdvanced(d, v) ? "true" : "false") + "}";
            }
        }
        s += "]}";
    }
    for (uint8_t i = 0; i < settingsStrCount(); i++) {
        StrSettingDef d; settingsStrDefAt(i, d);
        char k[48]; settingsKeyCopyStr(d, k, sizeof(k));
        s += ",{\"key\":" + q(k) + ",\"type\":\"str\",\"scope\":\"" + (d.scope == SettingScope::Device ? "device" : "profile") +
             "\",\"maxLen\":" + num(d.maxLen) + ",\"dev\":false,\"caps\":0,\"ro\":" +
             ((d.flags & SETTING_FLAG_DEVICE_ONLY) ? "true" : "false") + ",\"secret\":" +
             ((d.flags & SETTING_FLAG_SECRET) ? "true" : "false") + ",\"bridged\":false,\"bridge\":null,\"options\":[]}";
    }
    platformSetCaps(CAPS_DEFAULT);
    return s + "]}";
}

#define KEY(methods, match, path, onRequest, onUpload, arg) {path, methods, match},
static const ApiRouteKey kNormal[] = {API_ROUTES_NORMAL(KEY)};
static const ApiRouteKey kSafe[] = {API_ROUTES_SAFE(KEY)};
#undef KEY
static std::string routeList(const ApiRouteKey* r, size_t n) {
    std::string s = "[";
    for (size_t i = 0; i < n; i++) {
        if (i) s += ",";
        std::string m = r[i].methods == ROUTE_GET ? "[\"GET\"]" : r[i].methods == ROUTE_POST ? "[\"POST\"]" : "[\"GET\",\"POST\"]";
        s += "{\"path\":" + q(r[i].path) + ",\"methods\":" + m + ",\"match\":\"" + (r[i].match == ROUTE_PREFIX ? "prefix" : "exact") + "\"}";
    }
    return s + "]";
}
static std::string exportRoutes() {
    return "{\"normal\":" + routeList(kNormal, sizeof(kNormal) / sizeof(kNormal[0])) + ",\"safe\":" +
           routeList(kSafe, sizeof(kSafe) / sizeof(kSafe[0])) + "}";
}

// ---- menu ----
struct NodeRef { std::string id; MenuNode n; };
static std::vector<NodeRef> gAllNodes;   // filled by exportMenu: every node with its id (root children and quick)

static std::string slug(const std::string& label) {
    std::string o; bool dash = false;
    for (char c : label) {
        char l = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
        if ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9')) { if (dash && !o.empty()) o += '-'; o += l; dash = false; }
        else dash = true;
    }
    return o.empty() ? "item" : o;
}

static std::string nodeJson(const MenuNode& n, const std::string& id);

static std::string childrenJson(const MenuNode& parent, const std::string& parentId, const char* prefix) {
    std::vector<MenuNode> kids(parent.count);
    std::vector<std::string> base(parent.count);
    for (uint8_t i = 0; i < parent.count; i++) { menuNodeRead(&parent.children[i], kids[i]); base[i] = slug(menuText(kids[i].label)); }
    std::vector<std::string> ids(parent.count);
    for (uint8_t i = 0; i < parent.count; i++) {
        int same = 0;
        for (uint8_t j = 0; j < parent.count; j++) if (base[j] == base[i]) same++;
        std::string s = base[i];
        if (same > 1) s += "~" + (kids[i].type == N_SETTING ? settingKey(kids[i].ref) : std::string(kPredNames[kids[i].pred]));
        ids[i] = s;
    }
    for (uint8_t i = 0; i < parent.count; i++) {   // still equal (same pred): the position decides
        int same = 0;
        for (uint8_t j = 0; j < parent.count; j++) if (ids[j] == ids[i]) same++;
        if (same > 1) ids[i] += "~n" + num(i);
    }
    std::string s = "[";
    for (uint8_t i = 0; i < parent.count; i++) {
        std::string id = parentId.empty() ? std::string(prefix) + ids[i] : parentId + "/" + ids[i];
        if (i) s += ",";
        s += nodeJson(kids[i], id);
    }
    return s + "]";
}

static std::string nodeJson(const MenuNode& n, const std::string& id) {
    TEST_ASSERT_TRUE_MESSAGE(n.type <= N_HEADER, "unknown MenuNode type");
    TEST_ASSERT_TRUE_MESSAGE(n.flags < (1 << (sizeof(kFlagNames) / sizeof(kFlagNames[0]))), "unknown NF_ bit");
    if (!id.empty()) gAllNodes.push_back({id, n});
    std::string s = "{\"id\":" + q(id) + ",\"type\":\"" + kTypeNames[n.type] + "\",\"label\":" + q(menuText(n.label)) +
                    ",\"help\":" + q(n.help ? menuText(n.help) : "") + ",\"flags\":[";
    bool first = true;
    for (uint8_t b = 0; b < sizeof(kFlagNames) / sizeof(kFlagNames[0]); b++)
        if (n.flags & (1 << b)) { s += (first ? "\"" : ",\"") + std::string(kFlagNames[b]) + "\""; first = false; }
    s += "],\"pred\":\"" + std::string(kPredNames[n.pred]) + "\"";
    switch (n.type) {
        case N_SETTING:
            s += ",\"setting\":" + q(settingKey(n.ref));
            if (n.opts >= 100) s += ",\"step\":" + num(n.opts - 100);
            else if (n.opts) {
                const char* list = menuOptList(n.opts);
                s += ",\"options\":[";
                char name[40]; uint16_t v;
                for (uint8_t i = 0; list && menuOptAt(list, i, name, sizeof(name), v); i++)
                    s += std::string(i ? "," : "") + "{\"n\":" + q(name) + ",\"v\":" + num(v) + "}";
                s += "]";
            }
            break;
        case N_ACTION:
            if (n.flags & NF_FN) s += ",\"fn\":\"" + std::string(kFnNames[n.ref]) + "\"";
            else s += ",\"action\":" + q(actionName(n.ref));
            if (n.ref2) s += ",\"live\":\"" + std::string(kLiveNames[n.ref2]) + "\"";
            if (n.opts) s += ",\"text\":\"" + std::string(kTextNames[n.opts]) + "\"";
            break;
        case N_STEPPER:
            s += ",\"action\":" + q(actionName(n.ref)) + ",\"action_inc\":" + q(actionName(n.opts));
            if (n.ref2) s += ",\"live\":\"" + std::string(kLiveNames[n.ref2]) + "\"";
            break;
        case N_TOGGLE_FN: s += ",\"fn\":\"" + std::string(kFnNames[n.ref]) + "\""; break;
        case N_LIVE: s += ",\"text\":\"" + std::string(kTextNames[n.ref]) + "\""; break;
        case N_CHOICE: s += ",\"choice\":\"" + std::string(kChoiceNames[n.ref]) + "\""; break;
        case N_ADJUST2D:
            s += ",\"adjust\":\"" + std::string(kAdjustNames[n.ref]) + "\"";
            if (n.ref2) s += ",\"live\":\"" + std::string(kLiveNames[n.ref2]) + "\"";
            break;
        case N_SUB:
            if (n.flags & NF_FN) s += ",\"fn\":\"" + std::string(kFnNames[n.ref]) + "\"";
            else s += ",\"children\":" + childrenJson(n, id, id.empty() ? "" : "");
            break;
        default: break;
    }
    return s + "}";
}

static std::string choiceJson(uint8_t index) {
    MenuChoice c; menuChoiceRead(index, c);
    std::string s = "[";
    for (uint8_t i = 0; i < c.count; i++) {
        MenuChoiceOpt o; menuChoiceOptRead(c, i, o);
        if (i) s += ",";
        s += "{\"label\":" + q(menuText(o.label)) + ",\"writes\":[{\"key\":" + q(settingKey(o.sid1)) + ",\"value\":" + num(o.v1) + "}";
        if (o.sid2 != 0xFF) s += ",{\"key\":" + q(settingKey(o.sid2)) + ",\"value\":" + num(o.v2) + "}";
        s += "],\"flags\":[";
        bool first = true;
        for (uint8_t b = 0; b < 4; b++)
            if (o.flags & (1 << b)) { s += (first ? "\"" : ",\"") + std::string(kChoiceFlagNames[b]) + "\""; first = false; }
        s += "]}";
    }
    return s + "]";
}

static std::string exportMenu() {
    gAllNodes.clear();
    MenuNode root; menuNodeRead(menuRoot(), root);
    MenuNode quick; menuNodeRead(menuQuickRoot(), quick);
    std::string rootJson = "{\"id\":\"\",\"type\":\"sub\",\"label\":" + q(menuText(root.label)) + ",\"help\":\"\",\"flags\":[],\"pred\":\"none\",\"children\":" +
                           childrenJson(root, "", "") + "}";
    std::string quickJson = "{\"id\":\"quick\",\"type\":\"sub\",\"label\":" + q(menuText(quick.label)) + ",\"help\":\"\",\"flags\":[],\"pred\":\"none\",\"children\":" +
                            childrenJson(quick, "", "quick/") + "}";
    std::string s = "{\"version\":1,\"root\":" + rootJson + ",\"quick\":" + quickJson + ",\"choices\":{";
    for (uint8_t i = 0; i < 3; i++) s += std::string(i ? "," : "") + "\"" + kChoiceNames[i] + "\":" + choiceJson(i);
    s += "},\"adjust\":{";
    for (uint8_t i = 0; i < 4; i++) {
        MenuAdjust a; menuAdjustRead(i, a);
        s += std::string(i ? "," : "") + "\"" + kAdjustNames[i] + "\":{\"left\":" + q(actionName(a.left)) + ",\"right\":" +
             q(actionName(a.right)) + ",\"up\":" + q(actionName(a.up)) + ",\"down\":" + q(actionName(a.down)) + "}";
    }
    // menuEnabled reasons by name (the page's predicates port returns these names)
    static const struct { const char* name; uint16_t str; } kReasons[] = {
        {"wy_manual", S_R_WY_MANUAL}, {"rgbs", S_R_RGBS}, {"ace", S_R_ACE}, {"needs_bob", S_R_NEEDS_BOB},
        {"scanlines", S_R_SCANLINES}, {"2x_first", S_R_2X_FIRST}, {"2x_on", S_R_2X_ON}, {"force60", S_R_FORCE60},
        {"ftl_off", S_R_FTL_OFF}};
    s += "},\"reasons\":{";
    for (size_t i = 0; i < sizeof(kReasons) / sizeof(kReasons[0]); i++)
        s += std::string(i ? "," : "") + q(kReasons[i].name) + ":" + q(menuText(kReasons[i].str));
    return s + "}}";
}

static uint32_t countNodes(const MenuNode& n) {
    uint32_t c = 0;
    if (n.type == N_SUB && !(n.flags & NF_FN))
        for (uint8_t i = 0; i < n.count; i++) { MenuNode k; menuNodeRead(&n.children[i], k); c += 1 + countNodes(k); }
    return c;
}

// ---- predicates truth table ----
struct Scenario {
    const char* name; const char* board; uint8_t input; uint8_t menuMode; bool dev; int profile; bool wifiAp;
    bool noSignal, interlaced, scanlinesOk, advNtsc, updateAvailable, updateManual, advUpdate; uint8_t preset; uint16_t srcHz10;
    std::vector<std::pair<uint8_t, uint16_t>> values;
};

static std::vector<Scenario> scenarios() {
    std::vector<Scenario> v;
    auto base = [](const char* name, const char* board, uint8_t input, uint8_t mode) {
        Scenario s{name, board, input, mode, false, -1, false, false, false, true, false, false, false, false, 1, 600, {}};
        return s;
    };
    static char names[12][24];
    for (uint8_t in = 1; in <= 6; in++)
        for (uint8_t m = 0; m <= 1; m++) {
            char* nm = names[(in - 1) * 2 + m];
            snprintf(nm, 24, "pro-in%u-%s", in, m ? "advanced" : "easy");
            v.push_back(base(nm, "pro", in, m));
        }
    v.push_back(base("standard-rgbs-advanced", "standard", 1, 1));
    v.push_back(base("standard-vga-advanced", "standard", 3, 1));
    { Scenario s = base("developer", "pro", 1, 1); s.dev = true; v.push_back(s); }
    { Scenario s = base("no-signal", "pro", 1, 1); s.noSignal = true; s.scanlinesOk = false; s.srcHz10 = 0; v.push_back(s); }
    { Scenario s = base("interlaced-motion-adaptive", "pro", 1, 1); s.interlaced = true; s.scanlinesOk = false; s.values = {{SID_pic_deint, 0}}; v.push_back(s); }
    { Scenario s = base("svideo-2x-off", "pro", 5, 1); s.values = {{SID_adv_i2p, 0}}; v.push_back(s); }
    { Scenario s = base("svideo-ace-off", "pro", 5, 1); s.values = {{SID_adv_ace, 0}}; v.push_back(s); }
    { Scenario s = base("svideo-wy-override-off", "pro", 5, 1); s.values = {{SID_adv_filter_wy_ovr, 0}}; v.push_back(s); }
    { Scenario s = base("force60-pal-source", "pro", 1, 1); s.srcHz10 = 500; s.values = {{SID_output_force60, 1}}; v.push_back(s); }
    { Scenario s = base("smooth-scrolling-off", "pro", 1, 1); s.values = {{SID_output_ftl, 0}}; v.push_back(s); }
    { Scenario s = base("composite-ntsc", "pro", 6, 1); s.advNtsc = true; v.push_back(s); }
    { Scenario s = base("composite-pal", "pro", 6, 1); s.advNtsc = false; s.srcHz10 = 500; v.push_back(s); }
    { Scenario s = base("update-available", "pro", 1, 1); s.updateAvailable = true; v.push_back(s); }
    { Scenario s = base("update-manual", "pro", 1, 1); s.updateManual = true; v.push_back(s); }
    { Scenario s = base("adv-controller-update", "pro", 1, 1); s.advUpdate = true; v.push_back(s); }
    { Scenario s = base("hotspot", "pro", 1, 1); s.wifiAp = true; v.push_back(s); }
    { Scenario s = base("profile-active", "pro", 1, 1); s.profile = 0; v.push_back(s); }
    return v;
}

static std::string exportPredicates() {
    std::string s = "{\"scenarios\":[";
    std::vector<Scenario> list = scenarios();
    for (size_t k = 0; k < list.size(); k++) {
        const Scenario& sc = list[k];
        platformSetCaps(boardCaps(sc.board));
        FakeBackend be;
        be.u.menuMode = sc.menuMode;
        be.dev = sc.dev;
        be.u.devMode = sc.dev;
        be.activeIn = sc.input;
        be.u.inputChoice = sc.input;
        SettingDef src; settingsDefAt(SID_input_source, src); settingsPut(be.u, src, sc.input);
        be.active = sc.profile;
        be.texts[LT_WIFI_MODE] = sc.wifiAp ? "AP" : "Client";
        be.sig.noSignal = sc.noSignal; be.sig.interlaced = sc.interlaced; be.sig.scanlinesAllowed = sc.scanlinesOk;
        be.sig.advNtsc = sc.advNtsc; be.sig.updateAvailable = sc.updateAvailable; be.sig.updateManual = sc.updateManual;
        be.sig.advControllerUpdateAvailable = sc.advUpdate; be.sig.presetId = sc.preset; be.sig.srcHzX10 = sc.srcHz10;
        std::string vals;
        for (auto& p : sc.values) {
            SettingDef d; settingsDefAt(p.first, d); settingsPut(be.u, d, p.second);
            vals += std::string(vals.empty() ? "" : ",") + q(settingKey(p.first)) + ":" + num(p.second);
        }
        if (k) s += ",";
        s += "{\"name\":" + q(sc.name) + ",\"state\":{\"board\":" + q(sc.board) + ",\"input\":" + num(sc.input) +
             ",\"menu_mode\":" + num(sc.menuMode) + ",\"devmode\":" + (sc.dev ? "true" : "false") + ",\"profile_active\":" +
             num(sc.profile) + ",\"wifi_ap\":" + (sc.wifiAp ? "true" : "false") + ",\"signal\":{\"no_signal\":" +
             (sc.noSignal ? "true" : "false") + ",\"interlaced\":" + (sc.interlaced ? "true" : "false") + ",\"scanlines_ok\":" +
             (sc.scanlinesOk ? "true" : "false") + ",\"adv_ntsc\":" + (sc.advNtsc ? "true" : "false") + ",\"update_available\":" +
             (sc.updateAvailable ? "true" : "false") + ",\"update_manual\":" + (sc.updateManual ? "true" : "false") +
             ",\"adv_controller_update\":" + (sc.advUpdate ? "true" : "false") + ",\"preset_id\":" + num(sc.preset) +
             ",\"src_hz10\":" + num(sc.srcHz10) + "},\"values\":{" + vals + "}},\"visible\":[";
        std::string disabled;
        bool first = true;
        for (const NodeRef& r : gAllNodes) {
            if (menuVisible(r.n, be)) { s += (first ? "" : ",") + q(r.id); first = false; }
            uint16_t reason = S_NONE;
            if (!menuEnabled(r.n, be, &reason))
                disabled += std::string(disabled.empty() ? "" : ",") + q(r.id) + ":" + q(menuText(reason));
        }
        s += "],\"disabled\":{" + disabled + "}}";
    }
    platformSetCaps(CAPS_DEFAULT);
    return s + "]}";
}

// ---- validators on the vectors files ----
static std::string strOf(const JVal* v) { return v && v->kind == JVal::STR ? v->s : ""; }

static std::string exportValidate() {
    std::string s = "{\"settings\":[";
    JVal vec;
    std::string text = readFile(WEB_DIR "/sim/vectors/settings-requests.json");
    TEST_ASSERT_TRUE_MESSAGE(parseJson(text, vec) && vec.kind == JVal::ARR, "settings-requests.json");
    for (size_t i = 0; i < vec.a.size(); i++) {
        const JVal& t = vec.a[i];
        platformSetCaps(boardCaps(strOf(t.get("board"))));
        std::vector<const char*> keys, values;
        for (auto& k : t.get("keys")->a) keys.push_back(k.s.c_str());
        for (auto& k : t.get("values")->a) values.push_back(k.s.c_str());
        std::string err;
        SettingsPair out[32]; SettingsStrWrite sw; sw.index = -1;
        const JVal* dv = t.get("devmode");
        SettingsValidateResult r = settingsValidateRequest(keys.data(), values.data(), (uint8_t)keys.size(), dv && dv->b, sinkStr, &err, out, 32, &sw);
        if (i) s += ",";
        s += "{\"name\":" + q(strOf(t.get("name"))) + ",\"ok\":" + num(r.okCount) + ",\"errors\":{" + err + "},\"pairs\":[";
        for (uint8_t p = 0; p < r.pairCount; p++) s += std::string(p ? "," : "") + "{\"key\":" + q(settingKey(out[p].index)) + ",\"value\":" + num(out[p].value) + "}";
        s += "],\"str\":";
        if (sw.index >= 0) { StrSettingDef d; settingsStrDefAt((uint8_t)sw.index, d); char k[48]; settingsKeyCopyStr(d, k, sizeof(k)); s += "{\"key\":" + q(k) + ",\"value\":" + q(sw.value) + "}"; }
        else s += "null";
        s += "}";
    }
    s += "],\"actions\":[";
    JVal av;
    text = readFile(WEB_DIR "/sim/vectors/action-requests.json");
    TEST_ASSERT_TRUE_MESSAGE(parseJson(text, av) && av.kind == JVal::ARR, "action-requests.json");
    for (size_t i = 0; i < av.a.size(); i++) {
        const JVal& t = av.a[i];
        platformSetCaps(boardCaps(strOf(t.get("board"))));
        const JVal* dv = t.get("devmode");
        const JVal* data = t.get("data");
        bool hasData = data && data->kind == JVal::STR;
        std::string name = strOf(t.get("action"));
        // The checks of actionsPost (api-web.cpp), in the same order
        int status = 200;
        uint8_t buf[30]; uint8_t len = 0;
        int idx = actionsFind(name.c_str());
        ActionDef d;
        if (idx >= 0) actionsDefAt((uint8_t)idx, d);
        if (idx < 0 || !actionsAvailable(d, dv && dv->b)) status = 404;
        else {
            const char* txt = hasData ? data->s.c_str() : "";
            if ((d.flags & ACTION_PARAM_HEX) && (!hasData || !actionsParseHexTriplets(txt, buf, sizeof(buf), len))) status = 400;
            else if ((d.flags & ACTION_PARAM_OPT_BYTES) && hasData && !actionsParseHexBytes(txt, buf, sizeof(buf), len)) status = 400;
            else if ((d.flags & (ACTION_PARAM_TEXT | ACTION_PARAM_OPT_TEXT)) && (hasData || (d.flags & ACTION_PARAM_TEXT)) &&
                     !(hasData && actionsParseText(txt, buf, sizeof(buf), len))) status = 400;
            else if ((d.flags & ACTION_PARAM_BYTES) && (!hasData || !actionsParseHexBytes(txt, buf, sizeof(buf), len))) status = 400;
            else if (actionsValidateParams(d, buf, len) != ACT_PARAM_OK) status = 400;
        }
        if (i) s += ",";
        s += "{\"name\":" + q(strOf(t.get("name"))) + ",\"action\":" + q(name) + ",\"data\":" + (hasData ? q(data->s) : "null") +
             ",\"status\":" + num(status) + ",\"parsed\":";
        if (status == 200 && len && !(d.flags & (ACTION_PARAM_TEXT | ACTION_PARAM_OPT_TEXT))) {
            s += "[";
            for (uint8_t b = 0; b < len; b++) s += std::string(b ? "," : "") + num(buf[b]);
            s += "]";
        } else s += "null";
        s += ",\"valid\":" + std::string(status == 200 ? "true" : "false") + "}";
    }
    platformSetCaps(CAPS_DEFAULT);
    return s + "]}";
}

static std::string exportLogSample() {
    struct { uint32_t ms; uint8_t cat; const char* text; } rows[] = {
        {1234, LC_GBS, "preset applied"}, {123456789, LC_ADV, "status ok (fw 3.0.0)"}, {5, LC_API, "action menu.key"},
        {61000, LC_PROFILE, "load p03: ok"}, {7, LC_SYS, "quote \" and backslash \\"}};
    std::string s = "[";
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        LogRecord r; memset(&r, 0, sizeof(r));
        r.seq = (uint16_t)(i + 1); r.ms = rows[i].ms; r.cat = rows[i].cat; r.level = LL_INFO;
        r.len = (uint8_t)strlen(rows[i].text); memcpy(r.text, rows[i].text, r.len + 1);
        char line[200], rec[300];
        logFormatLine(r, line, sizeof(line));
        logRecordJson(r, rec, sizeof(rec));
        s += std::string(i ? "," : "") + "{\"record\":" + rec + ",\"line\":" + q(line) + "}";
    }
    return s + "]";
}

static std::string exportCatalog() {
    std::string s = "[";
    char name[40];
    for (uint8_t i = 0; i < menuCatalogCount(); i++) { menuCatalogName(i, name, sizeof(name)); s += std::string(i ? "," : "") + q(name); }
    return s + "]";
}

static std::map<std::string, std::string> exportAll() {
    std::map<std::string, std::string> f;
    for (const Board& b : kBoards) {
        f[std::string("schema.") + b.name + ".json"] = exportSchema(b.caps, false);
        f[std::string("schema.") + b.name + ".dev.json"] = exportSchema(b.caps, true);
        f[std::string("actions.") + b.name + ".json"] = exportActions(b.caps, false);
        f[std::string("actions.") + b.name + ".dev.json"] = exportActions(b.caps, true);
        f[std::string("values.") + b.name + ".json"] = exportValues(b.caps);
    }
    f["registry.json"] = exportRegistry();
    f["actions-all.json"] = exportActionsAll();
    f["routes.json"] = exportRoutes();
    f["menu.json"] = exportMenu();
    f["predicates.json"] = exportPredicates();
    f["validate.json"] = exportValidate();
    f["logsample.json"] = exportLogSample();
    f["catalog.json"] = exportCatalog();
    platformSetCaps(CAPS_DEFAULT);
    return f;
}

static bool writeAll(const std::string& dir, const std::map<std::string, std::string>& files) {
    hostMakeDir(dir);
    for (auto& kv : files) {
        FILE* fp = fopen((dir + "/" + kv.first).c_str(), "wb");
        if (!fp) return false;
        fwrite(kv.second.data(), 1, kv.second.size(), fp);
        fclose(fp);
    }
    return true;
}

// ---- tests ----
static std::map<std::string, std::string> gFiles;
static const std::map<std::string, std::string>& files() { if (gFiles.empty()) gFiles = exportAll(); return gFiles; }

void test_export_is_valid_json(void) {
    for (auto& kv : files()) { JVal v; TEST_ASSERT_TRUE_MESSAGE(parseJson(kv.second, v), kv.first.c_str()); }
}

void test_menu_ids_unique(void) {
    files();
    std::set<std::string> seen;
    for (auto& r : gAllNodes) TEST_ASSERT_TRUE_MESSAGE(seen.insert(r.id).second, r.id.c_str());
    TEST_ASSERT_TRUE(seen.count("input/video-standard~adv.format_sv") == 1);   // the S-Video/Composite tie rule
}

// The greyed-row reasons of menuEnabled, so the page shows the TV's words without copying them
void test_menu_reasons(void) {
    JVal m; TEST_ASSERT_TRUE(parseJson(files().at("menu.json"), m));
    const JVal* r = m.get("reasons");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("Turn 2X on first", r->get("2x_first")->s.c_str());
    TEST_ASSERT_EQUAL_STRING("RGB SCART input only", r->get("rgbs")->s.c_str());
    const char* keys[] = {"wy_manual", "rgbs", "ace", "needs_bob", "scanlines", "2x_first", "2x_on", "force60", "ftl_off"};
    for (const char* k : keys) TEST_ASSERT_NOT_NULL_MESSAGE(r->get(k), k);
}

void test_quick_ids_prefixed(void) {
    files();
    int quick = 0;
    for (auto& r : gAllNodes) if (r.id.rfind("quick/", 0) == 0) quick++;
    MenuNode qn; menuNodeRead(menuQuickRoot(), qn);
    TEST_ASSERT_EQUAL_INT((int)countNodes(qn), quick);
}

void test_menu_covers_every_node(void) {
    files();
    MenuNode root, quick; menuNodeRead(menuRoot(), root); menuNodeRead(menuQuickRoot(), quick);
    TEST_ASSERT_EQUAL_UINT32(countNodes(root) + countNodes(quick), (uint32_t)gAllNodes.size());
}

void test_menu_setting_keys_exist(void) {
    JVal pro, std_;
    TEST_ASSERT_TRUE(parseJson(files().at("schema.pro.dev.json"), pro));
    TEST_ASSERT_TRUE(parseJson(files().at("schema.standard.dev.json"), std_));
    std::set<std::string> keys;
    for (auto* s : {&pro, &std_}) for (auto& e : s->a) keys.insert(e.get("key")->s);
    for (auto& r : gAllNodes)
        if (r.n.type == N_SETTING) TEST_ASSERT_TRUE_MESSAGE(keys.count(settingKey(r.n.ref)), settingKey(r.n.ref).c_str());
}

static const JVal* registryEntry(const JVal& reg, const char* key) {
    for (auto& e : reg.get("settings")->a) if (e.get("key")->s == key) return &e;
    return nullptr;
}

void test_registry_lists_unavailable_settings(void) {
    JVal reg; TEST_ASSERT_TRUE(parseJson(files().at("registry.json"), reg));
    TEST_ASSERT_EQUAL_UINT32(settingsCount() + settingsStrCount(), reg.get("settings")->a.size());
    bool devKey = false, original = false;
    for (auto& e : reg.get("settings")->a) {
        if (e.get("dev")->b) devKey = true;
        if (((int)e.get("caps")->n) & CAP_ORIGINAL) original = true;
    }
    TEST_ASSERT_TRUE(devKey);
    TEST_ASSERT_TRUE(original);
}

void test_registry_marks_bridged(void) {
    JVal reg; TEST_ASSERT_TRUE(parseJson(files().at("registry.json"), reg));
    TEST_ASSERT_TRUE(registryEntry(reg, "picture.scanlines")->get("bridged")->b);
    TEST_ASSERT_TRUE(registryEntry(reg, "output.preset")->get("bridged")->b);
    TEST_ASSERT_FALSE(registryEntry(reg, "profiles.autoload")->get("bridged")->b);
    TEST_ASSERT_FALSE(registryEntry(reg, "adv.brightness")->get("bridged")->b);
    // toggles fire only on a change (an unchanged value is echoed); value maps fire for every mapped value
    TEST_ASSERT_EQUAL_STRING("toggle", registryEntry(reg, "picture.scanlines")->get("bridge")->s.c_str());
    const JVal* preset = registryEntry(reg, "output.preset")->get("bridge");
    TEST_ASSERT_EQUAL_INT(JVal::ARR, preset->kind);
    TEST_ASSERT_EQUAL_UINT32(5, preset->a.size());
    TEST_ASSERT_EQUAL_INT(JVal::NUL, registryEntry(reg, "adv.brightness")->get("bridge")->kind);
}

void test_predicate_scenarios_cover_inputs(void) {
    JVal p; TEST_ASSERT_TRUE(parseJson(files().at("predicates.json"), p));
    std::set<std::string> names;
    for (auto& s : p.get("scenarios")->a) names.insert(s.get("name")->s);
    char nm[24];
    for (int in = 1; in <= 6; in++) for (const char* m : {"easy", "advanced"}) {
        snprintf(nm, sizeof(nm), "pro-in%d-%s", in, m);
        TEST_ASSERT_TRUE_MESSAGE(names.count(nm), nm);
    }
}

void test_validate_vectors_run(void) {
    JVal v, vec, avec;
    TEST_ASSERT_TRUE(parseJson(files().at("validate.json"), v));
    TEST_ASSERT_TRUE(parseJson(readFile(WEB_DIR "/sim/vectors/settings-requests.json"), vec));
    TEST_ASSERT_TRUE(parseJson(readFile(WEB_DIR "/sim/vectors/action-requests.json"), avec));
    TEST_ASSERT_EQUAL_UINT32(vec.a.size(), v.get("settings")->a.size());
    TEST_ASSERT_EQUAL_UINT32(avec.a.size(), v.get("actions")->a.size());
    for (size_t i = 0; i < vec.a.size(); i++) TEST_ASSERT_EQUAL_STRING(vec.a[i].get("name")->s.c_str(), v.get("settings")->a[i].get("name")->s.c_str());
    TEST_ASSERT_TRUE(vec.a.size() >= 15);
}

// Every action with its flags whatever the board (the simulator validates like actionsPost)
void test_actions_all_has_flags(void) {
    JVal a; TEST_ASSERT_TRUE(parseJson(files().at("actions-all.json"), a));
    TEST_ASSERT_EQUAL_UINT32(actionsCount(), a.a.size());
    bool optBytes = false, dev = false;
    for (auto& e : a.a) {
        for (auto& f : e.get("flags")->a) { if (f.s == "opt_bytes") optBytes = true; if (f.s == "dev") dev = true; }
        if (e.get("name")->s == "input.termination") TEST_ASSERT_EQUAL_STRING("bytes", e.get("params")->a[0].get("type")->s.c_str());
    }
    TEST_ASSERT_TRUE(optBytes && dev);
}

void test_export_writes_files_when_asked(void) {
    std::string dir = hostTempDir("webexport");
    TEST_ASSERT_FALSE(dir.empty());
    TEST_ASSERT_TRUE(writeAll(dir, files()));
    TEST_ASSERT_EQUAL_UINT32(18, files().size());
    for (auto& kv : files()) {
        struct stat st;
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, stat((dir + "/" + kv.first).c_str(), &st), kv.first.c_str());
    }
    hostRemoveTree(dir);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_export_is_valid_json);
    RUN_TEST(test_menu_ids_unique);
    RUN_TEST(test_menu_reasons);
    RUN_TEST(test_quick_ids_prefixed);
    RUN_TEST(test_menu_covers_every_node);
    RUN_TEST(test_menu_setting_keys_exist);
    RUN_TEST(test_registry_lists_unavailable_settings);
    RUN_TEST(test_registry_marks_bridged);
    RUN_TEST(test_predicate_scenarios_cover_inputs);
    RUN_TEST(test_validate_vectors_run);
    RUN_TEST(test_actions_all_has_flags);
    RUN_TEST(test_export_writes_files_when_asked);
    int r = UNITY_END();
    const char* out = getenv("WEB_EXPORT_DIR");
    if (out && *out && r == 0) {
        if (!writeAll(out, files())) { printf("web export: cannot write %s\n", out); return 1; }
        printf("web export: %u files in %s\n", (unsigned)files().size(), out);
    }
    return r;
}
