#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include "information_installer.h"

void print_info_lainux()
{
    int x, y;
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(y/2 - 2, (x-30)/2, "ABOUT LAINUX INFORMATION PORTAL");
    attroff(COLOR_PAIR(1) | A_BOLD);
    
}