#define _GNU_SOURCE

#ifndef LINUX_REBOOT_CMD_KEXEC
#define LINUX_REBOOT_CMD_KEXEC 0x45584543
#endif

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <linux/kexec.h>
#include <sys/reboot.h>
#include <errno.h>
#include <string.h>

#include "kexec.h"


// interface by syscalls kexec file load
// this is call parse kernel config
static long kexec_file_load(int kernel_fd, int initrd_fd, const char *cmdline, unsigned long flags) {
    return syscall(SYS_kexec_file_load, kernel_fd, initrd_fd,
                   (unsigned long)strlen(cmdline), cmdline, flags);
}

int kexec_execute(KexecConfig *config) {
    if (!config || !config->kernel_path) return -1;

    printf("LainuxOS KEXEC: loading kernel %s...\n", config->kernel_path);

    int kernel_fd = open(config->kernel_path, O_RDONLY);
    if (kernel_fd < 0) {
        perror("error open kernel");
        return -1;
    }

    int initrd_fd = -1;
    if (config->initrd_path) {
        initrd_fd = open(config->initrd_path, O_RDONLY);
        if (initrd_fd < 0) {
            perror("error open initrd");
            close(kernel_fd);
            return -1;
        }
    }

    // load kernel in memory
    // flag 0 - default loading
    if (kexec_file_load(kernel_fd, initrd_fd, config->cmdline, 0) != 0) {
        fprintf(stderr, "kexec_file_load failed: %s\n", strerror(errno));
        close(kernel_fd);
        if (initrd_fd >= 0) close(initrd_fd);
        return -1;
    }

    close(kernel_fd);
    if (initrd_fd >= 0) close(initrd_fd);

    printf("LainuxOS KEXEC: Kernel Ready. Up...\n");

    // down up cash from disk
    sync();

    // reload kernel
    if (reboot(LINUX_REBOOT_CMD_KEXEC) == -1) {
        perror("reboot(KEXEC) failed");
        return -1;
    }

    return 0;
}


int main() {
    KexecConfig cfg = {
        .kernel_path = "/mnt/boot/vmlinuz-linux",
        .initrd_path = "/mnt/boot/initramfs-linux.img",
        .cmdline = "root=/dev/sda2 rw quiet"
    };
    kexec_execute(&cfg);
    return 0;
}











