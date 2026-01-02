#include <stdio.h>
#include <signal.h>
#include <curl/curl.h>
#include <string.h>
// header

#include "include/installer.h"
#include "locale/lang.h"
#include "settings/settings.h"
#include "ui/ui.h"
#include "configs/config.h"

// Main application
int main() {
    select_language();
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Initialize ncurses
    init_ncurses();


    char target_disk[32] = "";
    int menu_selection = 0;

    const char *menu_items[] = {
        _("INSTALL_ON_HARDWARE"),    // 0
        _("INSTALL_ON_VM"),          // 1
        _("HARDWARE_INFO"),          // 2
        _("SYSTEM_REQUIREMENTS"),    // 3
        _("CONF_SELECTION"),         // 4
        _("DISK_INFO"),              // 5
        _("SETTINGS"),               // 6
        _("EXIT_INSTALLER")          // 7
    };

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int center_x = max_x / 2;

    while (1) {
        clear();

        // Show Lainux logo
        show_logo();

        // Version and system info
        attron(COLOR_PAIR(7));
        int info1_x = (max_x - (int)strlen("Version v0.1 | UEFI Ready | Secure Boot Compatible")) / 2;

        attron(COLOR_PAIR(7));
        mvprintw(16, info1_x, "Version v0.1 | UEFI Ready | Secure Boot Compatible");
        attroff(COLOR_PAIR(7));

        // Display current time
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        int time_x = (max_x - (int)strlen("System time: 2025-12-25 23:59:59")) / 2;
        mvprintw(19, time_x, "System time: %s", time_str);
        attroff(COLOR_PAIR(7));

        int menu_count = sizeof(menu_items) / sizeof(menu_items[0]); // = 8

        int max_item_len = 0;
        for (int i = 0; i < menu_count; i++) {
            int len = strlen(menu_items[i]);
            if (len > max_item_len) max_item_len = len;
        }

        int menu_width = max_item_len + 4;
        int menu_start_x = (max_x - menu_width) / 2;

        for (int i = 0; i < menu_count; i++) {
            if (i == menu_selection) {
                // Highlight current selection with different colors
                if (i == 0) {
                    attron(A_REVERSE | COLOR_PAIR(2));  // Green for install
                } else if (i == 1) {
                    attron(A_REVERSE | COLOR_PAIR(4));  // Yellow for VM
                } else if (i == 2 || i == 3) {
                    attron(A_REVERSE | COLOR_PAIR(5));  // Magenta for info
                } else if (i == 4) {
                    attron(A_REVERSE | COLOR_PAIR(6));  // Blue for config
                } else if (i == 5) {
                    attron(A_REVERSE | COLOR_PAIR(7));  // White for disk info
                } else if (i == 6) {
                    attron(A_REVERSE | COLOR_PAIR(4));  // Yellow for settings
                } else if (i == 7) {
                    attron(A_REVERSE | COLOR_PAIR(3));  // Red for exit
                }
                mvprintw(22 + i * 2, menu_start_x, "› %s", menu_items[i]);

                // Turn off attributes
                if (i == 0) {
                    attroff(A_REVERSE | COLOR_PAIR(2));
                } else if (i == 1) {
                    attroff(A_REVERSE | COLOR_PAIR(4));
                } else if (i == 2 || i == 3) {
                    attroff(A_REVERSE | COLOR_PAIR(5));
                } else if (i == 4) {
                    attroff(A_REVERSE | COLOR_PAIR(6));
                } else if (i == 5) {
                    attroff(A_REVERSE | COLOR_PAIR(7));
                } else if (i == 6) {
                    attroff(A_REVERSE | COLOR_PAIR(4));
                } else if (i == 7) {
                    attroff(A_REVERSE | COLOR_PAIR(3));
                }
            } else {
                // Normal display with colors
                if (i == 0) {
                    attron(COLOR_PAIR(2));
                    mvprintw(22 + i * 2, menu_start_x + 2, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(2));
                } else if (i == 1) {
                    attron(COLOR_PAIR(4));
                    mvprintw(22 + i * 2, menu_start_x + 2, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(4));
                } else if (i == 2 || i == 3) {
                    attron(COLOR_PAIR(5));
                    mvprintw(22 + i * 2, menu_start_x + 2, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(5));
                } else if (i == 4) {
                    attron(COLOR_PAIR(6));
                    mvprintw(22 + i * 2, menu_start_x + 2, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(6));
                } else if (i == 5) {
                    attron(COLOR_PAIR(7));
                    mvprintw(22 + i * 2, menu_start_x + 2, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(7));
                } else if (i == 6) {
                    attron(COLOR_PAIR(4));
                    mvprintw(22 + i * 2, menu_start_x + 2, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(4));
                } else if (i == 7) {
                    attron(COLOR_PAIR(3));
                    mvprintw(22 + i * 2, menu_start_x + 2, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(3));
                }
            }
        }

        // Show system architecture
        FILE *fp = popen("uname -m", "r");
        char arch[32];
        if (fp) {
            fgets(arch, sizeof(arch), fp);
            arch[strcspn(arch, "\n")] = 0;
            pclose(fp);
        }

        // Show kernel version
        fp = popen("uname -r | cut -d- -f1", "r");
        char kernel[32];
        if (fp) {
            fgets(kernel, sizeof(kernel), fp);
            kernel[strcspn(kernel, "\n")] = 0;
            pclose(fp);
        }
        attroff(COLOR_PAIR(7));

        // navigate for center
        int nav_x = (max_x - (int)strlen("Navigate: ↑ ↓ • Select: Enter • Exit: Esc")) / 2;
        mvprintw(max_y - 3, nav_x, "Navigate: ↑ ↓ • Select: Enter • Exit: Esc");

        // system info -- right
        int right_col = max_x - 30; // 30 symbols with right

        mvprintw(max_y - 3, right_col, "Arch: %s", arch);
        mvprintw(max_y - 2, right_col, "Kernel: %s", kernel);
        mvprintw(max_y - 1, right_col, "Built with: GCC %s", __VERSION__);

        int input = getch();
        switch (input) {
            case KEY_UP:
                menu_selection = (menu_selection > 0) ? menu_selection - 1 : 7;
                break;
            case KEY_DOWN:
                menu_selection = (menu_selection < 7) ? menu_selection + 1 : 0;
                break;
            case 10: // Enter
                switch (menu_selection) {
                    case 0: // Install on Hardware
                        get_target_disk(target_disk, sizeof(target_disk));
                        if (strlen(target_disk) > 0) {
                            perform_installation(target_disk);
                        }
                        break;
                    case 1: // Install on VM
                        install_on_virtual_machine();
                        break;
                    case 2: // Hardware Information
                        show_hardware_info();
                        break;
                    case 3: // System Requirements Check
                        check_system_requirements();
                        break;
                    case 4: // Configuration Selection
                        show_configuration_menu();
                        break;
                    case 5: // Disk Info
                        show_disk_info();
                        break;
                    case 6: // Settings
                        clear();
                        print_settings();
                        break;
                    case 7: // Exit
                        if (confirm_action("Exit Lainux installer?", "EXIT")) {
                            cleanup_ncurses();
                            curl_global_cleanup();
                            return 0;
                        }
                        break;
                }
                break;
            case 27: // Escape
                if (confirm_action("Exit Lainux installer?", "EXIT")) {
                    cleanup_ncurses();
                    curl_global_cleanup();
                    return 0;
                }
                break;
        }
    }
    cleanup_ncurses();
    curl_global_cleanup();
    return 0;
}

