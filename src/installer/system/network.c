#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include "network.h"
#include <fcntl.h>


// check status network
int check_network() {
    int sock;
    struct sockaddr_in server;
    struct timeval tv;

    // create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;

    // timeout 1.5 sec
    tv.tv_sec = 1;
    tv.tv_usec = 500000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    server.sin_addr.s_addr = inet_addr("1.1.1.1"); // Cloudflare DNS, быстрый как понос
    server.sin_family = AF_INET;
    server.sin_port = htons(53);

    //
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        close(sock);
        return 0;
    }

    close(sock);
    return 1; // network on
}

// create socket and get ip address user
NetStatus check_network_vibe() {
    NetStatus status;
    status.is_online = 0;
    strcpy(status.local_ip, "0.0.0.0");
    strcpy(status.msg, "Initial check");

    int sock;
    struct sockaddr_in server;
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        strcpy(status.msg, "Could not create socket");
        return status;
    }

    server.sin_addr.s_addr = inet_addr("8.8.8.8");
    server.sin_family = AF_INET;
    server.sin_port = htons(53);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        strcpy(status.msg, "Connection failed. Internet is down, bro.");
    } else {
        status.is_online = 1;
        strcpy(status.msg, "We are in business! Internet is up.");

        if (getsockname(sock, (struct sockaddr *)&local_addr, &addr_len) == 0) {
            strcpy(status.local_ip, inet_ntoa(local_addr.sin_addr));
        }
    }

    close(sock);
    return status;
}

