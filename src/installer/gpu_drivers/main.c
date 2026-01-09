// test file for gpu_driver.c function

#include "gpu_drivers.h"
#include <stdio.h>

int main() {
    const gpu_info_t *gpu = detect_gpu();
    if (!gpu) {
      printf("GPU detection failed\n");
      return 1;
    }
    printf("log GPU info\n");
    printf("GPU Vendor: %s\n", gpu->vendor);
    printf("Driver Package: %s\n", gpu->driver_package);
    printf("Kernel Modules: %s\n", gpu->kernel_modules);
    return 0;
}