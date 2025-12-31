#ifndef LOG_MESSAGE_H
#define LOG_MESSAGE_H

#include <pthread.h>

void log_message(const char *format, ...);

extern pthread_mutex_t log_mutex;




#endif
