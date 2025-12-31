// src/locale/lang.c
#include <string.h>
#include "lang.h"
#include <ncurses.h>

Language current_lang = LANG_EN;

extern const char* en_strings[][2];
extern const char* ru_strings[][2];

const char* get_text(const char* key) {
    const char* (*dict)[2] = (current_lang == LANG_RU) ? ru_strings : en_strings;
    for (int i = 0; dict[i][0] != NULL; i++) {
        if (strcmp(dict[i][0], key) == 0) {
            return dict[i][1];
        }
    }
    return key;
}

void select_language(void) {
    clear();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int center_x = (max_x - 40) / 2;

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
