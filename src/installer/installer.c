/*


Lainux Installer, developed by the Lainux Development Lab team. Lead developer: Wienton.

Features:
1. Installation, mounting, and writing to disk
2. Internet connection check
3. Installation and creation of a configuration file on a VM (QEMU).
4. Ncurses graphical TUI interface.

Almost complete localization (Russian and English languages).

Project information and support:

@lainuxorg - global
@lainuxos - Russian portal.

Installation Engine



*/



#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <fcntl.h>

// ui, general function for UI
#include "kexec/kexec.h"
#include "ui/ui.h"
#include "utils/log_message.h"
// start command, system utils
#include "utils/run_command.h"
// system check hardware && requirements
#include "system/system_check.h"
// general installer function prototype and data struct
#include "include/installer.h"
// ncurses lib for good working with Ncurses UI
#include "../../libc/ncurses/ncurses_util.h"

/* Constants */
#define MIN_ROOT_SIZE_GB 15
#define MIN_EFI_SIZE_MB 512
#define BOOTLOADER_ID "lainux"
#define MAX_RETRIES 5
#define DEVICE_WAIT_TIME 2

/* Installation state */
volatile int install_running = 0;
static WindowCtx main_win;
WINDOW* log_win;
WINDOW* status_win;

/* Assembly-optimized memory operations */
static inline void secure_zero(void *ptr, size_t len) {
    volatile char *p = (volatile char *)ptr;
    while (len--) *p++ = 0;
}

static inline int atomic_test_and_set(volatile int *ptr) {
    int old;
    __asm__ __volatile__ (
        "lock xchg %0, %1"
        : "=r" (old), "+m" (*ptr)
        : "0" (1)
        : "memory"
    );
    return old;
}

/* System detection */
static int detect_boot_mode(void) {
    struct stat st;

    if (stat("/sys/firmware/efi", &st) == 0) {
        return 1; /* UEFI */
    }

    if (stat("/proc/device-tree", &st) == 0) {
        DIR *dir = opendir("/proc/device-tree");
        if (dir) {
            struct dirent *ent;
            while ((ent = readdir(dir))) {
                if (strstr(ent->d_name, "uefi") || strstr(ent->d_name, "efi")) {
                    closedir(dir);
                    return 1;
                }
            }
            closedir(dir);
        }
    }

    return 0; /* BIOS */
}

/* Safe disk path construction */
static int build_disk_path(char *dest, size_t size, const char *disk) {
    if (!dest || !disk || size < 32) return -1;

    const char *prefix = "/dev/";
    size_t prefix_len = strlen(prefix);
    size_t disk_len = strlen(disk);

    if (prefix_len + disk_len + 1 > size) return -1;

    memcpy(dest, prefix, prefix_len);
    memcpy(dest + prefix_len, disk, disk_len);
    dest[prefix_len + disk_len] = '\0';

    return 0;
}

/* Smart partition name detection */
static void get_partition_names(const char *dev_path, char *efi_part, char *root_part, size_t size) {
    if (strncmp(dev_path, "/dev/nvme", 9) == 0 ||
        strncmp(dev_path, "/dev/mmcblk", 11) == 0) {
        snprintf(efi_part, size, "%sp1", dev_path);
        snprintf(root_part, size, "%sp2", dev_path);
    } else if (strncmp(dev_path, "/dev/vd", 7) == 0) {
        snprintf(efi_part, size, "%s1", dev_path);
        snprintf(root_part, size, "%s2", dev_path);
    } else {
        snprintf(efi_part, size, "%s1", dev_path);
        snprintf(root_part, size, "%s2", dev_path);
    }
}

/* Safe file existence check with timeout */
static int safe_file_exists(const char *path, int max_wait) {
    struct stat st;
    int attempts = 0;

    while (attempts < max_wait) {
        if (stat(path, &st) == 0) {
            return 1;
        }
        sleep(500000); /* 500ms */
        attempts++;
    }
    return 0;
}

/* Secure partition creation */
static int create_secure_partitions(const char *disk, int boot_mode) {
    char dev_path[32];
    char cmd[256];

    if (build_disk_path(dev_path, sizeof(dev_path), disk) != 0) {
        return -1;
    }

    log_message("Creating partitions on %s", dev_path);

    /* Clear partition table */
    snprintf(cmd, sizeof(cmd), "sgdisk -Z %s", dev_path);
    if (run_command(cmd, 1) != 0) {
        snprintf(cmd, sizeof(cmd), "dd if=/dev/zero of=%s bs=512 count=1", dev_path);
        run_command(cmd, 1);
    }

    /* Wait for device reset */
    sleep(1);
    run_command("udevadm settle 2>/dev/null", 0);

    if (boot_mode) {
        /* UEFI: GPT with ESP */
        snprintf(cmd, sizeof(cmd),
            "sgdisk -n 1::+%dM -t 1:ef00 -c 1:lainux-efi %s",
            MIN_EFI_SIZE_MB, dev_path);
        if (run_command(cmd, 1) != 0) return -1;

        snprintf(cmd, sizeof(cmd),
            "sgdisk -n 2:: -t 2:8300 -c 2:lainux-root %s",
            dev_path);
        if (run_command(cmd, 1) != 0) return -1;
    } else {
        /* BIOS: MBR with boot flag */
        snprintf(cmd, sizeof(cmd),
            "fdisk %s << EOF\no\nn\np\n1\n\n+%dM\na\n1\nn\np\n2\n\n\nw\nEOF",
            dev_path, MIN_EFI_SIZE_MB);
        if (run_command(cmd, 1) != 0) return -1;
    }

    /* Force kernel to re-read partition table */
    snprintf(cmd, sizeof(cmd), "partprobe %s 2>/dev/null", dev_path);
    run_command(cmd, 0);

    run_command("udevadm settle 2>/dev/null", 0);
    sleep(DEVICE_WAIT_TIME);

    return 0;
}

/* Safe formatting with fallback */
static int format_partitions_safe(const char *efi_part, const char *root_part) {
    char cmd[512];
    int retry;

    /* Format EFI partition */
    log_message("Formatting %s as FAT32", efi_part);
    for (retry = 0; retry < MAX_RETRIES; retry++) {
        snprintf(cmd, sizeof(cmd), "mkfs.fat -F32 -n LAINUX_EFI %s", efi_part);
        if (run_command(cmd, 0) == 0) break;

        snprintf(cmd, sizeof(cmd), "mkfs.vfat -F32 %s", efi_part);
        if (run_command(cmd, 0) == 0) break;

        if (retry < MAX_RETRIES - 1) {
            log_message("Retrying EFI format...");
            sleep(1);
        }
    }

    if (retry == MAX_RETRIES) {
        log_message("Failed to format EFI partition");
        return -1;
    }

    /* Format root partition */
    log_message("Formatting %s as ext4", root_part);
    for (retry = 0; retry < MAX_RETRIES; retry++) {
        snprintf(cmd, sizeof(cmd), "mkfs.ext4 -F -L lainux_root %s", root_part);
        if (run_command(cmd, 0) == 0) break;

        snprintf(cmd, sizeof(cmd), "mkfs.ext4 %s", root_part);
        if (run_command(cmd, 0) == 0) break;

        if (retry < MAX_RETRIES - 1) {
            log_message("Retrying root format...");
            sleep(1);
        }
    }

    if (retry == MAX_RETRIES) {
        log_message("Failed to format root partition");
        return -1;
    }

    return 0;
}

/* Safe mount with verification */
static int safe_mount(const char *source, const char *target, const char *fstype) {
    struct stat st;
    int attempts = 0;

    if (stat(source, &st) != 0) {
        log_message("Source %s does not exist", source);
        return -1;
    }

    if (stat(target, &st) != 0) {
        if (mkdir(target, 0755) != 0) {
            log_message("Failed to create mount point %s", target);
            return -1;
        }
    }

    while (attempts < MAX_RETRIES) {
        if (mount(source, target, fstype, 0, NULL) == 0) {
            /* Verify mount succeeded */
            if (stat(target, &st) == 0) {
                return 0;
            }
        }

        if (attempts < MAX_RETRIES - 1) {
            sleep(500000);
            attempts++;
        }
    }

    log_message("Failed to mount %s to %s", source, target);
    return -1;
}

/* Universal bootloader installation */
static int install_universal_bootloader(const char *disk, int boot_mode, const char *root_mount) {
    char dev_path[32];
    char cmd[512];

    if (build_disk_path(dev_path, sizeof(dev_path), disk) != 0) {
        return -1;
    }

    log_message("Installing bootloader for %s mode", boot_mode ? "UEFI" : "BIOS");

    if (boot_mode) {
        /* UEFI bootloader */
        if (file_exists("/usr/bin/grub-install")) {
            snprintf(cmd, sizeof(cmd),
                "grub-install --target=x86_64-efi --efi-directory=%s/boot --bootloader-id=%s --recheck %s",
                root_mount, BOOTLOADER_ID, dev_path);
        } else if (file_exists("/usr/bin/systemd-bootctl")) {
            snprintf(cmd, sizeof(cmd),
                "bootctl install --esp-path=%s/boot --boot-path=%s/boot",
                root_mount, root_mount);
        } else {
            log_message("No UEFI bootloader found");
            return -1;
        }
    } else {
        /* BIOS bootloader */
        snprintf(cmd, sizeof(cmd),
            "grub-install --target=i386-pc --recheck --boot-directory=%s/boot %s",
            root_mount, dev_path);
    }

    if (run_command(cmd, 1) != 0) {
        log_message("Bootloader installation failed");
        return -1;
    }

    /* Generate bootloader config */
    if (file_exists("/usr/bin/grub-mkconfig")) {
        snprintf(cmd, sizeof(cmd),
            "grub-mkconfig -o %s/boot/grub/grub.cfg",
            root_mount);
        run_command(cmd, 1);
    }

    return 0;
}

/* Secure system configuration */
static int configure_system_secure(const char *root_mount) {
    char cmd[512];
    FILE *fp;

    /* Basic system configuration */
    snprintf(cmd, sizeof(cmd), "arch-chroot %s ln -sf /usr/share/zoneinfo/UTC /etc/localtime", root_mount);
    run_command(cmd, 0);

    snprintf(cmd, sizeof(cmd), "arch-chroot %s hwclock --systohc", root_mount);
    run_command(cmd, 0);

    /* Locale */
    fp = fopen("/mnt/etc/locale.gen", "w");
    if (fp) {
        fprintf(fp, "en_US.UTF-8 UTF-8\n");
        fprintf(fp, "en_US ISO-8859-1\n");
        fclose(fp);
    }

    snprintf(cmd, sizeof(cmd), "arch-chroot %s locale-gen", root_mount);
    run_command(cmd, 0);

    fp = fopen("/mnt/etc/locale.conf", "w");
    if (fp) {
        fprintf(fp, "LANG=en_US.UTF-8\n");
        fclose(fp);
    }

    /* Hostname */
    fp = fopen("/mnt/etc/hostname", "w");
    if (fp) {
        fprintf(fp, "lainux\n");
        fclose(fp);
    }

    /* Hosts */
    fp = fopen("/mnt/etc/hosts", "a");
    if (fp) {
        fprintf(fp, "127.0.1.1 lainux.localdomain lainux\n");
        fclose(fp);
    }

    /* Users */
    snprintf(cmd, sizeof(cmd), "echo 'root:lainux' | arch-chroot %s chpasswd", root_mount);
    run_command(cmd, 0);

    snprintf(cmd, sizeof(cmd), "arch-chroot %s useradd -m -G wheel -s /bin/bash lainux", root_mount);
    run_command(cmd, 0);

    snprintf(cmd, sizeof(cmd), "echo 'lainux:lainux' | arch-chroot %s chpasswd", root_mount);
    run_command(cmd, 0);

    /* Sudo */
    fp = fopen("/mnt/etc/sudoers.d/wheel", "w");
    if (fp) {
        fprintf(fp, "%%wheel ALL=(ALL) ALL\n");
        fclose(fp);
        snprintf(cmd, sizeof(cmd), "chmod 440 %s/etc/sudoers.d/wheel", root_mount);
        run_command(cmd, 0);
    }

    return 0;
}

/* Main installation procedure */
void perform_installation(const char *disk) {
    if (atomic_test_and_set(&install_running)) {
        log_message("Installation already running");
        return;
    }

    char dev_path[32];
    char efi_part[32], root_part[32];
    int boot_mode;

    /* Detect boot mode */
    boot_mode = detect_boot_mode();
    log_message("Detected %s boot mode", boot_mode ? "UEFI" : "BIOS");

    /* Build device path */
    if (build_disk_path(dev_path, sizeof(dev_path), disk) != 0) {
        log_message("Invalid disk name");
        install_running = 0;
        return;
    }

    /* Get partition names */
    get_partition_names(dev_path, efi_part, root_part, sizeof(efi_part));

    /* Pre-installation checks */
    if (!check_dependencies()) {
        log_message("Dependency check failed");
        install_running = 0;
        return;
    }

    if (!check_network()) {
        log_message("Network check failed");
        if (!confirm_action("Continue without network?", "CONTINUE")) {
            install_running = 0;
            return;
        }
    }

    /* Disk space check */
    long available = get_available_space("/");
    long required = MIN_ROOT_SIZE_GB * 1024;

    if (available < required) {
        log_message("Insufficient space: %ldMB < %ldMB", available, required);
        install_running = 0;
        return;
    }

    /* Partitioning */
    log_message("Creating partitions...");
    if (create_secure_partitions(disk, boot_mode) != 0) {
        log_message("Partitioning failed");
        install_running = 0;
        return;
    }

    /* Wait for partitions */
    if (!safe_file_exists(efi_part, 10) || !safe_file_exists(root_part, 10)) {
        log_message("Partitions not detected");
        install_running = 0;
        return;
    }

    /* Formatting */
    log_message("Formatting partitions...");
    if (format_partitions_safe(efi_part, root_part) != 0) {
        install_running = 0;
        return;
    }

    /* Mounting */
    log_message("Mounting filesystems...");

    /* Clean previous mounts */
    run_command("umount -R /mnt 2>/dev/null || true", 0);
    run_command("rmdir /mnt 2>/dev/null || true", 0);
    run_command("mkdir -p /mnt", 0);

    /* Mount root */
    if (safe_mount(root_part, "/mnt", "ext4") != 0) {
        log_message("Failed to mount root");
        install_running = 0;
        return;
    }

    /* Create and mount boot */
    run_command("mkdir -p /mnt/boot", 0);
    if (safe_mount(efi_part, "/mnt/boot", "vfat") != 0) {
        log_message("Failed to mount boot");
        install_running = 0;
        return;
    }

    /* Base system installation */
    log_message("Installing base system...");
    run_command("mkdir -p /mnt/var/cache/pacman/pkg", 0);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "pacstrap -K /mnt base linux linux-firmware");
    if (run_command(cmd, 1) != 0) {
        log_message("Base installation failed");
        install_running = 0;
        return;
    }

    /* Generate fstab */
    log_message("Generating fstab...");
    run_command("genfstab -U /mnt >> /mnt/etc/fstab", 1);

    /* System configuration */
    log_message("Configuring system...");
    configure_system_secure("/mnt");

    /* Install bootloader */
    log_message("Installing bootloader...");
    if (install_universal_bootloader(disk, boot_mode, "/mnt") != 0) {
        log_message("Bootloader installation failed");
    }

    /* Services */
    log_message("Enabling services...");
    snprintf(cmd, sizeof(cmd), "arch-chroot /mnt systemctl enable systemd-networkd systemd-resolved");
    run_command(cmd, 0);

    /* Cleanup */
    log_message("Cleaning up...");
    run_command("sync", 0);
    run_command("umount -R /mnt", 0);

    log_message("Installation complete!");

    /* Secure cleanup */
    secure_zero(dev_path, sizeof(dev_path));
    secure_zero(efi_part, sizeof(efi_part));
    secure_zero(root_part, sizeof(root_part));

     /* Services */
    log_message("Try enabling services...");
    snprintf(cmd, sizeof(cmd), "arch-chroot /mnt systemctl enable systemd-networkd systemd-resolved");
    run_command(cmd, 0);

    log_message("Preparing kexec transition...");

    char cmdline[512];
    snprintf(cmdline, sizeof(cmdline), "root=%s rw init=/usr/lib/systemd/systemd", root_part);
    /* configuration for kexec func using struct */
    KexecConfig k_cfg = {
        .kernel_path = "/mnt/boot/vmlinuz-linux",
        .initrd_path = "/mnt/boot/initramfs-linux.img",
        .cmdline = cmdline,
    };

    /* Cleanup */
    log_message("Cleaning up...");
    run_command("sync", 0);

    /* look final  */
    show_summary(disk);

    if (confirm_action("Boot into the new system immediately (kexec)?", "YES")) {
        log_message("[Kexec]: Jumping to new kernel...");

        kexec_execute(&k_cfg);
        log_message(" Kexec failed, proceeding with standard cleanup. :( ");
    }

    run_command("umount -R /mnt", 0);
    log_message("Woow! Installation complete! :D ");

    /* Secure cleanup */
    secure_zero(dev_path, sizeof(dev_path));
    secure_zero(efi_part, sizeof(efi_part));
    secure_zero(root_part, sizeof(root_part));

    install_running = 0;
}
