/**
* @brief Lainux Installer
* @version v0.3 beta only
* @author Wienton | Lainux Development Laboratory
* @license: GPL-3.0
* @details contact with general developer LainuxOS and this installer you can from telegram  @adaxies
* @status: 1
* @also: download iso from github release for virtual machine.

  @hosting: codeberg, github
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
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <curl/curl.h>
#include <ctype.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// general header for installer

#include "include/installer.h"

#include "utils/log_message.h"
#include "utils/run_command.h"
#include "locale/lang.h"
#include "turbo/turbo_installer.h"
#include "cleanup/cleaner.h"

// ui
#include "ui/ui.h"

#define LINK_ISO "" // TODO: ADD LINK FOR ISO FOR DOWNLOAD
#define PACKAGE_LINK "" // TODO: ADD PACKAGE LINKS FOR DOWNLOAD

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
