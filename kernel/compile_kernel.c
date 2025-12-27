#include <ncurses.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include "../include/printf.h"
#define MAX_VALUES 15
#define GENERAL_PATH "lainux-iso"

int init_kernel_lainux();
int start_system_compiler();

int init_kernel_lainux()
{
    printf("init system and start");
    return 0;
}
bool start_archiso_build()
{
    int result;

    // clean dir and files(artefact build)
    INFO("Cleaning all directories and files for building...");

    result = system("sudo rm -rf ./work");
    if(result != 0) {  // system() возвращает 0 при успехе
        ERROR("Error cleaning artefacts");
        return false;
    }
    SUCCESS("Clean completed");

    // build archiso
    INFO("Building Arch Linux ISO with mkarchiso...");
    result = system("sudo mkarchiso -v -w ./work -o ./out .");

    if(result != 0) {
        ERROR("Failed to build ISO");
        return false;
    }

    SUCCESS("ISO build completed successfully");
    return true;
}

int start_system_compiler()
{
    init_kernel_lainux();

    const char* array_for_start[MAX_VALUES] = {
        ""
    };
    static char* flags[] = {
        ""
    };

    size_t array_len = sizeof(array_for_start) / sizeof(array_for_start[0]);

    for(int i = 0; i < array_len; i++) {
        printf("compnents [%d]: %s\n", i, array_for_start[i]);
        bool result_command = unlink(array_for_start[i]);
        if(result_command) {
            fprintf(stderr, "error compile command, check please\n");
            return -1;
        }
        unlink(array_for_start[i]);

    }

    return 0;

}


int main()
{
      if(start_archiso_build()) {
        printf("Build process completed successfully!\n");
        return EXIT_SUCCESS;
    } else {
        printf("Build process failed!\n");
        return EXIT_FAILURE;
    }
}
