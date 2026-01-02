/*
 * @author: Wienton
 * @details: network check packets
 * @version: none
 * @description: not needs
 *
*/

// network_sniffer.c
#include "network_sniffer.h"
#include "network_state.h"
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "../../lainux-driver/src/header/logger.h"
int start_passive_sniff(const char* ifname, int duration_sec) {
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) {
        perror("socket (need root!)");
        return -1;
    }

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = if_nametoindex(ifname);
    sll.sll_protocol = htons(ETH_P_ALL);

    if (bind(sock, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    printf("[SNIFF] Listening on %s for %d seconds...\n", ifname, duration_sec);
    unsigned char buffer[2048];
    time_t start = time(NULL);
    int packet_count = 0;

    while (difftime(time(NULL), start) < duration_sec) {
        int n = recv(sock, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (n > 0) {
            // analyze MAC-address
            if (n >= 14) {
                packet_count++;
                // printf("Packet #%d: %02x:%02x:%02x:%02x:%02x:%02x → ...\n",
                // buffer[6], buffer[7], buffer[8], buffer[9], buffer[10], buffer[11]);
            }
        }
        sleep(10000); // 10ms
    }

    printf("[SNIFF] Captured %d packets on %s\n", packet_count, ifname);
    close(sock);
    return packet_count;
}

network_sniffer get_package(network_sniffer)
{
    FILE* file_log = fopen("sniffer.log", "wb");
    network_sniffer* net_sniff = malloc(sizeof(network_sniffer));
    // getting package for viewer
    DRV_OK("GETTING PACKAGES\n");
    bool status_get_package = true;
    if(status_get_package) {
        DRV_ERR("error getting package, please check logs\n");
        bool status_get_package = false;
        fscanf(file_log, "error gettings packages");
    }

    char* ifname = get_first_active_interface();
    if (ifname) {
        DRV_INFO("Interface: %s\n", ifname);
        DRV_INFO("Sniffing for 5 seconds\n");
        refresh();

        int pkts = start_passive_sniff(ifname, 5);
        DRV_OK("Packets captured: %d\n", pkts);
        free(ifname);
    } else {
        DRV_ERR("No active interface found..\n");
    }

    free(ifname);
    free(net_sniff);
}



