#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

// Макросы для цветного вывода
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RESET   "\033[0m"

#define BOLD          "\033[1m"

#define INFO(msg, ...)    printf(COLOR_CYAN "[INFO] " COLOR_RESET msg "\n", ##__VA_ARGS__)
#define SUCCESS(msg, ...) printf(COLOR_GREEN BOLD "[SUCCESS] " COLOR_RESET msg "\n", ##__VA_ARGS__)
#define ERROR(msg, ...)   printf(COLOR_RED "[ERROR] " COLOR_RESET msg "\n", ##__VA_ARGS__)
#define WARNING(msg, ...) printf(COLOR_YELLOW "[WARNING] " COLOR_RESET msg "\n", ##__VA_ARGS__)

// Функция для выполнения команды с захватом вывода
int execute_command(const char *cmd, char *output, size_t output_size) {
    FILE *pipe = popen(cmd, "r");
    if(!pipe) return -1;

    if(output && output_size > 0) {
        output[0] = '\0';
        char buffer[128];
        while(fgets(buffer, sizeof(buffer), pipe)) {
            strncat(output, buffer, output_size - strlen(output) - 1);
        }
    } else {
        char buffer[128];
        while(fgets(buffer, sizeof(buffer), pipe)) {
            printf("%s", buffer);
        }
    }

    return pclose(pipe);
}

// Проверка структуры директории
bool check_directory_structure() {
    INFO("Checking directory structure...");

    // Проверяем обязательные файлы
    const char *required_files[] = {
        "profiledef.sh",
        "packages.x86_64",
        "pacman.conf",
        NULL
    };

    bool all_ok = true;
    for(int i = 0; required_files[i] != NULL; i++) {
        if(access(required_files[i], F_OK) != 0) {
            ERROR("Missing required file: %s", required_files[i]);
            all_ok = false;
        } else {
            SUCCESS("Found: %s", required_files[i]);
        }
    }

    // Проверяем структуру
    const char *required_dirs[] = {
        "airootfs",
        "efiboot",
        "syslinux",
        NULL
    };

    for(int i = 0; required_dirs[i] != NULL; i++) {
        struct stat st;
        if(stat(required_dirs[i], &st) != 0 || !S_ISDIR(st.st_mode)) {
            ERROR("Missing or not a directory: %s", required_dirs[i]);
            all_ok = false;
        } else {
            SUCCESS("Found directory: %s", required_dirs[i]);
        }
    }

    return all_ok;
}

// Создание недостающих файлов
bool create_missing_files() {
    INFO("Creating missing configuration files...");

    // Проверяем и создаем customize_airootfs.sh если нужно
    if(access("airootfs/root/.automated_script/customize_airootfs.sh", F_OK) != 0) {
        system("mkdir -p airootfs/root/.automated_script");

        FILE *fp = fopen("airootfs/root/.automated_script/customize_airootfs.sh", "w");
        if(fp) {
            fprintf(fp, "#!/usr/bin/env bash\n\n");
            fprintf(fp, "# Minimal customization script\n");
            fprintf(fp, "set -e\n\n");
            fprintf(fp, "# Set hostname\n");
            fprintf(fp, "echo 'lainux' > /etc/hostname\n\n");
            fprintf(fp, "# Create user\n");
            fprintf(fp, "useradd -m -G wheel -s /bin/bash lain 2>/dev/null || true\n");
            fprintf(fp, "echo 'lain:lain' | chpasswd\n\n");
            fprintf(fp, "# Clean package cache\n");
            fprintf(fp, "pacman -Scc --noconfirm 2>/dev/null || true\n");
            fprintf(fp, "exit 0\n");
            fclose(fp);
            system("chmod +x airootfs/root/.automated_script/customize_airootfs.sh");
            SUCCESS("Created customize_airootfs.sh");
        }
    }

    return true;
}

// Проверка profiledef.sh на ошибки
bool validate_profiledef() {
    INFO("Validating profiledef.sh...");

    // Проверяем синтаксис
    int result = system("bash -n profiledef.sh 2>&1");
    if(result != 0) {
        ERROR("profiledef.sh has syntax errors");

        // Показываем ошибки
        system("bash -n profiledef.sh 2>&1");

        // Предлагаем исправить
        WARNING("Trying to fix profiledef.sh...");

        // Создаем простой profiledef.sh
        FILE *fp = fopen("profiledef.sh", "w");
        if(fp) {
            fprintf(fp, "#!/usr/bin/env bash\n\n");
            fprintf(fp, "# Simple profile for testing\n");
            fprintf(fp, "arch=\"x86_64\"\n");
            fprintf(fp, "iso_name=\"lainux\"\n");
            fprintf(fp, "iso_label=\"LAINUX\"\n");
            fprintf(fp, "iso_publisher=\"Lainux\"\n");
            fprintf(fp, "iso_application=\"Lainux Live\"\n");
            fprintf(fp, "install_dir=\"arch\"\n");
            fprintf(fp, "buildmodes=('iso')\n");
            fclose(fp);
            system("chmod +x profiledef.sh");
            SUCCESS("Recreated profiledef.sh with simple configuration");
        }
    } else {
        SUCCESS("profiledef.sh syntax is OK");
    }

    return true;
}

// Основная функция сборки с прямой обработкой mkarchiso
bool run_mkarchiso_direct() {
    INFO("Running mkarchiso directly...");

    char cwd[1024];
    if(!getcwd(cwd, sizeof(cwd))) {
        ERROR("Cannot get current directory");
        return false;
    }

    printf("Working directory: %s\n", cwd);

    // Подготавливаем команду
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "cd '%s' && "
             "sudo mkarchiso -v -w ./work -o ./out . 2>&1",
             cwd);

    printf("Executing: %s\n\n", cmd);

    // Создаем pipe для чтения вывода
    FILE *pipe = popen(cmd, "r");
    if(!pipe) {
        ERROR("Failed to execute mkarchiso");
        return false;
    }

    // Читаем вывод в реальном времени
    char buffer[1024];
    bool has_errors = false;
    bool build_started = false;

    while(fgets(buffer, sizeof(buffer), pipe)) {
        // Убираем перевод строки
        buffer[strcspn(buffer, "\n")] = 0;

        // Проверяем на ошибки
        if(strstr(buffer, "error:") ||
           strstr(buffer, "ERROR:") ||
           strstr(buffer, "realpath:") ||
           strstr(buffer, "No such file")) {
            ERROR("%s", buffer);
            has_errors = true;
        }
        else if(strstr(buffer, "WARNING:")) {
            WARNING("%s", buffer);
        }
        else if(strstr(buffer, "INFO:")) {
            INFO("%s", buffer);
        }
        else if(strlen(buffer) > 0) {
            // Обычный вывод
            printf("  %s\n", buffer);

            // Проверяем, началась ли сборка
            if(strstr(buffer, "Installing packages") ||
               strstr(buffer, "Creating")) {
                build_started = true;
            }
        }
    }

    int status = pclose(pipe);

    // Анализируем результат
    if(WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);

        if(exit_code == 0) {
            if(!build_started) {
                ERROR("mkarchiso exited with code 0 but build didn't start");
                ERROR("This usually means profiledef.sh has issues");
                return false;
            }

            // Проверяем наличие ISO
            if(access("./out", F_OK) == 0) {
                system("find ./out -name '*.iso' -type f 2>/dev/null");

                FILE *find_iso = popen("find ./out -name '*.iso' -type f 2>/dev/null | head -1", "r");
                if(find_iso) {
                    char iso_path[1024];
                    if(fgets(iso_path, sizeof(iso_path), find_iso)) {
                        iso_path[strcspn(iso_path, "\n")] = 0;
                        SUCCESS("\nISO successfully created: %s", iso_path);

                        // Показываем информацию
                        printf("\n" COLOR_GREEN "╔══════════════════════════════════════════╗\n");
                        printf(  "║          BUILD SUCCESSFUL!              ║\n");
                        printf(  "╚══════════════════════════════════════════╝\n" COLOR_RESET);

                        printf("\nISO file: %s\n", iso_path);
                        system("ls -lh ./out/*.iso");

                        return true;
                    }
                    pclose(find_iso);
                }
            }

            ERROR("Build completed but no ISO found in ./out");
            system("ls -la ./out/ 2>/dev/null || echo 'Output directory does not exist'");
            return false;
        } else {
            ERROR("mkarchiso failed with exit code: %d", exit_code);
            return false;
        }
    } else {
        ERROR("mkarchiso terminated abnormally");
        return false;
    }

    return false;
}

// Альтернатива: запуск тестовой сборки
bool run_test_build() {
    INFO("Running test build with simplified configuration...");

    // Создаем временную директорию
    char temp_dir[] = "/tmp/test-lainux-XXXXXX";
    if(!mkdtemp(temp_dir)) {
        ERROR("Cannot create temp directory");
        return false;
    }

    printf("Test directory: %s\n", temp_dir);

    // Копируем минимальный набор файлов
    char cmd[2048];

    // 1. Копируем profiledef.sh
    snprintf(cmd, sizeof(cmd), "cp profiledef.sh '%s/'", temp_dir);
    system(cmd);

    // 2. Создаем минимальный packages.x86_64
    snprintf(cmd, sizeof(cmd),
             "echo 'linux\nlinux-firmware\nbase\nbash' > '%s/packages.x86_64'",
             temp_dir);
    system(cmd);

    // 3. Создаем минимальную структуру
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/airootfs/root/.automated_script'", temp_dir);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/efiboot' '%s/syslinux'", temp_dir, temp_dir);
    system(cmd);

    // 4. Создаем минимальный customize_airootfs.sh
    snprintf(cmd, sizeof(cmd),
             "echo '#!/bin/bash\necho test' > '%s/airootfs/root/.automated_script/customize_airootfs.sh'",
             temp_dir);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "chmod +x '%s/airootfs/root/.automated_script/customize_airootfs.sh'", temp_dir);
    system(cmd);

    // 5. Запускаем mkarchiso
    printf("\nStarting test build...\n");
    snprintf(cmd, sizeof(cmd),
             "cd '%s' && "
             "sudo mkarchiso -v -w ./work -o ./out . 2>&1",
             temp_dir);

    FILE *pipe = popen(cmd, "r");
    if(pipe) {
        char buffer[1024];
        while(fgets(buffer, sizeof(buffer), pipe)) {
            printf("  %s", buffer);
        }
        int result = pclose(pipe);

        if(result == 0) {
            // Проверяем ISO
            snprintf(cmd, sizeof(cmd), "find '%s/out' -name '*.iso' -type f 2>/dev/null", temp_dir);
            FILE *find_iso = popen(cmd, "r");
            if(find_iso) {
                char iso_path[1024];
                if(fgets(iso_path, sizeof(iso_path), find_iso)) {
                    iso_path[strcspn(iso_path, "\n")] = 0;

                    // Копируем ISO обратно
                    snprintf(cmd, sizeof(cmd), "cp '%s' ./", iso_path);
                    system(cmd);

                    SUCCESS("\nTest build successful! ISO copied to current directory");

                    // Очистка
                    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", temp_dir);
                    system(cmd);

                    return true;
                }
                pclose(find_iso);
            }
        }
    }

    // Очистка
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", temp_dir);
    system(cmd);

    return false;
}

// Основная функция
int main() {
    printf("Lainux ISO Builder v1.4\n");
    printf("=======================\n\n");

    int choice;
    printf("1. Build ISO (standard)\n");
    printf("2. Build ISO (test/simple)\n");
    printf("3. Fix configuration files\n");
    printf("4. Show current status\n");
    printf("Choice: ");

    if(scanf("%d", &choice) != 1) {
        ERROR("Invalid input");
        return EXIT_FAILURE;
    }

    bool success = false;

    switch(choice) {
        case 1:
            // Проверяем структуру
            if(!check_directory_structure()) {
                ERROR("Directory structure check failed");
                printf("\nTry running option 3 first to fix configuration.\n");
                return EXIT_FAILURE;
            }

            // Создаем недостающие файлы
            create_missing_files();

            // Валидируем profiledef.sh
            validate_profiledef();

            // Очищаем
            system("sudo rm -rf ./work ./out 2>/dev/null");

            // Запускаем сборку
            success = run_mkarchiso_direct();
            break;

        case 2:
            success = run_test_build();
            break;

        case 3:
            INFO("Fixing configuration...");
            create_missing_files();
            validate_profiledef();
            success = true;
            break;

        case 4:
            printf("\nCurrent directory: ");
            system("pwd");
            printf("\nFiles:\n");
            system("ls -la");
            printf("\nprofiledef.sh contents (first 10 lines):\n");
            system("head -10 profiledef.sh");
            success = true;
            break;

        default:
            ERROR("Invalid choice");
            return EXIT_FAILURE;
    }

    if(success) {
        printf("\n" COLOR_GREEN "✓ Operation completed successfully!" COLOR_RESET "\n");
        return EXIT_SUCCESS;
    } else {
        printf("\n" COLOR_RED "✗ Operation failed!" COLOR_RESET "\n");

        // Рекомендации
        printf("\n" COLOR_YELLOW "Recommended next steps:" COLOR_RESET "\n");
        printf("1. Check profiledef.sh for syntax errors:\n");
        printf("   bash -n profiledef.sh\n\n");
        printf("2. Try running mkarchiso manually:\n");
        printf("   sudo mkarchiso -v -w ./work -o ./out .\n\n");
        printf("3. Check if archiso is properly installed:\n");
        printf("   pacman -Qi archiso\n\n");
        printf("4. Look for error messages above.\n");

        return EXIT_FAILURE;
    }
}
