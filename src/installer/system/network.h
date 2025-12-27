#ifndef NETWORK_H
#define NETWORK_H

// Структура для возврата инфы
typedef struct {
    int is_online;
    char local_ip[16];
    char msg[100];
} NetStatus;

NetStatus check_network_vibe();

#endif
