/**
 * @file network_state.c
 * @author Wienton
 * @brief network state connect for drivers
 * @version 0.1
 * @date 2025-12-25
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <stdio.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>



// * header

#include "network_state.h"
// get active network interface
char* get_first_active_interface(void) {
#ifdef __linux__
    struct ifaddrs *ifaddrs_ptr, *ifa;
    char *result = NULL;

    if (getifaddrs(&ifaddrs_ptr) == -1)
        return NULL;

    for (ifa = ifaddrs_ptr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;      // skip loopback
        if (!(ifa->ifa_flags & IFF_UP)) continue;         // skip down interfaces

        // take first non-loopback UP interface
        result = malloc(strlen(ifa->ifa_name) + 1);
        if (result) strcpy(result, ifa->ifa_name);
        break;
    }

    freeifaddrs(ifaddrs_ptr);
    return result;
#else
    return NULL;
#endif
}
connect_driver_net init_connect(void)
{
     connect_driver_net drv_net = {0};

    if(!&drv_net) {
        fprintf(stderr, "error memory out for drv_net");
        exit(EXIT_FAILURE);
    }

    printf("success init\n");

    drv_net.count_package = 10;
    #ifdef __linux__
       drv_net.name_device = get_first_active_interface();
    #else
        drv_net.name_device = NULL;
    #endif

    printf("\ninformation for start");
    printf("\tname: %s\n", drv_net.name_device);


}

void free_resource(char* buffer)
{
    free(buffer);
}

// int main(int argc, char** argv)
// {

//     connect_driver_net drv_net = init_connect();

//     if (drv_net.name_device) {
//         printf("Active interface: %s\n", drv_net.name_device);
//     } else {
//         printf("No active interface found or not on Linux.\n");
//     }

//     free_resource(drv_net.name_device);

//     return 0;
// }
