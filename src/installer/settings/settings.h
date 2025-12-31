// settings.h
#ifndef SETTINGS_H
#define SETTINGS_H

#include <ncurses.h>
#include "../locale/lang.h"

typedef struct {
    Language language;
    int theme;              // 0 = light, 1 = dark, 2 = system
    int keyboard_layout;    // 0 = en, 1 = ru
    int network_mode;       // 0 = dhcp, 1 = static
} InstallerSettings;

extern InstallerSettings settings;

void init_default_settings(void);
void print_settings(void);
void apply_language(Language lang);

const char* get_theme_name(void);
const char* get_keyboard_name(void);
const char* get_network_mode_name(void);

#endif
