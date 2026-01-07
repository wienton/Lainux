#ifndef KEXEC_H
#define KEXEC_H

typedef struct {
    const char *kernel_path;
    const char *initrd_path;
    const char *cmdline;
} KexecConfig;

#endif // kexec
