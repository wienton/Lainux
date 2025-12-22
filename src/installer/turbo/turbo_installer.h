#ifndef TURBO_INSTALLER_H
#define TURBO_INSTALLER_H

#include "../system/system.h"

// Turbo Install - одна кнопка, все автоматически
int turbo_install(DiskInfo *disk);

// Вспомогательные функции
int auto_detect_internet(void);
int auto_configure_system(void);
int auto_install_packages(void);

#endif