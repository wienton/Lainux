#ifndef KEXEC_H
#define KEXEC_H

typedef struct {
    const char *kernel_path;
    const char *initrd_path;
    const char *cmdline;
} KexecConfig;

static long kexec_file_load(int kernel_fd, int initrd_fd, const char *cmdline, unsigned long flags);
int kexec_execute(KexecConfig *config);

#endif // kexec
