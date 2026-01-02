#include <stdio.h>
#include <ncurses.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

// headers
#include "ui.h"

// Initialize ncurses with error handling
void init_ncurses() {
    setlocale(LC_ALL, "");

    if (initscr() == NULL) {
        fprintf(stderr, "Error initializing ncurses\n");
        exit(1);
    }

    start_color();
    use_default_colors();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);

    // Initialize color pairs
    init_pair(1, COLOR_CYAN, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_RED, -1);
    init_pair(4, COLOR_YELLOW, -1);
    init_pair(5, COLOR_MAGENTA, -1);
    init_pair(6, COLOR_BLUE, -1);
    init_pair(7, COLOR_WHITE, -1);
    init_pair(8, COLOR_BLACK, COLOR_CYAN);    // Selected item
    init_pair(9, COLOR_BLACK, COLOR_RED);     // Error background
    init_pair(10, COLOR_BLACK, COLOR_GREEN);  // Success background
}



// Enhanced confirmation with timeout
int confirm_action(const char *question, const char *required_input) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    WINDOW *confirm_win = newwin(8, max_x - 20, max_y/2 - 4, 10);
    box(confirm_win, 0, 0);
    mvwprintw(confirm_win, 1, 2, "%s", question);
    mvwprintw(confirm_win, 2, 2, "Type '%s' to confirm (ESC to cancel):", required_input);
    wrefresh(confirm_win);

    char input[32] = {0};
    int pos = 0;
    int ch;

    flushinp();

    while (pos < (int)sizeof(input) - 1) {
        ch = wgetch(confirm_win);
        if (ch == ERR) continue; // timeout

        if (ch == 27) { // ESC
            delwin(confirm_win);
            return 0;
        }
        if (ch == '\n' || ch == '\r') { // Enter
            break;
        }
        if (ch == KEY_BACKSPACE || ch == 127) {
            if (pos > 0) {
                pos--;
                input[pos] = '\0';
                mvwaddch(confirm_win, 3, 4 + pos, ' ');
                wmove(confirm_win, 3, 4 + pos);
            }
        } else if (ch >= 32 && ch <= 126) { // printable ASCII
            input[pos++] = (char)ch;
            input[pos] = '\0';
            mvwaddch(confirm_win, 3, 4 + pos - 1, ch);
        }

        wrefresh(confirm_win);
    }

    delwin(confirm_win);
    return (strcmp(input, required_input) == 0);
}




// Display status message
void display_status(const char *message) {
    if (status_win) {
        wclear(status_win);
        box(status_win, 0, 0);
        mvwprintw(status_win, 1, 2, " STATUS: %s", message);
        wrefresh(status_win);
    }
}
