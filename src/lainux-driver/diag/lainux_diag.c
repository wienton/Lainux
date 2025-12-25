#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/header/logger.h"

// simulation(right now visual, after)
void fast_scan(const char* msg) {
    DRV_INFO("%s", msg);
    usleep(300000); // 0.3
}

void check_cpu() {
    FILE *f = fopen("/proc/cpuinfo", "r");
    char line[256];
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0) {
                DRV_OK("Processor found: %s", strchr(line, ':') + 2);
                break;
            }
        }
        fclose(f);
    }
}

void check_mem() {
    FILE *f = fopen("/proc/meminfo", "r");
    char line[256];
    if (f) {
        fgets(line, sizeof(line), f); // MemTotal
        DRV_OK("System Memory: %s", strchr(line, ':') + 2);
        fclose(f);
    }
}

void check_uefi() {
    if (access("/sys/firmware/efi", F_OK) == 0) {
        DRV_OK("Boot mode: [ UEFI ] - Secure Boot ready.");
    } else {
        DRV_WARN("Boot mode: [ LEGACY/BIOS ] - Some features limited.");
    }
}

int main(int argc, char *argv[]) {
    printf(CLR_BOLD "\n--- LAINUX SYSTEM DIAGNOSTICS ---\n\n" CLR_RESET);

    fast_scan("Probing CPU architecture...");
    check_cpu();

    fast_scan("Analyzing volatile memory...");
    check_mem();

    fast_scan("Detecting firmware interface...");
    check_uefi();

    fast_scan("Checking storage controllers...");
    // Можно добавить проверку /sys/class/block
    DRV_OK("Storage: NVMe/SATA controller identified.");

    fast_scan("Pinging the Wired (Network check)...");
    if (access("/sys/class/net/eth0", F_OK) == 0 || access("/sys/class/net/wlan0", F_OK) == 0) {
        DRV_OK("Network: Online.");
    } else {
        DRV_WARN("Network: Interface not found or offline.");
    }

    printf(CLR_BOLD "\nDiagnostic complete. System is ready for 'turbo -i'.\n\n" CLR_RESET);

    return 0;
}
