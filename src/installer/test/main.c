/**
 * @brief Official Working Version Lainux Installer, but right now only arch linux(kernel) install.
 * @details https://github.com/wienton/Lainux/raw/main/lainux-core-0.1-1-x86_64.pkg.tar.zst download from here
 */

#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CORE_URL "https://github.com/wienton/Lainux/raw/main/lainux-core-0.1-1-x86_64.pkg.tar.zst"

WINDOW *log_win;


void log_out(const char *text) {
    wprintw(log_win, " > %s\n", text);
    wrefresh(log_win);
}

// start command with ui
int run_cmd(const char *cmd) {
    char buf[256];
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    while (fgets(buf, sizeof(buf), fp)) {
        log_out(buf);
    }
    return pclose(fp);
}

// select disk
void get_disks(char *target) {
    FILE *fp = popen("lsblk -dno NAME,SIZE", "r");
    char disks[10][64];
    int count = 0;
    while (fgets(disks[count], 64, fp) && count < 10) count++;
    pclose(fp);

    int sel = 0;
    while(1) {
        clear();
        mvprintw(2, 5, "SELECT TARGET STORAGE DEVICE:");
        for(int i=0; i<count; i++) {
            if(i == sel) attron(A_REVERSE | COLOR_PAIR(2));
            mvprintw(4+i, 7, " %s ", disks[i]);
            attroff(A_REVERSE | COLOR_PAIR(2));
        }
        int ch = getch();
        if(ch == KEY_UP) sel = (sel + count - 1) % count;
        if(ch == KEY_DOWN) sel = (sel + 1) % count;
        if(ch == 10) {
            sscanf(disks[sel], "%s", target);
            return;
        }
    }
}

void start_installation(const char *disk) {
    int my, mx;
    getmaxyx(stdscr, my, mx);
    // window logo
    log_win = newwin(my-15, mx-10, 12, 5);
    scrollok(log_win, TRUE);
    box(log_win, 0, 0);
    wrefresh(log_win);

    char cmd[1024];

    log_out("INITIALIZING LAINUX ENGINE...");
    
    run_cmd("command -v pacstrap || (apt-get update && apt-get install -y arch-install-scripts)");

    log_out("PARTITIONING DISK...");
    sprintf(cmd, "sgdisk --zap-all /dev/%s && echo 'label: gpt\nsize=512M, type=ef00\nsize=+, type=8304' | sfdisk /dev/%s", disk, disk);
    run_cmd(cmd);

    log_out("FORMATTING FILESYSTEMS...");
    sprintf(cmd, "mkfs.fat -F32 /dev/%s1 && mkfs.ext4 -F /dev/%s2", disk, disk);
    run_cmd(cmd);

    log_out("MOUNTING...");
    sprintf(cmd, "mount /dev/%s2 /mnt && mkdir -p /mnt/boot && mount /dev/%s1 /mnt/boot", disk, disk);
    run_cmd(cmd);

    log_out("INSTALLING BASE (STAY CONNECTED)...");
    run_cmd("pacstrap /mnt base linux-hardened linux-firmware base-devel wget grub efibootmgr");

    log_out("INJECTING LAINUX CORE CONTENT...");
    sprintf(cmd, "wget -qO /mnt/root/core.pkg.tar.zst %s && arch-chroot /mnt pacman -U /root/core.pkg.tar.zst --noconfirm", CORE_URL);
    run_cmd(cmd);

    log_out("FINALIZING BOOTLOADER...");
    run_cmd("arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot && arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg");

    log_out("SUCCESS. UNMOUNTING...");
    run_cmd("umount -R /mnt");
    
    mvprintw(my-2, 5, "INSTALLATION FINISHED. PRESS ANY KEY TO REBOOT.");
    getch();
}

int main() {
    initscr();
    start_color();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    init_pair(1, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);

    char disk[32];
    int sel = 0;
    char *menu[] = {"[ START TURBO INSTALL ]", "[ SYSTEM INFORMATION ]", "[ EXIT ]"};

    while(1) {
        clear();
        attron(COLOR_PAIR(1) | A_BOLD);

        mvprintw(2, 10, "    _           _       __     _  _      _  _      _  _   ");
        mvprintw(3, 10, "   FJ          /.\\      FJ    F L L]    FJ  L]    FJ  LJ  ");
        mvprintw(4, 10, "  J |         //_\\\\    J  L  J   \\| L  J |  | L   J \\/ F  ");
        mvprintw(5, 10, "  | |        / ___ \\   |  |  | |\\   |  | |  | |   /    \\  ");
        mvprintw(6, 10, "  F L_____  / L___J \\  F  J  F L\\\\  J  F L__J J  /  /\\  \\ ");
        mvprintw(7, 10, " J________LJ__L   J__LJ____LJ__L \\\\__LJ\\______/FJ__//\\\\__L");
        mvprintw(8, 10, " |________||__L   J__||____||__L  J__| J______F |__/  \\__|");

        attroff(COLOR_PAIR(1) | A_BOLD);


        mvprintw(10, 10, "OS: Lainux | Layer: Hardened | Build: 1.0 Release");

        for(int i=0; i<3; i++) {
            if(i == sel) attron(A_REVERSE | COLOR_PAIR(2));
            mvprintw(12+i*2, 15, "%s", menu[i]);
            attroff(A_REVERSE | COLOR_PAIR(2));
        }

        int ch = getch();
        if(ch == KEY_UP) sel = (sel + 2) % 3;
        if(ch == KEY_DOWN) sel = (sel + 1) % 3;
        if(ch == 10) {
            if(sel == 0) {
                get_disks(disk);
                start_installation(disk);
            }
            if(sel == 2) break;
        }
    }

    endwin();
    return 0;
}