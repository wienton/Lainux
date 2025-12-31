#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <string.h>

#include "../include/installer.h"

extern WINDOW *log_win;
extern WINDOW *status_win;



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
