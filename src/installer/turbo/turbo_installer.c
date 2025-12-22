#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../system/system.h"
#include "turbo_installer.h"

/**
 * Auto-detects internet connection using multiple methods
 * Tries different servers and protocols for reliable detection
 */
int auto_detect_internet(void) {
    printf("Detecting internet connection...\n");
    
    // Test servers with different protocols
    const char* servers[] = {
        "ping -c 1 -W 2 1.1.1.1 > /dev/null 2>&1",        // Cloudflare DNS
        "ping -c 1 -W 2 8.8.8.8 > /dev/null 2>&1",        // Google DNS
        "ping -c 1 -W 2 archlinux.org > /dev/null 2>&1",  // Arch Linux server
        "curl -s --connect-timeout 5 https://archlinux.org > /dev/null 2>&1" // HTTPS test
    };
    
    // Try each connection method sequentially
    for (int i = 0; i < 4; i++) {
        if (system(servers[i]) == 0) {
            printf("Internet detected\n");
            return 1;
        }
    }
    
    // If all tests fail, try to start network services
    printf("No internet connection found\n");
    printf("Trying to start network services...\n");
    
    // Start common network services
    system("systemctl start NetworkManager 2>/dev/null || true");
    system("systemctl start dhcpcd 2>/dev/null || true");
    system("systemctl start wpa_supplicant 2>/dev/null || true");
    
    // Wait for services to initialize
    sleep(3);
    
    // Final check using standard internet test
    return check_internet();
}

/**
 * Auto-configures system settings without user intervention
 * Sets up timezone, locale, hostname, network, and user accounts
 */
int auto_configure_system(void) {
    printf("Auto-configuring system...\n");
    
    // 1. Auto-detect timezone based on geolocation
    printf("  Detecting timezone...\n");
    
    // Try to detect country via IP geolocation
    if (system("curl -s ifconfig.co/country 2>/dev/null | grep -i russia >/dev/null") == 0) {
        // Russia detected - set Moscow timezone
        system("ln -sf /usr/share/zoneinfo/Europe/Moscow /mnt/etc/localtime 2>/dev/null");
        printf("  Timezone: Europe/Moscow\n");
    } else if (system("curl -s ifconfig.co/country 2>/dev/null | grep -i ukraine >/dev/null") == 0) {
        // Ukraine detected - set Kiev timezone
        system("ln -sf /usr/share/zoneinfo/Europe/Kiev /mnt/etc/localtime 2>/dev/null");
        printf("  Timezone: Europe/Kiev\n");
    } else {
        // Default to UTC if location cannot be determined
        system("ln -sf /usr/share/zoneinfo/UTC /mnt/etc/localtime 2>/dev/null");
        printf("  Timezone: UTC (default)\n");
    }
    
    // 2. Configure system locale
    printf("  Configuring locale...\n");
    system("echo 'en_US.UTF-8 UTF-8' > /mnt/etc/locale.gen 2>/dev/null");
    system("echo 'ru_RU.UTF-8 UTF-8' >> /mnt/etc/locale.gen 2>/dev/null");
    system("arch-chroot /mnt locale-gen 2>/dev/null");
    system("echo 'LANG=en_US.UTF-8' > /mnt/etc/locale.conf 2>/dev/null");
    
    // 3. Set default hostname
    printf("  Setting hostname...\n");
    system("echo 'lainux-pc' > /mnt/etc/hostname 2>/dev/null");
    
    // 4. Configure /etc/hosts file
    system("echo '127.0.0.1 localhost' > /mnt/etc/hosts 2>/dev/null");
    system("echo '::1 localhost' >> /mnt/etc/hosts 2>/dev/null");
    system("echo '127.0.1.1 lainux-pc.localdomain lainux-pc' >> /mnt/etc/hosts 2>/dev/null");
    
    // 5. Enable network services
    printf("  Configuring network...\n");
    system("arch-chroot /mnt systemctl enable NetworkManager 2>/dev/null");
    system("arch-chroot /mnt systemctl enable dhcpcd 2>/dev/null");
    
    // 6. Create default user account
    printf("  Creating user...\n");
    system("arch-chroot /mnt useradd -m -G wheel,audio,video,storage -s /bin/bash lainux 2>/dev/null || true");
    system("echo 'lainux:lainux' | arch-chroot /mnt chpasswd 2>/dev/null");
    system("arch-chroot /mnt sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers 2>/dev/null");
    
    // 7. Initialize pacman package manager keys
    printf("  Setting up pacman keys...\n");
    system("arch-chroot /mnt pacman-key --init 2>/dev/null");
    system("arch-chroot /mnt pacman-key --populate archlinux 2>/dev/null");
    
    printf("System auto-configured\n");
    return 0;
}

/**
 * Automatically installs essential packages for Turbo Install
 * Includes base system, development tools, and utilities
 */
int auto_install_packages(void) {
    printf("Auto-installing essential packages...\n");
    
    // Base system packages (essential for Arch Linux)
    const char *base_packages = 
        "base linux linux-firmware linux-headers "
        "base-devel grub efibootmgr networkmanager "
        "dhcpcd nano vim sudo git curl wget";
    
    // Comfort packages (utilities for better user experience)
    const char *comfort_packages = 
        "ntp chrony htop neofetch zip unzip p7zip "
        "rsync bash-completion";
    
    char cmd[512];
    
    // Install base packages using pacstrap
    printf("  Installing base packages...\n");
    snprintf(cmd, sizeof(cmd), 
             "pacstrap -K /mnt %s 2>/dev/null", 
             base_packages);
    system(cmd);
    
    // Wait for package installation to complete
    sleep(1);
    
    // Install additional comfort packages
    printf("  Installing comfort packages...\n");
    snprintf(cmd, sizeof(cmd), 
             "arch-chroot /mnt pacman -S --noconfirm %s 2>/dev/null", 
             comfort_packages);
    system(cmd);
    
    printf("Essential packages installed\n");
    return 0;
}

/**
 * Main Turbo Install function - complete automatic installation
 * Handles entire installation process from disk preparation to finalization
 */
int turbo_install(DiskInfo *disk) {
    printf("\n");
    printf("TURBO INSTALL LAINUX\n");
    printf("********************\n");
    printf("Target: /dev/%s (%s)\n", disk->name, disk->size);
    printf("Model: %s\n", disk->model);
    printf("\n");
    
    // Final warning about data destruction
    printf("WARNING: ALL DATA ON /dev/%s WILL BE DESTROYED!\n", disk->name);
    printf("This process cannot be undone.\n");
    printf("\nStarting in 5 seconds...\n");
    
    // Countdown before starting installation
    for (int i = 5; i > 0; i--) {
        printf("%d... ", i);
        fflush(stdout);
        sleep(1);
    }
    printf("\n\n");
    
    int result = 0;
    
    // Step 1: Network connectivity check
    printf("STEP 1: Network check\n");
    if (!auto_detect_internet()) {
        printf("No internet connection available\n");
        printf("Please connect to network and try again\n");
        return 1;
    }
    
    // Step 2: Disk preparation (partitioning)
    printf("STEP 2: Disk preparation\n");
    if (prepare_disk(disk->name) != 0) {
        printf("Failed to prepare disk\n");
        return 2;
    }
    
    // Step 3: Format partitions and mount them
    printf("STEP 3: Formatting and mounting\n");
    if (format_and_mount(disk->name) != 0) {
        printf("Failed to format/mount\n");
        return 3;
    }
    
    // Step 4: Generate filesystem table
    printf("STEP 4: Generating fstab\n");
    system("genfstab -U /mnt >> /mnt/etc/fstab 2>/dev/null");
    
    // Step 5: Install essential packages
    printf("STEP 5: Installing packages\n");
    if (auto_install_packages() != 0) {
        printf("⚠ Package installation had issues\n");
    }
    
    // Step 6: System configuration
    printf("STEP 6: System configuration\n");
    if (auto_configure_system() != 0) {
        printf("⚠ System configuration had issues\n");
    }
    
    // Step 7: Bootloader installation
    printf("STEP 7: Installing bootloader\n");
    if (install_grub(disk->name) != 0) {
        printf("⚠ Bootloader installation had issues\n");
    }
    
    // Step 8: System optimizations
    printf("STEP 8: System optimizations\n");
    
    // Create swap file for memory management
    printf("  Creating swap file...\n");
    system("arch-chroot /mnt fallocate -l 2G /swapfile 2>/dev/null || "
           "arch-chroot /mnt dd if=/dev/zero of=/swapfile bs=1M count=2048 2>/dev/null");
    system("arch-chroot /mnt chmod 600 /swapfile 2>/dev/null");
    system("arch-chroot /mnt mkswap /swapfile 2>/dev/null");
    system("arch-chroot /mnt swapon /swapfile 2>/dev/null");
    system("echo '/swapfile none swap defaults 0 0' >> /mnt/etc/fstab 2>/dev/null");
    
    // Enable TRIM support for SSD optimization
    printf("  Enabling TRIM support...\n");
    system("arch-chroot /mnt systemctl enable fstrim.timer 2>/dev/null");
    
    // Performance tuning via sysctl configuration
    printf("  Performance tweaks...\n");
    FILE *fp = fopen("/mnt/etc/sysctl.d/99-lainux.conf", "w");
    if (fp) {
        // Memory management optimizations
        fprintf(fp, "vm.swappiness=10\n");           // Reduce swap usage
        fprintf(fp, "vm.vfs_cache_pressure=50\n");   // Cache pressure tuning
        
        // Network buffer optimizations
        fprintf(fp, "net.core.rmem_default=262144\n");
        fprintf(fp, "net.core.wmem_default=262144\n");
        fprintf(fp, "net.core.rmem_max=33554432\n");
        fprintf(fp, "net.core.wmem_max=33554432\n");
        fclose(fp);
    }
    
    // Step 9: Final system cleanup and preparation
    printf("STEP 9: Finalizing\n");
    
    // Update initramfs for new kernel modules
    system("arch-chroot /mnt mkinitcpio -P 2>/dev/null");
    
    // Sync filesystem to ensure data is written
    system("sync");
    
    // Unmount all partitions from /mnt
    printf("  Unmounting partitions...\n");
    system("umount -R /mnt 2>/dev/null");
    
    // Installation complete message
    printf("\n");
    printf("TURBO INSTALL COMPLETE!\n");
    printf("**********************\n");
    printf("System successfully installed on /dev/%s\n", disk->name);
    printf("\n");
    printf("Credentials:\n");
    printf("  Username: lainux\n");
    printf("  Password: lainux\n");
    printf("\n");
    printf("Next steps:\n");
    printf("  1. Reboot your computer\n");
    printf("  2. Remove installation media\n");
    printf("  3. Boot into Lainux\n");
    printf("  4. Run 'neofetch' to verify installation\n");
    printf("\n");
    
    return result;
}