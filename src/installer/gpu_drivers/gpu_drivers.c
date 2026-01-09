#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "gpu_drivers.h"

static int read_file(const char* path, char* buf, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, size - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    if (buf[n-1] == '\n') buf[n-1] = '\0';
    return 0;
}
// print gpu info
const gpu_info_t* detect_gpu() {
    static gpu_info_t info;
    char vendor_id[16];
    
    const char* paths[] = {
        "/sys/class/drm/card0/device/vendor",
        "/sys/class/drm/card1/device/vendor",
        NULL
    };
    
    for (int i = 0; paths[i]; i++) {
        if (read_file(paths[i], vendor_id, sizeof(vendor_id)) == 0) {
            unsigned int id;
            if (sscanf(vendor_id, "%x", &id) == 1) {
                switch (id) {
                    case 0x8086:
                        info.vendor = "Intel";
                        info.driver_package = "xf86-video-intel";
                        info.kernel_modules = "i915";
                        return &info;
                    case 0x1002:
                        info.vendor = "AMD";
                        info.driver_package = "xf86-video-amdgpu";
                        info.kernel_modules = "amdgpu";
                        return &info;
                    case 0x10de:
                        info.vendor = "NVIDIA";
                        info.driver_package = "nvidia";
                        info.kernel_modules = "nvidia";
                        return &info;
                }
            }
        }
    }
    return NULL;
}