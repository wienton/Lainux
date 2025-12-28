#ifndef PRINTF_H
#define PRINTF_H

#include <stdio.h>

// ANSI codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

#define BOLD          "\033[1m"
#define UNDERLINE     "\033[4m"


#define INFO(msg, ...)    printf(COLOR_CYAN "[INFO] " COLOR_RESET msg "\n", ##__VA_ARGS__)
#define SUCCESS(msg, ...) printf(COLOR_GREEN BOLD "[SUCCESS] " COLOR_RESET msg "\n", ##__VA_ARGS__)
#define ERROR(msg, ...)   printf(COLOR_RED "[ERROR] " COLOR_RESET msg "\n", ##__VA_ARGS__)
#define WARNING(msg, ...) printf(COLOR_YELLOW "[WARNING] " COLOR_RESET msg "\n", ##__VA_ARGS__)


#endif
