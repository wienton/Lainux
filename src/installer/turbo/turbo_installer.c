#include <string.h>
#include <unistd.h>


#include "turbo_installer.h"
#include "../ui/ui.h"
#include "../utils/log_message.h"
#include "../utils/run_command.h"

#include "../system/system_check.h"
#include "../disk_utils/disk_info.h"
#include "../include/installer.h"



// Main installation procedure
void perform_installation(const char *disk) {
    install_running = 1;

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Create windows
    WINDOW *main_win = newwin(max_y - 4, max_x - 4, 2, 2);
    box(main_win, 0, 0);
    wrefresh(main_win);

    log_win = newwin(max_y - 10, max_x - 10, 5, 5);
    scrollok(log_win, TRUE);
    box(log_win, 0, 0);
    wrefresh(log_win);

    status_win = newwin(3, max_x - 10, max_y - 4, 5);
    box(status_win, 0, 0);
    wrefresh(status_win);

    char dev_path[32];
    char efi_part[32], root_part[32];

    snprintf(dev_path, sizeof(dev_path), "/dev/%s", disk);

    // Determine partition names
    if (strncmp(disk, "nvme", 4) == 0 || strncmp(disk, "vd", 2) == 0) {
        snprintf(efi_part, sizeof(efi_part), "%sp1", dev_path);
        snprintf(root_part, sizeof(root_part), "%sp2", dev_path);
    } else {
        snprintf(efi_part, sizeof(efi_part), "%s1", dev_path);
        snprintf(root_part, sizeof(root_part), "%s2", dev_path);
    }

    // Pre-installation checks
    display_status("Performing system checks...");
    log_message("Starting installation on %s", dev_path);

    if (!check_dependencies()) {
        log_message("Dependency check failed");
        display_status("Dependency check failed");
        install_running = 0;
        return;
    }

    if (!verify_efi()) {
        log_message("EFI system not detected. Legacy BIOS may not be supported.");
        if (!confirm_action("Continue without UEFI? (Legacy BIOS mode)", "CONTINUE")) {
            log_message("Installation aborted");
            display_status("Installation aborted");
            install_running = 0;
            return;
        }
    }

    if (!check_network()) {
        log_message("Network connectivity issue detected");
        if (!confirm_action("Continue without network?", "CONTINUE")) {
            log_message("Installation aborted");
            display_status("Installation aborted");
            install_running = 0;
            return;
        }
    }

    // Check disk space
    long required_space = 8000;  // 8GB minimum
    long available_space = get_available_space("/");

    if (available_space < required_space) {
        log_message("Insufficient disk space: %ldMB available, %ldMB required",
                   available_space, required_space);
        display_status("Insufficient disk space");
        install_running = 0;
        return;
    }

    // Partitioning
    display_status("Partitioning target disk...");
    create_partitions(disk);

    // Wait for partitions with timeout
    int attempts = 0;
    while ((!file_exists(efi_part) || !file_exists(root_part)) && attempts < 15) {
        log_message("Waiting for partitions... (attempt %d)", attempts + 1);
        display_status("Waiting for partitions...");
        sleep(1);
        attempts++;
        run_command("udevadm settle 2>/dev/null", 0);
    }

    if (!file_exists(efi_part) || !file_exists(root_part)) {
        log_message("Partition creation failed. Expected: %s, %s", efi_part, root_part);
        display_status("Partition creation failed");
        install_running = 0;
        return;
    }

    // Formatting
    display_status("Formatting partitions...");
    log_message("Formatting %s as FAT32", efi_part);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkfs.fat -F32 -n LAINUX_EFI %s", efi_part);
    if (run_command(cmd, 1) != 0) {
        log_message("EFI format failed, trying alternative...");
        snprintf(cmd, sizeof(cmd), "mkfs.vfat -F32 %s", efi_part);
        run_command(cmd, 1);
    }

    log_message("Formatting %s as ext4", root_part);
    snprintf(cmd, sizeof(cmd), "mkfs.ext4 -F -L lainux_root %s", root_part);
    run_command(cmd, 1);

    // Mounting
    display_status("Mounting filesystems...");

    // Clean up any existing mounts
    run_command("umount -R /mnt 2>/dev/null || true", 0);
    run_command("rmdir /mnt 2>/dev/null || true", 0);

    // Create mount point
    run_command("mkdir -p /mnt", 0);

    // Mount root with retry
    if (mount_with_retry(root_part, "/mnt", "ext4", 0) != 0) {
        log_message("Failed to mount root partition");
        display_status("Mount failed");
        install_running = 0;
        return;
    }

    // Create and mount boot
    run_command("mkdir -p /mnt/boot", 0);
    if (mount_with_retry(efi_part, "/mnt/boot", "vfat", 0) != 0) {
        log_message("Failed to mount boot partition");
        display_status("Mount failed");
        install_running = 0;
        return;
    }

    // Base system installation
    display_status("Installing base system...");

    // Check for existing pacman cache
    run_command("mkdir -p /mnt/var/cache/pacman/pkg", 0);

    // Install base system with pacstrap
    snprintf(cmd, sizeof(cmd), "pacstrap -K /mnt base linux linux-firmware base-devel");
    if (run_command(cmd, 1) != 0) {
        log_message("Pacstrap failed, trying alternative method...");
        // Alternative method could be implemented here
        display_status("Base installation failed");
        install_running = 0;
        return;
    }

    // Generate fstab
    display_status("Generating filesystem table...");
    run_command("genfstab -U /mnt >> /mnt/etc/fstab", 1);

    // Core system configuration
    display_status("Configuring system...");

    // Set timezone
    run_command("arch-chroot /mnt ln -sf /usr/share/zoneinfo/UTC /etc/localtime", 0);
    run_command("arch-chroot /mnt hwclock --systohc", 0);

    // Configure locale
    run_command("echo 'en_US.UTF-8 UTF-8' > /mnt/etc/locale.gen", 0);
    run_command("echo 'en_US ISO-8859-1' >> /mnt/etc/locale.gen", 0);
    run_command("arch-chroot /mnt locale-gen", 0);
    run_command("echo 'LANG=en_US.UTF-8' > /mnt/etc/locale.conf", 0);

    // Set hostname
    run_command("echo 'lainux' > /mnt/etc/hostname", 0);
    run_command("echo '127.0.1.1 lainux.localdomain lainux' >> /mnt/etc/hosts", 0);

    // Install Lainux core with fallback
    display_status("Installing Lainux core...");

    int download_result = download_file(CORE_URL, "/mnt/root/core.pkg.tar.zst");
    if (download_result != ERR_SUCCESS) {
        log_message("Primary download failed, trying fallback...");
        download_result = download_file(FALLBACK_CORE_URL, "/mnt/root/core.pkg.tar.zst");
    }

    if (download_result == ERR_SUCCESS) {
        run_command("arch-chroot /mnt pacman -U /root/core.pkg.tar.zst --noconfirm", 1);
    } else {
        log_message("Failed to download Lainux core. Installation will continue without it.");
    }

    // Install bootloader
    display_status("Installing bootloader...");

    // Check if we're in chroot or need arch-chroot
    if (file_exists("/mnt/usr/bin/grub-install")) {
        snprintf(cmd, sizeof(cmd), "arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot --bootloader-id=lainux --recheck");
    } else {
        snprintf(cmd, sizeof(cmd), "grub-install --target=x86_64-efi --efi-directory=/mnt/boot --bootloader-id=lainux --recheck");
    }

    if (run_command(cmd, 1) != 0) {
        log_message("GRUB installation failed, trying alternative...");
        // Try without target specification
        snprintf(cmd, sizeof(cmd), "arch-chroot /mnt grub-install --efi-directory=/boot --bootloader-id=lainux");
        run_command(cmd, 1);
    }

    // Generate GRUB config
    if (file_exists("/mnt/usr/bin/grub-mkconfig")) {
        run_command("arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg", 1);
    } else {
        run_command("grub-mkconfig -o /mnt/boot/grub/grub.cfg", 1);
    }

    // Set root password
    display_status("Setting up users...");
    run_command("echo 'root:lainux' | arch-chroot /mnt chpasswd", 0);

    // Create a regular user
    run_command("arch-chroot /mnt useradd -m -G wheel -s /bin/bash lainux", 0);
    run_command("echo 'lainux:lainux' | arch-chroot /mnt chpasswd", 0);

    // Configure sudo
    run_command("echo '%%wheel ALL=(ALL) ALL' > /mnt/etc/sudoers.d/wheel", 0);
    run_command("chmod 440 /mnt/etc/sudoers.d/wheel", 0);

    // Enable network services
    run_command("arch-chroot /mnt systemctl enable systemd-networkd systemd-resolved", 0);

    // Cleanup
    display_status("Cleaning up...");

    // Remove downloaded package
    run_command("rm -f /mnt/root/core.pkg.tar.zst 2>/dev/null", 0);

    // Clear pacman cache
    run_command("arch-chroot /mnt pacman -Scc --noconfirm", 0);

    // Unmount filesystems
    run_command("sync", 0);
    run_command("umount -R /mnt", 0);

    log_message("Installation complete!");
    display_status("Installation complete!");

    // Clean up windows
    delwin(log_win);
    delwin(status_win);
    delwin(main_win);
    log_win = NULL;
    status_win = NULL;

    install_running = 0;

    show_summary(disk);
}
