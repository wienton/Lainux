#ifndef NETWORK_STATE_H
#define NETWORK_STATE_H

#include <stdbool.h>
#include <openssl/sha.h>
// struct for connect network driver
typedef struct connect_driver_network {

    // count package for view
    unsigned int count_package;
    char* name_device; // name device (ethernet)


    struct params_for_device {

        char ip_address[30]; // get ip address users
        char* device_name;

    };

     enum status_network {

        SUCCESS_NETWORK, // success network connectioon status 1
        ERROR_NETWORK_CONNECT, // error network connection status 2
        ERROR_RESPONSE_PACKAGE, // error response package status 3
        ERROR_REQUEST_PACKAGE, // error request package encrypted status 4
        ERROR_ECNRYPT_PACKAGE,  // error encrypt package :( status 5
        ERROR_DECRYPT_PACKAGE,  // error decrypt package, status 6

    };

    SHA256_CTX* ctx;

} connect_driver_net;

// configuration check network
typedef struct NetworkConfig NetworkConfig;

// general function
bool check_network_connection(const NetworkConfig* config);
int connect_network_driver(void);
bool has_network_interfaces(void);
// get name network interface from kernel
char* get_first_active_interface(void);

// check methods
bool check_by_ping(const char* hostname, int timeout_sec);
// check by dns ports ping
bool check_by_dns(const char* dns_server, int timeout_sec);
bool check_by_http(const char* url, int timeout_sec);

#endif // NETWORK_STATE_H
