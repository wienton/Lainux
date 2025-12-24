#ifndef NETWORK_STATE_H
#define NETWORK_STATE_H

#include <stdbool.h>

typedef struct connect_driver_network {

    unsigned int count_package;
    char* name_device;

    struct params_for_device {

        char ip_address[30];
        char* device_name;

    };

     enum status_network {

        SUCCESS_NETWORK,
        ERROR_NETWORK_CONNECT,
        ERROR_RESPONSE_PACKAGE,
        ERROR_REQUEST_PACKAGE,
        ERROR_ECNRYPT_PACKAGE,
        ERROR_DECRYPT_PACKAGE,

    };

} connect_driver_net;

// Конфигурация проверки сети
typedef struct NetworkConfig NetworkConfig;

// Основные функции
bool check_network_connection(const NetworkConfig* config);
int connect_network_driver(void);
bool has_network_interfaces(void);

char* get_first_active_interface(void);

// Методы проверки
bool check_by_ping(const char* hostname, int timeout_sec);
bool check_by_dns(const char* dns_server, int timeout_sec);
bool check_by_http(const char* url, int timeout_sec);

#endif // NETWORK_STATE_H
