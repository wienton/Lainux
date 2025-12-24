#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <errno.h>
#include <if.h>

// Разные методы проверки соединения
typedef enum {
    METHOD_PING = 1,
    METHOD_DNS,
    METHOD_HTTP,
    METHOD_ALL
} CheckMethod;

// * Конфигурация проверки
typedef struct {
    const char* ping_host;
    const char* dns_server;
    const char* http_url;
    int timeout_sec;
    CheckMethod method;
} NetworkConfig;

// Проверка через ping (ICMP echo)
bool check_by_ping(const char* hostname, int timeout_sec) {
    char command[256];

    // Проверяем доступность ping команды
    if (system("which ping > /dev/null 2>&1") != 0) {
        printf("Ping command not found, skipping ping check\n");
        return false;
    }

    // Формируем команду ping
    snprintf(command, sizeof(command),
             "ping -c 1 -W %d %s > /dev/null 2>&1",
             timeout_sec, hostname);

    int result = system(command);
    return (result == 0);
}

// Проверка через разрешение DNS
bool check_by_dns(const char* dns_server, int timeout_sec) {
    struct addrinfo hints, *res;
    int sockfd, ret;
    struct timeval tv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Устанавливаем таймаут
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    // Пробуем разрешить DNS сервер
    if (getaddrinfo(dns_server, "53", &hints, &res) != 0) {
        return false;
    }

    // Пробуем создать сокет
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(res);
        return false;
    }

    // Устанавливаем таймаут на соединение
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // Пробуем подключиться
    ret = connect(sockfd, res->ai_addr, res->ai_addrlen);

    close(sockfd);
    freeaddrinfo(res);

    return (ret == 0);
}

// Проверка через HTTP запрос (упрощенная)
bool check_by_http(const char* url, int timeout_sec) {
    // Для HTTP проверки лучше использовать libcurl,
    // но для минималистичного подхода можно использовать wget/curl через system

    char command[512];

    // Проверяем доступность wget или curl
    if (system("which wget > /dev/null 2>&1") == 0) {
        snprintf(command, sizeof(command),
                 "wget --spider --timeout=%d --tries=1 %s > /dev/null 2>&1",
                 timeout_sec, url);
    } else if (system("which curl > /dev/null 2>&1") == 0) {
        snprintf(command, sizeof(command),
                 "curl --max-time %d --silent --output /dev/null --head %s > /dev/null 2>&1",
                 timeout_sec, url);
    } else {
        printf("Neither wget nor curl found, skipping HTTP check\n");
        return false;
    }

    int result = system(command);
    return (result == 0);
}

// Проверка наличия сетевых интерфейсов
bool has_network_interfaces() {
    struct ifaddrs *ifaddr, *ifa;
    int family;
    bool has_up_interface = false;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return false;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        // Проверяем IPv4 и IPv6 интерфейсы
        if (family == AF_INET || family == AF_INET6) {
            // Исключаем loopback интерфейс
            if (strncmp(ifa->ifa_name, "lo", 2) != 0) {
                // Проверяем, что интерфейс активен
                if (ifa->ifa_flags & IFF_UP) {
                    has_up_interface = true;
                    break;
                }
            }
        }
    }

    freeifaddrs(ifaddr);
    return has_up_interface;
}

// Основная функция проверки соединения
bool check_network_connection(const NetworkConfig* config) {
    printf("Checking network connection...\n");

    // Сначала проверяем наличие интерфейсов
    if (!has_network_interfaces()) {
        printf("No active network interfaces found\n");
        return false;
    }

    bool result = false;
    int methods_tried = 0;
    int methods_success = 0;

    // Выбираем метод проверки
    switch (config->method) {
        case METHOD_PING:
            printf("Trying ping to %s...\n", config->ping_host);
            result = check_by_ping(config->ping_host, config->timeout_sec);
            break;

        case METHOD_DNS:
            printf("Trying DNS resolution via %s...\n", config->dns_server);
            result = check_by_dns(config->dns_server, config->timeout_sec);
            break;

        case METHOD_HTTP:
            printf("Trying HTTP request to %s...\n", config->http_url);
            result = check_by_http(config->http_url, config->timeout_sec);
            break;

        case METHOD_ALL:
            // Пробуем все методы по порядку
            printf("Trying all available methods...\n");

            // Ping
            if (check_by_ping(config->ping_host, config->timeout_sec)) {
                printf("✓ Ping successful\n");
                methods_success++;
            }
            methods_tried++;

            // DNS
            if (check_by_dns(config->dns_server, config->timeout_sec)) {
                printf("✓ DNS resolution successful\n");
                methods_success++;
            }
            methods_tried++;

            // HTTP
            if (check_by_http(config->http_url, config->timeout_sec)) {
                printf("✓ HTTP check successful\n");
                methods_success++;
            }
            methods_tried++;

            result = (methods_success > 0);
            printf("Successfully completed %d/%d checks\n", methods_success, methods_tried);
            break;

        default:
            fprintf(stderr, "Unknown check method\n");
            return false;
    }

    return result;
}

// Функция для установки сетевого драйвера/соединения
int connect_network_driver() {
    // Конфигурация проверки сети
    NetworkConfig config = {
        .ping_host = "8.8.8.8",           // Google DNS
        .dns_server = "8.8.8.8",          // Google DNS
        .http_url = "http://google.com",  // Проверочный URL
        .timeout_sec = 5,                 // Таймаут 5 секунд
        .method = METHOD_ALL              // Использовать все методы
    };

    printf("*** Network Connection Check ***\n");

    if (!check_network_connection(&config)) {
        fprintf(stderr, "✗ No internet connection detected\n");

        // Дополнительные диагностические сообщения
        printf("\nDiagnostic information:\n");
        printf("1. Run 'ip link show' to see network interfaces\n");
        printf("2. Run 'ip route show' to check routing table\n");
        printf("3. Check if NetworkManager is running\n");

        return -1;
    }

    printf("✓ Network connection established successfully\n");
    return 0;
}

int main(void) {
    return connect_network_driver();
}

