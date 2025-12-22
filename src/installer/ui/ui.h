#ifndef UI_H
#define UI_H

#include "system/system.h"

void draw_box(int starty, int startx, int height, int width);
void show_welcome_screen(void);
int show_main_menu(void);
int show_disk_picker(DiskInfo disks[], int count);
void show_progress(const char *message, int step, int total_steps);
int show_desktop_menu(void);
void show_completion_screen(int success);
void show_error_message(const char *msg);
int show_turbo_install_menu(DiskInfo disks[], int count);
void show_turbo_progress(const char *phase, int step, int total);

#endif