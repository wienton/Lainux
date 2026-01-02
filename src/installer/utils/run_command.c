#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "log_message.h"


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
