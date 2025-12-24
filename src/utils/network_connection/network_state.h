#ifndef NETWORK_STATE_H
#define NETWORK_STATE_H

#include <stdbool.h>

// Конфигурация проверки сети
typedef struct NetworkConfig NetworkConfig;

// Основные функции
bool check_network_connection(const NetworkConfig* config);
int connect_network_driver(void);
bool has_network_interfaces(void);

// Методы проверки
bool check_by_ping(const char* hostname, int timeout_sec);
bool check_by_dns(const char* dns_server, int timeout_sec);
bool check_by_http(const char* url, int timeout_sec);

#endif // NETWORK_STATE_H