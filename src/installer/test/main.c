/**
 * @brief Lainux Installer - Professional Version
 * @details Secure, minimalist installation system with advanced features
 */

#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <locale.h>
#include <dirent.h>
#include <errno.h>

#define CORE_URL "https://github.com/wienton/Lainux/raw/main/lainux-core-0.1-1-x86_64.pkg.tar.zst"
#define MAX_DISKS 20
#define MAX_PATH 256
#define ARCH_ISO_URL "https://archlinux.org/iso/latest/archlinux-x86_64.iso"

WINDOW *log_win;

// Function prototypes
int file_exists(const char *path);
int check_network();
int check_dependencies();
void log_message(const char *text);
int run_command(const char *cmd);
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

// Check if file/device exists
int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

// Network connectivity check
int check_network() {
    char cmd[256];
    
    // Try ping to Google DNS
    strcpy(cmd, "ping -c 1 -W 3 8.8.8.8 > /dev/null 2>&1");
    if (system(cmd) == 0) return 1;
    
    // Try DNS resolution
    strcpy(cmd, "curl -s --connect-timeout 3 http://google.com > /dev/null 2>&1");
    if (system(cmd) == 0) return 1;
    
    return 0;
}

// Check for QEMU dependencies
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
            log_message(msg);
            missing++;
        }
    }
    
    if (missing > 0) {
        log_message("Installing QEMU virtualization tools...");
        
        if (file_exists("/etc/arch-release")) {
            run_command("pacman -Sy --noconfirm qemu libvirt virt-manager");
        } 
        else if (file_exists("/etc/debian_version")) {
            run_command("apt-get update && apt-get install -y qemu-system-x86 qemu-utils libvirt-clients libvirt-daemon-system");
        }
        else if (file_exists("/etc/fedora-release")) {
            run_command("dnf install -y qemu-kvm libvirt");
        }
        
        // Enable libvirt service
        run_command("systemctl enable --now libvirtd 2>/dev/null || true");
    }
    
    return (missing == 0);
}

// Check for required tools
int check_dependencies() {
    const char *tools[] = {
        "arch-chroot", "pacstrap", "mkfs.fat", "mkfs.ext4",
        "sgdisk", "mount", "umount", "wget", "curl", "grub-install",
        "lsblk", "genfstab", NULL
    };
    
    int missing = 0;
    for (int i = 0; tools[i] != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "command -v %s > /dev/null 2>&1", tools[i]);
        if (system(cmd) != 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Missing tool: %s", tools[i]);
            log_message(msg);
            missing++;
        }
    }
    
    if (missing > 0) {
        log_message("Installing missing dependencies...");
        
        if (file_exists("/etc/arch-release")) {
            run_command("pacman -Sy --noconfirm arch-install-scripts dosfstools e2fsprogs gptfdisk wget grub efibootmgr");
        } 
        else if (file_exists("/etc/debian_version")) {
            run_command("apt-get update && apt-get install -y arch-install-scripts dosfstools e2fsprogs gdisk grub-efi-amd64");
        }
    }
    
    return (missing == 0);
}

// Log output with timestamp
void log_message(const char *text) {
    if (log_win) {
        wprintw(log_win, " > %s\n", text);
        wrefresh(log_win);
    } else {
        printw(" > %s\n", text);
        refresh();
    }
}

// Execute command with logging
int run_command(const char *cmd) {
    log_message(cmd);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        log_message("Command execution failed");
        return -1;
    }
    
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\n")] = 0;
        if (strlen(buf) > 0) {
            log_message(buf);
        }
    }
    
    int status = pclose(fp);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        char err[256];
        snprintf(err, sizeof(err), "Exit code: %d", WEXITSTATUS(status));
        log_message(err);
    }
    
    return status;
}

// User confirmation with required input
int confirm_action(const char *question, const char *required_input) {
    echo();
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    WINDOW *confirm_win = newwin(7, max_x - 20, max_y/2 - 3, 10);
    box(confirm_win, 0, 0);
    
    mvwprintw(confirm_win, 1, 2, "%s", question);
    mvwprintw(confirm_win, 2, 2, "Type '%s' to confirm: ", required_input);
    
    char input[32];
    wmove(confirm_win, 3, 4);
    wrefresh(confirm_win);
    
    wgetnstr(confirm_win, input, sizeof(input) - 1);
    
    delwin(confirm_win);
    noecho();
    
    return (strcmp(input, required_input) == 0);
}

// Display disk information
void show_disk_info() {
    clear();
    
    mvprintw(2, 5, "Current storage configuration:");
    mvprintw(3, 5, "--------------------------------");
    
    run_command("lsblk -o NAME,SIZE,TYPE,MOUNTPOINT,FSTYPE | grep -E '^[s|n|v]'");
    
    mvprintw(20, 5, "Press any key to continue...");
    refresh();
    getch();
}

// Get hardware details
void get_hardware_details(char *cpu_info, char *memory_info, char *gpu_info, char *storage_info) {
    // Get CPU info
    FILE *fp = popen("lscpu | grep 'Model name' | cut -d: -f2 | head -1", "r");
    if (fp) {
        fgets(cpu_info, 128, fp);
        cpu_info[strcspn(cpu_info, "\n")] = 0;
        pclose(fp);
    } else {
        strcpy(cpu_info, "Unknown");
    }
    
    // Get memory info
    fp = popen("free -h | grep Mem | awk '{print $2}'", "r");
    if (fp) {
        fgets(memory_info, 32, fp);
        memory_info[strcspn(memory_info, "\n")] = 0;
        pclose(fp);
    } else {
        strcpy(memory_info, "Unknown");
    }
    
    // Get GPU info
    fp = popen("lspci | grep -i vga | cut -d: -f3 | head -1", "r");
    if (fp) {
        fgets(gpu_info, 128, fp);
        gpu_info[strcspn(gpu_info, "\n")] = 0;
        pclose(fp);
    } else {
        strcpy(gpu_info, "Unknown");
    }
    
    // Get storage info
    fp = popen("lsblk -d | grep disk | wc -l", "r");
    if (fp) {
        fgets(storage_info, 32, fp);
        storage_info[strcspn(storage_info, "\n")] = 0;
        pclose(fp);
        
        // Add disk count
        char temp[32];
        fp = popen("lsblk -d | grep disk | head -1 | awk '{print $4}'", "r");
        if (fp) {
            fgets(temp, 32, fp);
            temp[strcspn(temp, "\n")] = 0;
            pclose(fp);
            strcat(storage_info, " disks, ");
            strcat(storage_info, temp);
        }
    } else {
        strcpy(storage_info, "Unknown");
    }
}

// Show hardware information with better formatting
void show_hardware_info() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Get hardware details
    char cpu_info[128], memory_info[32], gpu_info[128], storage_info[64];
    get_hardware_details(cpu_info, memory_info, gpu_info, storage_info);
    
    clear();
    
    // Title
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(2, max_x/2 - 15, "SYSTEM HARDWARE INFORMATION");
    attroff(A_BOLD | COLOR_PAIR(1));
    
    // Separator
    mvprintw(3, max_x/2 - 15, "───────────────────────────────");
    
    // Display hardware info in organized format
    mvprintw(5, 10, "CPU:");
    attron(COLOR_PAIR(2));
    mvprintw(5, 20, "%s", cpu_info);
    attroff(COLOR_PAIR(2));
    
    mvprintw(6, 10, "Memory:");
    attron(COLOR_PAIR(2));
    mvprintw(6, 20, "%s RAM", memory_info);
    attroff(COLOR_PAIR(2));
    
    mvprintw(7, 10, "GPU:");
    attron(COLOR_PAIR(2));
    mvprintw(7, 20, "%s", gpu_info);
    attroff(COLOR_PAIR(2));
    
    mvprintw(8, 10, "Storage:");
    attron(COLOR_PAIR(2));
    mvprintw(8, 20, "%s", storage_info);
    attroff(COLOR_PAIR(2));
    
    // Additional system information
    mvprintw(10, 10, "Detailed Information:");
    
    // Get number of CPU cores
    FILE *fp = popen("nproc", "r");
    char cores[16];
    if (fp) {
        fgets(cores, sizeof(cores), fp);
        cores[strcspn(cores, "\n")] = 0;
        pclose(fp);
        mvprintw(11, 15, "CPU Cores: %s", cores);
    }
    
    // Get architecture
    fp = popen("uname -m", "r");
    char arch[32];
    if (fp) {
        fgets(arch, sizeof(arch), fp);
        arch[strcspn(arch, "\n")] = 0;
        pclose(fp);
        mvprintw(12, 15, "Architecture: %s", arch);
    }
    
    // Check virtualization support
    fp = popen("grep -E '(vmx|svm)' /proc/cpuinfo 2>/dev/null | head -1", "r");
    if (fp) {
        char virt[256];
        if (fgets(virt, sizeof(virt), fp)) {
            mvprintw(13, 15, "Virtualization: Supported");
        } else {
            mvprintw(13, 15, "Virtualization: Not available");
        }
        pclose(fp);
    }
    
    // Get kernel version
    fp = popen("uname -r", "r");
    char kernel[64];
    if (fp) {
        fgets(kernel, sizeof(kernel), fp);
        kernel[strcspn(kernel, "\n")] = 0;
        pclose(fp);
        mvprintw(14, 15, "Kernel: %s", kernel);
    }
    
    // Get hostname
    fp = popen("hostname", "r");
    char hostname[64];
    if (fp) {
        fgets(hostname, sizeof(hostname), fp);
        hostname[strcspn(hostname, "\n")] = 0;
        pclose(fp);
        mvprintw(15, 15, "Hostname: %s", hostname);
    }
    
    // Get uptime
    fp = popen("uptime -p", "r");
    char uptime[64];
    if (fp) {
        fgets(uptime, sizeof(uptime), fp);
        uptime[strcspn(uptime, "\n")] = 0;
        pclose(fp);
        mvprintw(16, 15, "Uptime: %s", uptime);
    }
    
    // Footer
    mvprintw(max_y - 2, max_x/2 - 15, "Press any key to continue...");
    refresh();
    getch();
}

// Get target disk with safety checks
void get_target_disk(char *target, size_t size) {
    clear();
    
    // Warning message
    attron(A_BOLD | COLOR_PAIR(3));
    mvprintw(2, 10, "WARNING: ALL DATA ON SELECTED DISK WILL BE PERMANENTLY ERASED!");
    mvprintw(3, 10, "Ensure you have backups of important data.");
    attroff(A_BOLD | COLOR_PAIR(3));
    
    mvprintw(5, 5, "Press any key to view available disks...");
    refresh();
    getch();
    
    // Show current disks
    clear();
    mvprintw(2, 5, "Available storage devices:");
    
    char cmd[] = "lsblk -dno NAME,SIZE,MODEL | grep -E '^sd|^nvme|^vd'";
    FILE *fp = popen(cmd, "r");
    
    char disks[MAX_DISKS][256];
    int count = 0;
    
    while (fgets(disks[count], sizeof(disks[count]), fp) && count < MAX_DISKS) {
        disks[count][strcspn(disks[count], "\n")] = 0;
        count++;
    }
    pclose(fp);
    
    if (count == 0) {
        mvprintw(5, 5, "No disks found. Please check connections.");
        refresh();
        sleep(2);
        strcpy(target, "");
        return;
    }
    
    int selected = 0;
    while (1) {
        clear();
        
        mvprintw(2, 5, "Select installation target:");
        mvprintw(3, 5, "Use ↑/↓ arrows, ENTER to select, ESC to cancel");
        
        for (int i = 0; i < count; i++) {
            if (i == selected) {
                attron(A_REVERSE | COLOR_PAIR(2));
                mvprintw(5 + i, 7, "→ /dev/%-40s", disks[i]);
                attroff(A_REVERSE | COLOR_PAIR(2));
            } else {
                mvprintw(5 + i, 9, "/dev/%-40s", disks[i]);
            }
        }
        
        mvprintw(5 + count + 2, 7, "Selected: /dev/%.10s", disks[selected]);
        
        int ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected > 0) ? selected - 1 : count - 1;
                break;
            case KEY_DOWN:
                selected = (selected < count - 1) ? selected + 1 : 0;
                break;
            case 10: // ENTER
                sscanf(disks[selected], "%s", target);
                
                // Final confirmation
                clear();
                char question[256];
                snprintf(question, sizeof(question), 
                        "CONFIRM ERASE: ALL data on /dev/%s will be deleted!", target);
                
                if (confirm_action(question, "ERASE")) {
                    return;
                }
                break;
            case 27: // ESC
                strcpy(target, "");
                return;
        }
    }
}

// Create partitions safely
void create_partitions(const char *disk) {
    char dev_path[32];
    snprintf(dev_path, sizeof(dev_path), "/dev/%s", disk);
    
    if (!file_exists(dev_path)) {
        log_message("Target device not found");
        return;
    }
    
    log_message("Creating partition table...");
    
    // Clear existing partitions
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "sgdisk --zap-all %s", dev_path);
    run_command(cmd);
    
    // Create GPT
    snprintf(cmd, sizeof(cmd), "sgdisk --clear %s", dev_path);
    run_command(cmd);
    
    // EFI partition (550MB for safety)
    snprintf(cmd, sizeof(cmd), "sgdisk --new=1:0:+550M --typecode=1:ef00 %s", dev_path);
    run_command(cmd);
    
    // Root partition (remainder)
    snprintf(cmd, sizeof(cmd), "sgdisk --new=2:0:0 --typecode=2:8304 %s", dev_path);
    run_command(cmd);
    
    // Update kernel partition table
    run_command("partprobe");
    sleep(2);
}

// Download Arch Linux ISO
void download_arch_iso() {
    log_message("Downloading Arch Linux ISO...");
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "wget -c %s -O archlinux.iso 2>&1 | tail -1", ARCH_ISO_URL);
    run_command(cmd);
    
    if (file_exists("archlinux.iso")) {
        log_message("Arch Linux ISO downloaded successfully");
    } else {
        log_message("Failed to download Arch Linux ISO");
    }
}

// Find ISO files in current directory
int find_iso_files(char files[][MAX_PATH], int max_files) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    
    dir = opendir(".");
    if (dir == NULL) {
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL && count < max_files) {
        if (entry->d_type == DT_REG) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext != NULL && (strcmp(ext, ".iso") == 0 || strcmp(ext, ".ISO") == 0)) {
                strncpy(files[count], entry->d_name, MAX_PATH - 1);
                files[count][MAX_PATH - 1] = '\0';
                count++;
            }
        }
    }
    
    closedir(dir);
    return count;
}

// Select ISO file from directory or download
void select_iso_file(char *iso_path, size_t size) {
    clear();
    
    mvprintw(2, 10, "ISO Selection");
    mvprintw(3, 10, "─────────────");
    
    mvprintw(5, 10, "Choose ISO source:");
    mvprintw(6, 15, "1. Download Arch Linux ISO");
    mvprintw(7, 15, "2. Use existing ISO file");
    mvprintw(8, 15, "3. Cancel");
    
    mvprintw(10, 10, "Enter choice (1-3): ");
    
    echo();
    char choice[2];
    mvgetnstr(10, 30, choice, sizeof(choice));
    noecho();
    
    switch (choice[0]) {
        case '1':
            // Download Arch ISO
            clear();
            mvprintw(5, 10, "Downloading Arch Linux ISO...");
            refresh();
            
            download_arch_iso();
            
            if (file_exists("archlinux.iso")) {
                strncpy(iso_path, "archlinux.iso", size);
                mvprintw(7, 10, "Download complete. Using archlinux.iso");
                refresh();
                sleep(2);
            } else {
                mvprintw(7, 10, "Download failed. Please try option 2.");
                refresh();
                sleep(2);
                iso_path[0] = '\0';
            }
            break;
            
        case '2':
            // Select from existing files
            clear();
            char iso_files[20][MAX_PATH];
            int file_count = find_iso_files(iso_files, 20);
            
            if (file_count == 0) {
                mvprintw(5, 10, "No ISO files found in current directory.");
                mvprintw(6, 10, "Please place an ISO file here and try again.");
                refresh();
                sleep(3);
                iso_path[0] = '\0';
                break;
            }
            
            mvprintw(2, 10, "Select ISO file:");
            mvprintw(3, 10, "────────────────");
            
            int selected = 0;
            while (1) {
                clear();
                mvprintw(2, 10, "Select ISO file:");
                mvprintw(3, 10, "────────────────");
                mvprintw(4, 10, "Use ↑/↓ arrows, ENTER to select, ESC to cancel");
                
                for (int i = 0; i < file_count; i++) {
                    if (i == selected) {
                        attron(A_REVERSE | COLOR_PAIR(2));
                        mvprintw(6 + i, 12, "→ %s", iso_files[i]);
                        attroff(A_REVERSE | COLOR_PAIR(2));
                    } else {
                        mvprintw(6 + i, 14, "%s", iso_files[i]);
                    }
                }
                
                int ch = getch();
                if (ch == KEY_UP) {
                    selected = (selected > 0) ? selected - 1 : file_count - 1;
                } else if (ch == KEY_DOWN) {
                    selected = (selected < file_count - 1) ? selected + 1 : 0;
                } else if (ch == 10) {
                    strncpy(iso_path, iso_files[selected], size);
                    break;
                } else if (ch == 27) {
                    iso_path[0] = '\0';
                    break;
                }
            }
            break;
            
        case '3':
        default:
            iso_path[0] = '\0';
            break;
    }
}

// Create virtual disk for QEMU
void create_virtual_disk() {
    log_message("Creating virtual disk image...");
    
    // Create 20GB qcow2 disk image
    run_command("qemu-img create -f qcow2 lainux-vm.qcow2 20G");
    
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
        fprintf(script, "echo 'Starting Lainux VM installation...'\n");
        fprintf(script, "\n");
        fprintf(script, "# Start VM with installation media\n");
        fprintf(script, "qemu-system-x86_64 \\\n");
        fprintf(script, "  -m 4096 \\\n");
        fprintf(script, "  -smp 4 \\\n");
        fprintf(script, "  -drive file=lainux-vm.qcow2,format=qcow2 \\\n");
        fprintf(script, "  -cdrom \"%s\" \\\n", iso_path);
        fprintf(script, "  -boot d \\\n");
        fprintf(script, "  -net nic \\\n");
        fprintf(script, "  -net user \\\n");
        fprintf(script, "  -vga virtio \\\n");
        fprintf(script, "  -display gtk \\\n");
        
        // Check for KVM support
        fprintf(script, "if grep -q -E '(vmx|svm)' /proc/cpuinfo; then\n");
        fprintf(script, "  echo 'KVM acceleration enabled'\n");
        fprintf(script, "  qemu-system-x86_64 -enable-kvm \\\n");
        fprintf(script, "    -m 4096 \\\n");
        fprintf(script, "    -smp 4 \\\n");
        fprintf(script, "    -drive file=lainux-vm.qcow2,format=qcow2 \\\n");
        fprintf(script, "    -cdrom \"%s\" \\\n", iso_path);
        fprintf(script, "    -boot d \\\n");
        fprintf(script, "    -net nic \\\n");
        fprintf(script, "    -net user \\\n");
        fprintf(script, "    -vga virtio \\\n");
        fprintf(script, "    -display gtk\n");
        fprintf(script, "else\n");
        fprintf(script, "  echo 'Running without KVM acceleration'\n");
        fprintf(script, "  qemu-system-x86_64 \\\n");
        fprintf(script, "    -m 4096 \\\n");
        fprintf(script, "    -smp 4 \\\n");
        fprintf(script, "    -drive file=lainux-vm.qcow2,format=qcow2 \\\n");
        fprintf(script, "    -cdrom \"%s\" \\\n", iso_path);
        fprintf(script, "    -boot d \\\n");
        fprintf(script, "    -net nic \\\n");
        fprintf(script, "    -net user \\\n");
        fprintf(script, "    -vga virtio \\\n");
        fprintf(script, "    -display gtk\n");
        fprintf(script, "fi\n");
        
        fclose(script);
        
        run_command("chmod +x install-lainux-vm.sh");
    }
    
    log_message("VM setup complete.");
}

// Show Lainux logo
void show_logo() {
    attron(A_BOLD | COLOR_PAIR(1));
    
    // Stylish Lainux logo
    mvprintw(3, 25, "╔══════════════════════════════════════════════════════╗");
    mvprintw(4, 25, "║                                                      ║");
    mvprintw(5, 25, "║      ██╗      █████╗ ██╗███╗   ██╗██╗   ██╗██╗  ██╗  ║");
    mvprintw(6, 25, "║      ██║     ██╔══██╗██║████╗  ██║██║   ██║╚██╗██╔╝  ║");
    mvprintw(7, 25, "║      ██║     ███████║██║██╔██╗ ██║██║   ██║ ╚███╔╝   ║");
    mvprintw(8, 25, "║      ██║     ██╔══██║██║██║╚██╗██║██║   ██║ ██╔██╗   ║");
    mvprintw(9, 25, "║      ███████╗██║  ██║██║██║ ╚████║╚██████╔╝██╔╝ ██╗  ║");
    mvprintw(10, 25, "║      ╚══════╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝  ╚═╝  ║");
    mvprintw(11, 25, "║                                                      ║");
    mvprintw(12, 25, "╚══════════════════════════════════════════════════════╝");
    
    attroff(A_BOLD | COLOR_PAIR(1));
    
    // Subtitle
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(14, 35, "Development Laboratory");
    attroff(COLOR_PAIR(2) | A_BOLD);
}

// Select configuration with CyberSecurity option
void select_configuration() {
    clear();
    
    mvprintw(2, 10, "Configuration Selection");
    mvprintw(3, 10, "───────────────────────");
    
    mvprintw(5, 10, "Available configurations:");
    
    const char *configs[] = {
        "Minimal (Base system only)",
        "Standard (Base + Desktop Environment)",
        "Development (Standard + Dev tools)",
        "Server (Minimal + Server packages)",
        "Security Hardened (Standard + Security tools)",
        "CyberSecurity (Advanced security tools)",
        "Custom (Manual package selection)",
        NULL
    };
    
    int selected = 0;
    int config_count = 7;
    
    while (1) {
        clear();
        
        mvprintw(2, 10, "Select Configuration:");
        mvprintw(3, 10, "─────────────────────");
        mvprintw(4, 10, "Use ↑/↓ arrows, ENTER to select, ESC to cancel");
        
        for (int i = 0; i < config_count; i++) {
            if (i == selected) {
                attron(A_REVERSE | COLOR_PAIR(2));
                mvprintw(6 + i, 12, "→ %s", configs[i]);
                attroff(A_REVERSE | COLOR_PAIR(2));
            } else {
                mvprintw(6 + i, 14, "%s", configs[i]);
            }
        }
        
        mvprintw(6 + config_count + 1, 10, "Selected: %s", configs[selected]);
        
        int ch = getch();
        if (ch == KEY_UP) {
            selected = (selected > 0) ? selected - 1 : config_count - 1;
        } else if (ch == KEY_DOWN) {
            selected = (selected < config_count - 1) ? selected + 1 : 0;
        } else if (ch == 10) {
            // Configuration selected
            clear();
            mvprintw(5, 10, "Configuration: %s", configs[selected]);
            mvprintw(7, 10, "This configuration will be applied during installation.");
            mvprintw(9, 10, "Configuration details:");
            
            // Show configuration details
            switch (selected) {
                case 0:
                    mvprintw(11, 12, "• Base Lainux system");
                    mvprintw(12, 12, "• Linux kernel");
                    mvprintw(13, 12, "• Essential utilities");
                    mvprintw(14, 12, "• ~500MB disk space");
                    break;
                case 1:
                    mvprintw(11, 12, "• Everything from Minimal");
                    mvprintw(12, 12, "• Xorg window system");
                    mvprintw(13, 12, "• Desktop environment");
                    mvprintw(14, 12, "• Network manager");
                    mvprintw(15, 12, "• ~2GB disk space");
                    break;
                case 2:
                    mvprintw(11, 12, "• Everything from Standard");
                    mvprintw(12, 12, "• GCC compiler and toolchain");
                    mvprintw(13, 12, "• Python, Node.js, Ruby");
                    mvprintw(14, 12, "• Git, Make, CMake, Docker");
                    mvprintw(15, 12, "• IDE and development tools");
                    mvprintw(16, 12, "• ~4GB disk space");
                    break;
                case 3:
                    mvprintw(11, 12, "• Everything from Minimal");
                    mvprintw(12, 12, "• SSH server (OpenSSH)");
                    mvprintw(13, 12, "• Web server (nginx)");
                    mvprintw(14, 12, "• Database (PostgreSQL, Redis)");
                    mvprintw(15, 12, "• Monitoring tools");
                    mvprintw(16, 12, "• ~1.5GB disk space");
                    break;
                case 4:
                    mvprintw(11, 12, "• Everything from Standard");
                    mvprintw(12, 12, "• Firewall (ufw, iptables)");
                    mvprintw(13, 12, "• Security scanning tools");
                    mvprintw(14, 12, "• Encryption tools (GPG, OpenSSL)");
                    mvprintw(15, 12, "• Audit tools");
                    mvprintw(16, 12, "• ~2.5GB disk space");
                    break;
                case 5:
                    mvprintw(11, 12, "• Everything from Security Hardened");
                    mvprintw(12, 12, "• Network analysis (Wireshark, tcpdump)");
                    mvprintw(13, 12, "• Penetration testing tools");
                    mvprintw(14, 12, "• Forensic analysis tools");
                    mvprintw(15, 12, "• Malware analysis");
                    mvprintw(16, 12, "• Advanced monitoring");
                    mvprintw(17, 12, "• ~3.5GB disk space");
                    break;
                case 6:
                    mvprintw(11, 12, "• Manual package selection");
                    mvprintw(12, 12, "• Interactive package chooser");
                    mvprintw(13, 12, "• Custom kernel options");
                    mvprintw(14, 12, "• Custom partitioning");
                    mvprintw(15, 12, "• Variable disk space");
                    break;
            }
            
            mvprintw(19, 10, "Save this configuration? (y/N): ");
            
            echo();
            char confirm[2];
            mvgetnstr(19, 40, confirm, sizeof(confirm));
            noecho();
            
            if (confirm[0] == 'y' || confirm[0] == 'Y') {
                // Save configuration to file
                FILE *config_file = fopen("lainux-config.txt", "w");
                if (config_file) {
                    fprintf(config_file, "Configuration: %s\n", configs[selected]);
                    fprintf(config_file, "Selected at: %ld\n", time(NULL));
                    
                    // Add configuration details
                    switch (selected) {
                        case 0:
                            fprintf(config_file, "Type: Minimal\n");
                            fprintf(config_file, "Disk Space: ~500MB\n");
                            break;
                        case 1:
                            fprintf(config_file, "Type: Standard\n");
                            fprintf(config_file, "Disk Space: ~2GB\n");
                            break;
                        case 2:
                            fprintf(config_file, "Type: Development\n");
                            fprintf(config_file, "Disk Space: ~4GB\n");
                            break;
                        case 3:
                            fprintf(config_file, "Type: Server\n");
                            fprintf(config_file, "Disk Space: ~1.5GB\n");
                            break;
                        case 4:
                            fprintf(config_file, "Type: Security Hardened\n");
                            fprintf(config_file, "Disk Space: ~2.5GB\n");
                            break;
                        case 5:
                            fprintf(config_file, "Type: CyberSecurity\n");
                            fprintf(config_file, "Disk Space: ~3.5GB\n");
                            break;
                        case 6:
                            fprintf(config_file, "Type: Custom\n");
                            fprintf(config_file, "Disk Space: Variable\n");
                            break;
                    }
                    
                    fclose(config_file);
                    
                    clear();
                    mvprintw(5, 10, "Configuration saved to lainux-config.txt");
                    mvprintw(7, 10, "This configuration will be used during installation.");
                    refresh();
                    sleep(2);
                }
            }
            
            break;
        } else if (ch == 27) {
            break;
        }
    }
}

// Main installation procedure
void perform_installation(const char *disk) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Create log window
    log_win = newwin(max_y - 12, max_x - 10, 10, 5);
    scrollok(log_win, TRUE);
    box(log_win, 0, 0);
    wrefresh(log_win);
    
    char dev_path[32];
    char efi_part[32], root_part[32];
    
    snprintf(dev_path, sizeof(dev_path), "/dev/%s", disk);
    
    // Determine partition names
    if (strncmp(disk, "nvme", 4) == 0) {
        snprintf(efi_part, sizeof(efi_part), "%sp1", dev_path);
        snprintf(root_part, sizeof(root_part), "%sp2", dev_path);
    } else {
        snprintf(efi_part, sizeof(efi_part), "%s1", dev_path);
        snprintf(root_part, sizeof(root_part), "%s2", dev_path);
    }
    
    // Pre-installation checks
    log_message("Performing system checks...");
    
    if (!check_dependencies()) {
        log_message("Dependency check failed");
        return;
    }
    
    if (!check_network()) {
        log_message("Network connectivity issue detected");
        if (!confirm_action("Continue without network?", "CONTINUE")) {
            log_message("Installation aborted");
            return;
        }
    }
    
    // Partitioning
    log_message("Partitioning target disk...");
    create_partitions(disk);
    
    // Wait for partitions
    int attempts = 0;
    while (!file_exists(efi_part) && attempts < 10) {
        log_message("Waiting for partitions...");
        sleep(1);
        attempts++;
    }
    
    if (!file_exists(efi_part)) {
        log_message("Partition creation failed");
        return;
    }
    
    // Formatting
    log_message("Formatting partitions...");
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkfs.fat -F32 %s", efi_part);
    if (run_command(cmd) != 0) {
        log_message("EFI format failed");
    }
    
    snprintf(cmd, sizeof(cmd), "mkfs.ext4 -F %s", root_part);
    if (run_command(cmd) != 0) {
        log_message("Root format failed");
    }
    
    // Mounting
    log_message("Mounting filesystems...");
    
    run_command("mkdir -p /mnt");
    snprintf(cmd, sizeof(cmd), "mount %s /mnt", root_part);
    run_command(cmd);
    
    run_command("mkdir -p /mnt/boot");
    snprintf(cmd, sizeof(cmd), "mount %s /mnt/boot", efi_part);
    run_command(cmd);
    
    // Base system installation
    log_message("Installing base system...");
    run_command("pacstrap /mnt base linux-hardened linux-firmware base-devel");
    
    log_message("Generating fstab...");
    run_command("genfstab -U /mnt >> /mnt/etc/fstab");
    
    // Core system configuration
    log_message("Configuring system...");
    
    run_command("arch-chroot /mnt ln -sf /usr/share/zoneinfo/UTC /etc/localtime");
    run_command("arch-chroot /mnt hwclock --systohc");
    run_command("echo 'en_US.UTF-8 UTF-8' > /mnt/etc/locale.gen");
    run_command("arch-chroot /mnt locale-gen");
    run_command("echo 'LANG=en_US.UTF-8' > /mnt/etc/locale.conf");
    run_command("echo 'lainux' > /mnt/etc/hostname");
    
    // Install Lainux core
    log_message("Installing Lainux core...");
    snprintf(cmd, sizeof(cmd), 
            "arch-chroot /mnt wget -qO /root/core.pkg.tar.zst %s && "
            "arch-chroot /mnt pacman -U /root/core.pkg.tar.zst --noconfirm", 
            CORE_URL);
    run_command(cmd);
    
    // Bootloader
    log_message("Installing bootloader...");
    snprintf(cmd, sizeof(cmd), 
            "arch-chroot /mnt grub-install --target=x86_64-efi "
            "--efi-directory=/boot --bootloader-id=lainux");
    run_command(cmd);
    
    run_command("arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg");
    
    // Set root password
    run_command("echo 'root:lainux' | arch-chroot /mnt chpasswd");
    
    // Cleanup
    log_message("Cleaning up...");
    run_command("umount -R /mnt");
    run_command("rm -f /mnt/root/core.pkg.tar.zst 2>/dev/null");
    
    log_message("Installation complete!");
    
    delwin(log_win);
    log_win = NULL;
    
    show_summary(disk);
}x

// Install on virtual machine
void install_on_virtual_machine() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Create log window
    log_win = newwin(max_y - 12, max_x - 10, 10, 5);
    scrollok(log_win, TRUE);
    box(log_win, 0, 0);
    wrefresh(log_win);
    
    log_message("Starting virtual machine installation...");
    
    // Check virtualization support
    log_message("Checking virtualization support...");
    run_command("grep -E '(vmx|svm)' /proc/cpuinfo | head -2");
    
    // Check QEMU dependencies
    if (!check_qemu_dependencies()) {
        log_message("QEMU dependencies check failed");
        return;
    }
    
    // Network check
    if (!check_network()) {
        log_message("Network required for VM installation");
        if (!confirm_action("Continue without network?", "CONTINUE")) {
            log_message("VM installation aborted");
            return;
        }
    }
    
    // Select ISO file
    char iso_path[MAX_PATH];
    log_message("Selecting installation media...");
    
    delwin(log_win);
    log_win = NULL;
    
    select_iso_file(iso_path, sizeof(iso_path));
    
    if (iso_path[0] == '\0') {
        log_message("ISO selection cancelled");
        return;
    }
    
    // Recreate log window
    log_win = newwin(max_y - 12, max_x - 10, 10, 5);
    scrollok(log_win, TRUE);
    box(log_win, 0, 0);
    wrefresh(log_win);
    
    log_message("Selected ISO:");
    log_message(iso_path);
    
    if (!file_exists(iso_path)) {
        log_message("ISO file not found");
        return;
    }
    
    // Create virtual disk
    create_virtual_disk();
    
    // Setup VM with selected ISO
    setup_qemu_vm(iso_path);
    
    log_message("Virtual machine setup complete!");
    
    // Show VM instructions
    delwin(log_win);
    log_win = NULL;
    
    clear();
    int center_x = max_x / 2 - 30;
    
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw(3, center_x, "╭────────────────────────────────────────────────────╮");
    mvprintw(4, center_x, "│        VIRTUAL MACHINE READY                      │");
    mvprintw(5, center_x, "╰────────────────────────────────────────────────────╯");
    attroff(A_BOLD | COLOR_PAIR(1));
    
    mvprintw(7, center_x + 5, "Lainux virtual machine has been prepared.");
    mvprintw(9, center_x + 5, "Files created:");
    attron(COLOR_PAIR(2));
    mvprintw(10, center_x + 10, "lainux-vm.qcow2        - 20GB virtual disk");
    mvprintw(11, center_x + 10, "install-lainux-vm.sh   - Installation script");
    attroff(COLOR_PAIR(2));
    
    mvprintw(13, center_x + 5, "To start the virtual machine:");
    mvprintw(14, center_x + 10, "Run: sudo ./install-lainux-vm.sh");
    
    mvprintw(16, center_x + 5, "VM Specifications:");
    mvprintw(17, center_x + 10, "CPU: 4 cores | RAM: 4GB | Disk: 20GB");
    mvprintw(18, center_x + 10, "ISO: %s", iso_path);
    
    mvprintw(20, center_x + 5, "Installation will start in the VM window.");
    mvprintw(21, center_x + 5, "Follow the on-screen instructions in the VM.");
    
    attron(A_BOLD | COLOR_PAIR(3));
    mvprintw(23, center_x + 5, "Note: Run the script with sudo for best performance");
    attroff(A_BOLD | COLOR_PAIR(3));
    
    mvprintw(25, center_x + 5, "Press any key to return to menu...");
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
    mvprintw(3, center_x, "╭────────────────────────────────────────────╮");
    mvprintw(4, center_x, "│         INSTALLATION COMPLETE              │");
    mvprintw(5, center_x, "╰────────────────────────────────────────────╯");
    attroff(A_BOLD | COLOR_PAIR(1));
    
    mvprintw(7, center_x + 5, "Lainux has been successfully installed on:");
    attron(COLOR_PAIR(2));
    mvprintw(8, center_x + 5, "  /dev/%s", disk);
    attroff(COLOR_PAIR(2));
    
    mvprintw(10, center_x + 5, "System will boot from this device.");
    
    mvprintw(12, center_x + 5, "Default credentials:");
    mvprintw(13, center_x + 10, "Username: root");
    mvprintw(14, center_x + 10, "Password: lainux");
    
    attron(A_BOLD | COLOR_PAIR(3));
    mvprintw(16, center_x + 5, "Remove installation media before rebooting!");
    attroff(A_BOLD | COLOR_PAIR(3));
    
    mvprintw(18, center_x + 5, "Press R to reboot now");
    mvprintw(19, center_x + 5, "Press Q to shutdown installer");
    
    while (1) {
        int ch = getch();
        if (ch == 'r' || ch == 'R') {
            if (confirm_action("Reboot system now?", "REBOOT")) {
                run_command("reboot");
            }
            break;
        } else if (ch == 'q' || ch == 'Q') {
            if (confirm_action("Exit installer?", "EXIT")) {
                break;
            }
        }
    }
}

// Main application
int main() {
    // Initialize locale and terminal
    setlocale(LC_ALL, "");
    
    initscr();
    start_color();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);
    
    // Color pairs
    init_pair(1, COLOR_CYAN, COLOR_BLACK);    // Titles and logo
    init_pair(2, COLOR_GREEN, COLOR_BLACK);   // Success/Highlight
    init_pair(3, COLOR_RED, COLOR_BLACK);     // Warnings/Errors
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);  // VM option
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK); // Hardware Info
    init_pair(6, COLOR_BLUE, COLOR_BLACK);    // Configuration
    init_pair(7, COLOR_WHITE, COLOR_BLACK);   // Normal text
    
    char target_disk[32] = "";
    int menu_selection = 0;
    const char *menu_items[] = {
        "Install on Hardware",
        "Install on Virtual Machine",
        "Hardware Information",
        "Configuration Selection",
        "View Disk Information",
        "Check Network",
        "Exit"
    };
    
    while (1) {
        clear();
        
        // Show Lainux logo
        show_logo();
        
        // System information line
        mvprintw(14, 35, "// I hope your RAM is ready for my malloc");
        attroff(COLOR_PAIR(7));
        attron(COLOR_PAIR(7));
        mvprintw(16, 30, "Secure Minimalist Linux Distribution");
        mvprintw(17, 35, "Version 1.0 | UEFI Required");
        attroff(COLOR_PAIR(7));
        
        // Menu
        for (int i = 0; i < 7; i++) {
            if (i == menu_selection) {
                // Highlight current selection
                if (i == 1) {
                    attron(A_REVERSE | COLOR_PAIR(4)); // Yellow for VM
                } else if (i == 2) {
                    attron(A_REVERSE | COLOR_PAIR(5)); // Magenta for Hardware Info
                } else if (i == 3) {
                    attron(A_REVERSE | COLOR_PAIR(6)); // Blue for Configuration
                } else {
                    attron(A_REVERSE | COLOR_PAIR(2)); // Green for others
                }
                mvprintw(20 + i * 2, 35, "› %s", menu_items[i]);
                // Turn off attributes
                if (i == 1) {
                    attroff(A_REVERSE | COLOR_PAIR(4));
                } else if (i == 2) {
                    attroff(A_REVERSE | COLOR_PAIR(5));
                } else if (i == 3) {
                    attroff(A_REVERSE | COLOR_PAIR(6));
                } else {
                    attroff(A_REVERSE | COLOR_PAIR(2));
                }
            } else {
                // Normal display
                if (i == 1) {
                    attron(COLOR_PAIR(4)); // Yellow for VM
                    mvprintw(20 + i * 2, 37, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(4));
                } else if (i == 2) {
                    attron(COLOR_PAIR(5)); // Magenta for Hardware Info
                    mvprintw(20 + i * 2, 37, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(5));
                } else if (i == 3) {
                    attron(COLOR_PAIR(6)); // Blue for Configuration
                    mvprintw(20 + i * 2, 37, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(6));
                } else {
                    attron(COLOR_PAIR(7));
                    mvprintw(20 + i * 2, 37, "%s", menu_items[i]);
                    attroff(COLOR_PAIR(7));
                }
            }
        }
        
        // Footer with navigation info
        attron(COLOR_PAIR(7) | A_BOLD);
        mvprintw(36, 25, "Navigate: ↑↓  Select: Enter  Exit: Esc");
        attroff(COLOR_PAIR(7) | A_BOLD);
        
        // Get and display system info in footer
        FILE *fp = popen("uname -m", "r");
        char arch[32];
        if (fp) {
            fgets(arch, sizeof(arch), fp);
            arch[strcspn(arch, "\n")] = 0;
            pclose(fp);
            mvprintw(37, 25, "Architecture: %s", arch);
        }
        
        // Check virtualization
        fp = popen("grep -E '(vmx|svm)' /proc/cpuinfo 2>/dev/null | head -1", "r");
        if (fp) {
            char virt[256];
            if (fgets(virt, sizeof(virt), fp)) {
                attron(COLOR_PAIR(2));
                mvprintw(37, 50, "Virtualization: ✓");
                attroff(COLOR_PAIR(2));
            } else {
                attron(COLOR_PAIR(3));
                mvprintw(37, 50, "Virtualization: ✗");
                attroff(COLOR_PAIR(3));
            }
            pclose(fp);
        }
        
        int input = getch();
        switch (input) {
            case KEY_UP:
                menu_selection = (menu_selection > 0) ? menu_selection - 1 : 6;
                break;
            case KEY_DOWN:
                menu_selection = (menu_selection < 6) ? menu_selection + 1 : 0;
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
                        clear();
                        mvprintw(5, 10, "Virtual Machine Installation");
                        mvprintw(6, 10, "─────────────────────────────");
                        mvprintw(8, 10, "This will set up a QEMU virtual machine");
                        mvprintw(9, 10, "and prepare it for Lainux installation.");
                        mvprintw(11, 10, "Requirements:");
                        mvprintw(12, 15, "- ISO file (will be downloaded or selected)");
                        mvprintw(13, 15, "- 20GB free disk space");
                        mvprintw(14, 15, "- Internet connection (for downloading)");
                        mvprintw(16, 10, "Continue with VM setup?");
                        mvprintw(18, 10, "Press Y to continue, any other key to cancel");
                        
                        refresh();
                        int ch = getch();
                        if (ch == 'y' || ch == 'Y') {
                            install_on_virtual_machine();
                        }
                        break;
                    case 2: // Hardware Information
                        show_hardware_info();
                        break;
                    case 3: // Configuration Selection
                        select_configuration();
                        break;
                    case 4: // Disk Info
                        show_disk_info();
                        break;
                    case 5: // Network Check
                        clear();
                        mvprintw(5, 10, "Network status: ");
                        if (check_network()) {
                            attron(COLOR_PAIR(2));
                            mvprintw(5, 27, "Connected ✓");
                            attroff(COLOR_PAIR(2));
                        } else {
                            attron(COLOR_PAIR(3));
                            mvprintw(5, 27, "No connection ✗");
                            attroff(COLOR_PAIR(3));
                        }
                        mvprintw(7, 10, "Press any key...");
                        refresh();
                        getch();
                        break;
                    case 6: // Exit
                        if (confirm_action("Exit Lainux installer?", "EXIT")) {
                            endwin();
                            return 0;
                        }
                        break;
                }
                break;
            case 27: // Escape
                if (confirm_action("Exit Lainux installer?", "EXIT")) {
                    endwin();
                    return 0;
                }
                break;
        }
    }
    
    endwin();
    return 0;
}