// network_sniffer.c
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
        usleep(10000); // 10ms
    }

    printf("[SNIFF] Captured %d packets on %s\n", packet_count, ifname);
    close(sock);
    return packet_count;
}
