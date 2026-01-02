#include <stdio.h>
#include <ncurses.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

// headers
#include "ui.h"
#include "../locale/lang.h"
#include "../utils/run_command.h"

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



// Draw progress bar
void draw_progress_bar(int y, int x, int width, float progress) {
    int bars = (int)(progress * width);

    attron(COLOR_PAIR(7));
    mvprintw(y, x, "[");

    attron(COLOR_PAIR(2));
    for (int i = 0; i < bars; i++) {
        addch('=');
    }

    attron(COLOR_PAIR(7));
    for (int i = bars; i < width; i++) {
        addch(' ');
    }
    addch(']');

    mvprintw(y, x + width + 2, "%3d%%", (int)(progress * 100));
    attroff(COLOR_PAIR(7));
}



// Show installation summary
void show_summary(const char *disk) {
    clear();

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int center_x = max_x / 2 - 25;

    attron(A_BOLD | COLOR_PAIR(1));

    mvprintw(3, center_x, "│         %s   │", get_text("INSTALL_COMPLETE"));
    attroff(A_BOLD | COLOR_PAIR(1));

    mvprintw(6, center_x + 5, "Lainux has been successfully installed!");

    mvprintw(8, center_x + 5, "Installation target:");
    attron(COLOR_PAIR(2));
    mvprintw(9, center_x + 5, "  /dev/%s", disk);
    attroff(COLOR_PAIR(2));

    mvprintw(11, center_x + 5, "Default credentials:");
    mvprintw(12, center_x + 10, "Username: root");
    mvprintw(13, center_x + 10, "Password: lainux");

    mvprintw(14, center_x + 10, "Username: lainux");
    mvprintw(15, center_x + 10, "Password: lainux");

    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(17, center_x + 5, "⚠ Remove installation media before rebooting!");
    attroff(COLOR_PAIR(3) | A_BOLD);

    mvprintw(19, center_x + 5, "Next steps:");
    mvprintw(20, center_x + 10, "1. Remove installation media");
    mvprintw(21, center_x + 10, "2. Reboot the system");
    mvprintw(22, center_x + 10, "3. Log in with credentials above");
    mvprintw(23, center_x + 10, "4. Run 'lainux-setup' for post-installation");

    mvprintw(25, center_x + 5, "Press R to reboot now");
    mvprintw(26, center_x + 5, "Press Q to shutdown installer");
    mvprintw(27, center_x + 5, "Press any other key to return to menu");

    while (1) {
        int ch = getch();
        if (ch == 'r' || ch == 'R') {
            if (confirm_action("Reboot system now?", "REBOOT")) {
                run_command("reboot", 0);
            }
            break;
        } else if (ch == 'q' || ch == 'Q') {
            if (confirm_action("Exit installer?", "EXIT")) {
                endwin();
                exit(0);
            }
        } else {
            break;
        }
    }
}
