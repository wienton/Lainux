#ifndef NETWORK_SNIFFER_H
#define NETWORK_SNIFFER_H

typedef struct {

    enum STATUS_SNIFF {

        SUCCESS,    // 1
        ERROR_SNIFF, // 2
        ERROR_GET_PACKAGE, // 3

    };



} network_sniffer;

int start_passive_sniff(const char* ifname, int duration_sec);

network_sniffer get_package(network_sniffer);
int network_package_encrypt();

#endif
