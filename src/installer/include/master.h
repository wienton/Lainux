#ifndef MASTER_H
#define MASTER_H

#include <stdbool.h>

void signal_handler(int sig);
void perform_installation(const char *disk);
void install_on_virtual_machine(void);
void show_hardware_info(void);
void check_system_requirements(void);
void get_system_info(void *info);
void get_hardware_details(void *info);
int download_file(const char *url, const char *dest);
char* select_iso_file(void);

#endif
