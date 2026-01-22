// ====================================================================================
// menu-output.cpp
// IR Menu Handler for Output Resolution Selection
// ====================================================================================

#include "../menu-core.h"

// ====================================================================================
// External References
// ====================================================================================

extern char userCommand;
extern void saveUserPrefs();

// ====================================================================================
// IR_handleOutputResolution - Output Resolution Menu
// ====================================================================================

bool IR_handleOutputResolution(void)
{
    if (oled_menuItem == OLED_OutputResolution_1200) {
        showMenu("Menu->Output", "1920x1200");

        if (irDecode()) {
            switch (results.value) {
                case IR_KEY_MENU:
                    exitMenu();
                    break;
                case IR_KEY_UP:
                    Menu_navigateTo(OLED_OutputResolution_1080);  // Wrap to last
                    break;
                case IR_KEY_DOWN:
                    Menu_navigateTo(OLED_OutputResolution_480);
                    break;
                case IR_KEY_OK:
                    userCommand = 'L';
                    break;
                case IR_KEY_EXIT:
                    Menu_navigateTo(OLED_OutputResolution);
                    break;
            }
            irResume();
        }
        return true;
    }
    if (oled_menuItem == OLED_OutputResolution_1080) {
        showMenu("Menu->Output", "1920x1080");

        if (irDecode()) {
            switch (results.value) {
                case IR_KEY_MENU:
                    exitMenu();
                    break;
                case IR_KEY_UP:
                    Menu_navigateTo(OLED_OutputResolution_480);  // Wrap to last
                    break;
                case IR_KEY_DOWN:
                    Menu_navigateTo(OLED_OutputResolution_1024);
                    break;
                case IR_KEY_OK:
                    userCommand = 's';
                    break;
                case IR_KEY_EXIT:
                    Menu_navigateTo(OLED_OutputResolution);
                    break;
            }
            irResume();
        }
        return true;
    }

    if (oled_menuItem == OLED_OutputResolution_1024) {
        showMenu("Menu->Output", "1280x1024");

        if (irDecode()) {
            switch (results.value) {
                case IR_KEY_MENU:
                    exitMenu();
                    break;
                case IR_KEY_UP:
                    Menu_navigateTo(OLED_OutputResolution_1080);
                    break;
                case IR_KEY_DOWN:
                    Menu_navigateTo(OLED_OutputResolution_960);
                    break;
                case IR_KEY_OK:
                    userCommand = 'p';
                    break;
                case IR_KEY_EXIT:
                    Menu_navigateTo(OLED_OutputResolution);
                    break;
            }
            irResume();
        }
        return true;
    }

    if (oled_menuItem == OLED_OutputResolution_960) {
        showMenu("Menu->Output", "1280x960");

        if (irDecode()) {
            switch (results.value) {
                case IR_KEY_MENU:
                    exitMenu();
                    break;
                case IR_KEY_UP:
                    Menu_navigateTo(OLED_OutputResolution_1024);
                    break;
                case IR_KEY_DOWN:
                    Menu_navigateTo(OLED_OutputResolution_720);
                    break;
                case IR_KEY_OK:
                    userCommand = 'f';
                    break;
                case IR_KEY_EXIT:
                    Menu_navigateTo(OLED_OutputResolution);
                    break;
            }
            irResume();
        }
        return true;
    }

    if (oled_menuItem == OLED_OutputResolution_720) {
        showMenu("Menu->Output", "1280x720");

        if (irDecode()) {
            switch (results.value) {
                case IR_KEY_MENU:
                    exitMenu();
                    break;
                case IR_KEY_UP:
                    Menu_navigateTo(OLED_OutputResolution_960);
                    break;
                case IR_KEY_DOWN:
                    Menu_navigateTo(OLED_OutputResolution_480);
                    break;
                case IR_KEY_OK:
                    userCommand = 'g';
                    break;
                case IR_KEY_EXIT:
                    Menu_navigateTo(OLED_OutputResolution);
                    break;
            }
            irResume();
        }
        return true;
    }

    if (oled_menuItem == OLED_OutputResolution_480) {
        showMenu("Menu->Output", "480p/576p");

        if (irDecode()) {
            switch (results.value) {
                case IR_KEY_MENU:
                    exitMenu();
                    break;
                case IR_KEY_UP:
                    Menu_navigateTo(OLED_OutputResolution_720);
                    break;
                case IR_KEY_DOWN:
                    Menu_navigateTo(OLED_OutputResolution_1080);  // Wrap to first
                    break;
                case IR_KEY_OK:
                    userCommand = 'h';
                    break;
                case IR_KEY_EXIT:
                    Menu_navigateTo(OLED_OutputResolution);
                    break;
            }
            irResume();
        }
        return true;
    }

    return false;
}
