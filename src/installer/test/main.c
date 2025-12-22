#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#define VERSION "v0.3-BETA"
#define CORE_URL "https://github.com/wienton/Lainux/raw/main/lainux-core-0.1-1-x86_64.pkg.tar.zst"

void setup_colors() {
    start_color();
    init_pair(1, COLOR_MAGENTA, COLOR_BLACK); // Title
    init_pair(2, COLOR_CYAN, COLOR_BLACK);    // Accents
    init_pair(3, COLOR_GREEN, COLOR_BLACK);   // Success
    init_pair(4, COLOR_RED, COLOR_BLACK);     // Error
    init_pair(5, COLOR_BLACK, COLOR_WHITE);   // Selection
}

// Красивая рамка с тенью
void draw_interface() {
    int my, mx;
    getmaxyx(stdscr, my, mx);
    
    // Тень
    attron(COLOR_PAIR(0));
    for(int i=0; i<my-2; i++) mvaddch(4+i, mx/2+31, ' ' | A_REVERSE);
    for(int i=0; i<62; i++) mvaddch(my/2+6, mx/2-29+i, ' ' | A_REVERSE);

    // Главное окно
    attron(COLOR_PAIR(2));
    move(3, mx/2-30); 
    hline(ACS_HLINE, 60);
    vline(ACS_VLINE, 20);
    move(23, mx/2-30); 
    hline(ACS_HLINE, 60);
    move(3, mx/2+30);
    vline(ACS_VLINE, 20);
    mvaddch(3, mx/2-30, ACS_ULCORNER);
    mvaddch(3, mx/2+30, ACS_URCORNER);
    mvaddch(23, mx/2-30, ACS_LLCORNER);
    mvaddch(23, mx/2+30, ACS_LRCORNER);
}

int exec(const char *step, const char *cmd) {
    endwin();
    printf("\033[1;35m[LAINUX-ENGINE]\033[0m %s...\n\033[0;32m%s\033[0m\n", step, cmd);
    int r = system(cmd);
    initscr();
    return (r == 0);
}

void auto_install() {
    char disk[32] = "sda"; // Можно добавить выбор через меню
    char cmd[1024];

    // Шаг 0: Универсальность (Подтягиваем тулзы если их нет)
    exec("Preparing Environment", "apt-get update && apt-get install -y arch-install-scripts || pacman -S --noconfirm arch-install-scripts || echo 'Assuming tools exist'");

    // Шаг 1: Диск
    sprintf(cmd, "sgdisk --zap-all /dev/%s && echo 'label: gpt\nsize=512M, type=ef00\nsize=+, type=8304' | sfdisk /dev/%s", disk, disk);
    if(!exec("Deep Partitioning", cmd)) return;

    // Шаг 2: ФС
    sprintf(cmd, "mkfs.fat -F32 /dev/%s1 && mkfs.ext4 -F /dev/%s2", disk, disk);
    exec("Formatting", cmd);

    // Шаг 3: Монтирование
    sprintf(cmd, "mount /dev/%s2 /mnt && mkdir -p /mnt/boot && mount /dev/%s1 /mnt/boot", disk, disk);
    exec("Mounting Filesystems", cmd);

    // Шаг 4: Развертывание (Используем зеркала)
    exec("Deploying Base", "pacstrap /mnt base linux-hardened linux-firmware base-devel wget grub efibootmgr");

    // Шаг 5: Облачный десант Lainux-Core
    sprintf(cmd, "wget -qO /mnt/root/core.tar.zst %s && arch-chroot /mnt pacman -U /root/core.tar.zst --noconfirm", CORE_URL);
    exec("Downloading & Injecting Core", cmd);

    // Шаг 6: GRUB
    exec("Bootloader Config", "arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot && arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg");

    exec("Finalizing", "umount -R /mnt");
    
    mvprintw(15, COLS/2-10, "SUCCESS! UNPLUG USB & REBOOT.");
    refresh();
    getch();
}

int main() {
    initscr();
    setup_colors();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    char*menu[] = {"[ 1. TURBO DEPLOY ]", "[ 2. NETWORK CONFIGURE ]", "[ 3. ABOUT SYSTEM ]", "[ 4. TERMINATE ]"};
    int sel = 0;

    while(1) {
        clear();
        draw_interface();
        
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(5, COLS/2-22, " _        _  ___ _   _ _   _ _  __");
        mvprintw(6, COLS/2-22, "| |      / \\|_ _| \\ | | | | \\ \\/ /");
        mvprintw(7, COLS/2-22, "| |     / _ \\ | |  \\| | | | |\\  / ");
        mvprintw(8, COLS/2-22, "| |___ / ___ \\| | |\\  | |_| |/  \\ ");
        mvprintw(9, COLS/2-22, "|_____/_/   \\_\\___|_| \\_|\\___//_/\\_\\ %s", VERSION);
        attroff(COLOR_PAIR(1) | A_BOLD);

        attron(COLOR_PAIR(2));
        mvprintw(11, COLS/2-15, "Automated Developer Platform");
        attroff(COLOR_PAIR(2));

        for(int i=0; i<4; i++) {
            if(i == sel) {
                attron(COLOR_PAIR(5));
                mvprintw(14+i*2, COLS/2-12, "%s", menu[i]);
                attroff(COLOR_PAIR(5));
            } else {
                mvprintw(14+i*2, COLS/2-12, "%s", menu[i]);
            }
        }

        int ch = getch();
        if(ch == KEY_UP) sel = (sel + 3) % 4;
        else if(ch == KEY_DOWN) sel = (sel + 1) % 4;
        else if(ch == 10) {
            if(sel == 0) auto_install();
            if(sel == 3) break;
        }
    }

    endwin();
    return 0;
}