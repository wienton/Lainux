#include <ncurses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ui/ui.h"
#include "system/system.h"

/**
 * Signal handler for Ctrl+C interruption
 * Cleanly exits the installer without leaving system in bad state
 */
void handle_sigint(int sig) {
    endwin();  // Restore terminal to normal state
    printf("\n[!] Lainux Installer aborted by user. System clean.\n");
    exit(0);
}

/**
 * Main installation function for Lainux
 * Handles the complete installation process step by step
 * Returns error code (0 = success, >0 = error at step)
 */
int install_lainux(DiskInfo *disk) {
    int result = 0;
    
    // Step 1: Partition the target disk
    show_progress("Partitioning drive...", 1, 8);
    if (prepare_disk(disk->name) != 0) {
        result = 1;
        goto cleanup;
    }

    // Step 2: Format partitions and mount them
    show_progress("Formatting and mounting...", 2, 8);
    if (format_and_mount(disk->name) != 0) {
        result = 2;
        goto cleanup;
    }

    // Step 3: Install base Arch Linux system
    show_progress("Installing base system...", 3, 8);
    if (install_base() != 0) {
        result = 3;
        goto cleanup;
    }

    // Step 4: Generate filesystem table (fstab)
    show_progress("Generating fstab...", 4, 8);
    if (run_system_command("genfstab -U /mnt >> /mnt/etc/fstab") != 0) {
        show_progress("Warning: fstab generation failed", 4, 8);
        sleep(1);
    }

    // Step 5: Final system configuration (timezone, locale, etc.)
    show_progress("Finalizing system...", 5, 8);
    if (finalize_installation() != 0) {
        result = 4;
        goto cleanup;
    }

    // Step 6: Set hostname, create user, configure network
    show_progress("Configuring system...", 6, 8);
    set_hostname("lainux");
    create_user("lainux");
    setup_network();

    // Step 7: Install GRUB bootloader
    show_progress("Installing bootloader...", 7, 8);
    if (install_grub(disk->name) != 0) {
        result = 5;
        goto cleanup;
    }

    // Step 8: Install XFCE desktop environment
    show_progress("Installing XFCE desktop...", 8, 9);
    if (install_desktop("xfce") != 0) {
        show_progress("Warning: Desktop installation had issues", 8, 9);
        sleep(1);
    }
    
    // Final step: Installation complete
    show_progress("Installation complete!", 9, 9);
    sleep(2);

cleanup:
    // Cleanup: Unmount all partitions from /mnt
    run_system_command("umount -R /mnt 2>/dev/null");
    
    return result;
}

/**
 * Displays installation result screen
 * Shows success or failure with appropriate messages
 */
void show_success_screen(int success, DiskInfo *disk) {
    int y, x;
    getmaxyx(stdscr, y, x);
    
    clear();
    
    if (success) {
        // Success screen with green color
        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(y/2 - 3, (x-40)/2, "INSTALLATION COMPLETE");
        attroff(COLOR_PAIR(2) | A_BOLD);
        
        mvprintw(y/2 - 1, (x-40)/2, "Lainux successfully installed on");
        mvprintw(y/2, (x-40)/2, "/dev/%s", disk->name);
        mvprintw(y/2 + 2, (x-40)/2, "Default credentials:");
        mvprintw(y/2 + 3, (x-40)/2, "  Username: lainux");
        mvprintw(y/2 + 4, (x-40)/2, "  Password: lainux");
        mvprintw(y/2 + 6, (x-40)/2, "Reboot to start using Lainux");
    } else {
        // Failure screen with red color
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(y/2 - 3, (x-40)/2, "INSTALLATION FAILED");
        attroff(COLOR_PAIR(3) | A_BOLD);
        
        mvprintw(y/2 - 1, (x-40)/2, "Error occurred at step %d", success);
        mvprintw(y/2 + 1, (x-40)/2, "Please check the following:");
        mvprintw(y/2 + 2, (x-40)/2, "1. Internet connection");
        mvprintw(y/2 + 3, (x-40)/2, "2. Disk permissions");
        mvprintw(y/2 + 4, (x-40)/2, "3. Available disk space");
    }
    
    mvprintw(y/2 + 8, (x-30)/2, "Press any key to continue...");
    refresh();
    getch();
}

/**
 * Turbo Install function - one-click automatic installation
 * Handles everything from disk selection to final configuration
 */
void run_turbo_install(void) {
    clear();
    int y, x;
    getmaxyx(stdscr, y, x);
    
    // Show Turbo Install welcome screen
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(y/2 - 2, (x-30)/2, "TURBO INSTALL LAINUX");
    attroff(COLOR_PAIR(1) | A_BOLD);
    
    mvprintw(y/2, (x-40)/2, "Automatic one-click installation");
    mvprintw(y/2 + 2, (x-40)/2, "All configuration will be done automatically");
    mvprintw(y/2 + 4, (x-40)/2, "Press any key to continue...");
    refresh();
    getch();
    
    // Get list of available disks
    DiskInfo disks[MAX_DISKS];
    int count = get_disk_list(disks);
    
    if (count == 0) {
        clear();
        mvprintw(10, 10, "No disks detected!");
        getch();
        return;
    }
    
    // Show disk selection menu
    int disk_idx = show_disk_picker(disks, count);
    
    if (disk_idx != -1) {
        // Show final warning before destruction
        clear();
        attron(COLOR_PAIR(4) | A_BOLD);
        mvprintw(10, 10, "WARNING: Turbo Install will DESTROY all data");
        mvprintw(11, 10, "on /dev/%s!", disks[disk_idx].name);
        attroff(COLOR_PAIR(4) | A_BOLD);
        
        mvprintw(13, 10, "Type 'TURBO' to confirm installation: ");
        
        echo();  // Enable echo for password input
        char confirm[20];
        getstr(confirm);
        noecho();  // Disable echo after input
        
        if (strcmp(confirm, "TURBO") == 0) {
            // Auto-detect and configure internet
            clear();
            mvprintw(10, 10, "Checking internet connection...");
            refresh();
            
            if (!check_internet()) {
                mvprintw(12, 10, "No internet connection detected!");
                mvprintw(13, 10, "Trying to start network services...");
                refresh();
                
                system("systemctl start NetworkManager 2>/dev/null");
                system("systemctl start dhcpcd 2>/dev/null");
                sleep(3);
                
                if (!check_internet()) {
                    mvprintw(15, 10, "Please connect to internet and try again");
                    getch();
                    return;
                }
            }
            
            // Run the main installation process
            int install_result = install_lainux(&disks[disk_idx]);
            
            // Show installation result
            show_success_screen(install_result == 0, &disks[disk_idx]);
            
            // Ask user if they want to reboot
            if (install_result == 0) {
                clear();
                mvprintw(10, 10, "Reboot now to start Lainux? (y/N): ");
                echo();
                char reboot[2];
                getstr(reboot);
                noecho();
                
                if (reboot[0] == 'y' || reboot[0] == 'Y') {
                    endwin();
                    system("reboot");
                }
            }
        }
    }
}

/**
 * Main function - entry point of the installer
 * Initializes UI and handles main menu navigation
 */
int main() {
    // Set up signal handler for Ctrl+C
    signal(SIGINT, handle_sigint);

    // Initialize ncurses for terminal UI
    initscr();
    start_color();
    cbreak();      // Line buffering disabled
    noecho();      // Don't echo input characters
    keypad(stdscr, TRUE);  // Enable function keys
    curs_set(0);   // Hide cursor

    // Initialize color pairs for UI
    init_pair(1, COLOR_CYAN, COLOR_BLACK);    // Main color for UI
    init_pair(2, COLOR_GREEN, COLOR_BLACK);   // Success messages
    init_pair(3, COLOR_RED, COLOR_BLACK);     // Error messages
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);  // Warning messages

    // Show welcome screen
    show_welcome_screen();

    // Main program loop
    int running = 1;
    while(running) {
        int res = show_main_menu();
        
        switch(res) {
            case 0: {  // Turbo Install Lainux
                run_turbo_install();
                break;
            }
            case 1: {  // Manual Installation
                if (!check_internet()) {
                    clear();
                    mvprintw(10, 10, "ERROR: No internet connection!");
                    mvprintw(11, 10, "Lainux requires internet to download packages.");
                    mvprintw(13, 10, "Press any key to continue...");
                    getch();
                    break;
                }

                DiskInfo disks[MAX_DISKS];
                int count = get_disk_list(disks);
                
                if (count == 0) {
                    clear();
                    mvprintw(10, 10, "No disks detected!");
                    getch();
                    break;
                }

                int disk_idx = show_disk_picker(disks, count);
                
                if (disk_idx != -1) {
                    // Final confirmation before installation
                    clear();
                    attron(COLOR_PAIR(4) | A_BOLD);
                    mvprintw(10, 10, "WARNING: All data on /dev/%s will be DESTROYED!", 
                            disks[disk_idx].name);
                    attroff(COLOR_PAIR(4) | A_BOLD);
                    mvprintw(12, 10, "Type 'YES' to confirm: ");
                    
                    echo();
                    char confirm[10];
                    getstr(confirm);
                    noecho();
                    
                    if (strcmp(confirm, "YES") == 0 || 
                        strcmp(confirm, "Y") == 0 || 
                        strcmp(confirm, "y") == 0) {
                        
                        // Run manual installation
                        int install_result = install_lainux(&disks[disk_idx]);
                        
                        // Show result screen
                        show_success_screen(install_result == 0, &disks[disk_idx]);
                    }
                }
                break;
            }
            case 2: {  // Install Toolchain (Future feature)
                clear();
                mvprintw(10, 10, "Toolchain installation");
                mvprintw(12, 10, "This feature will install development tools,");
                mvprintw(13, 10, "network utilities, and media codecs.");
                mvprintw(15, 10, "Available in next version");
                mvprintw(17, 10, "Press any key to continue...");
                getch();
                break;
            }
            case 3: {  // User Configuration (Future feature)
                clear();
                mvprintw(10, 10, "User Configuration");
                mvprintw(12, 10, "Configure users, groups, and permissions");
                mvprintw(14, 10, "Available in next version");
                mvprintw(16, 10, "Press any key to continue...");
                getch();
                break;
            }
            case 4: {  // System Deployment (Future feature)
                clear();
                mvprintw(10, 10, "System Deployment");
                mvprintw(12, 10, "Deploy pre-configured Lainux systems");
                mvprintw(14, 10, "Available in next version");
                mvprintw(16, 10, "Press any key to continue...");
                getch();
                break;
            }
            case 5:  // Exit installer
                running = 0;
                break;
        }
    }

    // Clean up ncurses and exit
    endwin();
    printf("\nLainux Installer - Installation complete.\n");
    printf("Thank you for using Lainux!\n");
    return 0;
}