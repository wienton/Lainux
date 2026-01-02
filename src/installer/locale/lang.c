#include <string.h>
#include <ncurses.h>
#include "lang.h"

Language current_lang = LANG_EN;

// English translations
const char* en_strings[][2] = {
    {"INSTALL_ON_HARDWARE", "INSTALL ON HARDWARE"},
    {"INSTALL_ON_VM", "INSTALL ON VM"},
    {"HARDWARE_INFO", "HARDWARE INFO"},
    {"SYSTEM_REQUIREMENTS", "SYSTEM REQUIREMENTS"},
    {"CONF_SELECTION", "CONFIGURATION SELECTION"},
    {"DISK_INFO", "DISK INFO"},
    {"SETTINGS", "SETTINGS"},
    {"EXIT_INSTALLER", "EXIT INSTALLER"},
    {"EXIT_CONFIRM_PROMPT", "Exit Lainux installer?"},
    {NULL, NULL}
};
// Russian translations
const char* ru_strings[][2] = {
    {"INSTALL_ON_HARDWARE", "УСТАНОВИТЬ НА ЖЕЛЕЗО"},
    {"INSTALL_ON_VM", "УСТАНОВИТЬ В ВИРТУАЛКУ"},
    {"HARDWARE_INFO", "ИНФОРМАЦИЯ ОБ ОБОРУДОВАНИИ"},
    {"SYSTEM_REQUIREMENTS", "СИСТЕМНЫЕ ТРЕБОВАНИЯ"},
    {"CONF_SELECTION", "ВЫБОР КОНФИГУРАЦИИ"},
    {"DISK_INFO", "ИНФОРМАЦИЯ О ДИСКАХ"},
    {"SETTINGS", "НАСТРОЙКИ"},
    {"EXIT_INSTALLER", "ВЫХОД ИЗ УСТАНОВЩИКА"},
    {"EXIT_CONFIRM_PROMPT", "Выйти из установщика Lainux?"},
    {NULL, NULL}
};

const char* get_text(const char* key) {
    const char* (*dict)[2] = (current_lang == LANG_RU) ? ru_strings : en_strings;
    for (int i = 0; dict[i][0] != NULL; i++) {
        if (strcmp(dict[i][0], key) == 0) {
            return dict[i][1];
        }
    }
    return key; // Fallback
}

void select_language(void) {
    clear();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int center_x = (max_x - 40) / 2;
    if (center_x < 0) center_x = 0;

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(max_y / 2 - 3, center_x, "ВЫБЕРИТЕ ЯЗЫК / SELECT LANGUAGE");
    mvprintw(max_y / 2 - 1, center_x + 10, "1. Русский");
    mvprintw(max_y / 2,     center_x + 10, "2. English");
    mvprintw(max_y / 2 + 2, center_x + 5,  "Выберите (1–2): ");
    attroff(A_BOLD | COLOR_PAIR(1));

    refresh();
    echo();
    char choice[2];
    mvgetnstr(max_y / 2 + 2, center_x + 24, choice, 2);
    noecho();

    if (choice[0] == '1') {
        current_lang = LANG_RU;
    } else {
        current_lang = LANG_EN;
    }
}
