/*
 * License: MIT
 */

#ifndef NCURSES_UTIL_H
#define NCURSES_UTIL_H

#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>

/* Types */
typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point pos;
    int width;
    int height;
} Rect;

typedef struct {
    WINDOW *win;
    Rect bounds;
    bool has_border;
    int bg_color;
    int fg_color;
} WindowCtx;

typedef enum {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
} TextAlign;

typedef enum {
    KEY_ESC = 27,
    KEY_ENTER_NL = 10,
    KEY_ENTER_CR = 13
} SpecialKeys;

/* Color pair management */
void nc_init_colorpairs(void);
int nc_colorpair(int fg, int bg);

/* Core initialization */
bool nc_init(void);
void nc_cleanup(void);
void nc_resize_handler(void);

/* Window management */
WindowCtx nc_create_window(Rect rect, bool border);
void nc_destroy_window(WindowCtx *ctx);
void nc_refresh_window(WindowCtx *ctx);
void nc_clear_window(WindowCtx *ctx);
Rect nc_get_screen_rect(void);

/* Drawing utilities */
void nc_draw_text(WindowCtx *ctx, int y, int x, const char *text);
void nc_draw_text_align(WindowCtx *ctx, int y, TextAlign align, const char *text);
void nc_draw_box(WindowCtx *ctx, Rect rect);
void nc_draw_horizontal_line(WindowCtx *ctx, int y, int x, int length);
void nc_draw_vertical_line(WindowCtx *ctx, int x, int y, int length);
void nc_draw_border(WindowCtx *ctx);

/* Input handling */
int nc_get_char(WindowCtx *ctx);
char *nc_get_input(WindowCtx *ctx, int y, int x, int max_len, bool echo);
int nc_get_int(WindowCtx *ctx, int y, int x, int max_digits);

/* UI Components */
int nc_menu(WindowCtx *ctx, const char **options, int count, const char *title);
bool nc_confirm(WindowCtx *ctx, const char *question);
void nc_progress_bar(WindowCtx *ctx, int y, int x, int width, float progress);
void nc_status_line(WindowCtx *ctx, const char *text);

/* Utility functions */
void nc_center_text(char *dest, const char *src, int width);
int nc_count_lines(const char *text, int width);
void nc_wait_key(const char *prompt);

#endif
