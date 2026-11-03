// Read-only view model produced by the menu engine and drawn by the OSD and OLED renderers
#pragma once
#include <stdint.h>

// Update overlay phase (MenuView.choice): the bar while receiving, else a centred status line (lines[1])
enum : uint8_t { UPV_PROGRESS, UPV_VERIFYING, UPV_DONE, UPV_FAILED };

enum : uint8_t { VIEW_CLOSED, VIEW_LIST, VIEW_OVERLAY };
enum : uint8_t { ROWF_SELECTED = 1, ROWF_DISABLED = 2, ROWF_CHANGED = 4, ROWF_SUB = 8, ROWF_HEADER = 16,
                 ROWF_EDITING = 32, ROWF_SLIDER = 64, ROWF_BACK = 128 };
// OV_STATUS: status pop-up, 2-3 lines; OV_SAME_SIGNAL: lines 0 title, 1 profile, choice 0..2
enum : uint8_t { OV_NONE, OV_MUTE, OV_VOLUME, OV_INFO, OV_CONFIRM, OV_REVERT, OV_ADJUST, OV_TOAST, OV_UPDATE,
                 OV_KEYBOARD, OV_SLIDER, OV_SAME_SIGNAL, OV_STATUS, OV_TVOSD };

// ROWK_PROFILE: Load list row; the value (region, active marker) is always drawn, the label is clipped/scrolled
enum : uint8_t { ROWK_VALUE, ROWK_CYCLE, ROWK_ACTION, ROWK_SUB, ROWK_HEADER, ROWK_SLIDER, ROWK_LIVE, ROWK_PROFILE };

static const uint8_t MENU_VIEW_ROWS = 7;

struct MenuViewRow {
    char label[25];                     // profile names are up to 24 (scrolled when they do not fit)
    char value[25];                     // a profile name as value (Save row) fits whole
    uint8_t flags;                      // ROWF_*
    int8_t slider;                      // 0..20 half cells, -1 none
    uint8_t kind;                       // ROWK_*: how the renderer draws the value column
};

struct MenuView {
    uint8_t mode;                       // VIEW_*
    uint16_t rev;                       // bumps on every visible change
    char title[25];                     // a profile sub-menu shows the whole name (scrolls when too long)
    uint8_t rowCount;                   // <= window
    uint8_t cursorRow;                  // index in rows of the selected row
    uint8_t posIndex, posCount;         // 1-based position of the cursor among the selectable rows of the level
    bool deep;                          // inside a sub-menu (title shows the back arrow)
    MenuViewRow rows[MENU_VIEW_ROWS];
    bool moreAbove, moreBelow;
    uint8_t listTop, listTotal, listWindow;   // scrollbar: first visible row, visible rows of the level, window
    char help[97];                      // description of the selected row (scrolls when longer than the space)
    uint8_t overlay;                    // OV_*
    uint8_t lineCount;                  // overlay lines <= 7
    char lines[MENU_VIEW_ROWS][30];     // 28 cells, or "PROF " + a 24-letter profile name (info panel)
    int16_t value;                      // OV_VOLUME level, OV_REVERT seconds, OV_UPDATE percent, OV_MUTE state
    uint8_t choice;                     // OV_CONFIRM: 1 = Yes selected; OV_ADJUST: 1 = vertical axis;
                                        // OV_KEYBOARD: 1 = upper case
    uint8_t caret;                      // OV_KEYBOARD: caret cell in lines[1] (value = selected key, row * 16 + col)
    bool overMenu;                      // overlay drawn over an open menu
};

// OLED home screen while the menu is closed (MenuEngine::idle)
struct MenuIdle {
    char title[21];                     // top bar: product name and firmware version (as the root menu)
    bool noSignal;
    bool searching;                     // Source = Auto is scanning: "Searching" instead of "No signal"
    char inRes[8];                      // source resolution, large: "240p", "480i", "262L"
    char inLine[20];                    // under it: short input and rate "RGB 59.9Hz"; no signal: "Input RGB SCART"
    char outRes[10];                    // output, large: "960p", "1080p", "Bypass"
    char outHz[10];                     // under it: "60.0Hz"
    bool locked;                        // frame lock (icon)
    char profile[25];                   // active profile, "" without one
    char region[8];                     // the profile's region ("NTSC-U", "PAL60"...), else "NTSC"/"PAL" from the source rate, else ""
    char net[24];                       // IP, "AP 192.168.4.1" in access-point mode
    uint8_t volume;                     // percent
    bool muted;                         // muted or 0 %
    int8_t rssi;                        // Wi-Fi dBm, 0 = unknown (AP mode)
};
