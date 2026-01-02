#ifndef DISK_INFO_H
#define DISK_INFO_H

#include <string.h>


void get_target_disk(char *target, size_t size);
void show_disk_info();

void create_partitions(const char *disk);
int mount_with_retry(const char *source, const char *target,
                     const char *fstype, unsigned long flags);
int secure_wipe(const char *device);

#endif // disk info h
