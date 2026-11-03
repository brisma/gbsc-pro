#include <stdio.h>
#include <string.h>
#include "../settings/settings-registry.h"
#include "../profiles/profile-list.h"
#include "menu-engine.h"
#include "../util/str-copy.h"
#include "../input/input-types.h"
#include "tv-osd.h"
#include "menu-catalog.h"
#include "../profiles/profile-names.h"
#include "menu-model.h"
#include "menu-signal.h"
#include "menu-strings.h"
#include "menu-tree.h"
#include "../api/api-actions.h"
#include "../profiles/profile-store.h"

const uint16_t MenuEngine::S_T_BUSY_ID = S_T_BUSY;

static uint8_t rowKind(const MenuNode& n, int slider) {
    switch (n.type) {
        case N_SUB: return ROWK_SUB;
        case N_HEADER: return ROWK_HEADER;
        case N_SETTING: {
            if (slider >= 0) return ROWK_SLIDER;
            if (n.opts && n.opts < 100) return ROWK_CYCLE;
            SettingDef d; settingsDefAt(n.ref, d);
            return d.type == SettingType::Bool ? ROWK_VALUE : ROWK_CYCLE;
        }
        case N_CHOICE: return ROWK_CYCLE;
        case N_TOGGLE_FN: case N_STEPPER: return ROWK_VALUE;
        case N_LIVE: return ROWK_LIVE;
        default: return ROWK_ACTION;
    }
}


MenuEngine::MenuEngine(MenuBackend& be) : be_(be) {
    memset(&view_, 0, sizeof(view_));
    memset(stack_, 0, sizeof(stack_));
}

void MenuEngine::setWindow(uint8_t rows) {
    window_ = rows < 1 ? 1 : rows > MENU_VIEW_ROWS ? MENU_VIEW_ROWS : rows;
    refreshSorted();
    rebuild();
}

bool MenuEngine::busy() const { return view_.mode != VIEW_CLOSED; }

// ---- Rows of a stack level ----

// Only while a profile list is shown: the insertion sort of up to 36 names must not run on every loop pass
void MenuEngine::refreshSorted() {
    bool list = false;
    for (uint8_t d = 0; d < depth_; d++) list = list || stack_[d].kind == STK_LOAD || stack_[d].kind == STK_MANAGE;
    if (list) sortN_ = be_.profiles().sorted(sortIdx_, PROFILE_MAX);
    matchN_ = be_.profileMatches(match_, PROFILE_MAX);
}

bool MenuEngine::isMatch(int8_t p) const {
    for (uint8_t i = 0; i < matchN_; i++) if ((int8_t)match_[i] == p) return true;
    return false;
}

void MenuEngine::profileRowLabel(int8_t p, char* out, size_t n) { menuProfileEntryLabel(be_.profiles().at((uint8_t)p), out, n); }

uint8_t MenuEngine::rowCount(const Entry& e) {
    uint8_t back = hasBack(e) ? 1 : 0;
    switch (e.kind) {
        case STK_NODE: { MenuNode n; menuNodeRead(e.node, n); return (uint8_t)(n.count + back + (hasAllSettings(e) ? 1 : 0)); }
        case STK_LOAD: return (uint8_t)(sortN_ + 1 + back);                          // Defaults first
        case STK_MANAGE: return (uint8_t)((sortN_ ? sortN_ : 1) + back);
        case STK_PROFILE: return (uint8_t)(4 + back);
        case STK_NAMES: return (uint8_t)(3 + menuCatalogCount() + back);   // Signal name, Edit name, header
        default: return 0;
    }
}

bool MenuEngine::rowAt(const Entry& e, uint8_t i, Row& out) {
    out.dyn = DYN_NONE;
    out.profile = -1;
    if (hasBack(e)) {
        if (i == 0) {
            memset(&out.node, 0, sizeof(out.node));
            out.node.type = N_ACTION;
            out.node.flags = NF_FN;
            out.node.label = S_BACK;
            out.dyn = DYN_BACK;
            return true;
        }
        i--;
    }
    if (e.kind == STK_NODE) {
        MenuNode n; menuNodeRead(e.node, n);
        if (i == n.count && hasAllSettings(e)) {        // last row of the quick menu
            memset(&out.node, 0, sizeof(out.node));
            out.node.type = N_SUB;
            out.node.flags = NF_FN;
            out.node.label = S_ALL_SETTINGS;
            out.node.help = H_ALL_SETTINGS;
            out.dyn = DYN_ALL_SETTINGS;
            return true;
        }
        if (i >= n.count) return false;
        menuNodeRead(&n.children[i], out.node);
        return true;
    }
    memset(&out.node, 0, sizeof(out.node));
    out.node.type = N_ACTION;
    out.node.flags = NF_FN;
    if (e.kind == STK_LOAD) {
        if (i == 0) { out.dyn = DYN_LOAD_DEFAULTS; out.node.label = S_DEFAULTS; return true; }
        if (--i >= sortN_) return false;
        out.profile = (int8_t)sortIdx_[i];
        out.dyn = DYN_LOAD_ITEM;
        return true;
    }
    if (e.kind == STK_MANAGE) {
        if (!sortN_) { if (i) return false; out.dyn = DYN_EMPTY; out.node.label = S_NO_PROFILES; return true; }
        if (i >= sortN_) return false;
        out.profile = (int8_t)sortIdx_[i];
        if (e.kind == STK_LOAD) out.dyn = DYN_LOAD_ITEM;
        else { out.dyn = DYN_MANAGE_ITEM; out.node.type = N_SUB; }
        return true;
    }
    if (e.kind == STK_PROFILE) {
        if (i > 3) return false;
        static const uint8_t kDyn[4] = {DYN_RENAME, DYN_REPLACE, DYN_FORGET, DYN_DELETE};
        static const uint16_t kLabel[4] = {S_RENAME, S_REPLACE, S_FORGET, S_DELETE};
        out.profile = e.profile;
        out.dyn = kDyn[i];
        out.node.label = kLabel[i];
        if (out.dyn == DYN_DELETE) out.node.flags |= NF_CONFIRM;
        return true;
    }
    if (e.kind == STK_NAMES) {
        if (i >= 3 + menuCatalogCount()) return false;
        if (i == 2) { out.node.type = N_HEADER; out.node.flags = 0; out.node.label = S_CONSOLES; return true; }
        out.dyn = i == 0 ? DYN_NAME_SOURCE : i == 1 ? DYN_NAME_EDIT : DYN_NAME_CONSOLE;
        out.node.label = i == 0 ? S_SIGNAL_NAME : i == 1 ? S_EDIT_NAME : S_NONE;
        out.node.help = H_NEW_PROFILE;
        out.profile = (int8_t)(i >= 3 ? i - 3 : -1);    // catalog index
        return true;
    }
    return false;
}

bool MenuEngine::rowEnabled(const Row& r, uint16_t* reason) {
    if (reason) *reason = S_NONE;
    if (freeRun_ && root_ != ROOT_MAIN && !noSignalRowAllowed(r)) {   // no preset reload over the free-run
        if (reason) *reason = S_T_NO_SIGNAL;
        return false;
    }
    if (r.dyn == DYN_EMPTY) return false;
    if (r.dyn == DYN_FORGET && !be_.profiles().at((uint8_t)r.profile).hasFp) {
        if (reason) *reason = S_R_NO_FP;
        return false;
    }
    if (r.dyn != DYN_NONE) return true;
    return menuEnabled(r.node, be_, reason);
}

bool MenuEngine::rowVisible(const Row& r) {
    if (r.dyn != DYN_NONE) return true;
    return menuVisible(r.node, be_);
}

bool MenuEngine::headerHasItems(const Entry& e, uint8_t i) {
    uint8_t count = rowCount(e);
    Row r;
    for (uint8_t j = i + 1; j < count; j++) {
        if (!rowAt(e, j, r)) continue;
        if (r.dyn == DYN_NONE && r.node.type == N_HEADER) return false;
        if (rowVisible(r)) return true;
    }
    return false;
}

bool MenuEngine::rowSelectable(const Row& r) {
    if (r.dyn == DYN_EMPTY) return false;
    return rowVisible(r) && r.node.type != N_HEADER;
}

void MenuEngine::rowLabel(const Row& r, char* out, size_t n) {
    if (r.dyn == DYN_NAME_CONSOLE) { menuCatalogName((uint8_t)r.profile, out, n); return; }
    if (r.dyn == DYN_LOAD_ITEM || r.dyn == DYN_MANAGE_ITEM) { profileRowLabel(r.profile, out, n); return; }
    menuStrCopy(r.node.label, out, n);
}

// Cursor = child index; when it is not selectable, the nearest selectable child at or before it, else after it
void MenuEngine::fixCursor(Entry& e) {
    uint8_t count = rowCount(e);
    if (!count) { e.cursor = 0; return; }
    if (e.cursor >= count) e.cursor = count - 1;
    Row r;
    int low = hasBack(e) ? 1 : 0;                       // the Back row is the last resort
    for (int i = e.cursor; i >= low; i--)
        if (rowAt(e, (uint8_t)i, r) && rowSelectable(r)) { e.cursor = (uint8_t)i; return; }
    for (uint8_t i = e.cursor; i < count; i++)
        if (rowAt(e, i, r) && rowSelectable(r)) { e.cursor = i; return; }
    if (low) e.cursor = 0;
}

void MenuEngine::firstRealRow(Entry& e) {
    e.cursor = hasBack(e) ? 1 : 0;
    uint8_t count = rowCount(e);
    Row r;
    for (uint8_t i = e.cursor; i < count; i++)
        if (rowAt(e, i, r) && rowSelectable(r)) { e.cursor = i; return; }
    e.cursor = 0;
    fixCursor(e);
}

// A level whose SUB (or profile) is no longer visible is popped with everything below it
void MenuEngine::validateStack() {
    uint8_t depthBefore = depth_;
    uint8_t cursorBefore = depth_ ? stack_[depth_ - 1].cursor : 0;
    for (uint8_t d = 1; d < depth_; d++) {
        if (stack_[d].kind == STK_PROFILE && !be_.profiles().at((uint8_t)stack_[d].profile).used) { depth_ = d; break; }
        Entry& parent = stack_[d - 1];
        Row r;
        bool ok = rowAt(parent, parent.cursor, r) && rowVisible(r);
        if (!ok) { depth_ = d; break; }
    }
    if (depth_) fixCursor(stack_[depth_ - 1]);
    if (depth_ != depthBefore || (depth_ && stack_[depth_ - 1].cursor != cursorBefore)) cancelEdit();
}

void MenuEngine::push(const Row& r) {
    cancelEdit();
    if (depth_ >= kMaxDepth) return;
    Entry& e = stack_[depth_];
    e.kind = STK_NODE;
    e.profile = r.profile;
    e.node = nullptr;
    // the PROGMEM address of the child node is needed: find it in the current level
    Entry& cur = stack_[depth_ - 1];
    MenuNode parent; menuNodeRead(cur.node, parent);
    e.node = &parent.children[childIndex(cur)];
    e.top = 0;
    depth_++;
    firstRealRow(e);
}

void MenuEngine::pushDynamic(const Row& r) {
    cancelEdit();
    if (depth_ >= kMaxDepth) return;
    Entry& cur = stack_[depth_ - 1];
    MenuNode parent; menuNodeRead(cur.node, parent);
    Entry& e = stack_[depth_];
    e.node = &parent.children[childIndex(cur)];
    e.kind = (r.node.ref == FN_PROFILE_LOAD_LIST || r.node.ref == FN_QUICK_PROFILE) ? STK_LOAD
           : r.node.ref == FN_PROFILE_NAME_LIST ? STK_NAMES : STK_MANAGE;
    e.profile = -1;
    if (e.kind == STK_NAMES) {                          // region per catalog row, reset on every open
        memset(regions_, defaultRegion(), sizeof(regions_));
    }
    e.top = 0;
    depth_++;
    refreshSorted();
    firstRealRow(e);
}

void MenuEngine::pushProfile(const Row& r) {
    cancelEdit();
    if (depth_ >= kMaxDepth) return;
    Entry& e = stack_[depth_];
    e.node = stack_[depth_ - 1].node;
    e.kind = STK_PROFILE;
    e.profile = r.profile;
    e.top = 0;
    depth_++;
    firstRealRow(e);
}

void MenuEngine::openAtProfiles(uint32_t now) {
    cancelEdit();
    if (!depth_ || root_ != ROOT_MAIN) openMenu(now, ROOT_MAIN);   // SAVE from the Quick menu: the full menu
    depth_ = 1;
    Entry& root = stack_[0];
    Row r;
    for (uint8_t i = 0; i < rowCount(root); i++)
        if (rowAt(root, i, r) && r.node.label == S_PROFILES) { root.cursor = i; push(r); break; }
    if (depth_ < 2) return;
    Entry& e = stack_[depth_ - 1];                      // SAVE: Save changes with a profile, else New profile
    uint16_t want = be_.profileActive() >= 0 ? S_SAVE_CHANGES : S_NEW_PROFILE;
    for (uint8_t i = 0; i < rowCount(e); i++)
        if (rowAt(e, i, r) && r.node.label == want && rowSelectable(r)) { e.cursor = i; break; }
}

void MenuEngine::openNewProfile() {
    Entry& e = stack_[depth_ - 1];
    Row r;
    for (uint8_t i = 0; i < rowCount(e); i++)
        if (rowAt(e, i, r) && r.dyn == DYN_NONE && r.node.type == N_SUB && (r.node.flags & NF_FN) &&
            r.node.ref == FN_PROFILE_NAME_LIST) { e.cursor = i; pushDynamic(r); return; }
}

uint16_t MenuEngine::confirmQuestion(const MenuNode& n) {
    if (n.type != N_ACTION) return 0;
    if (n.flags & NF_FN) {
        switch (n.ref) {
            case FN_DECODER_RESET: return S_Q_RESET_DECODER;
            case FN_PICTURE_RESET: return S_Q_RESET_PICTURE;
            case FN_GEOMETRY_RESET: return S_Q_RESET_GEOMETRY;
            case FN_WIFI_RESET: return S_Q_RESET_AP;
            case FN_INSTALL_MANUAL: return S_Q_INSTALL_MANUAL;
            default: return 0;
        }
    }
    switch (n.ref) {
        case AID_sys_restart: return S_Q_RESTART;
        case AID_sys_factory_reset: return S_Q_FACTORY_RESET;
        case AID_sys_install_update: return S_Q_INSTALL;
        case AID_sys_update_adv_controller: return S_Q_UPDATE_ADV_CONTROLLER;
        case AID_adv_ace_defaults: return S_Q_RESET_ADAPTIVE;
        case AID_adv_filt_defaults: return S_Q_RESET_FILTERS;
        case AID_pic_reset_colors: return S_Q_RESET_COLORS;
        case AID_dev_reset_chip: return S_Q_RESET_CHIP;
        default: return 0;
    }
}

void MenuEngine::openConfirm(const Row& r, const char* text) {
    confirmNode_ = r.node;
    confirmDyn_ = r.dyn;
    confirmProfile_ = r.profile;
    confirmChoice_ = 0;
    prompt_ = false;
    copyStr(confirmText_, sizeof(confirmText_), text);
    openOverlay(OV_CONFIRM);
}

void MenuEngine::openMenu(uint32_t now, uint8_t root) {
    back_ = false;                                      // K_CLICK sets it again after opening
    setRoot(root);
    lastKey_ = now;
}

void MenuEngine::setRoot(uint8_t root) {
    cancelEdit();
    root_ = root;
    depth_ = 1;
    stack_[0].node = root == ROOT_QUICK ? menuQuickRoot() : menuRoot();
    stack_[0].kind = STK_NODE;
    stack_[0].profile = -1;
    stack_[0].cursor = 0;
    stack_[0].top = 0;
    fixCursor(stack_[0]);
}

void MenuEngine::freeRunEnter() {
    if (freeRun_ || !be_.signal().noSignal || !(be_.caps() & CAP_OSD)) return;
    be_.noSignalEnter();
    freeRun_ = true;
    sourceChanged_ = false;
}

// No signal: the Quick menu opens on its first row that works (Source), not on the red Profile row
void MenuEngine::freeRunQuickCursor() {
    if (!freeRun_ || root_ != ROOT_QUICK || depth_ != 1) return;
    Entry& e = stack_[0];
    Row r;
    for (uint8_t i = 0, n = rowCount(e); i < n; i++) {
        if (rowAt(e, i, r) && r.dyn == DYN_NONE && noSignalRowAllowed(r)) {
            e.cursor = i;
            fixCursor(e);
            return;
        }
    }
}

bool MenuEngine::noSignalRowAllowed(const Row& r) const {
    if (r.dyn != DYN_NONE) return true;
    return r.node.type == N_SETTING && (r.node.ref == SID_input_source || r.node.ref == SID_sys_menu_mode);
}

bool MenuEngine::quickProfileRow(const Row& r) const {
    return r.dyn == DYN_NONE && r.node.type == N_ACTION && (r.node.flags & NF_FN) && r.node.ref == FN_QUICK_PROFILE;
}

void MenuEngine::closeMenu() {
    if (rv_.open) revertUndo();
    if (ov_ == OV_ADJUST) adjustUndo();
    if (ov_ == OV_CONFIRM || ov_ == OV_REVERT || ov_ == OV_ADJUST || ov_ == OV_KEYBOARD || ov_ == OV_SLIDER ||
        ov_ == OV_SAME_SIGNAL || ov_ == OV_TVOSD) closeOverlay();
    cancelEdit();
    depth_ = 0;
    back_ = false;
}

// ---- Overlays ----

uint8_t MenuEngine::prio(uint8_t ov) {
    switch (ov) {
        case OV_UPDATE: return 8;
        case OV_CONFIRM: case OV_SAME_SIGNAL: return 7;
        case OV_KEYBOARD: return 6;
        case OV_REVERT: return 5;
        case OV_ADJUST: case OV_SLIDER: case OV_TVOSD: return 4;
        case OV_INFO: return 3;
        case OV_VOLUME: case OV_MUTE: return 2;
        case OV_TOAST: case OV_STATUS: return 1;
        default: return 0;
    }
}

void MenuEngine::leaveOverlay(uint8_t old) {
    if (old == OV_STATUS) discarded_[0] = '\0';
    if (old == OV_TVOSD) be_.tvOsd(TVOSD_CLOSE);           // every exit turns the TV5725 icons off

}

bool MenuEngine::openOverlay(uint8_t ov) {
    if (prio(ov) < prio(ov_)) return false;
    if (ov_ == OV_REVERT && ov != OV_REVERT) revertUndo();   // replaced: the countdown ends as an undo
    if (ov_ == OV_ADJUST && ov != OV_ADJUST) adjustUndo();   // replaced (upload): no half-kept geometry
    if (ov_ != ov) leaveOverlay(ov_);
    ov_ = ov;
    ovAt_ = now_;
    return true;
}

void MenuEngine::closeOverlay() {
    leaveOverlay(ov_);
    ov_ = OV_NONE;
    if (toastQn_) {
        copyStr(toast_, sizeof(toast_), toastQ_[0]);
        copyStr(toastQ_[0], sizeof(toastQ_[0]), toastQ_[1]);
        toastQn_--;
        ov_ = OV_TOAST;
        ovAt_ = now_;
    }
}

void MenuEngine::toastText(const char* text) {
    if (toastHook_) toastHook_(text);
    bool fresh = ov_ == OV_TOAST && now_ - ovAt_ < kToastKeepMs;
    if (prio(ov_) <= prio(OV_TOAST) && !fresh && ov_ != OV_STATUS) {   // the pop-up is not replaced by a toast
        copyStr(toast_, sizeof(toast_), text);
        ov_ = OV_TOAST;
        ovAt_ = now_;
        return;
    }
    if (toastQn_ == 2) { copyStr(toastQ_[0], sizeof(toastQ_[0]), toastQ_[1]); toastQn_ = 1; }   // drop the oldest
    copyStr(toastQ_[toastQn_++], sizeof(toastQ_[0]), text);
}

void MenuEngine::toast(uint16_t str) {
    char t[29];
    menuStrCopy(str, t, sizeof(t));
    toastText(t);
}

// ---- Revert countdown ----

void MenuEngine::revertUndo() {
    if (!rv_.open) return;
    rv_.open = false;
    cancelEdit();                                        // a knob edit ends with its undo (else the next turn re-edits)
    if (ov_ == OV_REVERT) ov_ = OV_NONE;
    rv_.settling = true;
    rv_.settleAt = now_;
    rv_.undoPending = !revertWriteOld();
}

bool MenuEngine::revertWriteOld() {
    if (rv_.node.type == N_CHOICE) return applyChanged(rv_.oldW, rv_.oldN);
    return be_.apply(rv_.node.ref, rv_.old);
}

bool MenuEngine::revertSettled() {
    if (rv_.node.type != N_CHOICE) return be_.settled(rv_.node.ref);
    for (uint8_t i = 0; i < rv_.oldN; i++) if (!be_.settled(rv_.oldW[i].sid)) return false;
    return true;
}

void MenuEngine::revertKeep() {
    rv_.open = false;
    rv_.settling = false;
    rv_.undoPending = false;
    be_.saverHold(false);
    closeOverlay();
}

void MenuEngine::revertTick() {
    if (rv_.open && now_ - rv_.at >= kRevertMs) { revertUndo(); closeOverlay(); }
    if (rv_.undoPending) rv_.undoPending = !revertWriteOld();
    // released once the undo write has landed (a later change by the web or a profile load must not keep the hold),
    // or after kSettleGiveUpMs so the saver can never stay held
    if (rv_.settling && ((!rv_.undoPending && revertSettled()) || now_ - rv_.settleAt >= kSettleGiveUpMs)) {
        rv_.settling = false;
        be_.saverHold(false);
    }
}

// ---- Edits ----

bool MenuEngine::changeSetting(const MenuNode& n, uint16_t v) {
    if (n.flags & NF_REVERT) {
        if (rv_.settling) { toast(S_T_WAIT); return false; }
        if (!rv_.open) {
            uint16_t old = be_.value(n.ref);
            if (!be_.apply(n.ref, v)) { busyToast(); return false; }
            be_.saverHold(true);
            rv_.open = true;
            rv_.node = n;
            rv_.old = old;
            openOverlay(OV_REVERT);
        } else if (!be_.apply(n.ref, v)) {
            busyToast();
            return false;
        }
        rv_.at = now_;
        return true;
    }
    if (!be_.apply(n.ref, v)) { busyToast(); return false; }
    if (n.flags & NF_RESTART_NOTE) toast(S_T_RESTART);
    return true;
}

// A choice's option: one or two writes; with NF_REVERT the countdown keeps the old value of every written setting
// Only the writes that change a value: rewriting an unchanged setting would run its apply group again
// (Filter and Scanlines pair a light setting with an APPLY_PRESET one that a reload would blank the screen for)
bool MenuEngine::applyChanged(const MenuWrite* w, uint8_t n) {
    MenuWrite ch[2];
    uint8_t m = 0;
    for (uint8_t i = 0; i < n && m < 2; i++)
        if (be_.value(w[i].sid) != w[i].v) ch[m++] = w[i];
    return !m || be_.applyMany(ch, m);
}

void MenuEngine::changeChoice(const MenuNode& n, uint8_t opt) {
    MenuWrite w[2];
    uint8_t cnt = menuChoiceWrites(n.ref, opt, w);
    if (n.flags & NF_REVERT) {
        if (rv_.settling) { toast(S_T_WAIT); return; }
        MenuWrite old[2];
        uint8_t oldN = rv_.open ? rv_.oldN : 0;
        if (rv_.open) memcpy(old, rv_.oldW, sizeof(old));
        for (uint8_t i = 0; i < cnt; i++) {   // a later step may write a setting the first one did not: keep its old value
            bool known = false;
            for (uint8_t j = 0; j < oldN; j++) known = known || old[j].sid == w[i].sid;
            if (!known && oldN < 2) old[oldN++] = {w[i].sid, be_.value(w[i].sid)};
        }
        if (!applyChanged(w, cnt)) { busyToast(); return; }
        memcpy(rv_.oldW, old, sizeof(old));
        rv_.oldN = oldN;
        if (!rv_.open) {
            be_.saverHold(true);
            rv_.open = true;
            rv_.node = n;
            openOverlay(OV_REVERT);
        }
        rv_.at = now_;
        return;
    }
    if (!applyChanged(w, cnt)) busyToast();
}

static bool isNumeric(const MenuNode& n) {
    if (n.type != N_SETTING || (n.opts && n.opts < 100)) return false;
    SettingDef d; settingsDefAt(n.ref, d);
    return d.type == SettingType::U8 || d.type == SettingType::U16;
}

void MenuEngine::stepRow(const Row& r, int dir, bool repeat) {
    const MenuNode& n = r.node;
    if (!rowEnabled(r, nullptr)) return;
    switch (n.type) {
        case N_SETTING: {
            if (repeat && !isNumeric(n)) return;
            uint16_t v;
            if (n.flags & NF_COMMIT) {
                if (!menuStepFrom(n, be_, pendingValid_ ? pending_ : be_.value(n.ref), dir, v)) return;
                pending_ = v;
                pendingSid_ = n.ref;
                pendingValid_ = true;
                editing_ = true;
                return;
            }
            if (menuStepValue(n, be_, dir, v)) changeSetting(n, v);
            return;
        }
        case N_CHOICE: {
            if (repeat) return;
            bool commit = n.flags & NF_COMMIT;
            int cur = (commit && pendingValid_ && pendingChoice_ && pendingSid_ == n.ref) ? pending_ : menuChoiceCurrent(n.ref, be_);
            uint8_t next;
            if (!menuChoiceStep(n.ref, be_, cur, dir, next)) return;
            if (commit) {                                // Left/Right edit a pending option, OK applies it
                pending_ = next;
                pendingSid_ = n.ref;
                pendingChoice_ = true;
                pendingValid_ = true;
                editing_ = true;
                return;
            }
            changeChoice(n, next);
            return;
        }
        case N_STEPPER:
            if (!be_.run(dir < 0 ? n.ref : n.opts)) busyToast();
            return;
        case N_TOGGLE_FN:
            if (!repeat) be_.toggleSet(n.ref, !be_.toggleGet(n.ref));
            return;
        case N_ACTION:
            if (!repeat && n.ref2 != LV_NONE) activate(r, K_OK);   // toggle-like actions with a readback
            return;
        default:
            return;
    }
}

void MenuEngine::commitPending(const Row& r) {
    uint16_t v = pending_;
    bool choice = pendingChoice_;
    bool same = r.dyn == DYN_NONE && (r.node.flags & NF_COMMIT) && r.node.ref == pendingSid_ &&
                r.node.type == (choice ? N_CHOICE : N_SETTING);
    cancelEdit();
    if (!same) return;                    // never apply a pending value to another row
    if (choice) { changeChoice(r.node, (uint8_t)v); return; }
    if (!changeSetting(r.node, v)) return;            // refused: the page and the free-run stay
    if (freeRun_ && r.node.ref == SID_input_source) {   // leave so detection resumes
        sourceChanged_ = true;
        closeMenu();
    }
}

uint32_t MenuEngine::menuTimeoutMs() {
    // osd.timeout options; the longest is 10 min, not never: a menu left open would freeze the OLED (no
    // screensaver) and hold the profile auto-load
    static const uint16_t kSeconds[6] = {5, 10, 15, 30, 60, 600};
    uint16_t v = be_.value(SID_osd_timeout);
    return v < 6 ? kSeconds[v] * 1000UL : kMenuTimeoutMs;
}

void MenuEngine::profileOp(uint8_t kind, int8_t index, const char* name, uint8_t region) {
    uint16_t expect = (uint16_t)(be_.profileResultSeq() + be_.profileOpsPending() + 1);   // ops run in order
    if (!be_.profileOp(kind, index, name, region)) { busyToast(); return; }
    userWait_.on = true;
    userWait_.seq = expect;
    userWait_.at = now_;
}

void MenuEngine::cycleRegion(const Row& r, int dir) {
    uint8_t& g = regions_[(uint8_t)r.profile];
    g = (uint8_t)((g + PROFILE_REGION_COUNT + dir) % PROFILE_REGION_COUNT);
}

void MenuEngine::saveConsole(const Row& r) {
    char name[PROFILE_NAME_BUF];                                      // the region is its own field: not added to the name
    menuCatalogName((uint8_t)r.profile, name, sizeof(name));
    uint8_t region = regions_[(uint8_t)r.profile];
    cancelEdit();
    saveNew(POP_SAVEAS, name, region);
}

uint8_t MenuEngine::defaultRegion() { return profileDefaultRegion(be_.signal().palSource, be_.signal().advStandard); }

void MenuEngine::saveNew(uint8_t kind, const char* name, uint8_t region) {
    saveNew_.kind = kind;
    copyStr(saveNew_.name, sizeof(saveNew_.name), name ? name : "");
    saveNew_.region = region;
    saveNew_.target = matchN_ ? (int8_t)match_[0] : -1;
    if (saveNew_.target >= 0 && openOverlay(OV_SAME_SIGNAL)) { sameChoice_ = 0; return; }
    sameSignalPick(0);
}

void MenuEngine::sameSignalPick(uint8_t choice) {
    const char* name = saveNew_.name[0] ? saveNew_.name : nullptr;
    if (choice == 2) return;                            // Cancel: back to the list
    if (choice == 1) {                                  // Replace it: a typed or picked name renames it too
        bool named = saveNew_.kind == POP_SAVEAS;
        profileOp(POP_REPLACE, saveNew_.target, named ? name : nullptr, named ? saveNew_.region : PROFILE_REGION_NONE);
    } else {
        profileOp(saveNew_.kind, -1, name, saveNew_.region);
    }
    popNames();
}

void MenuEngine::popNames() {
    if (depth_ > 1 && stack_[depth_ - 1].kind == STK_NAMES) { depth_--; cancelEdit(); }
}

void MenuEngine::execute(const MenuNode& n, uint8_t dyn, int8_t profile) {
    switch (dyn) {
        case DYN_LOAD_ITEM: profileOp(POP_LOAD, profile); return;
        case DYN_LOAD_DEFAULTS: profileOp(POP_DEFAULTS, -1); return;
        case DYN_REPLACE: profileOp(POP_REPLACE, profile); return;
        case DYN_DELETE: profileOp(POP_DELETE, profile); return;
        case DYN_FORGET: profileOp(POP_CLEAR_FP, profile); return;
        case DYN_RENAME: openKeyboard(KB_RENAME, be_.profiles().at((uint8_t)profile).name, profile); return;
        case DYN_NAME_EDIT: {
            char t[PROFILE_NAME_BUF]; be_.text(LT_SOURCE_NAME, t, sizeof(t));
            openKeyboard(KB_PROFILE_NEW, t, -1);
            return;
        }
        case DYN_NAME_SOURCE: saveNew(POP_SAVEAS_GENERATED, nullptr, PROFILE_REGION_NONE); return;
        default: break;
    }
    if (n.flags & NF_FN) {
        switch (n.ref) {
            case FN_PROFILE_SAVE_ACTIVE: profileOp(POP_SAVE, (int8_t)be_.profileActive()); return;
            case FN_PROFILE_DISCARD: profileOp(POP_LOAD, (int8_t)be_.profileActive()); return;
            case FN_OPEN_INFO: openInfo(now_); return;
            case FN_TV_OSD: if (openOverlay(OV_TVOSD)) be_.tvOsd(TVOSD_OPEN); return;
            case FN_CHECK_UPDATE: toast(S_T_CHECKING); checkUpdate_ = true; return;
            case FN_EDIT_OTA: openKeyboard(KB_OTA_PASSWORD, "", -1); return;   // secret: never prefilled
            case FN_EDIT_HOSTS: {
                char t[64]; be_.text(LT_EXTRA_HOSTS, t, sizeof(t));
                openKeyboard(KB_EXTRA_HOSTS, t, -1);
                return;
            }
            case FN_DECODER_RESET: case FN_GEOMETRY_RESET: case FN_WIFI_RESET: case FN_INSTALL_MANUAL:
                if (!be_.fn(n.ref)) busyToast();
                return;
            case FN_CHECK_CHANNEL_STABLE: case FN_CHECK_CHANNEL_BETA:
                toast(S_T_CHECKING);                     // the result arrives as a message()
                if (!be_.fn(n.ref)) busyToast();
                return;
            case FN_PICTURE_RESET: {   // 12 writes in one all-or-nothing batch: never a half-reset page
                MenuWrite w[12];
                if (!be_.applyMany(w, menuPictureResetWrites(w))) busyToast();
                return;
            }
            default: return;
        }
    }
    if (!be_.run(n.ref)) busyToast();
}

void MenuEngine::activate(const Row& r, uint8_t k) {
    const MenuNode& n = r.node;
    if (r.dyn == DYN_BACK) { depth_--; cancelEdit(); return; }
    if (r.dyn == DYN_ALL_SETTINGS) { setRoot(ROOT_MAIN); return; }   // keeps the encoder session
    if (r.dyn == DYN_NAME_CONSOLE) {                    // IR OK saves; the encoder edits the region first
        if (k == K_CLICK && !editing_) editing_ = true;
        else saveConsole(r);
        return;
    }
    if (n.type == N_SUB) {
        if (r.dyn == DYN_MANAGE_ITEM) pushProfile(r);
        else if (n.flags & NF_FN) pushDynamic(r);
        else push(r);
        return;
    }
    if (!rowEnabled(r, nullptr)) return;
    if (quickProfileRow(r)) { pushDynamic(r); return; }   // the Quick menu's Profile row opens Load
    char t[29];
    if (r.dyn == DYN_LOAD_ITEM || r.dyn == DYN_LOAD_DEFAULTS) {   // never load silently over changes
        if (be_.profileActive() >= 0 && be_.profileModified()) { menuStrCopy(S_T_LOSE, t, sizeof(t)); openConfirm(r, t); }
        else execute(n, r.dyn, r.profile);
        return;
    }
    if (r.dyn == DYN_REPLACE) {                         // moving the fingerprint is said out loud
        menuStrCopy(isMatch(r.profile) ? S_T_REPLACE_Q : S_T_NEW_SIGNAL, t, sizeof(t));
        openConfirm(r, t);
        return;
    }
    if (r.dyn == DYN_NONE && n.type == N_ACTION && (n.flags & NF_FN) && n.ref == FN_PROFILE_SAVE_ACTIVE) {
        int a = be_.profileActive();
        if (a < 0) { openNewProfile(); return; }        // nothing to save into: create one
        char s[12], name[PROFILE_NAME_BUF], base[21];
        menuStrCopy(S_T_SAVE_TO, s, sizeof(s));
        profileRowLabel((int8_t)a, name, sizeof(name));
        copyStr(base, sizeof(base), name);              // 20 characters: "Save to " + name fill the question's 28 cells
        for (size_t l = strlen(base); l && base[l - 1] == ' '; l--) base[l - 1] = '\0';
        snprintf(t, sizeof(t), "%s %s", s, base);
        openConfirm(r, t);
        return;
    }
    if (r.dyn == DYN_NONE && n.type == N_ACTION && (n.flags & NF_FN) && n.ref == FN_PROFILE_DISCARD) {
        if (be_.profileActive() < 0 || !be_.profileModified()) { toast(S_T_NO_CHANGES); return; }   // nothing to go back to
        menuStrCopy(S_T_LOSE, t, sizeof(t));
        openConfirm(r, t);
        return;
    }
    bool click = k == K_CLICK;
    switch (n.type) {
        case N_SETTING: {
            if (n.ref == SID_audio_volume) { openOverlay(OV_VOLUME); return; }   // the remote's volume pill
            if (menuSliderHalves(n, be_) >= 0 && !(n.flags & NF_COMMIT)) {   // quick slider (OK and click)
                sliderNode_ = n;
                openOverlay(OV_SLIDER);
                return;
            }
            SettingDef d; settingsDefAt(n.ref, d);
            if (d.type == SettingType::Bool && !(n.opts && n.opts < 100)) { stepRow(r, +1, false); return; }
            if (click) { editing_ = true; return; }
            if (!isNumeric(n) && !(n.flags & NF_COMMIT)) stepRow(r, +1, false);
            return;
        }
        case N_CHOICE:
            if (click) editing_ = true;
            else if (!(n.flags & NF_COMMIT)) stepRow(r, +1, false);
            return;
        case N_STEPPER:
            if (click) editing_ = true;
            return;
        case N_TOGGLE_FN:
            stepRow(r, +1, false);
            return;
        case N_ADJUST2D:
            openAdjust(n);
            return;
        case N_ACTION: {
            bool confirm = (n.flags & NF_CONFIRM) != 0;
            if (!(n.flags & NF_FN)) { ActionDef d; actionsDefAt(n.ref, d); confirm = confirm || (d.flags & ACTION_CONFIRM); }
            if (confirm) {
                // OTA installs name a channel switch or an older version (backend text, "" = the node's question)
                uint8_t lt = (n.flags & NF_FN) ? (n.ref == FN_INSTALL_MANUAL ? LT_MANUAL_CONFIRM : 0)
                                               : (n.ref == AID_sys_install_update ? LT_UPDATE_CONFIRM : 0);
                char t[29] = "";
                if (lt) { be_.text(lt, t, sizeof(t)); menuSanitize(t); }
                openConfirm(r, t);
                return;
            }
            execute(n, r.dyn, r.profile);
            return;
        }
        default:
            return;
    }
}

// ---- Keyboard ----

void MenuEngine::openKeyboard(uint8_t purpose, const char* initial, int8_t profile) {
    if (!openOverlay(OV_KEYBOARD)) return;
    kb_.open(purpose, initial);
    kbProfile_ = profile;
    copyStr(kbOrig_, sizeof(kbOrig_), initial);
    kbMsg_ = 0;
}

void MenuEngine::keyboardKey(uint8_t k) {
    if (k == K_BACK) { closeOverlay(); return; }
    if (k == K_MENU) { closeOverlay(); closeMenu(); return; }
    kb_.key(k);
    if (kb_.done()) { kb_.clearDone(); keyboardDone(); }
}

void MenuEngine::keyboardDone() {
    if (!kb_.valid()) { keyboardMessage(S_T_INVALID); return; }
    switch (kb_.purpose()) {
        case KB_OTA_PASSWORD: case KB_EXTRA_HOSTS:
            if (kb_.purpose() == KB_OTA_PASSWORD && !kb_.text()[0]) { closeOverlay(); return; }   // opens empty: keep it
            if (!be_.setText(kb_.purpose() == KB_OTA_PASSWORD ? LT_OTA_PASSWORD : LT_EXTRA_HOSTS, kb_.text())) {
                keyboardMessage(S_T_BUSY_ID);
                return;
            }
            closeOverlay();
            return;
        case KB_PROFILE_NEW: {
            char name[PROFILE_NAME_BUF]; profileNameSanitize(kb_.text(), name, sizeof(name));
            closeOverlay();
            saveNew(POP_SAVEAS, name, PROFILE_REGION_NONE);
            return;
        }
        case KB_RENAME: {
            char name[PROFILE_NAME_BUF]; profileNameSanitize(kb_.text(), name, sizeof(name));
            closeOverlay();
            const ProfileEntry& pe = be_.profiles().at((uint8_t)kbProfile_);
            if (!pe.used || strcmp(pe.name, kbOrig_)) {    // deleted or renamed from the web meanwhile
                char f[12], t[29]; menuStrCopy(S_T_FAILED, f, sizeof(f));
                snprintf(t, sizeof(t), "%s changed", f);
                toastText(t);
                return;
            }
            profileOp(POP_RENAME, kbProfile_, name, PROFILE_REGION_NONE);
            if (depth_ > 2 && stack_[depth_ - 1].kind == STK_PROFILE) { depth_ = (uint8_t)(depth_ - 2); cancelEdit(); }
            return;
        }
        default:
            closeOverlay();
            return;
    }
}

void MenuEngine::toggleMute() {
    uint16_t muted = be_.value(SID_audio_mute) ? 0 : 1;
    if (!be_.apply(SID_audio_mute, muted)) { busyToast(); return; }
    openOverlay(OV_MUTE);
}

static const int kVolumeStep = 2;   // percent per Volume key / encoder step (1.2 dB)

void MenuEngine::stepVolume(int dir) {
    if (be_.value(SID_audio_mute) && !be_.apply(SID_audio_mute, 0)) { busyToast(); return; }
    int v = (int)be_.value(SID_audio_volume) + dir * kVolumeStep;   // percent
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    if (v != (int)be_.value(SID_audio_volume) && !be_.apply(SID_audio_volume, (uint16_t)v)) { busyToast(); return; }
    openOverlay(OV_VOLUME);
}

void MenuEngine::openAdjust(const MenuNode& n) {
    if (geoPending_) { busyToast(); return; }          // the previous undo/keep is still waiting
    if (!openOverlay(OV_ADJUST)) return;
    adjustNode_ = n;
    adjustVertical_ = false;
    adjustStepped_ = false;
    adjustUndoable_ = n.ref != AJ_BORDERS && n.ref != AJ_BORDER_MOVE;   // the masks have no undo
}

// Undo/keep run only once the step actions already queued have run (they are slow legacy commands): a late
// step must not land after the restore, nor change the geometry after the keep's save
void MenuEngine::geoFinish(uint8_t what) {
    if (!be_.actionsIdle()) { geoPending_ = what; return; }
    geoPending_ = GEO_NONE;
    bool ok = be_.fn(what == GEO_UNDO ? FN_GEOMETRY_UNDO : FN_GEOMETRY_KEEP);
    if (!ok) busyToast();
}

void MenuEngine::adjustUndo() {
    if (adjustStepped_ && adjustUndoable_) geoFinish(GEO_UNDO);
    adjustStepped_ = false;
}

void MenuEngine::sliderKey(uint8_t k, bool repeat) {
    switch (k) {
        case K_LEFT: case K_RIGHT: case K_ROT_L: case K_ROT_R: {
            Row r = {};
            r.node = sliderNode_;
            r.dyn = DYN_NONE;
            r.profile = -1;
            stepRow(r, (k == K_LEFT || k == K_ROT_L) ? -1 : +1, repeat);
            return;
        }
        case K_OK: case K_CLICK: case K_BACK: case K_LONG: closeOverlay(); return;
        case K_MENU: closeOverlay(); closeMenu(); return;
        default: return;                                // other shortcuts are ignored while sliding
    }
}

void MenuEngine::adjustKey(uint8_t k, bool repeat) {
    (void)repeat;
    MenuAdjust a; menuAdjustRead(adjustNode_.ref, a);
    int aid = -1;
    switch (k) {
        case K_LEFT: aid = a.left; break;
        case K_RIGHT: aid = a.right; break;
        case K_UP: aid = a.up; break;
        case K_DOWN: aid = a.down; break;
        case K_ROT_L: aid = adjustVertical_ ? a.up : a.left; break;
        case K_ROT_R: aid = adjustVertical_ ? a.down : a.right; break;
        case K_CLICK: adjustVertical_ = !adjustVertical_; return;
        case K_OK: case K_LONG:                         // keep: saved with the profile scope
            if (adjustStepped_) geoFinish(GEO_KEEP);   // borders too: the profile keeps this mode's registers
            adjustStepped_ = false;
            closeOverlay();
            return;
        case K_BACK: adjustUndo(); closeOverlay(); return;
        case K_MENU: adjustUndo(); closeOverlay(); closeMenu(); return;
        default: return;
    }
    if (!be_.actionsIdle()) return;                    // one step at a time: no backlog behind a held key
    if (be_.run((uint8_t)aid)) adjustStepped_ = true;
    else busyToast();
}

void MenuEngine::openInfo(uint32_t now) {
    now_ = now;
    lastKey_ = now;
    if (!openOverlay(OV_INFO)) return;
    freeRunEnter();
}

bool MenuEngine::promptAdvController(uint32_t now) {
    now_ = now;
    if (depth_ || (ov_ != OV_NONE && ov_ != OV_STATUS && ov_ != OV_TOAST)) return false;
    MenuNode n = {};
    n.type = N_ACTION;
    n.label = S_UPDATE_ADV_CONTROLLER;
    n.help = H_UPDATE_ADV_CONTROLLER;
    n.ref = AID_sys_update_adv_controller;
    n.pred = P_ADV_CONTROLLER_UPDATE;
    n.opts = LT_ADV_CONTROLLER_NEW;
    confirmNode_ = n;
    confirmDyn_ = 0;
    confirmProfile_ = -1;
    confirmChoice_ = 0;
    confirmText_[0] = '\0';                             // the node's question: "Update the ADV Controller?"
    if (!openOverlay(OV_CONFIRM)) return false;
    prompt_ = true;
    promptMoved_ = false;
    lastKey_ = now;
    rebuild();
    return true;
}

void MenuEngine::overlayKey(uint8_t k, bool repeat) {
    switch (ov_) {
        case OV_CONFIRM:
            switch (k) {
                case K_LEFT: case K_ROT_L: confirmChoice_ = 1; promptMoved_ = true; ovAt_ = now_; break;   // the boot
                case K_RIGHT: case K_ROT_R: confirmChoice_ = 0; promptMoved_ = true; ovAt_ = now_; break;  // prompt's 16 s restart
                case K_OK: case K_CLICK: {
                    bool yes = confirmChoice_ == 1;
                    bool skip = prompt_ && !yes && promptMoved_;
                    prompt_ = false;
                    closeOverlay();
                    if (yes) execute(confirmNode_, confirmDyn_, confirmProfile_);
                    else if (skip) be_.fn(FN_ADV_CONTROLLER_SKIP);
                    break;
                }
                case K_BACK: case K_LONG: prompt_ = false; closeOverlay(); break;
                case K_MENU: prompt_ = false; closeOverlay(); closeMenu(); break;
                default: break;
            }
            return;
        case OV_SAME_SIGNAL:
            switch (k) {
                case K_LEFT: case K_ROT_L: sameChoice_ = (uint8_t)((sameChoice_ + 2) % 3); break;
                case K_RIGHT: case K_ROT_R: sameChoice_ = (uint8_t)((sameChoice_ + 1) % 3); break;
                case K_OK: case K_CLICK: {
                    if (repeat) return;
                    uint8_t c = sameChoice_;
                    closeOverlay();
                    sameSignalPick(c);
                    break;
                }
                case K_BACK: case K_LONG: closeOverlay(); break;
                case K_MENU: closeOverlay(); closeMenu(); break;
                default: break;
            }
            return;
        case OV_REVERT:
            switch (k) {
                case K_LEFT: case K_RIGHT: case K_ROT_L: case K_ROT_R: {
                    if (repeat) return;
                    uint16_t v;
                    int dir = (k == K_LEFT || k == K_ROT_L) ? -1 : +1;
                    if (rv_.node.type == N_CHOICE) {
                        uint8_t next;
                        if (menuChoiceStep(rv_.node.ref, be_, menuChoiceCurrent(rv_.node.ref, be_), dir, next))
                            changeChoice(rv_.node, next);
                    } else if (menuStepValue(rv_.node, be_, dir, v)) changeSetting(rv_.node, v);
                    break;
                }
                case K_OK: case K_CLICK: cancelEdit(); revertKeep(); break;
                case K_BACK: case K_LONG: revertUndo(); closeOverlay(); break;
                case K_MENU: revertUndo(); closeOverlay(); closeMenu(); break;
                default: break;
            }
            return;
        default:
            return;
    }
}


void MenuEngine::moveCursor(int dir) {
    Entry& e = stack_[depth_ - 1];
    uint8_t count = rowCount(e);
    Row r;
    for (uint8_t step = 1; step <= count; step++) {
        uint8_t i = (uint8_t)((e.cursor + count + dir * step) % count);
        if (rowAt(e, i, r) && rowSelectable(r)) { e.cursor = i; return; }
    }
}

void MenuEngine::listKey(uint8_t k, bool repeat, uint32_t now) {
    (void)now;
    Entry& e = stack_[depth_ - 1];
    Row r;
    bool have = rowAt(e, e.cursor, r) && rowSelectable(r);
    if (have && r.dyn == DYN_NAME_CONSOLE) {
        if (k == K_LEFT || k == K_RIGHT) { cycleRegion(r, k == K_LEFT ? -1 : +1); return; }
        if (editing_ && (k == K_ROT_L || k == K_ROT_R)) { cycleRegion(r, k == K_ROT_L ? -1 : +1); return; }
        if (editing_ && k == K_CLICK) { saveConsole(r); return; }
    }
    switch (k) {
        case K_UP: case K_DOWN:
            cancelEdit();
            moveCursor(k == K_UP ? -1 : +1);
            break;
        case K_ROT_L: case K_ROT_R:
            if (editing_ && have) stepRow(r, k == K_ROT_L ? -1 : +1, repeat);
            else moveCursor(k == K_ROT_L ? -1 : +1);
            break;
        case K_LEFT: case K_RIGHT:
            if (!have) break;
            if (quickProfileRow(r)) {                   // the profiles of this signal
                if (!repeat && !be_.profileOpsPending() && rowEnabled(r, nullptr)) loadMatch(k == K_LEFT ? -1 : +1);
                break;
            }
            if (k == K_RIGHT && r.node.type == N_SUB) { activate(r, K_OK); break; }
            if (r.node.type == N_ADJUST2D) break;
            stepRow(r, k == K_LEFT ? -1 : +1, repeat);
            break;
        case K_OK:
            if (repeat || !have) break;
            if (pendingValid_) commitPending(r);
            else activate(r, K_OK);
            break;
        case K_CLICK:
            if (!have) break;
            if (editing_) { if (pendingValid_) commitPending(r); else cancelEdit(); }
            else activate(r, K_CLICK);
            break;
        case K_BACK:
            if (pendingValid_ || editing_) { cancelEdit(); break; }
            if (depth_ > 1) { depth_--; cancelEdit(); }
            else closeMenu();
            break;
        case K_LONG:
            if (editing_) { cancelEdit(); break; }
            if (depth_ > 1) { depth_--; cancelEdit(); }
            else closeMenu();                           // the quick menu too: "All settings" opens the full one
            break;
        default: break;
    }
}

// TV OSD mode: turn changes the current icon, click moves to the next one, hold / BACK / MENU leaves
void MenuEngine::tvOsdKey(uint8_t k, bool repeat) {
    switch (k) {
        case K_ROT_L: case K_LEFT: be_.tvOsd(TVOSD_LEFT); break;
        case K_ROT_R: case K_RIGHT: be_.tvOsd(TVOSD_RIGHT); break;
        case K_CLICK: case K_OK: if (!repeat) be_.tvOsd(TVOSD_ENTER); break;
        case K_LONG: case K_BACK: case K_MENU: closeOverlay(); return;
        default: return;
    }
    ovAt_ = now_;
}

void MenuEngine::key(uint8_t k, bool repeat, uint32_t now) {
    now_ = now;
    refreshSorted();
    if (ov_ == OV_UPDATE) return;                      // locked while an upload runs
    lastKey_ = now;
    bool closedToast = ov_ == OV_TOAST;
    if (ov_ == OV_TOAST) closeOverlay();               // any key closes a toast and is then handled normally
    if (ov_ == OV_STATUS) {                            // Left/Right load the next match, others act
        if (k == K_LEFT || k == K_RIGHT || k == K_ROT_L || k == K_ROT_R) {
            if (!repeat && !be_.profileOpsPending()) {  // each load writes a file and reloads the preset
                loadMatch((k == K_LEFT || k == K_ROT_L) ? -1 : +1);
                if (ov_ == OV_STATUS) ovAt_ = now;
            }
            rebuild();
            return;
        }
        closeOverlay();
    }
    if (ov_ == OV_CONFIRM || ov_ == OV_REVERT || ov_ == OV_SAME_SIGNAL) { overlayKey(k, repeat); rebuild(); return; }
    if (ov_ == OV_ADJUST) { adjustKey(k, repeat); rebuild(); return; }
    if (ov_ == OV_KEYBOARD) { keyboardKey(k); rebuild(); return; }
    if (ov_ == OV_SLIDER) { sliderKey(k, repeat); rebuild(); return; }
    if (ov_ == OV_TVOSD) { tvOsdKey(k, repeat); rebuild(); return; }
    bool quick = ov_ == OV_MUTE || ov_ == OV_VOLUME;
    switch (k) {
        case K_MUTE:
            if (!repeat) { if (ov_ == OV_INFO) closeOverlay(); toggleMute(); }
            rebuild();
            return;
        case K_VOLUP: case K_VOLDOWN:
            if (ov_ == OV_INFO) closeOverlay();
            stepVolume(k == K_VOLUP ? +1 : -1);
            rebuild();
            return;
        case K_INFO:
            if (ov_ == OV_INFO) closeOverlay();
            else if (!repeat) openInfo(now);
            rebuild();
            return;
        default: break;
    }
    if (ov_ == OV_INFO) {
        bool absent = be_.signal().noSignal || be_.signal().advFreeRun;
        if ((k == K_OK || k == K_CLICK) && absent && !depth_ && !repeat) {   // the hint: OK picks the input
            closeOverlay();
            openMenu(now, ROOT_QUICK);
            back_ = k == K_CLICK && backOption_;
            freeRunEnter();                              // already on from the info panel: no second enter
            freeRunQuickCursor();
            rebuild();
            return;
        }
        if (k == K_OK || k == K_BACK || k == K_CLICK || k == K_LONG) closeOverlay();
        else if (k == K_MENU) { closeOverlay(); closeMenu(); }
        rebuild();
        return;
    }
    if (quick) {
        if (ov_ == OV_VOLUME && (k == K_LEFT || k == K_RIGHT || k == K_ROT_L || k == K_ROT_R)) {
            stepVolume((k == K_LEFT || k == K_ROT_L) ? -1 : +1);
            rebuild();
            return;
        }
        // Any other key closes the pill and acts at once. EXIT only dismisses it, and so do OK,
        // click and long press over an open menu (the Volume row's pill: OK again would reopen it)
        bool dismiss = k == K_BACK || ((k == K_OK || k == K_CLICK || k == K_LONG) && depth_);
        closeOverlay();
        if (dismiss) { rebuild(); return; }
    }
    if (!depth_ && (k == K_OK || k == K_CLICK)) {      // the Quick menu
        if (!repeat && !closedToast) { openMenu(now, ROOT_QUICK); back_ = k == K_CLICK && backOption_; freeRunEnter(); freeRunQuickCursor(); }
        rebuild();
        return;
    }
    if (k == K_MENU || k == K_SAVE) {
        if (depth_ && k == K_MENU) {
            if (root_ == ROOT_QUICK && freeRun_) { closeMenu(); openInfo(now); }   // no signal: the info panel
            else if (root_ == ROOT_QUICK) setRoot(ROOT_MAIN);
            else closeMenu();
        }
        else if (be_.signal().noSignal) openInfo(now);
        else if (k == K_SAVE) openAtProfiles(now);
        else openMenu(now, ROOT_MAIN);
        rebuild();
        return;
    }
    if (depth_) listKey(k, repeat, now);
    rebuild();
}

bool MenuEngine::waitDone(Wait& w, bool autoload) {
    char res[24], t[29];
    bool found = be_.profileResultFor(w.seq, res, sizeof(res));
    if (!found && now_ - w.at < kWaitGiveUpMs) return false;
    w.on = false;
    if (!found) copyStr(res, sizeof(res), "timeout");
    bool discarded = !res[0] && autoload && w.discarded[0];   // a failed load dropped nothing
    if (autoload) { discarded_[0] = '\0'; noticePending_ = false; loaded_[0] = '\0'; }   // a notice never shown belongs to an older load
    if (!res[0] && autoload && statusEnabled()) {           // the pop-up names the loaded profile
        if (discarded) { copyStr(discarded_, sizeof(discarded_), w.discarded); noticePending_ = true; }   // third line
        copyStr(loaded_, sizeof(loaded_), w.name);          // for a menu opened before the pop-up
        loadedAt_ = now_;
        return true;
    }
    if (!res[0]) {
        char d[12]; menuStrCopy(autoload ? S_T_LOADED : S_T_DONE, d, sizeof(d));
        if (autoload) snprintf(t, sizeof(t), "%s %s", d, w.name);
        else copyStr(t, sizeof(t), d);
    } else {
        char f[12]; menuStrCopy(S_T_FAILED, f, sizeof(f));
        menuSanitize(res);
        snprintf(t, sizeof(t), "%s %s", f, res);
    }
    toastText(t);
    if (discarded) { discardedText(w.discarded, t, sizeof(t)); toastText(t); }   // queued behind "Loaded ..."
    return true;
}

void MenuEngine::discardedText(const char* name, char* out, size_t len) {
    char u[16];
    menuStrCopy(S_T_DISCARDED, u, sizeof(u));
    snprintf(out, len, "%s %s", u, name);
}

void MenuEngine::autoloadStarted(const char* name, uint32_t now, const char* discarded) {
    now_ = now;
    autoWait_.on = true;
    autoWait_.seq = (uint16_t)(be_.profileResultSeq() + be_.profileOpsPending());   // its op is the last queued
    autoWait_.at = now;
    copyStr(autoWait_.name, sizeof(autoWait_.name), name);
    menuSanitize(autoWait_.name);
    copyStr(autoWait_.discarded, sizeof(autoWait_.discarded), discarded ? discarded : "");
    menuSanitize(autoWait_.discarded);
}

void MenuEngine::message(const char* text, uint32_t now) {
    now_ = now;
    char t[29];
    copyStr(t, sizeof(t), text);
    menuSanitize(t);
    toastText(t);
    rebuild();
}

void MenuEngine::notice(uint16_t str, uint32_t now) {
    now_ = now;
    toast(str);
    rebuild();
}

void MenuEngine::updateResult(bool ok, bool advController, uint32_t now) {
    now_ = now;
    if (ov_ == OV_UPDATE) {
        updOutcome_ = ok ? UPV_DONE : UPV_FAILED;
        updOutcomeAt_ = now;
    } else if (advController) {
        toast(ok ? S_T_ADV_CONTROLLER_DONE : S_T_ADV_CONTROLLER_FAIL);
    }
    rebuild();
}

bool MenuEngine::statusEnabled() { return (be_.caps() & CAP_OSD) && be_.value(SID_osd_status_popup) != 0; }

void MenuEngine::statusPopup(uint32_t now, bool due) {
    now_ = now;
    bool fresh = now - loadedAt_ < kLoadedNameMs;      // both notices belong to a load of the last few seconds
    bool notice = noticePending_ && fresh;
    noticePending_ = false;                             // shown now (pop-up or toast) or dropped: once
    if (!notice) discarded_[0] = '\0';
    char loaded[PROFILE_NAME_BUF];
    copyStr(loaded, sizeof(loaded), fresh ? loaded_ : "");
    loaded_[0] = '\0';                                  // the pop-up names the active profile itself
    if (!statusEnabled() || (!due && !notice)) { discarded_[0] = '\0'; return; }
    if (depth_ || (ov_ != OV_NONE && ov_ != OV_TOAST && ov_ != OV_STATUS)) {   // busy: no late pop-up, toasts
        char t[29];
        if (loaded[0]) {                                // what the pop-up would have named
            char d[12];
            menuStrCopy(S_T_LOADED, d, sizeof(d));
            snprintf(t, sizeof(t), "%s %s", d, loaded);
            toastText(t);
        }
        if (notice) {
            discardedText(discarded_, t, sizeof(t));
            discarded_[0] = '\0';
            toastText(t);
        }
        rebuild();                                      // the view shows the toast now, not at its expiry
        return;
    }
    char keep[PROFILE_NAME_BUF];
    copyStr(keep, sizeof(keep), discarded_);            // a replaced toast must not clear it
    if (ov_ != OV_STATUS) leaveOverlay(ov_);
    copyStr(discarded_, sizeof(discarded_), keep);
    ov_ = OV_STATUS;
    ovAt_ = now;
    rebuild();
}

uint8_t MenuEngine::sortedMatches(uint8_t* out) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < matchN_; i++) {
        uint8_t at = n++;
        while (at > 0 && out[at - 1] > match_[i]) { out[at] = out[at - 1]; at--; }
        out[at] = match_[i];
    }
    return n;
}

void MenuEngine::loadMatch(int dir) {
    uint8_t idx[PROFILE_MAX];
    uint8_t n = sortedMatches(idx);
    if (!n) return;
    int a = be_.profileActive(), pos = -1;
    for (uint8_t i = 0; i < n; i++) if ((int)idx[i] == a) pos = i;
    int next = pos < 0 ? (dir > 0 ? 0 : n - 1) : (pos + dir + n) % n;
    if ((int)idx[next] == a) return;
    Row r;
    memset(&r, 0, sizeof(r));
    r.node.type = N_ACTION;
    r.node.flags = NF_FN;
    r.dyn = DYN_LOAD_ITEM;
    r.profile = (int8_t)idx[next];
    if (a >= 0 && be_.profileModified()) {
        char t[29]; menuStrCopy(S_T_LOSE, t, sizeof(t));
        openConfirm(r, t);
        return;
    }
    if (!be_.profileOp(POP_LOAD, r.profile)) busyToast();   // no "Done" toast: the pop-up shows the profile
    refreshSorted();
}

void MenuEngine::tick(uint32_t now) {
    now_ = now;
    refreshSorted();
    bool changed = false;
    if (checkUpdate_) {
        checkUpdate_ = false;
        if (!be_.run(AID_sys_check_update)) busyToast();
        changed = true;
    }
    revertTick();
    uint8_t mode = be_.menuMode();
    if (mode != lastMode_) {
        if (lastMode_ == 1 && mode == 0 && depth_) toast(S_T_ADV_STAY);   // hidden rows keep applying
        lastMode_ = mode;
        changed = true;
    }
    if (geoPending_ && be_.actionsIdle()) { geoFinish(geoPending_); changed = true; }
    if (userWait_.on && waitDone(userWait_, false)) changed = true;
    if (autoWait_.on && waitDone(autoWait_, true)) changed = true;
    if (prompt_ && ov_ != OV_CONFIRM) prompt_ = false;   // replaced (e.g. by an upload)
    if (prompt_ && !depth_ && now - ovAt_ >= kPromptMs) { prompt_ = false; closeOverlay(); changed = true; }
    int8_t pct = be_.signal().updatePct;
    if (pct >= 0 && ov_ != OV_UPDATE) { openOverlay(OV_UPDATE); updOutcome_ = UPV_PROGRESS; changed = true; }
    if (pct >= 0) updAdv_ = be_.signal().updateAdvController;
    bool outcomeOver = updOutcome_ != UPV_PROGRESS && now - updOutcomeAt_ >= kUpdateResultMs;
    if (ov_ == OV_UPDATE && (outcomeOver || (pct < 0 && updOutcome_ == UPV_PROGRESS))) {
        ov_ = OV_NONE;
        updOutcome_ = UPV_PROGRESS;
        closeMenu();
        changed = true;
    }
    if (ov_ == OV_TOAST && now - ovAt_ >= kToastMs) { closeOverlay(); changed = true; }
    if (ov_ == OV_STATUS) {
        uint8_t idx[PROFILE_MAX];
        uint32_t ms = !discarded_[0] && sortedMatches(idx) >= 2 ? kStatusChoiceMs : kStatusMs;   // as the third line reads
        if (now - ovAt_ >= ms) { closeOverlay(); changed = true; }
    }
    if ((ov_ == OV_MUTE || ov_ == OV_VOLUME) && now - lastKey_ >= kQuickTimeoutMs) { closeOverlay(); changed = true; }
    uint32_t timeout = menuTimeoutMs();
    if (ov_ == OV_INFO && timeout && now - lastKey_ >= timeout) { closeOverlay(); changed = true; }
    if (ov_ == OV_SLIDER && (now - lastKey_ >= kSliderTimeoutMs || !menuVisible(sliderNode_, be_) ||
                             !menuEnabled(sliderNode_, be_, nullptr))) { closeOverlay(); changed = true; }
    // the menu timeout pauses; also during the revert countdown, which has its own 15 s (a short osd.timeout
    // would otherwise undo the change before the TV has resynced)
    if (ov_ == OV_TVOSD && (now - ovAt_ >= kTvOsdMs ||
                            !tvOsdAllowed((be_.caps() & CAP_OSD) != 0, !be_.signal().noSignal, be_.signal().presetId))) {
        closeOverlay();                                  // back to the Quick menu, with a fresh menu timeout
        lastKey_ = now;
        changed = true;
    }
    if (ov_ == OV_ADJUST || ov_ == OV_UPDATE || ov_ == OV_KEYBOARD || ov_ == OV_REVERT || ov_ == OV_TVOSD) lastKey_ = now;
    if (depth_ && timeout && now - lastKey_ >= timeout) { closeMenu(); changed = true; }
    if (changed || now - lastRefresh_ >= kRefreshMs) rebuild();
}

void MenuEngine::onSignalChange() { refreshSorted(); rebuild(); }


// ---- View ----

void MenuEngine::rebuild() {
    lastRefresh_ = now_;
    if (freeRun_ && !depth_ && ov_ == OV_NONE) {        // the session ended: exactly one exit
        freeRun_ = false;
        be_.noSignalExit(!sourceChanged_);
    }
    if (depth_) validateStack();
    MenuView v;
    memset(&v, 0, sizeof(v));
    v.rev = view_.rev;
    if (!depth_) {
        v.mode = VIEW_CLOSED;
    } else {
        v.mode = VIEW_LIST;
        Entry& e = stack_[depth_ - 1];
        MenuNode cur; menuNodeRead(e.node, cur);
        if (e.kind == STK_PROFILE) {
            profileRowLabel(e.profile, v.title, sizeof(v.title));
        } else if (depth_ == 1 && root_ != ROOT_MAIN) {
            menuStrCopy(S_QUICK, v.title, sizeof(v.title));
        } else if (e.kind == STK_LOAD) {
            menuStrCopy(S_LOAD, v.title, sizeof(v.title));
        } else if (depth_ == 1) {
            mainTitle(v.title, sizeof(v.title));
        } else {
            menuStrCopy(cur.label, v.title, sizeof(v.title));
        }
        v.deep = depth_ > 1;
        uint8_t vis[kMaxRows];
        uint8_t nvis = 0, cpos = 0;
        uint8_t count = rowCount(e);
        Row r;
        for (uint8_t i = 0; i < count && nvis < kMaxRows; i++) {
            if (!rowAt(e, i, r) || !rowVisible(r)) continue;
            if (r.dyn == DYN_NONE && r.node.type == N_HEADER && !headerHasItems(e, i)) continue;
            if (rowSelectable(r) && r.dyn != DYN_BACK) {
                v.posCount++;
                if (i == e.cursor) v.posIndex = v.posCount;
            }
            if (i == e.cursor) cpos = nvis;
            vis[nvis++] = i;
        }
        if (cpos < e.top) e.top = cpos;
        if (cpos >= e.top + window_) e.top = (uint8_t)(cpos - window_ + 1);
        if (e.top + window_ > nvis) e.top = nvis > window_ ? (uint8_t)(nvis - window_) : 0;
        v.moreAbove = e.top > 0;
        v.moreBelow = e.top + window_ < nvis;
        v.listTop = e.top;
        v.listTotal = nvis;
        v.listWindow = window_;
        for (uint8_t i = e.top; i < nvis && v.rowCount < window_; i++) {
            rowAt(e, vis[i], r);
            MenuViewRow& row = v.rows[v.rowCount];
            rowLabel(r, row.label, sizeof(row.label));
            row.slider = -1;
            if (r.node.type == N_HEADER) row.flags |= ROWF_HEADER;
            if (r.node.type == N_SUB) row.flags |= ROWF_SUB;
            if (r.dyn == DYN_BACK) row.flags |= ROWF_BACK;
            uint16_t reason = S_NONE;
            if (!rowEnabled(r, &reason)) row.flags |= ROWF_DISABLED;
            if (r.dyn == DYN_LOAD_ITEM) {                 // Active / Unsaved / Match
                bool act = r.profile == be_.profileActive();
                uint16_t tag = act ? (be_.profileModified() ? S_V_UNSAVED : S_V_ACTIVE) : isMatch(r.profile) ? S_V_MATCH : S_NONE;
                menuStrCopy(tag, row.value, sizeof(row.value));
            }
            if (r.dyn == DYN_LOAD_DEFAULTS) menuStrCopy(be_.profileActive() < 0 ? S_V_ACTIVE : S_NONE, row.value, sizeof(row.value));
            bool sel = vis[i] == e.cursor;
            row.kind = (r.dyn == DYN_MANAGE_ITEM || r.dyn == DYN_ALL_SETTINGS) ? ROWK_SUB : r.dyn == DYN_EMPTY ? ROWK_LIVE
                     : (r.dyn == DYN_LOAD_ITEM || r.dyn == DYN_LOAD_DEFAULTS) ? ROWK_PROFILE
                     : r.dyn == DYN_NAME_CONSOLE ? ROWK_CYCLE
                     : r.dyn != DYN_NONE ? ROWK_ACTION : rowKind(r.node, menuSliderHalves(r.node, be_));
            if (r.dyn == DYN_NAME_CONSOLE)
                copyStr(row.value, sizeof(row.value), profileRegionName(regions_[(uint8_t)r.profile]));
            if (r.dyn == DYN_NAME_SOURCE) { be_.text(LT_SOURCE_NAME, row.value, sizeof(row.value)); menuSanitize(row.value); }
            if (r.dyn == DYN_NONE && r.node.type == N_SUB && r.node.opts) menuFormatValue(r.node, be_, row.value, sizeof(row.value));
            if (r.dyn == DYN_NONE && r.node.type != N_SUB && r.node.type != N_HEADER) {
                if (sel && pendingValid_ && pendingChoice_) menuFormatChoice(r.node.ref, pending_, row.value, sizeof(row.value));
                else if (sel && pendingValid_) menuFormatSettingValue(r.node, pending_, row.value, sizeof(row.value));
                else menuFormatValue(r.node, be_, row.value, sizeof(row.value));
                row.slider = (int8_t)menuSliderHalves(r.node, be_);
                if (row.slider >= 0) row.flags |= ROWF_SLIDER;
                if (menuIsChanged(r.node, be_)) row.flags |= ROWF_CHANGED;
            }
            if (sel) {
                row.flags |= ROWF_SELECTED;
                if (editing_) row.flags |= ROWF_EDITING;
                v.cursorRow = v.rowCount;
                menuStrCopy(reason != S_NONE ? reason : r.node.help, v.help, sizeof(v.help));
            }
            v.rowCount++;
        }
    }
    if (ov_ != OV_NONE) buildOverlay(v);
    if (memcmp(&v, &view_, sizeof(v)) != 0) {
        v.rev = (uint16_t)(view_.rev + 1);
        view_ = v;
    }
}

void MenuEngine::buildOverlay(MenuView& v) {
    v.overMenu = depth_ > 0;
    v.mode = VIEW_OVERLAY;
    v.overlay = ov_;
    char tmp[29];
    switch (ov_) {
        case OV_TOAST:
            copyStr(v.lines[0], sizeof(v.lines[0]), toast_);
            v.lineCount = 1;
            break;
        case OV_CONFIRM: {                              // a real question, a row of its own
            uint16_t q = confirmQuestion(confirmNode_);
            if (confirmText_[0]) copyStr(v.lines[0], sizeof(v.lines[0]), confirmText_);
            else if (q) menuStrCopy(q, v.lines[0], sizeof(v.lines[0]));
            else {                                      // no question in the table: "<label>?"
                menuStrCopy(confirmNode_.label, tmp, sizeof(tmp) - 1);
                snprintf(v.lines[0], sizeof(v.lines[0]), "%s?", tmp);
            }
            v.lineCount = 1;
            v.choice = confirmChoice_;
            break;
        }
        case OV_STATUS:
            buildStatus(v);
            break;
        case OV_SAME_SIGNAL:
            menuStrCopy(S_T_SAME_SIGNAL, v.lines[0], sizeof(v.lines[0]));
            if (saveNew_.target >= 0) profileRowLabel(saveNew_.target, v.lines[1], sizeof(v.lines[1]));
            menuStrCopy(S_KEEP_BOTH, v.lines[2], sizeof(v.lines[2]));
            menuStrCopy(S_REPLACE_IT, v.lines[3], sizeof(v.lines[3]));
            menuStrCopy(S_CANCEL, v.lines[4], sizeof(v.lines[4]));
            v.lineCount = 5;
            v.choice = sameChoice_;
            break;
        case OV_REVERT: {
            char label[21], value[21];
            menuStrCopy(rv_.node.label, label, sizeof(label));
            menuFormatValue(rv_.node, be_, value, sizeof(value));
            snprintf(v.lines[0], sizeof(v.lines[0]), "%s: %s", label, value);
            menuStrCopy(S_T_KEEP, v.lines[1], sizeof(v.lines[1]));
            v.lineCount = 2;
            uint32_t left = kRevertMs - (now_ - rv_.at);
            v.value = (int16_t)((left + 999) / 1000);
            break;
        }
        case OV_MUTE: {
            bool muted = be_.value(SID_audio_mute) != 0;
            menuStrCopy(muted ? S_T_MUTE_ON : S_T_MUTE_OFF, v.lines[0], sizeof(v.lines[0]));
            v.lineCount = 1;
            v.value = muted ? 1 : 0;
            break;
        }
        case OV_VOLUME:
            menuStrCopy(S_VOLUME, v.lines[0], sizeof(v.lines[0]));
            v.lineCount = 1;
            v.value = (int16_t)be_.value(SID_audio_volume);
            break;
        case OV_INFO:
            buildInfo(v);
            break;
        case OV_TVOSD: {
            menuStrCopy(S_TV_OSD, v.lines[0], sizeof(v.lines[0]));
            menuStrCopy(S_T_TVOSD_HELP, v.lines[1], sizeof(v.lines[1]));
            v.lineCount = 2;
            uint32_t used = now_ - ovAt_;
            v.value = (int16_t)(used >= kTvOsdMs ? 0 : (kTvOsdMs - used + 999) / 1000);   // seconds left
            break;
        }
        case OV_SLIDER:
            menuStrCopy(sliderNode_.label, v.lines[0], sizeof(v.lines[0]));
            menuFormatValue(sliderNode_, be_, v.lines[1], sizeof(v.lines[1]));
            v.lineCount = 2;
            v.value = (int16_t)be_.value(sliderNode_.ref);
            v.choice = (uint8_t)menuSliderHalves(sliderNode_, be_);
            break;
        case OV_KEYBOARD: {
            static const uint16_t kTitles[4] = {S_K_NAME, S_K_NAME, S_OTA_PASSWORD, S_EXTRA_HOSTS};
            bool msg = kbMsg_ && now_ - kbMsgAt_ < kKbMsgMs;
            menuStrCopy(msg ? kbMsg_ : kTitles[kb_.purpose() & 3], v.lines[0], sizeof(v.lines[0]));
            v.caret = kb_.window(26, v.lines[1], sizeof(v.lines[1]));
            for (uint8_t r = 0; r < MenuKeyboard::kRows; r++) kb_.rowText(r, v.lines[2 + r], sizeof(v.lines[0]));
            v.lineCount = 6;
            v.value = (int16_t)(kb_.row() * 16 + kb_.col());
            v.choice = kb_.upper() ? 1 : 0;
            menuStrCopy(S_T_KB_HELP, v.help, sizeof(v.help));
            break;
        }
        case OV_UPDATE:
            {
                const MenuSignal& sg = be_.signal();
                menuStrCopy(updAdv_ ? S_T_UPDATING_ADV_CONTROLLER : S_T_UPDATING, v.lines[0], sizeof(v.lines[0]));
                uint8_t phase = updOutcome_ != UPV_PROGRESS ? updOutcome_
                              : sg.updateStage == UPD_DONE ? UPV_DONE
                              : sg.updateStage == UPD_VERIFYING ? UPV_VERIFYING : UPV_PROGRESS;
                v.choice = phase;
                v.lineCount = 1;
                if (phase != UPV_PROGRESS) {
                    menuStrCopy(phase == UPV_VERIFYING ? S_T_VERIFYING : phase == UPV_DONE ? S_T_UPDATED : S_T_UPDATE_FAILED,
                                v.lines[1], sizeof(v.lines[1]));
                    v.lineCount = 2;
                }
                v.value = sg.updatePct;
            }
            break;
        case OV_ADJUST:
            menuStrCopy(adjustNode_.label, v.lines[0], sizeof(v.lines[0]));
            menuFormatValue(adjustNode_, be_, v.lines[1], sizeof(v.lines[1]));
            menuStrCopy(adjustUndoable_ ? S_T_ADJUST_UNDO : S_T_ADJUST, v.lines[2], sizeof(v.lines[2]));
            v.lineCount = 3;
            v.choice = adjustVertical_ ? 1 : 0;         // axis for the encoder legend
            break;
        default:
            break;
    }
}

static const char* presetName(uint8_t id) {
    switch (id) {
        case 0x01: case 0x11: return "1280x960";
        case 0x02: case 0x12: return "1280x1024";
        case 0x03: case 0x13: return "1280x720";
        case 0x05: case 0x15: return "1920x1080";
        case 0x07: case 0x17: return "1920x1200";
        case 0x06: case 0x16: return "Downscale";
        case 0x04: return "720x480";
        case 0x14: return "768x576";
        default: return "Bypass";
    }
}

// A rate in tenths of Hz: "49.7Hz"; 0 is "not measured yet" (the first ~1.5 s of a source): "--.-Hz"
static void hzText(uint16_t x10, char* out, size_t n) {
    if (!x10) copyStr(out, n, "--.-Hz");
    else snprintf(out, n, "%u.%uHz", x10 / 10, x10 % 10);
}

static bool near(uint16_t v, uint16_t target) { return v + 10 >= target && v <= target + 10; }

static void sourceRes(const MenuSignal& s, char* res, size_t n) {
    if (near(s.srcLines, 262)) copyStr(res, n, s.interlaced ? "480i" : "240p");
    else if (near(s.srcLines, 312)) copyStr(res, n, s.interlaced ? "576i" : "288p");
    else if (near(s.srcLines, 525)) copyStr(res, n, "480p");
    else if (near(s.srcLines, 625)) copyStr(res, n, "576p");
    else if (!s.srcLines) copyStr(res, n, "--");   // not stable yet (MenuLineFilter)
    else snprintf(res, n, "%uL", s.srcLines);
}

// Source text of the info panel (after "IN   ")
static void sourceText(MenuBackend& be, char* out, size_t n) {
    const MenuSignal& s = be.signal();
    if (s.noSignal || s.advFreeRun) { menuStrCopy(S_T_NO_SIGNAL, out, n); return; }
    uint8_t in = menuInput(be);
    char res[8];
    sourceRes(s, res, sizeof(res));
    char hz[8];
    hzText(s.srcHzX10, hz, sizeof(hz));
    snprintf(out, n, "%s %s  %s", inputLongName(in), res, hz);
}

// Status pop-up: "<input> <lines> <standard>", "<output> <rate> <profile>" (the profile clipped first),
// then the kept-changes notice or "Profile i of N: Left/Right" when several profiles share the signal
void MenuEngine::buildStatus(MenuView& v) {
    const MenuSignal& s = be_.signal();
    uint8_t in = menuInput(be_);
    char res[8], label[PROFILE_NAME_BUF];
    sourceRes(s, res, sizeof(res));
    const char* std = s.advStandard >= 0 ? menuAdvStandardName(s.advStandard) : menuRateStandard(s.srcHzX10);
    snprintf(v.lines[0], sizeof(v.lines[0]), std[0] ? "%s %s %s" : "%s %s", inputLongName(in), res, std);
    char hz[8];
    hzText(s.outHzX10, hz, sizeof(hz));
    int n = snprintf(v.lines[1], sizeof(v.lines[1]), "%s %s ", presetName(s.presetId), hz);
    menuProfileLabel(be_, label, sizeof(label));
    if (n > 0 && n < (int)sizeof(v.lines[1])) copyStr(v.lines[1] + n, sizeof(v.lines[1]) - (size_t)n, label);
    v.lineCount = 2;
    if (discarded_[0]) {
        discardedText(discarded_, v.lines[2], sizeof(v.lines[2]));
        v.lineCount = 3;
        return;
    }
    uint8_t idx[PROFILE_MAX];
    uint8_t m = sortedMatches(idx);
    if (m < 2) return;
    int a = be_.profileActive(), pos = 0;
    for (uint8_t i = 0; i < m; i++) if ((int)idx[i] == a) pos = i + 1;
    char keys[12]; menuStrCopy(S_T_OTHER, keys, sizeof(keys));
    if (pos) {                                         // "Profile 1 of 2: Left/Right" (28 cells at 36 of 36)
        char prof[10], of[4];
        menuStrCopy(S_PROFILE, prof, sizeof(prof));
        menuStrCopy(S_T_OF, of, sizeof(of));
        snprintf(v.lines[2], sizeof(v.lines[2]), "%s %d %s %u: %s", prof, pos, of, m, keys);
    } else {                                           // none of them active: "2 profiles: Left/Right"
        char profs[10]; menuStrCopy(S_T_PROFILES, profs, sizeof(profs));
        snprintf(v.lines[2], sizeof(v.lines[2]), "%u %s: %s", m, profs, keys);
    }
    v.lineCount = 3;
}

// Output resolution in a few characters (the OLED home screen draws it large in a 56 px column)
static const char* presetShort(uint8_t id) {
    switch (id) {
        case 0x01: case 0x11: return "960p";
        case 0x02: case 0x12: return "1024p";
        case 0x03: case 0x13: return "720p";
        case 0x05: case 0x15: return "1080p";
        case 0x07: case 0x17: return "1200p";
        case 0x06: case 0x16: return "Down";
        case 0x04: return "480p";
        case 0x14: return "576p";
        default: return "Bypass";
    }
}

// Top bar of the main screens: "GBSC PRO v<version>" (the name alone when the version is unknown)
void MenuEngine::mainTitle(char* out, size_t n) {
    char ver[12];
    be_.text(LT_FW_GBS, ver, sizeof(ver));
    menuStrCopy(S_MENU, out, n);
    if (ver[0]) {
        size_t l = strlen(out);
        snprintf(out + l, n - l, " v%s", ver);
    }
}

void MenuEngine::idle(MenuIdle& o) {
    const MenuSignal& s = be_.signal();
    memset(&o, 0, sizeof(o));                          // whole-struct compare in the renderer
    mainTitle(o.title, sizeof(o.title));
    uint8_t in = menuInput(be_);
    if (in > 6) in = 0;
    bool absent = s.noSignal || s.advFreeRun;
    o.noSignal = absent;
    o.searching = absent && s.searching;
    if (o.searching) {
        copyStr(o.inLine, sizeof(o.inLine), "Source: Auto");      // not the path being probed
    } else if (absent) {
        snprintf(o.inLine, sizeof(o.inLine), "Input %s", inputLongName(in));
    } else {
        sourceRes(s, o.inRes, sizeof(o.inRes));
        char hz[8];
        hzText(s.srcHzX10, hz, sizeof(hz));
        snprintf(o.inLine, sizeof(o.inLine), "%s %s", inputShortName(in), hz);
        copyStr(o.outRes, sizeof(o.outRes), presetShort(s.presetId));
        hzText(s.outHzX10, o.outHz, sizeof(o.outHz));
        o.locked = s.frameLocked;
    }
    int a = be_.profileActive();
    if (a >= 0) {                                       // the region is shown on its own: no region word here
        const ProfileEntry& pe = be_.profiles().at((uint8_t)a);
        profileNameWithoutRegion(pe.name, pe.region, o.profile, sizeof(o.profile));
        menuSanitize(o.profile);
    }
    uint8_t region = a >= 0 ? be_.profiles().at((uint8_t)a).region : PROFILE_REGION_NONE;
    if (a >= 0 && region < PROFILE_REGION_NONE) copyStr(o.region, sizeof(o.region), profileRegionName(region));
    else if (!absent && s.advStandard >= 0) copyStr(o.region, sizeof(o.region), menuAdvStandardName(s.advStandard));
    else if (!absent) copyStr(o.region, sizeof(o.region), menuRateStandard(s.srcHzX10));
    char ip[20], mode[12];
    be_.text(LT_WIFI_IP, ip, sizeof(ip));
    be_.text(LT_WIFI_MODE, mode, sizeof(mode));
    snprintf(o.net, sizeof(o.net), "%s%s", strcmp(mode, "AP") ? "" : "AP ", ip);
    o.volume = (uint8_t)be_.value(SID_audio_volume);
    o.muted = be_.value(SID_audio_mute) != 0 || o.volume == 0;
    o.rssi = (int8_t)be_.live(LV_WIFI_RSSI);
}

void MenuEngine::buildInfo(MenuView& v) {
    const MenuSignal& s = be_.signal();
    char t[29];
    uint8_t n = 0;
    // Fixed label column (4 letters padded to 5 cells), main value, two spaces, details (renderer colours them).
    // No sync/stripper rows (not useful at a glance): the IP address instead
    char src[24];
    sourceText(be_, src, sizeof(src));
    snprintf(v.lines[n++], sizeof(v.lines[0]), "IN   %s", src);
    if (s.noSignal || s.advFreeRun) {                   // say how to pick the input
        char c[24]; menuStrCopy(S_T_CHOOSE_INPUT, c, sizeof(c));
        snprintf(v.lines[n++], sizeof(v.lines[0]), "OK   %s", c);
    }
    if (!s.noSignal) {
        char hz[8];
        hzText(s.outHzX10, hz, sizeof(hz));
        snprintf(v.lines[n++], sizeof(v.lines[0]), "OUT  %s  %s%s", presetName(s.presetId), hz, s.frameLocked ? " LOCK" : "");
    }
    be_.text(LT_PROFILE_ACTIVE, t, sizeof(t));
    menuSanitize(t);
    if (!t[0]) menuStrCopy(S_DEFAULTS, t, sizeof(t));   // no active profile: "Defaults", as the Profiles row
    snprintf(v.lines[n++], sizeof(v.lines[0]), "PROF %s", t);
    be_.text(LT_WIFI_SSID, t, sizeof(t));
    if (!t[0]) be_.text(LT_WIFI_MODE, t, sizeof(t));
    menuSanitize(t);
    snprintf(v.lines[n++], sizeof(v.lines[0]), "NET  %s", t);
    be_.text(LT_WIFI_IP, t, sizeof(t));
    snprintf(v.lines[n++], sizeof(v.lines[0]), "IP   %s", t);
    v.lineCount = n;
}
