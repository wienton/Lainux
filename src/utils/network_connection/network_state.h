#ifndef NETWORK_STATE_H
#define NETWORK_STATE_H

#include <stdbool.h>

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

        SUCCESS_NETWORK, // success network connectioon
        ERROR_NETWORK_CONNECT, // error network connection
        ERROR_RESPONSE_PACKAGE, // error response package
        ERROR_REQUEST_PACKAGE,
        ERROR_ECNRYPT_PACKAGE,
        ERROR_DECRYPT_PACKAGE,

    };

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
