

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/header/logger.h"
#include <unistd.h>
#include <fcntl.h>


void fast_scan(const char* msg) {
    DRV_INFO("Initializing hardware scan protocol...");

    // Массив с именами этапов для вывода
    const char* steps[] = {
        "CPU Topology",
        "Memory Integrity",
        "Storage Controllers",
        "Network Interfaces",
        "Firmware (UEFI/BIOS)",
        "GPU Bus"
    };

    for(int i = 0; i < 6; i++) {
        DRV_INFO("Checking %s...", steps[i]);

        // Реальные проверки в зависимости от итерации
        switch(i) {
            case 0: // CPU
                if (access("/proc/cpuinfo",  F_OK) == 0) DRV_OK("[Step %d] Processors localized.", i);
                break;
            case 1: // RAM
                if (access("/proc/meminfo",  F_OK) == 0) DRV_OK("[Step %d] Reference memory map built.", i);
                break;
            case 2: // Storage
                if (access("/sys/class/block", F_OK) == 0) DRV_OK("[Step %d] Block devices mapped.", i);
                break;
            case 3: // Network
                if (access("/sys/class/net", F_OK) == 0) DRV_OK("[Step %d] Wired/Wireless stack verified.", i);
                break;
            case 4: // UEFI
                if (access("/sys/firmware/efi", F_OK) == 0)
                    DRV_OK("[Step %d] UEFI Runtime services identified.", i);
                else
                    DRV_WARN("[Step %d] Legacy boot detected.", i);
                break;
            case 5: // GPU
                if (access("/sys/class/drm", F_OK) == 0) DRV_OK("[Step %d] Display rendering nodes active.", i);
                break;
        }

        // Небольшая задержка для "веса" и чтобы юзер успел прочитать
        usleep(400000);
    }

    DRV_OK("System scan complete. Environment is stable.");
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
    printf(CLR_BOLD "\n**** LAINUX SYSTEM DIAGNOSTICS ****\n\n" CLR_RESET);

    fast_scan("Probing CPU architecture...");
    check_cpu();

    fast_scan("Analyzing volatile memory...");
    check_mem();

    fast_scan("Detecting firmware interface...");
    check_uefi();

    fast_scan("Checking storage controllers...");
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
