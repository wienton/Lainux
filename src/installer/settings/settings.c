// settings.c
#include <stdio.h>
#include <string.h>
#include "settings.h"
#include "../locale/lang.h"  // for _("KEY")


InstallerSettings settings;


void init_default_settings(void) {
    settings.language = LANG_EN;
    settings.theme = 1; // dark
    settings.keyboard_layout = 0; // en
    settings.network_mode = 0;    // dhcp
}


void apply_language(Language lang) {

    extern Language current_lang;
    current_lang = lang;
    settings.language = lang;
}

const char* get_theme_name(void) {
    switch (settings.theme) {
        case 0: return _("THEME_LIGHT");
        case 1: return _("THEME_DARK");
        case 2: return _("THEME_SYSTEM");
        default: return _("THEME_DARK");
    }
}

const char* get_keyboard_name(void) {
    return (settings.keyboard_layout == 0) ? _("KB_EN") : _("KB_RU");
}

const char* get_network_mode_name(void) {
    return (settings.network_mode == 0) ? _("NET_DHCP") : _("NET_STATIC");
}

// general function - interactive settings approve
void print_settings(void) {
    int selected = 0;
    const int total_items = 5;
    int quit = 0;

    while (!quit) {
        clear();
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        int start_y = max_y / 2 - 6;
        int start_x = (max_x - 52) / 2;

        // header
        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw(start_y, start_x + 15, "%s", _("SETTINGS_TITLE"));
        mvprintw(start_y + 1, start_x, "----------------------------------------------------");
        attroff(A_BOLD | COLOR_PAIR(1));

        // pointers menu
        const char* items[] = {
            _("LANG_SETTING"),
            _("THEME_SETTING"),
            _("KEYBOARD_SETTING"),
            _("NETWORK_SETTING"),
            _("BACK_TO_MAIN")
        };

        for (int i = 0; i < total_items; i++) {
            int y = start_y + 2 + i * 2;
            if (i == selected) {
                attron(A_REVERSE | COLOR_PAIR(2));
                mvprintw(y, start_x + 2, "> %s", items[i]);
                attroff(A_REVERSE | COLOR_PAIR(2));
            } else {
                mvprintw(y, start_x + 4, "%s", items[i]);
            }

            const char* value = "";
            switch (i) {
                case 0: value = (settings.language == LANG_RU) ? "Русский" : "English"; break;
                case 1: value = get_theme_name(); break;
                case 2: value = get_keyboard_name(); break;
                case 3: value = get_network_mode_name(); break;
                default: continue;
            }
            mvprintw(y, start_x + 38, ": %s", value);
        }

        refresh();

        int ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected <= 0) ? total_items - 1 : selected - 1;
                break;
            case KEY_DOWN:
                selected = (selected >= total_items - 1) ? 0 : selected + 1;
                break;
            case 10: // Enter
            case '\0':
                switch (selected) {
                    case 0: // lang
                        apply_language((settings.language == LANG_EN) ? LANG_RU : LANG_EN);
                        break;
                    case 1: // theme
                        settings.theme = (settings.theme + 1) % 3;
                        break;
                    case 2: // keyboard
                        settings.keyboard_layout = 1 - settings.keyboard_layout;
                        // system(settings.keyboard_layout ? "loadkeys ru" : "loadkeys us");
                        break;
                    case 3: // net
                        settings.network_mode = 1 - settings.network_mode;
                        break;
                    case 4: // back
                        quit = 1;
                        break;
                }
                break;
            case 27: // ESC
                quit = 1;
                break;
        }
    }
}
