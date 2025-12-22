#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Функция для выполнения системных команд с проверкой
void execute_step(const char *msg, const char *command) {
    endwin(); // Временно выходим из ncurses, чтобы видеть лог команды
    printf("\n[LAINUX STEP] %s...\n", msg);
    printf("Running: %s\n", command);
    
    int res = system(command);
    if (res != 0) {
        printf("\n[!] ERROR: Command failed. Press Enter to exit.");
        getchar();
        exit(1);
    }
    initscr(); // Возвращаемся в ncurses
}

// Логика установки
void install_logic(const char *target_disk) {
    char cmd[1024];
    const char *pkg_url = "https://github.com/wienton/Lainux/raw/main/lainux-core-0.1-1-x86_64.pkg.tar.zst";
    const char *pkg_name = "lainux-core-0.1-1-x86_64.pkg.tar.zst";

    // 1. Проверка интернета
    execute_step("Checking network", "ping -c 1 github.com");

    // 2. Подготовка диска (GPA, 512MB Boot, All Root)
    sprintf(cmd, "sgdisk --zap-all /dev/%s", target_disk);
    execute_step("Cleaning disk", cmd);

    sprintf(cmd, "echo 'label: gpt\nsize=512M, type=ef00\nsize=+, type=8304' | sfdisk /dev/%s", target_disk);
    execute_step("Partitioning", cmd);

    // 3. Форматирование (Проверяем тип диска для именования разделов)
    // Для NVMe это p1, для SDA это 1. Упростим до sda для теста.
    sprintf(cmd, "mkfs.fat -F32 /dev/%s1", target_disk); 
    execute_step("Formatting Boot (EFI)", cmd);
    sprintf(cmd, "mkfs.ext4 -F /dev/%s2", target_disk);
    execute_step("Formatting Root", cmd);

    // 4. Монтирование
    sprintf(cmd, "mount /dev/%s2 /mnt", target_disk);
    execute_step("Mounting Root", cmd);
    execute_step("Creating Boot dir", "mkdir -p /mnt/boot");
    sprintf(cmd, "mount /dev/%s1 /mnt/boot", target_disk);
    execute_step("Mounting Boot", cmd);

    // 5. Установка базы через pacstrap
    execute_step("Installing Base (Arch Hardened)", "pacstrap /mnt base linux-hardened linux-firmware base-devel nasm gcc wget grub efibootmgr");

    // 6. Генерация FSTAB
    execute_step("Generating FSTAB", "genfstab -U /mnt >> /mnt/etc/fstab");

    // 7. СКАЧИВАНИЕ И УСТАНОВКА LAINUX-CORE
    sprintf(cmd, "wget -O /mnt/root/%s %s", pkg_name, pkg_url);
    execute_step("Downloading Lainux-Core from GitHub", cmd);

    sprintf(cmd, "arch-chroot /mnt pacman -U /root/%s --noconfirm", pkg_name);
    execute_step("Installing Lainux-Core package", cmd);

    // 8. Настройка загрузчика
    execute_step("Installing GRUB", "arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot --bootloader-id=LAINUX");
    execute_step("Configuring GRUB", "arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg");

    // 9. Финиш
    execute_step("Unmounting", "umount -R /mnt");
}

void show_info() {
    clear();
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(2, 5, "=== LAINUX PROJECT INFO ===");
    attroff(COLOR_PAIR(1) | A_BOLD);
    mvprintw(4, 5, "Platform for system developers and security experts.");
    mvprintw(5, 5, "Philosophy: Deep control, minimal noise.");
    mvprintw(7, 5, "Developer: wienton");
    mvprintw(8, 5, "GitHub: github.com/wienton/Lainux");
    mvprintw(10, 5, "Press any key to go back...");
    refresh();
    getch();
}

void run_install_menu() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    WINDOW *win = newwin(12, 60, max_y/2 - 6, max_x/2 - 30);
    box(win, 0, 0);
    
    mvwprintw(win, 2, 5, "        TURBO INSTALLATION MODE");
    mvwprintw(win, 4, 5, "Target disk: /dev/sda");
    mvwprintw(win, 5, 5, "Packages: Base + Linux-Hardened + Lainux-Core");
    mvwprintw(win, 7, 5, "WARNING: This will wipe all data on /dev/sda!");
    mvwprintw(win, 9, 5, "Press [ENTER] to confirm or [ESC] to exit.");
    wrefresh(win);

    int ch = wgetch(win);
    if (ch == 10) { // ENTERinstall_logic("sda");
        clear();
        mvprintw(max_y/2, max_x/2 - 15, "INSTALLATION COMPLETE! REBOOT NOW.");
        refresh();
        getch();
    }
    delwin(win);
}

int main() {
    initscr();
    start_color();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    init_pair(1, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);

    int choice;
    int highlight = 0;
    char *choices[] = {"1. Turbo Install (/dev/sda)", "2. Custom Setup (WIP)", "3. What is Lainux?", "4. Exit"};
    int n_choices = 4;

    while(1) {
        clear();
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(1, 10, " _       LAINUX SYSTEM INSTALLER       _ ");
        mvprintw(2, 10, "| |    Everything is connected...      | |");
        mvprintw(3, 10, "| |____________________________________| |");
        attroff(COLOR_PAIR(1) | A_BOLD);

        for(int i = 0; i < n_choices; i++) {
            if(i == highlight) attron(A_REVERSE | COLOR_PAIR(2));
            mvprintw(i + 8, 15, "%s", choices[i]);
            attroff(A_REVERSE | COLOR_PAIR(2));
        }
        refresh();

        choice = getch();
        switch(choice) {
            case KEY_UP: highlight = (highlight == 0) ? n_choices - 1 : highlight - 1; break;
            case KEY_DOWN: highlight = (highlight == n_choices - 1) ? 0 : highlight + 1; break;
            case 10:
                if(highlight == 0) run_install_menu();
                if(highlight == 2) show_info();
                if(highlight == 3) { endwin(); return 0; }
                break;
        }
    }
    endwin();
    return 0;
}