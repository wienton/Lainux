#ifndef GPU_DRIVERS_H
#define GPU_DRIVERS_H

typedef struct {
  const char *vendor;
  const char *driver_package;
  const char *kernel_modules;
} gpu_info_t;

const gpu_info_t *detect_gpu();

#endif