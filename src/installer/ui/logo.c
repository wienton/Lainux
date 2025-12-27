/**
 * @file logo.c
 * @author wienton, lainux development laboratory
 * @brief logotype for installer, using in ../installer.c
 * @version 0.1
 * @date 2025-12-25
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "../include/installer.h"
#include <ncurses.h>
#include <string.h>

// Show Lainux logo with enhanced design
void show_logo() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int start_x = (max_x - 62) / 2; // length strings logo

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(1, start_x, "╔══════════════════════════════════════════════════════════════╗");
    mvprintw(2, start_x, "║                                                              ║");
    mvprintw(3, start_x, "║      ██╗      █████╗ ██╗███╗   ██╗██╗   ██╗██╗  ██╗          ║");
    mvprintw(4, start_x, "║      ██║     ██╔══██╗██║████╗  ██║██║   ██║╚██╗██╔╝          ║");
    mvprintw(5, start_x, "║      ██║     ███████║██║██╔██╗ ██║██║   ██║ ╚███╔╝           ║");
    mvprintw(6, start_x, "║      ██║     ██╔══██║██║██║╚██╗██║██║   ██║ ██╔██╗           ║");
    mvprintw(7, start_x, "║      ███████╗██║  ██║██║██║ ╚████║╚██████╔╝██╔╝ ██╗          ║");
    mvprintw(8, start_x, "║      ╚══════╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝  ╚═╝          ║");
    mvprintw(9, start_x, "║                                                              ║");
    mvprintw(10, start_x, "╚══════════════════════════════════════════════════════════════╝");
    attroff(A_BOLD | COLOR_PAIR(1));

    // center
    int slogan1_x = (max_x - (int)strlen("Development Laboratory")) / 2;
    int slogan2_x = (max_x - (int)strlen("Simplicity in design, security in execution")) / 2;
    int slogan3_x = (max_x - (int)strlen("Minimalism with purpose, freedom with responsibility")) / 2;

    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(12, slogan1_x, "Development Laboratory");
    attroff(COLOR_PAIR(2) | A_BOLD);

    attron(COLOR_PAIR(7));
    mvprintw(13, slogan2_x, "Simplicity in design, security in execution");
    mvprintw(14, slogan3_x, "Minimalism with purpose, freedom with responsibility");
    attroff(COLOR_PAIR(7));
}
