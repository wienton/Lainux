#ifndef SYSTEM_CHEKS_H
#define SYSTEM_CHEKS_H

int check_dependencies(void);
int verify_efi(void);
int check_network(void);
int file_exists(const char *path);
int check_filesystem(const char *path);
long get_available_space(const char *path);
void check_system_requirements();

#endif
