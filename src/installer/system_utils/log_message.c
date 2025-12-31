#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <ncurses.h>

extern WINDOW *log_win;
extern WINDOW *status_win;
pthread_mutex_t log_mutex  = PTHREAD_MUTEX_INITIALIZER;

// Thread-safe logging with timestamp
void log_message(const char *format, ...) {
    pthread_mutex_lock(&log_mutex);

    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Только если log_win создан - пишем в него
    if (log_win) {
        wprintw(log_win, "[%s] %s\n", timestamp, message);
        wrefresh(log_win);
    } else {
        // Иначе просто в stdout (для отладки)
        printf("[%s] %s\n", timestamp, message);
        fflush(stdout);
    }

    pthread_mutex_unlock(&log_mutex);
}
