#ifndef TURBO_INSTALLER_H
#define TURBO_INSTALLER_H


#include <ncurses.h>

void perform_installation(const char *disk);


extern WINDOW *log_win;
extern WINDOW *status_win;

extern volatile  int install_running;

#endif
