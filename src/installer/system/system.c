#include <stdio.h>
#include <string.h>
#include "system.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
/**
 * Retrieves list of available physical disks from system
 * Filters out virtual devices and partitions
 * Returns number of disks found
 */
int get_disk_list(DiskInfo disks[MAX_DISKS]) {
    FILE *fp = fopen("/proc/partitions", "r");
    if (fp == NULL) {
        printf("Cannot open /proc/partitions\n");
        return 0;
    }

    char line[256];
    int count = 0;

    // Skip header lines
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp) && count < MAX_DISKS) {
        unsigned int major, minor;
        long long blocks;
        char name[32];

        if (sscanf(line, "%u %u %lld %31s", &major, &minor, &blocks, name) == 4) {
            // Skip virtual devices
            if (strncmp(name, "loop", 4) == 0 ||
                strncmp(name, "ram", 3) == 0 ||
                strncmp(name, "dm-", 3) == 0 ||
                strncmp(name, "zram", 4) == 0) {
                continue;
            }

            // Extract base disk name (remove partition numbers)
            char base_name[32];
            strcpy(base_name, name);

            int len = strlen(base_name);
            while (len > 0 && base_name[len-1] >= '0' && base_name[len-1] <= '9') {
                len--;
            }
            base_name[len] = '\0';

            if (len == 0) continue;

            // Check if disk already added
            int already_added = 0;
            for (int i = 0; i < count; i++) {
                if (strcmp(disks[i].name, base_name) == 0) {
                    already_added = 1;
                    break;
                }
            }

            if (already_added) continue;

            // Add disk to list
            strncpy(disks[count].name, base_name, 19);
            disks[count].name[19] = '\0';

            // Convert blocks to GB
            double gb = (double)blocks / 1024.0 / 1024.0;
            if (gb < 0.1) continue;

            snprintf(disks[count].size, 19, "%.1f GB", gb);

            // Try to get disk model from sysfs
            char model_path[128];
            snprintf(model_path, sizeof(model_path), "/sys/block/%s/device/model", base_name);
            FILE *model_fp = fopen(model_path, "r");
            if (model_fp) {
                if (fgets(disks[count].model, 49, model_fp)) {
                    disks[count].model[strcspn(disks[count].model, "\n")] = 0;
                } else {
                    strcpy(disks[count].model, "Unknown");
                }
                fclose(model_fp);
            } else {
                strcpy(disks[count].model, "Unknown");
            }

            count++;
        }
    }

    fclose(fp);

    // Fallback method using lsblk if /proc/partitions fails
    if (count == 0) {
        fp = popen("lsblk -d -o NAME,SIZE,MODEL -n | grep -v '^loop' | grep -v '^sr'", "r");
        if (fp) {
            while (fgets(line, sizeof(line), fp) && count < MAX_DISKS) {
                char name[32], size[32], model[128];
                if (sscanf(line, "%s %s %[^\n]", name, size, model) >= 2) {
                    strncpy(disks[count].name, name, 19);
                    disks[count].name[19] = '\0';

                    strncpy(disks[count].size, size, 19);
                    disks[count].size[19] = '\0';

                    strncpy(disks[count].model, model, 49);
                    disks[count].model[49] = '\0';

                    count++;
                }
            }
            pclose(fp);
        }
    }

    return count;
}

/**
 * Checks internet connectivity by pinging Cloudflare DNS
 * Returns 1 if internet is available, 0 otherwise
 */
int check_internet() {
    int res = system("ping -c 1 1.1.1.1 > /dev/null 2>&1");
    return (res == 0);
}

/**
 * Wrapper for system() command execution
 * Provides centralized command execution
 */
int run_system_command(const char *cmd) {
    int res = system(cmd);
    return res;
}

/**
 * Prepares disk for installation:
 * - Validates disk exists
 * - Unmounts any mounted partitions
 * - Creates GPT partition table
 * - Creates EFI and root partitions
 */
int prepare_disk(const char *disk_name) {
    printf("\nPreparёing disk %s\n", disk_name);

    char cmd[512];
    char path[64];
    snprintf(path, sizeof(path), "/dev/%s", disk_name);


    // Verify disk exists
    if (access(path, F_OK) != 0) {
        printf("Disk not found: %s\n", path);
        system("lsblk -d -o NAME,SIZE,MODEL,TYPE | grep -v 'loop\\|rom\\|part'");
        return 1;
    }

    // Check if it's a whole disk (not a partition)
    FILE *fp = popen("lsblk -d -o NAME -n", "r");
    if (fp) {
        char found = 0;
        char line[32];
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = 0;
            if (strcmp(line, disk_name) == 0) {
                found = 1;
                break;
            }
        }
        pclose(fp);

        if (!found) {
            printf("%s is not a whole disk (might be a partition)\n", disk_name);
            return 1;
        }
    }

    // Final warning before destruction
    printf("\nWARNING: This will DESTROY ALL DATA on %s!\n", path);
    printf("Type 'YES' or 'Y/y' to continue: ");

    char confirm[10];
    if (fgets(confirm, sizeof(confirm), stdin)) {
        confirm[strcspn(confirm, "\n")] = 0;
        if (strcmp(confirm, "YES") != 0 || strcmp(confirm, "Y") != 0 || strcmp(confirm, "y") != 0) {
            printf("Installation cancelled.\n");
            return 1;
        }
    }

    // Unmount any existing partitions
    printf("\nUnmounting any partitions...\n");
    snprintf(cmd, sizeof(cmd), "umount /dev/%s* 2>/dev/null || true", disk_name);
    system(cmd);
    sleep(2);

    // Clear old partition table
    printf("Cleaning old partition table...\n");
    snprintf(cmd, sizeof(cmd), "wipefs -a %s 2>/dev/null || true", path);
    system(cmd);
    sleep(1);

    // Create GPT partition table
    printf("Creating GPT partition table...\n");
    snprintf(cmd, sizeof(cmd), "parted -s %s mklabel gpt", path);

    if (system(cmd) != 0) {
        printf("Failed to create GPT table\n");
        printf("Trying alternative method with sgdisk...\n");

        snprintf(cmd, sizeof(cmd), "sgdisk -Z %s", path);
        system(cmd);

        snprintf(cmd, sizeof(cmd), "sgdisk -o %s", path);
        if (system(cmd) != 0) {
            printf("All methods failed\n");
            return 1;
        }
    }

    sleep(1);

    // Create EFI partition (512MB)
    printf("Creating EFI partition (512MB)...\n");
    snprintf(cmd, sizeof(cmd), "parted -s %s mkpart ESP fat32 1MiB 513MiB", path);

    if (system(cmd) != 0) {
        printf("Failed to create EFI partition\n");
        printf("Trying with sgdisk...\n");

        snprintf(cmd, sizeof(cmd), "sgdisk -n 1:1MiB:513MiB -t 1:ef00 %s", path);
        if (system(cmd) != 0) {
            printf("All methods failed for EFI\n");
            return 1;
        }
    }

    // Set ESP flag for EFI partition
    printf("Setting ESP flag...\n");
    snprintf(cmd, sizeof(cmd), "parted -s %s set 1 esp on", path);
    system(cmd);

    // Create root partition (remaining space)
    printf("Creating root partition (rest of disk)...\n");
    snprintf(cmd, sizeof(cmd), "parted -s %s mkpart root ext4 513MiB 100%%", path);

    if (system(cmd) != 0) {
        printf("Failed to create root partition\n");
        printf("Trying with sgdisk...\n");

        snprintf(cmd, sizeof(cmd), "sgdisk -n 2:513MiB:0 -t 2:8304 %s", path);
        if (system(cmd) != 0) {
            printf("All methods failed for root\n");
            return 1;
        }
    }

    // Update kernel partition table
    system("partprobe 2>/dev/null || true");
    sleep(2);

    // Verify created partitions
    printf("\nVerifying partitions...\n");
    snprintf(cmd, sizeof(cmd), "lsblk -f %s", path);
    system(cmd);

    printf("\nDisk preparation completed successfully!\n");
    return 0;
}

/**
 * Formats and mounts partitions:
 * - Formats EFI as FAT32
 * - Formats root as ext4
 * - Mounts root to /mnt and EFI to /mnt/boot
 */
int format_and_mount(const char *disk_name) {
    char cmd[512];
    char part_efi[64], part_root[64];

    // Determine partition names based on disk type
    if (strstr(disk_name, "nvme")) {
        snprintf(part_efi, 64, "/dev/%sp1", disk_name);
        snprintf(part_root, 64, "/dev/%sp2", disk_name);
    } else {
        snprintf(part_efi, 64, "/dev/%s1", disk_name);
        snprintf(part_root, 64, "/dev/%s2", disk_name);
    }

    // Format EFI partition as FAT32
    snprintf(cmd, 512, "mkfs.fat -F32 %s", part_efi);
    if(run_system_command(cmd) != 0) return 1;

    // Format root partition as ext4
    snprintf(cmd, 512, "mkfs.ext4 -F %s", part_root);
    if(run_system_command(cmd) != 0) return 2;

    // Unmount any existing mounts at /mnt
    run_system_command("umount -R /mnt 2>/dev/null");

    // Mount root partition
    snprintf(cmd, 512, "mount %s /mnt", part_root);
    if(run_system_command(cmd) != 0) return 3;

    // Create boot directory and mount EFI
    run_system_command("mkdir -p /mnt/boot");
    snprintf(cmd, 512, "mount %s /mnt/boot", part_efi);
    if(run_system_command(cmd) != 0) return 4;

    return 0;
}

/**
 * Installs base Arch Linux system using pacstrap
 * Includes kernel, firmware, and development tools
 */
int install_base() {
    return run_system_command("pacstrap -K /mnt base linux linux-firmware base-devel");
}

/**
 * Installs GRUB bootloader
 * Auto-detects UEFI or BIOS mode
 */
int install_grub(const char *disk_name) {
    printf("\nInstalling GRUB Bootloader\n");

    char cmd[512];
    char disk_path[64];
    snprintf(disk_path, sizeof(disk_path), "/dev/%s", disk_name);

    // Detect boot mode (UEFI or BIOS)
    if (access("/sys/firmware/efi", F_OK) == 0) {
        printf("UEFI mode detected\n");
        // UEFI installation
        snprintf(cmd, sizeof(cmd),
                 "arch-chroot /mnt grub-install --target=x86_64-efi "
                 "--efi-directory=/boot/efi --bootloader-id=LAINUX --recheck %s",
                 disk_path);
    } else {
        printf("BIOS mode detected\n");
        // BIOS installation
        snprintf(cmd, sizeof(cmd),
                 "arch-chroot /mnt grub-install --target=i386-pc --recheck %s",
                 disk_path);
    }

    if (run_system_command(cmd) != 0) {
        printf("Failed to install GRUB\n");
        return 1;
    }

    // Generate GRUB configuration
    if (run_system_command("arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg") != 0) {
        printf("Failed to create GRUB config\n");
        return 1;
    }

    printf("GRUB installed successfully\n");
    return 0;
}

/**
 * Sets system hostname
 * Updates /etc/hostname and /etc/hosts
 */
int set_hostname(const char *hostname) {
    printf("\nSetting Hostname: %s\n", hostname);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "echo '%s' > /mnt/etc/hostname", hostname);
    run_system_command(cmd);

    // Add entry to hosts file
    snprintf(cmd, sizeof(cmd),
             "echo '127.0.1.1 %s.localdomain %s' >> /mnt/etc/hosts",
             hostname, hostname);
    run_system_command(cmd);

    printf("Hostname set to '%s'\n", hostname);
    return 0;
}

/**
 * Creates default user account
 * Sets up password and sudo permissions
 */
int create_user(const char *username) {
    printf("\nCreating User: %s\n", username);

    char cmd[256];

    // Create user with home directory and wheel group
    snprintf(cmd, sizeof(cmd), "arch-chroot /mnt useradd -m -G wheel %s", username);
    if (run_system_command(cmd) != 0) {
        printf("Failed to create user\n");
        return 1;
    }

    // Set password (same as username by default)
    snprintf(cmd, sizeof(cmd),
             "echo '%s:%s' | arch-chroot /mnt chpasswd", username, username);
    run_system_command(cmd);

    // Enable sudo for wheel group
    run_system_command("arch-chroot /mnt sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers");

    printf("User '%s' created with password '%s'\n", username, username);
    return 0;
}

/**
 * Configures network services
 * Enables NetworkManager and DHCP client
 */
int setup_network(void) {
    printf("\nSetting up Network\n");

    // Enable NetworkManager service
    if (run_system_command("arch-chroot /mnt systemctl enable NetworkManager") != 0) {
        printf("Failed to enable NetworkManager\n");
        return 1;
    }

    // Enable DHCP client
    run_system_command("arch-chroot /mnt systemctl enable dhcpcd");

    printf("Network configured\n");
    return 0;
}

/**
 * Installs desktop environment
 * Supports XFCE, GNOME, KDE, or minimal X setup
 */
int install_desktop(const char *desktop_type) {
    printf("\nInstalling Desktop: %s\n", desktop_type);

    char cmd[512];
    const char *packages = "";
    const char *display_manager = "";

    // Select packages based on desktop choice
    if (strcmp(desktop_type, "xfce") == 0) {
        packages = "xfce4 xfce4-goodies lightdm lightdm-gtk-greeter firefox";
        display_manager = "lightdm";
    } else if (strcmp(desktop_type, "gnome") == 0) {
        packages = "gnome gnome-tweaks gdm firefox";
        display_manager = "gdm";
    } else if (strcmp(desktop_type, "kde") == 0) {
        packages = "plasma-desktop sddm dolphin konsole firefox";
        display_manager = "sddm";
    } else if (strcmp(desktop_type, "minimal") == 0) {
        packages = "xorg-server xorg-xinit xterm";
        display_manager = "none";
    } else {
        printf("Unknown desktop type\n");
        return 1;
    }

    // Install desktop packages
    snprintf(cmd, sizeof(cmd), "arch-chroot /mnt pacman -S --noconfirm %s", packages);
    if (run_system_command(cmd) != 0) {
        printf("Failed to install desktop packages\n");
        return 1;
    }

    // Enable display manager if not minimal
    if (strcmp(display_manager, "none") != 0) {
        snprintf(cmd, sizeof(cmd), "arch-chroot /mnt systemctl enable %s", display_manager);
        run_system_command(cmd);
    }

    printf("Desktop '%s' installed\n", desktop_type);
    return 0;
}

/**
 * Final system configuration:
 * - Timezone setup
 * - Locale generation
 * - Clock synchronization
 */
int finalize_installation(void) {
    printf("\nFinalizing Installation\n");

    // Set timezone to Moscow (default)
    run_system_command("arch-chroot /mnt ln -sf /usr/share/zoneinfo/Europe/Moscow /etc/localtime");
    run_system_command("arch-chroot /mnt hwclock --systohc");

    // Configure locale
    run_system_command("echo 'en_US.UTF-8 UTF-8' > /mnt/etc/locale.gen");
    run_system_command("echo 'ru_RU.UTF-8 UTF-8' >> /mnt/etc/locale.gen");
    run_system_command("arch-chroot /mnt locale-gen");
    run_system_command("echo 'LANG=en_US.UTF-8' > /mnt/etc/locale.conf");

    printf("Installation finalized\n");
    return 0;
}

// ***************************************************************************
// Turbo Install Functions
// **************************************************************************

/**
 * Auto-detects internet connection
 * Tries multiple servers and starts network services if needed
 */
int auto_detect_internet(void) {
    printf("Detecting internet connection...\n");

    const char* servers[] = {
        "ping -c 1 -W 2 1.1.1.1 > /dev/null 2>&1",
        "ping -c 1 -W 2 8.8.8.8 > /dev/null 2>&1",
        "curl -s --connect-timeout 5 https://archlinux.org > /dev/null 2>&1"
    };

    // Try each server
    for (int i = 0; i < 3; i++) {
        if (system(servers[i]) == 0) {
            printf("Internet connection detected\n");
            return 1;
        }
    }

    // If no connection, try to start network services
    printf("Starting network services...\n");
    system("systemctl start NetworkManager 2>/dev/null || true");
    system("systemctl start dhcpcd 2>/dev/null || true");
    sleep(3);

    // Final check
    return check_internet();
}

/**
 * Auto-configures system settings:
 * - Timezone
 * - Locale
 * - Hostname
 * - Network services
 * - User creation
 */
int auto_configure_system(void) {
    printf("Auto-configuring system...\n");

    // Set timezone to Moscow
    printf("Setting timezone...\n");
    system("ln -sf /usr/share/zoneinfo/Europe/Moscow /mnt/etc/localtime 2>/dev/null");
    system("arch-chroot /mnt hwclock --systohc 2>/dev/null");

    // Configure locale
    printf("Configuring locale...\n");
    system("echo 'en_US.UTF-8 UTF-8' > /mnt/etc/locale.gen 2>/dev/null");
    system("echo 'ru_RU.UTF-8 UTF-8' >> /mnt/etc/locale.gen 2>/dev/null");
    system("arch-chroot /mnt locale-gen 2>/dev/null");
    system("echo 'LANG=en_US.UTF-8' > /mnt/etc/locale.conf 2>/dev/null");

    // Set hostname
    printf("Setting hostname...\n");
    system("echo 'lainux' > /mnt/etc/hostname 2>/dev/null");

    // Configure hosts file
    system("echo '127.0.0.1 localhost' > /mnt/etc/hosts 2>/dev/null");
    system("echo '::1 localhost' >> /mnt/etc/hosts 2>/dev/null");
    system("echo '127.0.1.1 lainux.localdomain lainux' >> /mnt/etc/hosts 2>/dev/null");

    // Enable network services
    printf("Configuring network...\n");
    system("arch-chroot /mnt systemctl enable NetworkManager 2>/dev/null");
    system("arch-chroot /mnt systemctl enable dhcpcd 2>/dev/null");

    // Create default user
    printf("Creating user...\n");
    system("arch-chroot /mnt useradd -m -G wheel,audio,video,storage -s /bin/bash lainux 2>/dev/null");
    system("echo 'lainux:lainux' | arch-chroot /mnt chpasswd 2>/dev/null");
    system("arch-chroot /mnt sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers 2>/dev/null");

    // Initialize pacman keys
    printf("Setting up pacman keys...\n");
    system("arch-chroot /mnt pacman-key --init 2>/dev/null");
    system("arch-chroot /mnt pacman-key --populate archlinux 2>/dev/null");

    printf("System auto-configured\n");
    return 0;
}

/**
 * Auto-installs essential packages for Turbo Install
 * Includes base system, tools, and utilities
 */
int auto_install_packages(void) {
    printf("Auto-installing packages...\n");

    // Base system packages
    const char *base_packages =
        "base linux linux-firmware linux-headers "
        "base-devel grub efibootmgr networkmanager "
        "dhcpcd nano vim sudo git curl wget";

    // Additional comfort packages
    const char *comfort_packages =
        "htop neofetch zip unzip rsync bash-completion";

    char cmd[512];

    // Install base packages
    printf("Installing base packages...\n");
    snprintf(cmd, sizeof(cmd),
             "pacstrap -K /mnt %s 2>/dev/null",
             base_packages);
    system(cmd);

    sleep(1);

    // Install additional packages
    printf("Installing comfort packages...\n");
    snprintf(cmd, sizeof(cmd),
             "arch-chroot /mnt pacman -S --noconfirm %s 2>/dev/null",
             comfort_packages);
    system(cmd);

    printf("Packages installed\n");
    return 0;
}

/**
 * Turbo Install - One-click automatic installation
 * Handles complete installation process automatically
 */
int turbo_install(const char *disk_name) {
    printf("\n");
    printf("Starting Turbo Install on /dev/%s\n", disk_name);
    printf("********************************************\n");

    // Final warning before data destruction
    printf("\nWARNING: ALL DATA ON /dev/%s WILL BE DESTROYED!\n", disk_name);
    printf("Starting installation in 3 seconds...\n");
    sleep(3);

    int result = 0;

    // Step 1: Network connectivity check
    printf("\nStep 1: Network check\n");
    if (!auto_detect_internet()) {
        printf("No internet connection available\n");
        return 1;
    }

    // Step 2: Disk preparation
    printf("\nStep 2: Disk preparation\n");
    if (prepare_disk(disk_name) != 0) {
        printf("Failed to prepare disk\n");
        return 2;
    }

    // Step 3: Format and mount partitions
    printf("\nStep 3: Formatting and mounting\n");
    if (format_and_mount(disk_name) != 0) {
        printf("Failed to format/mount\n");
        return 3;
    }

    // Step 4: Generate filesystem table
    printf("\nStep 4: Generating fstab\n");
    system("genfstab -U /mnt >> /mnt/etc/fstab 2>/dev/null");

    // Step 5: Install packages
    printf("\nStep 5: Installing packages\n");
    if (auto_install_packages() != 0) {
        printf("Package installation had issues\n");
    }

    // Step 6: System configuration
    printf("\nStep 6: System configuration\n");
    if (auto_configure_system() != 0) {
        printf("System configuration had issues\n");
    }

    // Step 7: Bootloader installation
    printf("\nStep 7: Installing bootloader\n");
    if (install_grub(disk_name) != 0) {
        printf("Bootloader installation had issues\n");
    }

    // Step 8: System optimizations
    printf("\nStep 8: System optimizations\n");

    // Create swap file
    printf("Creating swap file...\n");
    system("arch-chroot /mnt fallocate -l 2G /swapfile 2>/dev/null || "
           "arch-chroot /mnt dd if=/dev/zero of=/swapfile bs=1M count=2048 2>/dev/null");
    system("arch-chroot /mnt chmod 600 /swapfile 2>/dev/null");
    system("arch-chroot /mnt mkswap /swapfile 2>/dev/null");
    system("arch-chroot /mnt swapon /swapfile 2>/dev/null");
    system("echo '/swapfile none swap defaults 0 0' >> /mnt/etc/fstab 2>/dev/null");

    // Enable TRIM for SSD support
    printf("Enabling TRIM support...\n");
    system("arch-chroot /mnt systemctl enable fstrim.timer 2>/dev/null");

    // Performance tweaks
    printf("Performance tweaks...\n");
    FILE *fp = fopen("/mnt/etc/sysctl.d/99-lainux.conf", "w");
    if (fp) {
        fprintf(fp, "vm.swappiness=10\n");
        fprintf(fp, "vm.vfs_cache_pressure=50\n");
        fclose(fp);
    }

    // Step 9: Finalization
    printf("\nStep 9: Finalizing\n");
    system("arch-chroot /mnt mkinitcpio -P 2>/dev/null");
    system("sync");

    // Unmount partitions
    printf("Unmounting partitions...\n");
    system("umount -R /mnt 2>/dev/null");

    // Installation complete message
    printf("\n");
    printf("Turbo Install complete!\n");
    printf("*************************\n");
    printf("System installed on /dev/%s\n", disk_name);
    printf("\n");
    printf("Credentials:\n");
    printf("  Username: lainux\n");
    printf("  Password: lainux\n");
    printf("\n");
    printf("Reboot to start using Lainux\n");

    return result;
}
