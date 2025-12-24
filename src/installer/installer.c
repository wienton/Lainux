/**
* @brief Lainux Installer
* @version v0.3 beta only
* @author Wienton | Lainux Development Laboratory
* @license: GPL-3.0
* @details contact with general developer LainuxOS and this installer you can from telegram @openrtc
* @status: STATUS WORK **TRUE**

* @update:
* * feat: center-align all UI elements including logo, menu, and footer

    Dynamically calculate terminal width to center logo, version info, time, and navigation hints
    Implement automatic menu alignment based on the longest item width
    Display compiler version (GCC) in footer using __VERSION__
    Ensure consistent and responsive layout across different terminal sizes

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
// Configuration
#define CORE_URL "https://github.com/wienton/Lainux/raw/main/lainux-core-0.1-1-x86_64.pkg.tar.zst"
#define FALLBACK_CORE_URL "https://mirror.lainux.org/core/lainux-core-0.1-1-x86_64.pkg.tar.zst"
#define ARCH_ISO_URL "https://archlinux.gay/archlinux/iso/2025.12.01/archlinux-2025.12.01-x86_64.iso" // links for arch iso download
#define MAX_DISKS 32 // max count disks
#define MAX_PATH 512 // max path count
#define LOG_BUFFER_SIZE 8192
#define INSTALL_TIMEOUT 3600

// Error codes
#define ERR_SUCCESS 0
#define ERR_IO_FAILURE 1
#define ERR_NETWORK 2
#define ERR_DEPENDENCY 3
#define ERR_PERMISSION 4
#define ERR_DISK_SPACE 5

// Global structures
typedef struct {
    char name[64];
    char size[32];
    char model[128];
    char type[16];
} DiskInfo;

typedef struct {
    int total_cores;
    int avail_cores;
    long total_ram;
    long avail_ram;
    char arch[32];
    char hostname[64];
    char kernel[64];
} SystemInfo;

// Global variables
WINDOW *log_win;
WINDOW *status_win;
char log_buffer[LOG_BUFFER_SIZE];
volatile int install_running = 0;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void init_ncurses();
void cleanup_ncurses();
void signal_handler(int sig);
int file_exists(const char *path);
int check_filesystem(const char *path);
long get_available_space(const char *path);
int check_network();
int check_dependencies();
void log_message(const char *format, ...);
int run_command(const char *cmd, int show_output);
int run_command_with_fallback(const char *cmd, const char *fallback);
void get_target_disk(char *target, size_t size);
void show_disk_info();
int confirm_action(const char *question, const char *required_input);
void create_partitions(const char *disk);
void perform_installation(const char *disk);
void show_summary(const char *disk);
void install_on_virtual_machine();
int check_qemu_dependencies();
void create_virtual_disk();
void setup_qemu_vm(const char *iso_path);
void download_arch_iso();
void select_iso_file(char *iso_path, size_t size);
int find_iso_files(char files[][MAX_PATH], int max_files);
void show_hardware_info();
void select_configuration();
void show_logo();
void get_hardware_details(char *cpu_info, char *memory_info, char *gpu_info, char *storage_info);
void draw_progress_bar(int y, int x, int width, float progress);
void check_system_requirements();
int verify_efi();
int secure_wipe(const char *device);
void emergency_cleanup();
void *download_thread(void *arg);
int download_file(const char *url, const char *output);
void get_system_info(SystemInfo *info);
void display_status(const char *message);
int mount_with_retry(const char *source, const char *target, const char *fstype, unsigned long flags);

// Initialize ncurses with error handling
void init_ncurses() {
    setlocale(LC_ALL, "");

    if (initscr() == NULL) {
        fprintf(stderr, "Error initializing ncurses\n");
        exit(1);
    }

    start_color();
    use_default_colors();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);

    // Initialize color pairs
    init_pair(1, COLOR_CYAN, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_RED, -1);
    init_pair(4, COLOR_YELLOW, -1);
    init_pair(5, COLOR_MAGENTA, -1);
    init_pair(6, COLOR_BLUE, -1);
    init_pair(7, COLOR_WHITE, -1);
    init_pair(8, COLOR_BLACK, COLOR_CYAN);    // Selected item
    init_pair(9, COLOR_BLACK, COLOR_RED);     // Error background
    init_pair(10, COLOR_BLACK, COLOR_GREEN);  // Success background
}

// Cleanup function
void cleanup_ncurses() {
    if (log_win) delwin(log_win);
    if (status_win) delwin(status_win);
    endwin();
}

// Signal handler for graceful exit
void signal_handler(int sig) {
    install_running = 0;
    log_message("Signal %d received, cleaning up...", sig);
    emergency_cleanup();
    cleanup_ncurses();
    exit(0);
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

// Check filesystem health
int check_filesystem(const char *path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "fsck -n %s > /dev/null 2>&1", path);
    return (system(cmd) == 0);
}

// Get available space in MB
long get_available_space(const char *path) {
    struct statvfs st;
    if (statvfs(path, &st) == 0) {
        return (st.f_bavail * st.f_frsize) / (1024 * 1024);
    }
    return 0;
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
// Thread-safe logging with timestamp
void log_message(const char *format, ...) {
    pthread_mutex_lock(&log_mutex);

    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Только если log_win создан - пишем в него
    if (log_win) {
        wprintw(log_win, "[%s] %s\n", timestamp, message);
        wrefresh(log_win);
    } else {
        // Иначе просто в stdout (для отладки)
        printf("[%s] %s\n", timestamp, message);
        fflush(stdout);
    }

    pthread_mutex_unlock(&log_mutex);
}
// Execute command with detailed error handling
int run_command(const char *cmd, int show_output) {
    log_message("Executing: %s", cmd);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        log_message("Failed to execute command");
        return -1;
    }

    if (show_output) {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), fp)) {
            buffer[strcspn(buffer, "\n")] = 0;
            if (strlen(buffer) > 0) {
                log_message("%s", buffer);
            }
        }
    } else {
        // Just consume output
        while (fgetc(fp) != EOF);
    }

    int status = pclose(fp);
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            log_message("Exit code: %d", exit_code);
        }
        return exit_code;
    }

    return -1;
}

// Run command with fallback option
int run_command_with_fallback(const char *cmd, const char *fallback) {
    int result = run_command(cmd, 0);
    if (result != 0 && fallback != NULL) {
        log_message("Primary command failed, trying fallback...");
        result = run_command(fallback, 0);
    }
    return result;
}

// Display status message
void display_status(const char *message) {
    if (status_win) {
        wclear(status_win);
        box(status_win, 0, 0);
        mvwprintw(status_win, 1, 2, " STATUS: %s", message);
        wrefresh(status_win);
    }
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
        info->avail_ram = si.freeram / (1024 * 1024);
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

// Verify EFI support
int verify_efi() {
    if (file_exists("/sys/firmware/efi")) {
        return 1;
    }

    // Check via efibootmgr
    int result = run_command("efibootmgr > /dev/null 2>&1", 0);
    return (result == 0 || result == 1);  // Exit code 1 means no entries but EFI present
}

// Secure wipe (optional)
int secure_wipe(const char *device) {
    log_message("Performing secure wipe on %s...", device);

    // Quick wipe with dd (can be enhanced with multiple passes)
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "dd if=/dev/zero of=%s bs=1M count=10 status=progress 2>/dev/null", device);
    return run_command(cmd, 1);
}

// Emergency cleanup
void emergency_cleanup() {
    log_message("Performing emergency cleanup...");

    // Unmount anything mounted under /mnt
    run_command("umount -R /mnt 2>/dev/null || true", 0);
    run_command("swapoff -a 2>/dev/null || true", 0);

    // Remove temporary files
    run_command("rm -f /tmp/lainux-*.tmp 2>/dev/null || true", 0);
    run_command("rm -f /mnt/root/core.pkg.tar.zst 2>/dev/null || true", 0);
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

// Mount with retry logic
int mount_with_retry(const char *source, const char *target, const char *fstype, unsigned long flags) {
    int retries = 3;
    int delay = 1;

    while (retries > 0) {
        if (mount(source, target, fstype, flags, NULL) == 0) {
            return 0;
        }

        log_message("Mount failed (retry %d): %s", 4 - retries, strerror(errno));
        retries--;
        sleep(delay);
        delay *= 2;
    }

    return -1;
}

// Enhanced confirmation with timeout
int confirm_action(const char *question, const char *required_input) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    WINDOW *confirm_win = newwin(8, max_x - 20, max_y/2 - 4, 10);
    box(confirm_win, 0, 0);
    mvwprintw(confirm_win, 1, 2, "%s", question);
    mvwprintw(confirm_win, 2, 2, "Type '%s' to confirm (ESC to cancel):", required_input);
    wrefresh(confirm_win);

    char input[32] = {0};
    int pos = 0;
    int ch;

    flushinp();

    while (pos < (int)sizeof(input) - 1) {
        ch = wgetch(confirm_win);
        if (ch == ERR) continue; // timeout

        if (ch == 27) { // ESC
            delwin(confirm_win);
            return 0;
        }
        if (ch == '\n' || ch == '\r') { // Enter
            break;
        }
        if (ch == KEY_BACKSPACE || ch == 127) {
            if (pos > 0) {
                pos--;
                input[pos] = '\0';
                mvwaddch(confirm_win, 3, 4 + pos, ' ');
                wmove(confirm_win, 3, 4 + pos);
            }
        } else if (ch >= 32 && ch <= 126) { // printable ASCII
            input[pos++] = (char)ch;
            input[pos] = '\0';
            mvwaddch(confirm_win, 3, 4 + pos - 1, ch);
        }

        wrefresh(confirm_win);
    }

    delwin(confirm_win);
    return (strcmp(input, required_input) == 0);
}
// Enhanced disk info display
void show_disk_info() {
    clear();

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(2, 5, "STORAGE DEVICE INFORMATION");
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

// Create partitions with enhanced error handling
void create_partitions(const char *disk) {
    char dev_path[32];
    snprintf(dev_path, sizeof(dev_path), "/dev/%s", disk);

    if (!file_exists(dev_path)) {
        log_message("Target device not found: %s", dev_path);
        return;
    }

    // Check if device is mounted
    char check_cmd[256];
    snprintf(check_cmd, sizeof(check_cmd), "mount | grep -q '^%s'", dev_path);
    if (system(check_cmd) == 0) {
        log_message("Device %s is mounted. Attempting to unmount...", dev_path);
        snprintf(check_cmd, sizeof(check_cmd), "umount %s* 2>/dev/null", dev_path);
        run_command(check_cmd, 0);
        sleep(1);
    }

    log_message("Creating partition table on %s...", dev_path);

    // Clear existing partitions with multiple methods
    char cmd[512];

    // Method 1: sgdisk zap
    snprintf(cmd, sizeof(cmd), "sgdisk --zap-all %s 2>/dev/null", dev_path);
    if (run_command(cmd, 0) != 0) {
        // Method 2: dd wipe partition table
        log_message("sgdisk failed, trying alternative method...");
        snprintf(cmd, sizeof(cmd), "dd if=/dev/zero of=%s bs=512 count=1 conv=notrunc 2>/dev/null", dev_path);
        run_command(cmd, 0);

        // Refresh kernel partition table
        run_command("partprobe 2>/dev/null", 0);
        sleep(2);
    }

    // Create GPT partition table
    log_message("Creating GPT partition table...");
    snprintf(cmd, sizeof(cmd), "sgdisk --clear %s", dev_path);
    if (run_command(cmd, 0) != 0) {
        log_message("Failed to create GPT, trying fallback...");
        snprintf(cmd, sizeof(cmd), "parted -s %s mklabel gpt", dev_path);
        run_command(cmd, 0);
    }

    // Create EFI partition (550MB)
    log_message("Creating EFI system partition (550MB)...");
    snprintf(cmd, sizeof(cmd), "sgdisk --new=1:0:+550M --typecode=1:ef00 %s", dev_path);
    if (run_command(cmd, 0) != 0) {
        snprintf(cmd, sizeof(cmd), "parted -s %s mkpart primary fat32 1MiB 551MiB", dev_path);
        run_command(cmd, 0);
        snprintf(cmd, sizeof(cmd), "parted -s %s set 1 esp on", dev_path);
        run_command(cmd, 0);
    }

    // Create root partition (rest of disk)
    log_message("Creating root partition...");
    snprintf(cmd, sizeof(cmd), "sgdisk --new=2:0:0 --typecode=2:8304 %s", dev_path);
    if (run_command(cmd, 0) != 0) {
        snprintf(cmd, sizeof(cmd), "parted -s %s mkpart primary ext4 551MiB 100%%", dev_path);
        run_command(cmd, 0);
    }

    // Update kernel partition table
    log_message("Updating partition table...");
    run_command_with_fallback("partprobe", "blockdev --rereadpt");
    sleep(3);

    // Verify partitions were created
    char part1[32], part2[32];
    if (strncmp(disk, "nvme", 4) == 0 || strncmp(disk, "vd", 2) == 0) {
        snprintf(part1, sizeof(part1), "%sp1", dev_path);
        snprintf(part2, sizeof(part2), "%sp2", dev_path);
    } else {
        snprintf(part1, sizeof(part1), "%s1", dev_path);
        snprintf(part2, sizeof(part2), "%s2", dev_path);
    }

    int attempts = 0;
    while ((!file_exists(part1) || !file_exists(part2)) && attempts < 15) {
        log_message("Waiting for partitions to appear (attempt %d)...", attempts + 1);
        sleep(1);
        attempts++;
        run_command("udevadm settle 2>/dev/null", 0);
    }

    if (!file_exists(part1) || !file_exists(part2)) {
        log_message("Partition creation failed. Expected: %s, %s", part1, part2);
        log_message("Trying manual check...");
        snprintf(cmd, sizeof(cmd), "ls -la %s*", dev_path);
        run_command(cmd, 1);
    } else {
        log_message("Partitions created successfully");
    }
}

// Download Arch Linux ISO with progress
void download_arch_iso() {
    log_message("Downloading Arch Linux ISO...");

    // Check existing file
    if (file_exists("archlinux.iso")) {
        log_message("Existing ISO found, checking integrity...");
        long size = 0;
        struct stat st;
        if (stat("archlinux.iso", &st) == 0) {
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
    snprintf(cmd, sizeof(cmd), "wget -c --timeout=30 --tries=3 '%s' -O archlinux.iso 2>&1 | grep --line-buffered -E '([0-9]+)%%|speed'", ARCH_ISO_URL);
    int result = run_command(cmd, 1);

    if (result != 0) {
        log_message("wget failed, trying curl...");
        snprintf(cmd, sizeof(cmd), "curl -L -C - --connect-timeout 30 --retry 3 '%s' -o archlinux.iso 2>&1 | grep --line-buffered -E '([0-9]+[.][0-9]*%%)|speed'", ARCH_ISO_URL);
        run_command(cmd, 1);
    }

    if (file_exists("archlinux.iso")) {
        struct stat st;
        if (stat("archlinux.iso", &st) == 0) {
            log_message("Download complete: %ld MB", st.st_size / (1024 * 1024));
        }
    } else {
        log_message("Failed to download Arch Linux ISO");
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
    mvprintw(5, 15, "1. Download latest Arch Linux ISO");
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
            mvprintw(5, 10, "Downloading latest Arch Linux ISO...");
            mvprintw(6, 10, "This may take several minutes depending on your connection.");
            refresh();

            // Create download thread
            pthread_t download_thread_id;
            char *args[2];
            args[0] = (char*)ARCH_ISO_URL;
            args[1] = "archlinux.iso";

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

            if (file_exists("archlinux.iso")) {
                strncpy(iso_path, "archlinux.iso", size);

                // Verify ISO size
                struct stat st;
                if (stat("archlinux.iso", &st) == 0) {
                    clear();
                    mvprintw(5, 10, "Download complete!");
                    mvprintw(6, 10, "File: archlinux.iso");
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

// Check QEMU dependencies
int check_qemu_dependencies() {
    const char *qemu_tools[] = {
        "qemu-system-x86_64", "qemu-img",
        NULL
    };

    int missing = 0;
    for (int i = 0; qemu_tools[i] != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "command -v %s > /dev/null 2>&1", qemu_tools[i]);
        if (system(cmd) != 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Missing QEMU tool: %s", qemu_tools[i]);
            log_message("%s", msg);
            missing++;
        }
    }

    if (missing > 0) {
        log_message("Installing QEMU virtualization tools...");

        char pkg_manager[32] = "";
        if (file_exists("/usr/bin/pacman")) strcpy(pkg_manager, "pacman");
        else if (file_exists("/usr/bin/apt-get")) strcpy(pkg_manager, "apt");
        else if (file_exists("/usr/bin/dnf")) strcpy(pkg_manager, "dnf");
        else if (file_exists("/usr/bin/yum")) strcpy(pkg_manager, "yum");

        if (strcmp(pkg_manager, "pacman") == 0) {
            run_command("pacman -Sy --noconfirm --needed qemu libvirt virt-manager virt-viewer", 1);
        } else if (strcmp(pkg_manager, "apt") == 0) {
            run_command("apt-get update && apt-get install -y qemu-system-x86 qemu-utils libvirt-clients libvirt-daemon-system virt-manager", 1);
        } else if (strcmp(pkg_manager, "dnf") == 0 || strcmp(pkg_manager, "yum") == 0) {
            run_command("dnf install -y qemu-kvm libvirt virt-manager", 1);
        }

        // Enable libvirt service
        run_command("systemctl enable --now libvirtd 2>/dev/null || true", 0);
        run_command("systemctl enable --now virtlogd 2>/dev/null || true", 0);
    }

    return (missing == 0);
}

// Create virtual disk
void create_virtual_disk() {
    log_message("Creating virtual disk image...");

    // Check available space
    long free_space = get_available_space(".");
    if (free_space < 25000) {  // Need ~25GB for VM
        log_message("Insufficient disk space: %ld MB available, need 25000 MB", free_space);
        return;
    }

    // Create 20GB qcow2 disk image with compression
    run_command("qemu-img create -f qcow2 -o compression_type=zlib lainux-vm.qcow2 20G", 1);

    log_message("Virtual disk created: lainux-vm.qcow2");
}

// Setup QEMU virtual machine
void setup_qemu_vm(const char *iso_path) {
    log_message("Setting up QEMU virtual machine...");

    // Create installation script
    FILE *script = fopen("install-lainux-vm.sh", "w");
    if (script) {
        fprintf(script, "#!/bin/bash\n");
        fprintf(script, "# Lainux VM Installation Script\n");
        fprintf(script, "# Generated on: %s", ctime(&(time_t){time(NULL)}));
        fprintf(script, "\n");
        fprintf(script, "echo 'Starting Lainux VM installation...'\n");
        fprintf(script, "echo 'ISO: %s'\n", iso_path);
        fprintf(script, "\n");

        // Check for KVM support
        fprintf(script, "KVM_SUPPORT=$(grep -E '(vmx|svm)' /proc/cpuinfo 2>/dev/null | head -1)\n");
        fprintf(script, "if [ -n \"$KVM_SUPPORT\" ]; then\n");
        fprintf(script, "  echo 'KVM acceleration available'\n");
        fprintf(script, "  ACCEL=\"-enable-kvm -cpu host\"\n");
        fprintf(script, "else\n");
        fprintf(script, "  echo 'Running without KVM acceleration'\n");
        fprintf(script, "  ACCEL=\"-cpu qemu64\"\n");
        fprintf(script, "fi\n");
        fprintf(script, "\n");

        // Check if running as root
        fprintf(script, "if [ \"$EUID\" -ne 0 ]; then\n");
        fprintf(script, "  echo 'Warning: Not running as root. Some features may be limited.'\n");
        fprintf(script, "fi\n");
        fprintf(script, "\n");

        // VM configuration
        fprintf(script, "# VM Configuration\n");
        fprintf(script, "MEMORY=\"4096\"\n");
        fprintf(script, "CORES=\"$(nproc)\"\n");
        fprintf(script, "if [ \"$CORES\" -gt 4 ]; then\n");
        fprintf(script, "  CORES=\"4\"\n");
        fprintf(script, "fi\n");
        fprintf(script, "\n");
        fprintf(script, "echo \"Starting VM with ${CORES} cores and ${MEMORY}MB RAM\"\n");
        fprintf(script, "\n");

        // Start VM command
        fprintf(script, "qemu-system-x86_64 \\\n");
        fprintf(script, "  $ACCEL \\\n");
        fprintf(script, "  -m $MEMORY \\\n");
        fprintf(script, "  -smp $CORES \\\n");
        fprintf(script, "  -drive file=lainux-vm.qcow2,format=qcow2 \\\n");
        fprintf(script, "  -cdrom \"%s\" \\\n", iso_path);
        fprintf(script, "  -boot order=d \\\n");
        fprintf(script, "  -netdev user,id=net0 \\\n");
        fprintf(script, "  -device virtio-net,netdev=net0 \\\n");
        fprintf(script, "  -vga virtio \\\n");
        fprintf(script, "  -display sdl,gl=on \\\n");
        fprintf(script, "  -usb \\\n");
        fprintf(script, "  -device usb-tablet \\\n");
        fprintf(script, "  -soundhw hda \\\n");
        fprintf(script, "  -rtc base=utc \\\n");
        fprintf(script, "  -name \"Lainux-VM\"\n");

        fclose(script);
        run_command("chmod +x install-lainux-vm.sh", 0);
    }

    // Create README file
    script = fopen("VM-README.txt", "w");
    if (script) {
        fprintf(script, "Lainux Virtual Machine Installation\n");
        fprintf(script, "====================================\n\n");
        fprintf(script, "Files created:\n");
        fprintf(script, "1. lainux-vm.qcow2    - Virtual disk (20GB)\n");
        fprintf(script, "2. install-lainux-vm.sh - Installation script\n\n");
        fprintf(script, "To start the virtual machine:\n");
        fprintf(script, "  sudo ./install-lainux-vm.sh\n\n");
        fprintf(script, "Recommended settings:\n");
        fprintf(script, "- Ensure virtualization is enabled in BIOS/UEFI\n");
        fprintf(script, "- Run as root for best performance\n");
        fprintf(script, "- At least 8GB host RAM recommended\n\n");
        fprintf(script, "Once the VM starts:\n");
        fprintf(script, "1. Follow the on-screen instructions\n");
        fprintf(script, "2. Install to the virtual disk\n");
        fprintf(script, "3. Reboot the VM after installation\n");
        fclose(script);
    }

    log_message("VM setup complete. Scripts created.");
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

// Show Lainux logo with enhanced design
void show_logo() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int start_x = (max_x - 62) / 2; // длина самой длинной строки логотипа

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(1, start_x, "╔══════════════════════════════════════════════════════════════╗");
    mvprintw(2, start_x, "║                                                              ║");
    mvprintw(3, start_x, "║      ██╗      █████╗ ██╗███╗   ██╗██╗   ██╗██╗  ██╗          ║");
    mvprintw(4, start_x, "║      ██║     ██╔══██╗██║████╗  ██║██║   ██║╚██╗██╔╝          ║");
    mvprintw(5, start_x, "║      ██║     ███████║██║██╔██╗ ██║██║   ██║ ╚███╔╝           ║");
    mvprintw(6, start_x, "║      ██║     ██╔══██║██║██║╚██╗██║██║   ██║ ██╔██╗           ║");
    mvprintw(7, start_x, "║      ███████╗██║  ██║██║██║ ╚████║╚██████╔╝██╔╝ ██╗          ║");
    mvprintw(8, start_x, "║      ╚══════╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝  ╚═╝          ║");
    mvprintw(9, start_x, "║                                                              ║");
    mvprintw(10, start_x, "╚══════════════════════════════════════════════════════════════╝");
    attroff(A_BOLD | COLOR_PAIR(1));

    // Центрированные подписи
    int slogan1_x = (max_x - (int)strlen("Development Laboratory")) / 2;
    int slogan2_x = (max_x - (int)strlen("Simplicity in design, security in execution")) / 2;
    int slogan3_x = (max_x - (int)strlen("Minimalism with purpose, freedom with responsibility")) / 2;

    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(12, slogan1_x, "Development Laboratory");
    attroff(COLOR_PAIR(2) | A_BOLD);

    attron(COLOR_PAIR(7));
    mvprintw(13, slogan2_x, "Simplicity in design, security in execution");
    mvprintw(14, slogan3_x, "Minimalism with purpose, freedom with responsibility");
    attroff(COLOR_PAIR(7));
}

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

// Install on virtual machine
void install_on_virtual_machine() {
    clear();

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(2, 10, "VIRTUAL MACHINE INSTALLATION");
    attroff(A_BOLD | COLOR_PAIR(1));

    mvprintw(4, 10, "This will create a QEMU virtual machine for Lainux.");
    mvprintw(5, 10, "Requirements:");
    mvprintw(6, 15, "- KVM or virtualization support in CPU");
    mvprintw(7, 15, "- 20GB free disk space");
    mvprintw(8, 15, "- 4GB RAM available");
    mvprintw(9, 15, "- Internet connection (for downloads)");

    mvprintw(11, 10, "Continue with VM setup? (y/N): ");

    echo();
    char confirm[2];
    mvgetnstr(11, 40, confirm, sizeof(confirm));
    noecho();

    if (confirm[0] != 'y' && confirm[0] != 'Y') {
        return;
    }

    // Create log window
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    log_win = newwin(max_y - 10, max_x - 10, 5, 5);
    scrollok(log_win, TRUE);
    box(log_win, 0, 0);
    wrefresh(log_win);

    log_message("Starting virtual machine installation...");

    // Check virtualization support
    FILE *fp = popen("grep -E '(vmx|svm)' /proc/cpuinfo", "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp)) {
            log_message("Virtualization support detected: %s", buffer);
        } else {
            log_message("Warning: No hardware virtualization support detected");
            log_message("VM will run in software emulation mode (slower)");
        }
        pclose(fp);
    }

    // Check QEMU dependencies
    if (!check_qemu_dependencies()) {
        log_message("Failed to install QEMU dependencies");
        delwin(log_win);
        log_win = NULL;
        return;
    }

    // Network check
    if (!check_network()) {
        log_message("Warning: No network connectivity");
        if (!confirm_action("Continue without network?", "CONTINUE")) {
            log_message("VM installation cancelled");
            delwin(log_win);
            log_win = NULL;
            return;
        }
    }

    // Select ISO file
    delwin(log_win);
    log_win = NULL;

    char iso_path[MAX_PATH];
    select_iso_file(iso_path, sizeof(iso_path));

    if (iso_path[0] == '\0') {
        log_message("ISO selection cancelled");
        return;
    }

    // Recreate log window
    log_win = newwin(max_y - 10, max_x - 10, 5, 5);
    scrollok(log_win, TRUE);
    box(log_win, 0, 0);
    wrefresh(log_win);

    log_message("Selected ISO: %s", iso_path);

    if (!file_exists(iso_path)) {
        log_message("ISO file not found: %s", iso_path);
        delwin(log_win);
        log_win = NULL;
        return;
    }

    // Create virtual disk
    create_virtual_disk();

    // Setup VM
    setup_qemu_vm(iso_path);

    log_message("Virtual machine setup complete!");

    delwin(log_win);
    log_win = NULL;

    // Show completion message
    clear();

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(3, 20, "╭────────────────────────────────────────────────────╮");
    mvprintw(4, 20, "│        VIRTUAL MACHINE SETUP COMPLETE             │");
    mvprintw(5, 20, "╰────────────────────────────────────────────────────╯");
    attroff(A_BOLD | COLOR_PAIR(1));

    mvprintw(7, 25, "Files created in current directory:");
    attron(COLOR_PAIR(2));
    mvprintw(8, 30, "lainux-vm.qcow2        - 20GB virtual disk");
    mvprintw(9, 30, "install-lainux-vm.sh   - VM startup script");
    mvprintw(10, 30, "VM-README.txt         - Instructions");
    attroff(COLOR_PAIR(2));

    mvprintw(12, 25, "To start the virtual machine:");
    attron(A_BOLD);
    mvprintw(13, 30, "sudo ./install-lainux-vm.sh");
    attroff(A_BOLD);

    mvprintw(15, 25, "VM Specifications:");
    mvprintw(16, 30, "CPU: 4 cores (or available cores)");
    mvprintw(17, 30, "RAM: 4GB");
    mvprintw(18, 30, "Disk: 20GB (qcow2 format)");
    mvprintw(19, 30, "ISO: %s", iso_path);

    attron(COLOR_PAIR(4) | A_BOLD);
    mvprintw(21, 25, "Note: Run with sudo for best performance");
    attroff(COLOR_PAIR(4) | A_BOLD);

    mvprintw(23, 25, "Press any key to return to menu...");
    refresh();
    getch();
}

// Show installation summary
void show_summary(const char *disk) {
    clear();

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int center_x = max_x / 2 - 25;

    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(2, center_x, "╭────────────────────────────────────────────╮");
    mvprintw(3, center_x, "│         INSTALLATION COMPLETE              │");
    mvprintw(4, center_x, "╰────────────────────────────────────────────╯");
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
        "Install on Hardware",
        "Install on Virtual Machine",
        "Hardware Information",
        "System Requirements Check",
        "Configuration Selection",
        "View Disk Information",
        "Check Network",
        "Network Diagnostics",
        "Exit Installer"
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
        int info1_x = (max_x - (int)strlen("Version 1.0 | UEFI Ready | Secure Boot Compatible")) / 2;
        int info2_x = (max_x - (int)strlen("// Where simplicity meets security")) / 2;

        attron(COLOR_PAIR(7));
        mvprintw(16, info1_x, "Version 1.0 | UEFI Ready | Secure Boot Compatible");
        mvprintw(17, info2_x, "// Where simplicity meets security");
        attroff(COLOR_PAIR(7));

        // Display current time
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        int time_x = (max_x - (int)strlen("System time: 2025-12-25 23:59:59")) / 2;
        mvprintw(19, time_x, "System time: %s", time_str);
        attroff(COLOR_PAIR(7));

        int menu_count = sizeof(menu_items) / sizeof(menu_items[0]); // = 9
        // Вычисляем позицию начала самого длинного пункта меню
        int max_item_len = 0;
        for (int i = 0; i < menu_count; i++) {
            int len = strlen(menu_items[i]);
            if (len > max_item_len) max_item_len = len;
        }
        // Добавляем 4 символа на "› " и отступ
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
                } else if (i == 8) {
                    attron(A_REVERSE | COLOR_PAIR(3));  // Red for exit
                } else  {
                    attron(A_REVERSE | COLOR_PAIR(7));  // White for others
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
                } else if (i == 8) {
                    attroff(A_REVERSE | COLOR_PAIR(3));
                } else {
                    attroff(A_REVERSE | COLOR_PAIR(7));
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
                } else if (i == 8) {
                    attron(COLOR_PAIR(3));
                    mvprintw(22 + i * 2, menu_start_x + 2, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(3));
                } else {
                    attron(COLOR_PAIR(7));
                    mvprintw(22 + i * 2, menu_start_x + 2, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(7));
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
        int right_col = max_x - 30; // 30 symbols with rigth

        mvprintw(max_y - 3, right_col, "Arch: %s", arch);
        mvprintw(max_y - 2, right_col, "Kernel: %s", kernel);
        mvprintw(max_y - 1, right_col, "Built with: GCC %s", __VERSION__);

        int input = getch();
        switch (input) {
            case KEY_UP:
                menu_selection = (menu_selection > 0) ? menu_selection - 1 : 8;
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
                    case 6: // Network Check
                        clear();
                        mvprintw(5, 10, "Network status: ");
                        if (check_network()) {
                            attron(COLOR_PAIR(2));
                            mvprintw(5, 27, "Connected ✓");
                            attroff(COLOR_PAIR(2));

                            // Show IP address
                            FILE *fp = popen("curl -s https://checkip.amazonaws.com 2>/dev/null || echo 'Unknown'", "r");
                            char ip[64];
                            if (fp) {
                                fgets(ip, sizeof(ip), fp);
                                ip[strcspn(ip, "\n")] = 0;
                                pclose(fp);
                                mvprintw(7, 10, "Public IP: %s", ip);
                            }
                        } else {
                            attron(COLOR_PAIR(3));
                            mvprintw(5, 27, "No connection ✗");
                            attroff(COLOR_PAIR(3));
                        }
                        mvprintw(9, 10, "Press any key...");
                        refresh();
                        getch();
                        break;
                    case 7:
                        clear();
                        mvprintw(5, 10, "Starting network sniff...");
                        refresh();

                        char* ifname = get_first_active_interface();
                        if (ifname) {
                            mvprintw(6, 10, "Interface: %s", ifname);
                            mvprintw(7, 10, "Sniffing for 5 seconds...");
                            refresh();

                            int pkts = start_passive_sniff(ifname, 5);
                            mvprintw(8, 12, "Packets captured: %d", pkts);
                            free(ifname);
                        } else {
                            attron(COLOR_PAIR(3));
                            mvprintw(6, 10, "No active interface found!");
                            attroff(COLOR_PAIR(3));
                        }

                        mvprintw(10, 10, "Press any key...");
                        refresh();
                        getch();
                        break;
                    case 8: // Exit
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
