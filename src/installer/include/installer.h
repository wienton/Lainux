#ifndef INSTALLER_H
#define INSTALLER_H


#include <pthread.h>
#include <ncurses.h>
// Configuration
#define CORE_URL "https://github.com/wienton/Lainux/raw/main/lainux-core-0.1-1-x86_64.pkg.tar.zst" // core(kernel) lainux from github
#define FALLBACK_CORE_URL "https://mirror.lainux.org/core/lainux-core-0.1-1-x86_64.pkg.tar.zst"
#define ARCH_ISO_URL "https://github.com/wienton/Lainux/releases/download/lainuxiso/lainuxiso-2025.12.25-x86_64.iso" // links for arch iso download
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

// struct of the system info
typedef struct {
    int total_cores;
    int avail_cores;
    long total_ram;
    long avail_ram;
    char arch[32];
    char hostname[64];
    char kernel[64];
} SystemInfo;

extern pthread_mutex_t log_mutex;

// // Global variables
// static WINDOW *log_win;
// static WINDOW *status_win;
// static char log_buffer[LOG_BUFFER_SIZE];
// static volatile int install_running = 0;
// static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

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

#endif
