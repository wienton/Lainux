#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../system_utils/log_message.h"
#include "../system_utils/run_command.h"
#include "../include/installer.h"

// localization

#include "../locale/lang.h"

// Enhanced disk info display
void show_disk_info() {
    clear();

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(2, 5,  "STORAGE DEVICE INFORMATION");
    attroff(A_BOLD | COLOR_PAIR(1));

    mvprintw(4, 5, "Device     Size      Type      Mountpoint      Filesystem");
    mvprintw(5, 5, "────────────────────────────────────────────────────────────");

    // Use lsblk with detailed output
    FILE *fp = popen("lsblk -o NAME,SIZE,TYPE,MOUNTPOINT,FSTYPE,MODEL | grep -E '^[s|n|v]'", "r");
    if (fp) {
        char line[256];
        int y = 6;
        while (fgets(line, sizeof(line), fp) && y < 24) {
            line[strcspn(line, "\n")] = 0;
            mvprintw(y++, 5, "%s", line);
        }
        pclose(fp);
    }

    // Show disk usage
    mvprintw(24, 5, "Disk usage summary:");
    run_command("df -h / /home /boot 2>/dev/null | tail -3", 1);

    attron(COLOR_PAIR(4));
    mvprintw(28, 5, "Note: Only devices starting with sd, nvme, or vd are shown");
    attroff(COLOR_PAIR(4));

    mvprintw(30, 5, "Press any key to continue...");
    refresh();
    getch();
}



// Get target disk with enhanced safety
void get_target_disk(char *target, size_t size) {
    clear();

    // Warning with enhanced visuals
    attron(A_BOLD | COLOR_PAIR(3));
    mvprintw(2, 5, "╔════════════════════════════════════════════════════════════════╗");
    mvprintw(3, 5, "║                    ⚠  CRITICAL WARNING  ⚠                    ║");
    mvprintw(4, 5, "║      ALL DATA ON SELECTED DISK WILL BE PERMANENTLY ERASED!    ║");
    mvprintw(5, 5, "╚════════════════════════════════════════════════════════════════╝");
    attroff(A_BOLD | COLOR_PAIR(3));

    mvprintw(7, 5, "Ensure you have backups of all important data before continuing.");
    mvprintw(8, 5, "The installer will perform the following operations:");
    mvprintw(9, 10, "• Create new partition table (GPT)");
    mvprintw(10, 10, "• Create EFI and root partitions");
    mvprintw(11, 10, "• Format partitions with appropriate filesystems");
    mvprintw(12, 10, "• Install Lainux operating system");

    attron(COLOR_PAIR(4));
    mvprintw(14, 5, "Press any key to view available disks...");
    attroff(COLOR_PAIR(4));

    refresh();
    getch();

    // Show available disks
    clear();
    mvprintw(2, 5, "Scanning storage devices...");
    refresh();

    DiskInfo disks[MAX_DISKS];
    int count = 0;

    // Get disk information using lsblk
    FILE *fp = popen("lsblk -dno NAME,SIZE,TYPE,MODEL 2>/dev/null | grep -E '^sd|^nvme|^vd'", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp) && count < MAX_DISKS) {
            line[strcspn(line, "\n")] = 0;

            // Parse disk information
            char *token = strtok(line, " ");
            if (token) strncpy(disks[count].name, token, sizeof(disks[count].name)-1);

            token = strtok(NULL, " ");
            if (token) strncpy(disks[count].size, token, sizeof(disks[count].size)-1);

            token = strtok(NULL, " ");
            if (token) strncpy(disks[count].type, token, sizeof(disks[count].type)-1);

            // Get model name (rest of the line)
            token = strtok(NULL, "\n");
            if (token) {
                strncpy(disks[count].model, token, sizeof(disks[count].model)-1);
            } else {
                strcpy(disks[count].model, "Unknown");
            }

            count++;
        }
        pclose(fp);
    }

    if (count == 0) {
        mvprintw(5, 5, "No suitable disks found. Please check connections.");
        mvprintw(6, 5, "Make sure you have at least one SATA, NVMe, or VirtIO disk.");
        refresh();
        sleep(3);
        strcpy(target, "");
        return;
    }

    int selected = 0;
    while (1) {
        clear();

        // Header
        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw(2, 5, "SELECT INSTALLATION TARGET");
        attroff(A_BOLD | COLOR_PAIR(1));

        mvprintw(3, 5, "Use ↑/↓ to navigate, ENTER to select, ESC to cancel");
        mvprintw(4, 5, "─────────────────────────────────────────────────────");

        // Display disks
        for (int i = 0; i < count; i++) {
            if (i == selected) {
                attron(A_REVERSE | COLOR_PAIR(8));
                mvprintw(6 + i, 7, "→ /dev/%-6s %-10s %-8s %-30s",
                        disks[i].name, disks[i].size, disks[i].type, disks[i].model);
                attroff(A_REVERSE | COLOR_PAIR(8));
            } else {
                mvprintw(6 + i, 9, "/dev/%-6s %-10s %-8s %-30s",
                        disks[i].name, disks[i].size, disks[i].type, disks[i].model);
            }
        }

        // Show selection info
        mvprintw(6 + count + 1, 5, "Selected: /dev/%-10s %-10s",
                disks[selected].name, disks[selected].size);

        // Show disk model
        if (strlen(disks[selected].model) > 0) {
            mvprintw(6 + count + 2, 5, "Model: %s", disks[selected].model);
        }

        // Show warning for selected disk
        attron(COLOR_PAIR(3));
        mvprintw(6 + count + 4, 5, "WARNING: All data on this disk will be lost!");
        attroff(COLOR_PAIR(3));

        int ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected > 0) ? selected - 1 : count - 1;
                break;
            case KEY_DOWN:
                selected = (selected < count - 1) ? selected + 1 : 0;
                break;
            case 10: // ENTER
                strncpy(target, disks[selected].name, size - 1);
                target[size - 1] = '\0';

                // Final confirmation with device path
                clear();
                char question[256];
                char device_path[32];
                snprintf(device_path, sizeof(device_path), "/dev/%s", target);

                snprintf(question, sizeof(question),
                        "FINAL CONFIRMATION: ALL data on %s (%s %s) will be deleted!",
                        device_path, disks[selected].size, disks[selected].model);

                if (confirm_action(question, "ERASE")) {
                    // Optional secure wipe
                    clear();
                    mvprintw(5, 5, "Perform secure wipe before installation?");
                    mvprintw(6, 5, "This will overwrite the first 10MB with zeros.");
                    mvprintw(7, 5, "Type 'WIPE' to perform secure wipe, any other key to skip:");

                    char wipe_confirm[10];
                    echo();
                    mvgetnstr(8, 5, wipe_confirm, sizeof(wipe_confirm)-1);
                    noecho();

                    if (strcmp(wipe_confirm, "WIPE") == 0) {
                        secure_wipe(device_path);
                    }

                    return;
                }
                break;
            case 27: // ESC
                strcpy(target, "");
                return;
        }
    }
}
