#ifndef UI_H
#define UI_H


#include <ncurses.h>

extern WINDOW *log_win;
extern WINDOW *status_win;

int confirm_action(const char *question, const char *required_input);
void init_ncurses();
void display_status(const char *message);

#endif
