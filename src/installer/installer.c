/**
* @brief Lainux Installer
* @version v0.3 beta only
* @author Wienton | Lainux Development Laboratory
* @license: GPL-3.0
* @details contact with general developer LainuxOS and this installer you can from telegram @openrtc
* @status: STATUS WORK **TRUE**
* @also: download iso from github release for virtual machine.

  @hosting: codeberg
*/

#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <time.h>
#include <locale.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <curl/curl.h>
#include <ctype.h>

#include "../utils/network_connection/network_sniffer.h"
#include "../utils/network_connection/network_state.h"
#include "vm/vm.h"
// general header for installer

#include "include/installer.h"

#include "utils/log_message.h"
#include "utils/run_command.h"
#include "disk_utils/disk_info.h"
#include "locale/lang.h"
#include "settings/settings.h"
#include "turbo/turbo_installer.h"
#include "cleanup/cleaner.h"

// ui
#include "ui/ui.h"

#define LINK_ISO ""
#define PACKAGE_LINK ""

// Global variables
WINDOW *log_win;
WINDOW *status_win;
char log_buffer[LOG_BUFFER_SIZE];
volatile int install_running = 0;


// Signal handler for graceful exit
void signal_handler(int sig) {
    install_running = 0;
    log_message("Signal %d received, cleaning up...", sig);
    emergency_cleanup();
    cleanup_ncurses();
    exit(0);
}


// Get system information
void get_system_info(SystemInfo *info) {
    memset(info, 0, sizeof(SystemInfo));

    // CPU cores
    info->total_cores = sysconf(_SC_NPROCESSORS_ONLN);
    info->avail_cores = sysconf(_SC_NPROCESSORS_CONF);

    // Memory
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info->total_ram = si.totalram / (1024 * 1024);
        info->avail_ram = si.freeram / (1024 * 102);
   }
    // Architecture
    FILE *fp = popen("uname -m", "r");
    if (fp) {
        fgets(info->arch, sizeof(info->arch), fp);
        info->arch[strcspn(info->arch, "\n")] = 0;
        pclose(fp);
    }

    // Hostname
    gethostname(info->hostname, sizeof(info->hostname));

    // Kernel
     fp = popen("uname -r", "r");
    if (fp) {
        fgets(info->kernel, sizeof(info->kernel), fp);
        info->kernel[strcspn(info->kernel, "\n")] = 0;
        pclose(fp);
    }
}


// Download thread function
void *download_thread(void *arg) {
    char *url = ((char**)arg)[0];
    char *output = ((char**)arg)[1];

    log_message("Downloading %s to %s", url, output);
    download_file(url, output);

    return NULL;
}

// Download file with curl
int download_file(const char *url, const char *output) {
    CURL *curl;
    FILE *fp;
    CURLcode res;

    curl = curl_easy_init();
    if (!curl) {
        log_message("Failed to initialize curl");
        return ERR_NETWORK;
    }

    fp = fopen(output, "wb");
    if (!fp) {
        log_message("Failed to create file: %s", output);
        curl_easy_cleanup(curl);
        return ERR_IO_FAILURE;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Lainux-Installer/1.0");
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    res = curl_easy_perform(curl);

    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        log_message("Download failed: %s", curl_easy_strerror(res));
        remove(output);
        return ERR_NETWORK;
    }

    log_message("Download completed: %s", output);
    return ERR_SUCCESS;
}


// Get hardware details
void get_hardware_details(char *cpu_info, char *memory_info, char *gpu_info, char *storage_info) {
    // CPU info with fallbacks
    FILE *fp = popen("lscpu 2>/dev/null | grep 'Model name' | head -1 | cut -d: -f2- | sed 's/^[ \\t]*//'", "r");
    if (fp && fgets(cpu_info, 128, fp)) {
        cpu_info[strcspn(cpu_info, "\n")] = 0;
        pclose(fp);
    } else {
        fp = popen("cat /proc/cpuinfo | grep 'model name' | head -1 | cut -d: -f2- | sed 's/^[ \\t]*//'", "r");
        if (fp && fgets(cpu_info, 128, fp)) {
            cpu_info[strcspn(cpu_info, "\n")] = 0;
            pclose(fp);
        } else {
            strcpy(cpu_info, "Unknown CPU");
        }
    }

    // Memory info
    fp = popen("free -h | grep Mem | awk '{print $2}'", "r");
    if (fp && fgets(memory_info, 32, fp)) {
        memory_info[strcspn(memory_info, "\n")] = 0;
        pclose(fp);
    } else {
        strcpy(memory_info, "Unknown");
    }

    // GPU info with multiple fallbacks
    fp = popen("lspci 2>/dev/null | grep -i 'vga\\|3d\\|display' | head -1 | cut -d: -f3- | sed 's/^[ \\t]*//'", "r");
    if (fp && fgets(gpu_info, 128, fp)) {
        gpu_info[strcspn(gpu_info, "\n")] = 0;
        pclose(fp);
    } else {
        strcpy(gpu_info, "Unknown GPU");
    }

    // Storage info
    int disk_count = 0;
    fp = popen("lsblk -dno NAME 2>/dev/null | grep -E '^sd|^nvme|^vd' | wc -l", "r");
    if (fp && fgets(storage_info, 32, fp)) {
        storage_info[strcspn(storage_info, "\n")] = 0;
        disk_count = atoi(storage_info);
        pclose(fp);
    }

    if (disk_count > 0) {
        fp = popen("lsblk -dno SIZE 2>/dev/null | grep -E '^[0-9]' | head -1", "r");
        char size[32] = "Unknown";
        if (fp && fgets(size, sizeof(size), fp)) {
            size[strcspn(size, "\n")] = 0;
            pclose(fp);
        }
        snprintf(storage_info, 64, "%d disks, %s total", disk_count, size);
    } else {
        strcpy(storage_info, "No disks detected");
    }
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

// Download Arch Linux ISO with progress
void download_arch_iso() {
    log_message("Downloading Lainux ISO...");

    // Check existing file
    if (file_exists("lainux.iso")) {
        log_message("Existing ISO found, checking integrity...");
        long size = 0;
        struct stat st;
        if (stat("lainux.iso", &st) == 0) {
            size = st.st_size;
        }

        if (size > 500 * 1024 * 1024) {  // Rough check for complete ISO
            log_message("Using existing ISO file (%ld MB)", size / (1024 * 1024));
            return;
        }
    }

    // Download with wget and curl fallback
    char cmd[512];

    // Try wget first (supports continue)
    snprintf(cmd, sizeof(cmd), "wget -c --timeout=30 --tries=3 '%s' -O lainux.iso 2>&1 | grep --line-buffered -E '([0-9]+)%%|speed'", ARCH_ISO_URL);
    int result = run_command(cmd, 1);

    if (result != 0) {
        log_message("wget failed, trying curl...");
        snprintf(cmd, sizeof(cmd), "curl -L -C - --connect-timeout 30 --retry 3 '%s' -o lainux.iso 2>&1 | grep --line-buffered -E '([0-9]+[.][0-9]*%%)|speed'", ARCH_ISO_URL);
        run_command(cmd, 1);
    }

    if (file_exists("lainux.iso")) {
        struct stat st;
        if (stat("lainux.iso", &st) == 0) {
            log_message("Download complete: %ld MB", st.st_size / (1024 * 1024));
        }
    } else {
        log_message("Failed to download LainuxOS ISO");
    }
}

// Find ISO files
int find_iso_files(char files[][MAX_PATH], int max_files) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    dir = opendir(".");
    if (dir == NULL) {
        return 0;
    }

    while ((entry = readdir(dir)) != NULL && count < max_files) {
        if (entry->d_type == DT_REG || entry->d_type == DT_LNK) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext != NULL) {
                char lower_ext[8];
                strncpy(lower_ext, ext, sizeof(lower_ext)-1);
                for (int i = 0; lower_ext[i]; i++) {
                    lower_ext[i] = tolower((unsigned char)lower_ext[i]);
                }

                if (strcmp(lower_ext, ".iso") == 0 ||
                    strcmp(lower_ext, ".img") == 0 ||
                    strstr(entry->d_name, "arch") != NULL ||
                    strstr(entry->d_name, "ARCH") != NULL) {
                    strncpy(files[count], entry->d_name, MAX_PATH - 1);
                    files[count][MAX_PATH - 1] = '\0';

                    // Get file size
                    struct stat st;
                    if (stat(entry->d_name, &st) == 0) {
                        char size_str[32];
                        if (st.st_size > 1024*1024*1024) {
                            snprintf(size_str, sizeof(size_str), "%.1fGB", st.st_size/(1024.0*1024.0*1024.0));
                        } else {
                            snprintf(size_str, sizeof(size_str), "%.1fMB", st.st_size/(1024.0*1024.0));
                        }

                        // Append size to filename
                        strcat(files[count], " (");
                        strcat(files[count], size_str);
                        strcat(files[count], ")");
                    }

                    count++;
                }
            }
        }
    }

    closedir(dir);

    // Sort files alphabetically
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strcmp(files[i], files[j]) > 0) {
                char temp[MAX_PATH];
                strcpy(temp, files[i]);
                strcpy(files[i], files[j]);
                strcpy(files[j], temp);
            }
        }
    }

    return count;
}

// Select ISO file
void select_iso_file(char *iso_path, size_t size) {
    clear();

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(2, 10, "INSTALLATION MEDIA SELECTION");
    attroff(A_BOLD | COLOR_PAIR(1));

    mvprintw(4, 10, "Choose ISO source:");
    attron(COLOR_PAIR(2));
    mvprintw(5, 15, "1. Download latest LainuxOS ISO");
    attroff(COLOR_PAIR(2));
    mvprintw(6, 15, "2. Use existing ISO file");
    mvprintw(7, 15, "3. Cancel and return to menu");

    mvprintw(9, 10, "Enter choice (1-3): ");

    echo();
    char choice[2];
    mvgetnstr(9, 30, choice, sizeof(choice));
    noecho();

    switch (choice[0]) {
        case '1': {
            clear();
            mvprintw(5, 10, "Downloading latest LainuxOS ISO...");
            mvprintw(6, 10, "This may take several minutes depending on your connection.");
            refresh();

            // Create download thread
            pthread_t download_thread_id;
            char *args[2];
            args[0] = (char*)ARCH_ISO_URL;
            args[1] = "lainux.iso";

            mvprintw(8, 10, "Download in progress...");
            mvprintw(9, 10, "Please wait.");
            refresh();

            if (pthread_create(&download_thread_id, NULL, download_thread, args) == 0) {
                // Show progress animation
                for (int i = 0; i < 30; i++) {
                    mvprintw(9, 24 + (i % 4), ".   " + (i % 4));
                    refresh();
                    usleep(100000);

                    // Check if download completed (simple non-portable wait)
                    int try_result;
                    // Use non-blocking check
                    struct timespec ts = {0, 10000000}; // 10ms
                    nanosleep(&ts, NULL);

                    // Try to join with zero timeout (non-portable, but works on Linux)
                    void *result;
                    #ifdef __linux__
                    usleep(100000);
                    #else
                    // For non-Linux systems, just wait
                    if (i == 29) {
                        pthread_join(download_thread_id, NULL);
                    }
                    #endif
                    pthread_join(download_thread_id, NULL);
                }

                pthread_join(download_thread_id, NULL);
            }

            if (file_exists("lainux.iso")) {
                strncpy(iso_path, "lainux.iso", size);

                // Verify ISO size
                struct stat st;
                if (stat("lainux.iso", &st) == 0) {
                    clear();
                    mvprintw(5, 10, "Download complete!");
                    mvprintw(6, 10, "File: lainux.iso");
                    mvprintw(7, 10, "Size: %.2f GB", st.st_size / (1024.0 * 1024.0 * 1024.0));
                    refresh();
                    sleep(2);
                }
            } else {
                clear();
                mvprintw(5, 10, "Download failed. Please try option 2.");
                mvprintw(6, 10, "Make sure you have internet connection and disk space.");
                refresh();
                sleep(3);
                iso_path[0] = '\0';
            }
            break;
        }

        case '2': {
            clear();
            char iso_files[20][MAX_PATH];
            int file_count = find_iso_files(iso_files, 20);

            if (file_count == 0) {
                mvprintw(5, 10, "No ISO files found in current directory.");
                mvprintw(6, 10, "Please place an ISO file here and try again.");
                mvprintw(7, 10, "Supported formats: .iso, .img");
                refresh();
                sleep(3);
                iso_path[0] = '\0';
                break;
            }

            int selected = 0;
            while (1) {
                clear();

                attron(A_BOLD | COLOR_PAIR(1));
                mvprintw(2, 10, "SELECT ISO FILE");
                attroff(A_BOLD | COLOR_PAIR(1));

                mvprintw(3, 10, "Use ↑/↓ arrows, ENTER to select, ESC to cancel");
                mvprintw(4, 10, "─────────────────────────────────────────────");

                for (int i = 0; i < file_count; i++) {
                    if (i == selected) {
                        attron(A_REVERSE | COLOR_PAIR(2));
                        mvprintw(6 + i, 12, "→ %-60s", iso_files[i]);
                        attroff(A_REVERSE | COLOR_PAIR(2));
                    } else {
                        mvprintw(6 + i, 14, "%-60s", iso_files[i]);
                    }
                }

                // Extract actual filename (without size)
                char actual_filename[MAX_PATH];
                strncpy(actual_filename, iso_files[selected], MAX_PATH-1);
                char *paren = strchr(actual_filename, '(');
                if (paren) *(paren-1) = '\0';  // Remove trailing space before parenthesis

                mvprintw(6 + file_count + 2, 10, "Selected: %s", actual_filename);

                int ch = getch();
                if (ch == KEY_UP) {
                    selected = (selected > 0) ? selected - 1 : file_count - 1;
                } else if (ch == KEY_DOWN) {
                    selected = (selected < file_count - 1) ? selected + 1 : 0;
                } else if (ch == 10) {
                    // Extract actual filename
                    strncpy(actual_filename, iso_files[selected], MAX_PATH-1);
                    paren = strchr(actual_filename, '(');
                    if (paren) *(paren-1) = '\0';

                    strncpy(iso_path, actual_filename, size);

                    // Verify file exists
                    if (!file_exists(iso_path)) {
                        clear();
                        mvprintw(5, 10, "Error: File not found: %s", iso_path);
                        refresh();
                        sleep(2);
                        iso_path[0] = '\0';
                    }
                    break;
                } else if (ch == 27) {
                    iso_path[0] = '\0';
                    break;
                }
            }
            break;
        }

        default:
            iso_path[0] = '\0';
            break;
    }
}

// Draw progress bar
void draw_progress_bar(int y, int x, int width, float progress) {
    int bars = (int)(progress * width);

    attron(COLOR_PAIR(7));
    mvprintw(y, x, "[");

    attron(COLOR_PAIR(2));
    for (int i = 0; i < bars; i++) {
        addch('=');
    }

    attron(COLOR_PAIR(7));
    for (int i = bars; i < width; i++) {
        addch(' ');
    }
    addch(']');

    mvprintw(y, x + width + 2, "%3d%%", (int)(progress * 100));
    attroff(COLOR_PAIR(7));
}

// Select configuration
void select_configuration() {
    clear();

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(2, 10, "SYSTEM CONFIGURATION");
    attroff(A_BOLD | COLOR_PAIR(1));

    mvprintw(4, 10, "Select system configuration:");

    const char *configs[] = {
        "Minimal        (Base system only, ~500MB)",
        "Standard       (Base + Desktop, ~2GB)",
        "Development    (Standard + Dev tools, ~4GB)",
        "Server         (Minimal + Server packages, ~1.5GB)",
        "Security       (Standard + Security tools, ~2.5GB)",
        "CyberSecurity  (Advanced security suite, ~3.5GB)",
        "Custom         (Manual package selection)",
        NULL
    };

    int selected = 0;
    int config_count = 7;

    while (1) {
        clear();

        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw(2, 10, "SELECT CONFIGURATION");
        attroff(A_BOLD | COLOR_PAIR(1));

        mvprintw(3, 10, "Use ↑/↓ arrows, ENTER to select, ESC to cancel");
        mvprintw(4, 10, "──────────────────────────────────────────────");

        for (int i = 0; i < config_count; i++) {
            if (i == selected) {
                attron(A_REVERSE | COLOR_PAIR(2));
                mvprintw(6 + i, 12, "→ %s", configs[i]);
                attroff(A_REVERSE | COLOR_PAIR(2));
            } else {
                mvprintw(6 + i, 14, "%s", configs[i]);
            }
        }

        mvprintw(6 + config_count + 2, 10, "Selected: %s", configs[selected]);

        int ch = getch();
        if (ch == KEY_UP) {
            selected = (selected > 0) ? selected - 1 : config_count - 1;
        } else if (ch == KEY_DOWN) {
            selected = (selected < config_count - 1) ? selected + 1 : 0;
        } else if (ch == 10) {
            // Save configuration
            FILE *config_file = fopen("lainux-config.txt", "w");
            if (config_file) {
                fprintf(config_file, "# Lainux Installation Configuration\n");
                fprintf(config_file, "# Generated: %s", ctime(&(time_t){time(NULL)}));
                fprintf(config_file, "Configuration: %s\n", configs[selected]);

                switch (selected) {
                    case 0:
                        fprintf(config_file, "Type=minimal\n");
                        fprintf(config_file, "Packages=base linux linux-firmware\n");
                        break;
                    case 1:
                        fprintf(config_file, "Type=standard\n");
                        fprintf(config_file, "Packages=base linux linux-firmware xorg desktop-environment network-manager\n");
                        break;
                    case 2:
                        fprintf(config_file, "Type=development\n");
                        fprintf(config_file, "Packages=base linux linux-firmware xorg desktop-environment network-manager base-devel git python nodejs docker\n");
                        break;
                    case 3:
                        fprintf(config_file, "Type=server\n");
                        fprintf(config_file, "Packages=base linux linux-firmware openssh nginx postgresql redis\n");
                        break;
                    case 4:
                        fprintf(config_file, "Type=security\n");
                        fprintf(config_file, "Packages=base linux-hardened linux-firmware xorg desktop-environment ufw openssl auditd\n");
                        break;
                    case 5:
                        fprintf(config_file, "Type=cybersecurity\n");
                        fprintf(config_file, "Packages=base linux-hardened linux-firmware xorg desktop-environment wireshark nmap metasploit volatility autopsy\n");
                        break;
                    case 6:
                        fprintf(config_file, "Type=custom\n");
                        fprintf(config_file, "Packages=manual_selection\n");
                        break;
                }

                fclose(config_file);

                clear();
                mvprintw(5, 10, "Configuration saved to lainux-config.txt");
                mvprintw(7, 10, "This configuration will be used during installation.");
                refresh();
                sleep(2);
            }
            break;
        } else if (ch == 27) {
            break;
        }
    }
}

// Show installation summary
void show_summary(const char *disk) {
    clear();

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int center_x = max_x / 2 - 25;

    attron(A_BOLD | COLOR_PAIR(1));

    mvprintw(3, center_x, "│         %s   │", _("INSTALL_COMPLETE"));
    attroff(A_BOLD | COLOR_PAIR(1));

    mvprintw(6, center_x + 5, "Lainux has been successfully installed!");

    mvprintw(8, center_x + 5, "Installation target:");
    attron(COLOR_PAIR(2));
    mvprintw(9, center_x + 5, "  /dev/%s", disk);
    attroff(COLOR_PAIR(2));

    mvprintw(11, center_x + 5, "Default credentials:");
    mvprintw(12, center_x + 10, "Username: root");
    mvprintw(13, center_x + 10, "Password: lainux");

    mvprintw(14, center_x + 10, "Username: lainux");
    mvprintw(15, center_x + 10, "Password: lainux");

    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(17, center_x + 5, "⚠ Remove installation media before rebooting!");
    attroff(COLOR_PAIR(3) | A_BOLD);

    mvprintw(19, center_x + 5, "Next steps:");
    mvprintw(20, center_x + 10, "1. Remove installation media");
    mvprintw(21, center_x + 10, "2. Reboot the system");
    mvprintw(22, center_x + 10, "3. Log in with credentials above");
    mvprintw(23, center_x + 10, "4. Run 'lainux-setup' for post-installation");

    mvprintw(25, center_x + 5, "Press R to reboot now");
    mvprintw(26, center_x + 5, "Press Q to shutdown installer");
    mvprintw(27, center_x + 5, "Press any other key to return to menu");

    while (1) {
        int ch = getch();
        if (ch == 'r' || ch == 'R') {
            if (confirm_action("Reboot system now?", "REBOOT")) {
                run_command("reboot", 0);
            }
            break;
        } else if (ch == 'q' || ch == 'Q') {
            if (confirm_action("Exit installer?", "EXIT")) {
                endwin();
                exit(0);
            }
        } else {
            break;
        }
    }
}

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
                        select_configuration();
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
