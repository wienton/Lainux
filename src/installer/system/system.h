#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_DISKS 10

/**
 * Disk information structure
 * Contains name, size, and model information for storage devices
 */
typedef struct {
    char name[20];      // Device name (e.g., "sda", "nvme0n1")
    char size[20];      // Formatted size string (e.g., "500.5 GB")
    char model[50];     // Disk model name from hardware
} DiskInfo;

// *****************************************************************************
// Core System Functions
// *****************************************************************************

/**
 * Retrieves list of available physical disks
 * Filters out virtual devices and partitions
 * Returns number of disks found
 */
int get_disk_list(DiskInfo disks[MAX_DISKS]);

/**
 * Checks internet connectivity by pinging external servers
 * Returns 1 if connection is available, 0 otherwise
 */
int check_internet(void);

/**
 * Wrapper for system command execution with logging
 * Provides centralized command execution interface
 */
int run_system_command(const char *cmd);

/**
 * Prepares disk for installation: creates partitions, sets up GPT
 * Destroys all existing data on the disk
 */
int prepare_disk(const char *disk_name);

/**
 * Formats partitions and mounts them to /mnt
 * Creates EFI and root partitions with appropriate filesystems
 */
int format_and_mount(const char *disk_name);

/**
 * Installs base Arch Linux system using pacstrap
 * Includes kernel, firmware, and essential packages
 */
int install_base(void);

// *****************************************************************************
// System Configuration Functions
// *****************************************************************************

/**
 * Installs GRUB bootloader with auto-detection of UEFI/BIOS
 * Configures boot entries and generates configuration
 */
int install_grub(const char *disk_name);

/**
 * Sets system hostname and updates /etc/hosts file
 */
int set_hostname(const char *hostname);

/**
 * Creates default user account with sudo privileges
 * Sets up home directory and basic permissions
 */
int create_user(const char *username);

/**
 * Configures network services and enables automatic startup
 * Sets up NetworkManager and DHCP client
 */
int setup_network(void);

/**
 * Installs desktop environment with display manager
 * Supports XFCE, GNOME, KDE, and minimal X setup
 */
int install_desktop(const char *desktop_type);

/**
 * Finalizes system installation with timezone, locale, and clock
 * Performs essential post-installation configuration
 */
int finalize_installation(void);

// *****************************************************************************
// Advanced System Features
// *****************************************************************************

/**
 * Creates and activates swap file of specified size
 * Configures swap for better memory management
 */
int setup_swapfile(int size_gb);

/**
 * Installs network utilities and tools
 * Includes SSH, network monitoring, and diagnostic tools
 */
int install_network_tools(void);

/**
 * Installs development tools and compilers
 * Sets up programming environment with common tools
 */
int install_development_tools(void);

/**
 * Installs multimedia codecs and playback software
 * Enables media file support for various formats
 */
int install_media_codecs(void);

/**
 * Applies system performance optimizations
 * Configures kernel parameters and I/O settings
 */
int optimize_system(void);

/**
 * Enables TRIM support for SSD drives
 * Configures automatic filesystem maintenance
 */
int enable_trim_support(void);

/**
 * Configures system locales and keyboard layout
 * Sets up language and regional settings
 */
int configure_locales(void);

// *****************************************************************************
// Turbo Install Functions (Automatic Installation)
// *****************************************************************************

/**
 * Main Turbo Install function - one-click automatic installation
 * Handles complete installation process end-to-end
 */
int turbo_install(const char *disk_name);

/**
 * Auto-detects internet connection with multiple fallback methods
 * Attempts to start network services if needed
 */
int auto_detect_internet(void);

/**
 * Auto-configures system settings without user intervention
 * Sets up timezone, locale, hostname, and user accounts
 */
int auto_configure_system(void);

/**
 * Automatically installs essential packages for Turbo Install
 * Includes base system, utilities, and comfort packages
 */
int auto_install_packages(void);

#endif