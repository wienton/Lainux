// lavd manager
// only for nvidia

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>

// logger

#include "../lainux-driver/src/header/logger.h"

int started_lavd_system();
bool init_system_lavd();

bool init_system_lavd()
{

    DRV_OK("SYSTEM LAVD: init success!");
/*
    bool result = system("sudo pacman -Syu");

    if(result == false) {
        DRV_ERR("update packages pacman, command 'sudo pacman -Syu' error, please try after..\n");
        return 1;
    }
*/

    bool success_init_lavd = false;

    while(success_init_lavd) {
        sleep(1);
    }

    return true;
}

int started_lavd_system()
{
    DRV_INFO("metrics\n");
    bool result = system("sudo lavdctl metrics show cpu_frequency");

    if(result == false) {
        DRV_ERR("metrics failed started\n");
        system( "sudo lavdctl metrics show cpu_frequency");
    }

    system("sudo lavdctl metrics show memory_bandwidth");


    DRV_OK("layer 02: kernel diagnostics \n");
    system("sudo lavdctl metrics show cpu_sheduler");
    system("lavdctl trace --kfunc shedule");

    printf("layer 03: openrc service diagnostics\n");
    system("sudo lavdctl profile --service openrc");

    DRV_OK("layer 04: userspace diagnostics\n");
    system("sudo lavdctl top --sort cpu");

    DRV_OK("wired: full system analysis\n");
    system("sudo lavdctl diagnose full --output /tmp/lainux-diag-$(date +%s).json");

    int layer;
    for(layer = 0; layer < 4; layer++) {
        DRV_INFO("checking layer %d\n", layer);
        system("/usr/local/bin/lainux-lavd-integration.sh layer%layer");
    }

    DRV_INFO("lets all love lain\n");
    DRV_OK("lainux lavd integration\n");
    DRV_INFO("usage: %d {layer1| layer2| layer3| layer4| wired}", layer);

    return true;
}

int main(void)
{
    DRV_INFO("hello, started system lavdctl ? (y/n): ");

    char select_option;

    scanf("%s", &select_option);

    switch(select_option) {
        case 'Y':
        case 'y':
            init_system_lavd();
            break;
        case 'n':
        case 'N':
            DRV_WARN("exit lavdctl system init manager");
            exit(EXIT_FAILURE);
            break;
        default:
            DRV_ERR("unknow option\n");
            DRV_INFO("please enter y or n and try later\n");
            exit(EXIT_FAILURE);
            break;
    }

    return 0;
}
