#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "system_check.h"
#include "../utils/log_message.h"
#include "../utils/run_command.h"

#include "../include/installer.h"

// Enhanced dependency check with package manager detection
int check_dependencies() {
    const char *essential_tools[] = {
        "arch-chroot", "pacstrap", "mkfs.fat", "mkfs.ext4",
        "sgdisk", "mount", "umount", "wget", "curl", "grub-install",
        "lsblk", "genfstab", "blkid", "partprobe", NULL
    };

    int missing = 0;
    char pkg_manager[32] = "";

    // Detect package manager
    if (file_exists("/usr/bin/pacman")) strcpy(pkg_manager, "pacman");
    else if (file_exists("/usr/bin/apt-get")) strcpy(pkg_manager, "apt");
    else if (file_exists("/usr/bin/dnf")) strcpy(pkg_manager, "dnf");
    else if (file_exists("/usr/bin/yum")) strcpy(pkg_manager, "yum");
    else if (file_exists("/usr/bin/zypper")) strcpy(pkg_manager, "zypper");

    // Check essential tools
    for (int i = 0; essential_tools[i] != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "command -v %s > /dev/null 2>&1", essential_tools[i]);
        if (system(cmd) != 0) {
            log_message("Missing: %s", essential_tools[i]);
            missing++;
        }
    }

    if (missing > 0) {
        log_message("Installing missing dependencies...");

        if (strcmp(pkg_manager, "pacman") == 0) {
            run_command("pacman -Sy --noconfirm --needed arch-install-scripts dosfstools e2fsprogs gptfdisk grub efibootmgr", 1);
        } else if (strcmp(pkg_manager, "apt") == 0) {
            run_command("apt-get update && apt-get install -y arch-install-scripts dosfstools e2fsprogs gdisk grub-efi-amd64", 1);
        } else if (strcmp(pkg_manager, "dnf") == 0 || strcmp(pkg_manager, "yum") == 0) {
            run_command("dnf install -y arch-install-scripts dosfstools e2fsprogs gdisk grub2-efi-x64", 1);
        }

        // Verify installation
        for (int i = 0; essential_tools[i] != NULL; i++) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "command -v %s > /dev/null 2>&1", essential_tools[i]);
            if (system(cmd) != 0) {
                log_message("Failed to install: %s", essential_tools[i]);
            }
        }
    }

    return (missing == 0);
}



// Check filesystem health
int check_filesystem(const char *path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "fsck -n %s > /dev/null 2>&1", path);
    return (system(cmd) == 0);
}


// Verify EFI support
int verify_efi() {
    if (file_exists("/sys/firmware/efi")) {
        return 1;
    }

    // Check via efibootmgr
    int result = run_command("efibootmgr > /dev/null 2>&1", 0);
    return (result == 0 || result == 1);  // Exit code 1 means no entries but EFI present
}



// Enhanced network check with multiple endpoints
int check_network() {
    const char *endpoints[] = {
        "8.8.8.8",      // Google DNS
        "1.1.1.1",      // Cloudflare DNS
        "archlinux.org",
        "google.com",
        NULL
    };

    char cmd[256];
    for (int i = 0; endpoints[i] != NULL; i++) {
        if (strchr(endpoints[i], '.') && !strchr(endpoints[i], ' ')) {
            snprintf(cmd, sizeof(cmd), "ping -c 1 -W 2 %s > /dev/null 2>&1", endpoints[i]);
            if (system(cmd) == 0) {
                return 1;
            }
        }
    }

    // Try curl as fallback
    strcpy(cmd, "curl -s --connect-timeout 3 --max-time 5 https://checkip.amazonaws.com > /dev/null 2>&1");
    return (system(cmd) == 0);
}



// Enhanced file existence check with stat details
int file_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }

    // Additional verification
    if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            close(fd);
            return 1;
        }
        return 0;
    }

    return 1;
}



// Get available space in MB
long get_available_space(const char *path) {
    struct statvfs st;
    if (statvfs(path, &st) == 0) {
        return (st.f_bavail * st.f_frsize) / (1024 * 1024);
    }
    return 0;
}



// Check system requirements
void check_system_requirements() {
    SystemInfo info;
    get_system_info(&info);

    clear();

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(2, 5, "SYSTEM REQUIREMENTS CHECK");
    attroff(A_BOLD | COLOR_PAIR(1));

    mvprintw(4, 5, "CPU Cores: %d/%d", info.avail_cores, info.total_cores);
    mvprintw(5, 5, "RAM: %ld/%ld MB", info.avail_ram, info.total_ram);
    mvprintw(6, 5, "Architecture: %s", info.arch);
    mvprintw(7, 5, "Kernel: %s", info.kernel);

    // Check minimum requirements
    int meets_requirements = 1;

    if (info.total_ram < 1024) {  // Less than 1GB RAM
        attron(COLOR_PAIR(3));
        mvprintw(9, 5, "WARNING: Minimum 1GB RAM recommended");
        attroff(COLOR_PAIR(3));
        meets_requirements = 0;
    }

    if (info.avail_cores < 2) {   // Less than 2 cores
        attron(COLOR_PAIR(3));
        mvprintw(10, 5, "WARNING: Dual-core CPU recommended");
        attroff(COLOR_PAIR(3));
        meets_requirements = 0;
    }

    // Check disk space in /tmp
    long free_space = get_available_space("/tmp");
    mvprintw(11, 5, "Available space in /tmp: %ld MB", free_space);

    if (free_space < 2048) {  // Less than 2GB
        attron(COLOR_PAIR(3));
        mvprintw(12, 5, "WARNING: At least 2GB free space required in /tmp");
        attroff(COLOR_PAIR(3));
        meets_requirements = 0;
    }

    if (meets_requirements) {
        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(14, 5, "✓ System meets minimum requirements");
        attroff(COLOR_PAIR(2) | A_BOLD);
    } else {
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(14, 5, "⚠ System may not perform optimally");
        attroff(COLOR_PAIR(3) | A_BOLD);
    }

    mvprintw(16, 5, "Press any key to continue...");
    refresh();
    getch();
}



// Enhanced hardware information display
void show_hardware_info() {
    SystemInfo sys_info;
    get_system_info(&sys_info);

    char cpu_info[128], memory_info[32], gpu_info[128], storage_info[64];
    get_hardware_details(cpu_info, memory_info, gpu_info, storage_info);

    clear();

    // Title with border
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(1, 5, "╔══════════════════════════════════════════════════════╗");
    mvprintw(2, 5, "║               HARDWARE INFORMATION                  ║");
    mvprintw(3, 5, "╚══════════════════════════════════════════════════════╝");
    attroff(A_BOLD | COLOR_PAIR(1));

    // System overview
    attron(A_BOLD);
    mvprintw(5, 10, "System Overview:");
    attroff(A_BOLD);

    mvprintw(6, 15, "Hostname: ");
    attron(COLOR_PAIR(2));
    printw("%s", sys_info.hostname);
    attroff(COLOR_PAIR(2));

    mvprintw(7, 15, "Architecture: ");
    attron(COLOR_PAIR(2));
    printw("%s", sys_info.arch);
    attroff(COLOR_PAIR(2));

    mvprintw(8, 15, "Kernel: ");
    attron(COLOR_PAIR(2));
    printw("%s", sys_info.kernel);
    attroff(COLOR_PAIR(2));

    // CPU Information
    attron(A_BOLD);
    mvprintw(10, 10, "CPU:");
    attroff(A_BOLD);
    mvprintw(10, 25, "%s", cpu_info);
    mvprintw(11, 25, "Cores: %d physical, %d logical", sys_info.avail_cores, sys_info.total_cores);

    // Memory Information
    attron(A_BOLD);
    mvprintw(13, 10, "Memory:");
    attroff(A_BOLD);
    mvprintw(13, 25, "%s RAM", memory_info);
    mvprintw(14, 25, "Available: %ld MB / %ld MB", sys_info.avail_ram, sys_info.total_ram);

    // GPU Information
    attron(A_BOLD);
    mvprintw(16, 10, "Graphics:");
    attroff(A_BOLD);
    mvprintw(16, 25, "%s", gpu_info);

    // Storage Information
    attron(A_BOLD);
    mvprintw(18, 10, "Storage:");
    attroff(A_BOLD);
    mvprintw(18, 25, "%s", storage_info);

    // Advanced Information
    attron(A_BOLD);
    mvprintw(20, 10, "Advanced:");
    attroff(A_BOLD);

    // Check virtualization support
    FILE* fp = popen("grep -E '(vmx|svm)' /proc/cpuinfo 2>/dev/null | head -1", "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp)) {
            mvprintw(21, 15, "Virtualization: ");
            attron(COLOR_PAIR(2));
            printw("Supported (KVM available)");
            attroff(COLOR_PAIR(2));
        } else {
            mvprintw(21, 15, "Virtualization: ");
            attron(COLOR_PAIR(3));
            printw("Not available");
            attroff(COLOR_PAIR(3));
        }
        pclose(fp);
    }

    // UEFI/BIOS detection
    if (verify_efi()) {
        mvprintw(22, 15, "Firmware: ");
        attron(COLOR_PAIR(2));
        printw("UEFI");
        attroff(COLOR_PAIR(2));
    } else {
        mvprintw(22, 15, "Firmware: ");
        attron(COLOR_PAIR(4));
        printw("Legacy BIOS");
        attroff(COLOR_PAIR(4));
    }

    // Show uptime
    fp = popen("uptime -p 2>/dev/null || uptime 2>/dev/null", "r");
    if (fp) {
        char uptime[64];
        if (fgets(uptime, sizeof(uptime), fp)) {
            uptime[strcspn(uptime, "\n")] = 0;
            mvprintw(23, 15, "Uptime: %s", uptime);
        }
        pclose(fp);
    }

    // Show load average
    fp = popen("cat /proc/loadavg | cut -d' ' -f1-3", "r");
    if (fp) {
        char load[32];
        if (fgets(load, sizeof(load), fp)) {
            load[strcspn(load, "\n")] = 0;
            mvprintw(24, 15, "Load avg: %s", load);
        }
        pclose(fp);
    }

    // Footer
    attron(COLOR_PAIR(7) | A_BOLD);
    mvprintw(26, 5, "Press any key to continue...");
    attroff(COLOR_PAIR(7) | A_BOLD);

    refresh();
    getch();
}
