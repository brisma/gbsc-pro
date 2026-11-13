// Menu engine: navigation stack, edits, confirmations, revert countdown, overlays, timeouts.
// Pure: reads and writes only through MenuBackend; renderers read view().
#pragma once
#include <stdint.h>
#include "menu-types.h"
#include "menu-backend.h"
#include "menu-view.h"
#include "menu-keyboard.h"
#include "../profiles/profile-list.h"

class MenuEngine {
public:
    explicit MenuEngine(MenuBackend& be);
    void key(uint8_t k, bool repeat, uint32_t nowMs);   // MenuKey
    void tick(uint32_t nowMs);                         // timeouts, countdowns, pending work, view refresh
    void onSignalChange();                             // re-evaluate visibility (pop hidden sub-menus)
    void openInfo(uint32_t nowMs);
    void setWindow(uint8_t rows);                      // list window, 1..7 (OSD 7, OLED 5)
    bool busy() const;                                 // a menu or an overlay is shown
    // The confirm question of a node (a menu string id ending in '?', <= 28 cells), 0 = the node asks none
    static uint16_t confirmQuestion(const MenuNode& n);
    void setEncoderBackRow(bool on) { backOption_ = on; }   // "Back" row atop sub-lists in sessions opened by K_CLICK
    void idle(MenuIdle& out);                          // status block for the OLED while the menu is closed
    // Auto-load (device glue): call right after its POP_LOAD was queued; the toast follows the op result.
    // discarded: the profile whose unsaved changes the load drops, named once it succeeded
    void autoloadStarted(const char* name, uint32_t nowMs, const char* discarded = nullptr);
    void notice(uint16_t str, uint32_t nowMs);            // a one-off toast from the device (e.g. the ADV Controller does not reply)
    void message(const char* text, uint32_t nowMs);       // a one-off toast with device text (OTA check results)
    void setToastHook(void (*hook)(const char* text)) { toastHook_ = hook; }   // console log: every toast text
    // An update ended (before the next tick): the update overlay shows the outcome for kUpdateResultMs; without
    // the overlay (refused before it started) an ADV Controller result is a toast
    void updateResult(bool ok, bool advController, uint32_t nowMs);
    // Boot prompt: the "ADV Controller" update confirmation while nothing is shown; false: not opened.
    // Yes runs the update, No after moving the cursor skips this build (FN_ADV_CONTROLLER_SKIP), anything else is "not now".
    bool promptAdvController(uint32_t nowMs);
    const MenuView& view() const { return view_; }
    // Status pop-up, from the device glue once a new signal settled: due = the osd.status_popup rule
    // says so; a discarded-changes notice (autoloadStarted) shows even when not due. OSD boards only.
    void statusPopup(uint32_t nowMs, bool due);
    // What holds auto-load and the boot prompt back: an open menu or an overlay other than the pop-up or a toast
    bool blocking() const { return depth_ > 0 || (ov_ != OV_NONE && ov_ != OV_STATUS && ov_ != OV_TOAST); }
    bool statusNoticePending() const { return noticePending_; }   // a discarded-changes notice waits for the pop-up

    static const uint32_t kMenuTimeoutMs = 15000;     // osd.timeout default (15 s)
    uint32_t menuTimeoutMs();                          // osd.timeout in ms (5 s .. 10 min)
    static const uint32_t kQuickTimeoutMs = 3000;      // mute / volume
    static const uint32_t kToastMs = 2000;
    static const uint32_t kRevertMs = 15000;
    static const uint32_t kRefreshMs = 250;
    static const uint32_t kPromptMs = 16000;           // boot prompt timeout (menu closed)
    static const uint32_t kStatusMs = 4000;            // status pop-up
    static const uint32_t kStatusChoiceMs = 6000;      // status pop-up offering another profile (Profile 1 of 2: Left/Right)
    static const uint32_t kTvOsdMs = 8000;             // TV OSD mode: closes 8 s after the last key
    static const uint32_t kUpdateResultMs = 3000;      // update overlay: the outcome line

private:
    enum : uint8_t { STK_NODE, STK_LOAD, STK_MANAGE, STK_PROFILE, STK_NAMES };
    enum : uint8_t { DYN_NONE, DYN_LOAD_ITEM, DYN_MANAGE_ITEM, DYN_DELETE, DYN_FORGET, DYN_EMPTY, DYN_BACK,
                     DYN_RENAME, DYN_NAME_EDIT, DYN_NAME_SOURCE, DYN_NAME_CONSOLE, DYN_LOAD_DEFAULTS, DYN_REPLACE,
                     DYN_ALL_SETTINGS };
    struct Entry { const MenuNode* node; uint8_t kind; int8_t profile; uint8_t cursor; uint8_t top; };
    struct Row { MenuNode node; uint8_t dyn; int8_t profile; };
    static const uint8_t kMaxDepth = 6;
    static const uint8_t kMaxRows = 40;

    uint8_t rowCount(const Entry& e);
    bool hasBack(const Entry& e) const { return back_ && &e != &stack_[0]; }
    // "All settings" is the last row of the Quick menu in an encoder session; not on the no-signal output
    bool hasAllSettings(const Entry& e) const { return back_ && root_ == ROOT_QUICK && &e == &stack_[0] && !freeRun_; }
    uint8_t childIndex(const Entry& e) const { return (uint8_t)(e.cursor - (hasBack(e) ? 1 : 0)); }
    void firstRealRow(Entry& e);          // cursor on the first selectable row after Back (else Back)
    bool rowAt(const Entry& e, uint8_t i, Row& out);
    bool rowVisible(const Row& r);
    bool headerHasItems(const Entry& e, uint8_t i);   // a HEAD row shows only when an item under it is visible
    bool rowSelectable(const Row& r);
    void rowLabel(const Row& r, char* out, size_t n);
    void fixCursor(Entry& e);
    void freeRunQuickCursor();
    void validateStack();
    void push(const Row& r);
    enum : uint8_t { ROOT_MAIN, ROOT_QUICK };
    void openMenu(uint32_t now, uint8_t root = ROOT_MAIN);
    void setRoot(uint8_t root);           // depth 1 of this root (keeps the encoder session)
    bool quickProfileRow(const Row& r) const;
    void freeRunEnter();                  // once per session, with no signal, on boards with the OSD chip
    bool noSignalRowAllowed(const Row& r) const;   // Source and Menu mode stay usable on the free-run
    void closeMenu();
    void moveCursor(int dir);
    void rebuild();
    void listKey(uint8_t k, bool repeat, uint32_t now);
    // edits and overlays
    static uint8_t prio(uint8_t ov);
    bool openOverlay(uint8_t ov);         // false when a higher-priority overlay is open
    void closeOverlay();
    void toastText(const char* text);
    void toast(uint16_t str);
    void busyToast() { toast(S_T_BUSY_ID); }
    bool changeSetting(const MenuNode& n, uint16_t v);   // false: refused (wait or busy toast shown)
    void stepRow(const Row& r, int dir, bool repeat);
    void activate(const Row& r, uint8_t k);
    void execute(const MenuNode& n, uint8_t dyn, int8_t profile);
    void commitPending(const Row& r);
    void cancelEdit() { editing_ = false; pendingValid_ = false; pendingChoice_ = false; }
    bool applyChanged(const MenuWrite* w, uint8_t n);
    void changeChoice(const MenuNode& n, uint8_t opt);
    bool revertWriteOld();
    bool revertSettled();
    void revertUndo();
    void revertKeep();
    void revertTick();
    void overlayKey(uint8_t k, bool repeat);
    void buildOverlay(MenuView& v);
    void mainTitle(char* out, size_t n);
    void buildInfo(MenuView& v);
    void leaveOverlay(uint8_t old);
    void tvOsdKey(uint8_t k, bool repeat);
    bool rowEnabled(const Row& r, uint16_t* reason);
    void pushDynamic(const Row& r);
    void pushProfile(const Row& r);
    void openAtProfiles(uint32_t now);
    void toggleMute();
    void stepVolume(int dir);
    void adjustKey(uint8_t k, bool repeat);
    void openAdjust(const MenuNode& n);
    void adjustUndo();                    // restore the captured screen.* values when a step ran
    void sliderKey(uint8_t k, bool repeat);
    void openKeyboard(uint8_t purpose, const char* initial, int8_t profile);
    void keyboardKey(uint8_t k);
    void keyboardDone();
    void keyboardMessage(uint16_t str) { kbMsg_ = str; kbMsgAt_ = now_; }
    void profileOp(uint8_t kind, int8_t index, const char* name = nullptr, uint8_t region = PROFILE_REGION_NONE);
    void cycleRegion(const Row& r, int dir);
    void saveConsole(const Row& r);
    void popNames();                      // after a save from the names list: back to Profiles
    void refreshSorted();                 // sorted profile indexes and signal matches, once per key/tick (not per row)
    bool isMatch(int8_t p) const;         // the profile is among the current signal's matches
    void openConfirm(const Row& r, const char* text);   // confirmation with its own line
    void openNewProfile();                // the New profile list of the Profiles page on top
    void profileRowLabel(int8_t p, char* out, size_t n);
    uint8_t defaultRegion();              // PAL60 from the decoder, PAL on a 50 Hz source, else NTSC-U
    // New profile saves: asks "Same signal as" first when a profile already has this signal
    void saveNew(uint8_t kind, const char* name, uint8_t region);
    void sameSignalPick(uint8_t choice);
    bool statusEnabled();                 // osd.status_popup on and the board has the OSD chip
    uint8_t sortedMatches(uint8_t* out);  // the current signal's profiles by index: a stable cycling order
    void loadMatch(int dir);              // next/previous profile of this signal (lose-changes confirm)
    void buildStatus(MenuView& v);

    static const uint16_t S_T_BUSY_ID;
    MenuBackend& be_;
    uint8_t ov_ = OV_NONE;
    uint32_t ovAt_ = 0;
    char toast_[29] = {0};
    char toastQ_[2][29] = {{0}};            // toasts waiting behind the shown one (oldest first)
    void (*toastHook_)(const char* text) = nullptr;
    uint8_t toastQn_ = 0;
    static const uint32_t kToastKeepMs = 1000;   // a toast shown for less than this is not replaced
    MenuNode confirmNode_ = {};
    uint8_t confirmDyn_ = 0;
    int8_t confirmProfile_ = -1;
    uint8_t confirmChoice_ = 0;
    bool prompt_ = false;                   // the confirmation is the ADV Controller boot prompt
    bool promptMoved_ = false;              // the cursor was moved in the boot prompt
    // old: the setting's value; a choice keeps the old values of every setting it writes (oldW/oldN)
    struct Revert { bool open, settling, undoPending; MenuNode node; uint16_t old; MenuWrite oldW[2]; uint8_t oldN;
                    uint32_t at, settleAt; } rv_ = {};
    static const uint32_t kSettleGiveUpMs = 5000;
    bool editing_ = false, pendingValid_ = false, pendingChoice_ = false;   // pendingChoice_: pending_ is an option index
    uint16_t pending_ = 0;
    uint8_t pendingSid_ = 0;
    uint8_t sortIdx_[40];
    uint8_t sortN_ = 0;
    uint8_t match_[PROFILE_MAX];
    uint8_t matchN_ = 0;
    char confirmText_[29] = {0};          // replaces confirmQuestion() when set (a row of its own: 28 cells)
    struct SaveNew { uint8_t kind; char name[PROFILE_NAME_BUF]; uint8_t region; int8_t target; } saveNew_ = {};
    uint8_t sameChoice_ = 0;              // OV_SAME_SIGNAL: 0 Keep both, 1 Replace it, 2 Cancel
    char discarded_[PROFILE_LABEL_BUF] = {0};           // auto-load discarded this profile's changes: third line
    bool noticePending_ = false;                        // discarded_ not shown yet (the pop-up takes it once)
    char loaded_[PROFILE_LABEL_BUF] = {0};              // auto-load's profile: a menu opened before the pop-up
    uint32_t loadedAt_ = 0;                             // turns the pop-up into "Loaded X"
    static const uint32_t kLoadedNameMs = 10000;        // a pop-up gate that opens later names nothing old
    bool checkUpdate_ = false;
    bool freeRun_ = false;                // this menu session runs the no-signal 720x480 free-run
    bool sourceChanged_ = false;          // input.source written during it: no blanking restore at the exit
    uint8_t lastMode_ = 0xFF;               // system.menu_mode seen at the last tick (toast on leaving Advanced)
    uint8_t updOutcome_ = UPV_PROGRESS;     // update overlay: UPV_DONE / UPV_FAILED once the update ended
    bool updAdv_ = false;                   // the update shown is the ADV Controller one (kept after it ends)
    uint32_t updOutcomeAt_ = 0;
    MenuNode adjustNode_ = {};
    bool adjustVertical_ = false;
    bool adjustStepped_ = false;
    enum : uint8_t { GEO_NONE, GEO_UNDO, GEO_KEEP };
    uint8_t geoPending_ = GEO_NONE;          // undo/keep waiting for step actions still in the queue
    void geoFinish(uint8_t what);
    bool adjustUndoable_ = false;           // Position/Size: Back reloads the preset (Borders: no undo)
    MenuNode sliderNode_ = {};
    static const uint32_t kSliderTimeoutMs = 5000;
    uint8_t regions_[32];                   // STK_NAMES: region per catalog row
    MenuKeyboard kb_;
    int8_t kbProfile_ = -1;                 // KB_RENAME: profile index
    char kbOrig_[PROFILE_NAME_BUF] = {0};                 // KB_RENAME: name when the keyboard opened
    uint16_t kbMsg_ = 0;                    // message shown in the keyboard title line for 2 s
    uint32_t kbMsgAt_ = 0;
    static const uint32_t kKbMsgMs = 2000;
    // A profile operation whose result becomes a toast: matched by its sequence number
    struct Wait { bool on; uint16_t seq; uint32_t at; char name[PROFILE_LABEL_BUF]; char discarded[PROFILE_LABEL_BUF]; };
    Wait userWait_ = {}, autoWait_ = {};
    static const uint32_t kWaitGiveUpMs = 5000;
    bool waitDone(Wait& w, bool autoload);    // true when the wait ended (toast shown or queued)
    static void discardedText(const char* name, char* out, size_t len);   // "Discarded: <profile>"
    Entry stack_[kMaxDepth];
    uint8_t depth_ = 0;                 // 0: menu closed
    uint8_t root_ = ROOT_MAIN;          // the page at depth 1: main menu or Quick menu
    bool backOption_ = false;           // board has an encoder
    bool back_ = false;                 // this session shows the Back row (latched at open)
    uint8_t window_ = MENU_VIEW_ROWS;
    uint32_t now_ = 0, lastKey_ = 0, lastRefresh_ = 0;
    MenuView view_;
};
