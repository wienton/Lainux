/*
 * ncurses_util.c - Implementation of ncurses utility library
 * License: only for LAINUX
 */

#include "ncurses_util.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_COLORPAIRS 256
#define MAX_INPUT_LEN 256

static int colorpair_table[16][16];

void nc_init_colorpairs(void) {
    int pair = 1;
    for (int bg = 0; bg < 8; bg++) {
        for (int fg = 0; fg < 8; fg++) {
            if (pair < MAX_COLORPAIRS) {
                init_pair(pair, fg, bg);
                colorpair_table[fg][bg] = pair++;
            }
        }
    }
}

int nc_colorpair(int fg, int bg) {
    if (fg >= 0 && fg < 8 && bg >= 0 && bg < 8) {
        return colorpair_table[fg][bg];
    }
    return 0;
}

bool nc_init(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        use_default_colors();
        nc_init_colorpairs();
        return true;
    }
    return false;
}

void print_background_image(void)
{
    // anscii

    // LAINUXOS
    mvprintw(6,10, "LA");

}

void nc_cleanup(void) {
    endwin();
}

void nc_resize_handler(void) {
    endwin();
    refresh();
    clear();
}

Rect nc_get_screen_rect(void) {
    Rect rect;
    getmaxyx(stdscr, rect.height, rect.width);
    rect.pos.x = 0;
    rect.pos.y = 0;
    return rect;
}

WindowCtx nc_create_window(Rect rect, bool border) {
    WindowCtx ctx;
    ctx.bounds = rect;
    ctx.has_border = border;
    ctx.bg_color = COLOR_BLACK;
    ctx.fg_color = COLOR_WHITE;

    if (border) {
        ctx.win = newwin(rect.height, rect.width, rect.pos.y, rect.pos.x);
        box(ctx.win, 0, 0);
    } else {
        ctx.win = newwin(rect.height, rect.width, rect.pos.y, rect.pos.x);
    }

    wbkgd(ctx.win, COLOR_PAIR(nc_colorpair(ctx.fg_color, ctx.bg_color)));
    return ctx;
}

void nc_destroy_window(WindowCtx *ctx) {
    if (ctx && ctx->win) {
        delwin(ctx->win);
        ctx->win = NULL;
    }
}

void nc_refresh_window(WindowCtx *ctx) {
    if (ctx && ctx->win) {
        wrefresh(ctx->win);
    }
}

void nc_clear_window(WindowCtx *ctx) {
    if (ctx && ctx->win) {
        werase(ctx->win);
        if (ctx->has_border) {
            box(ctx->win, 0, 0);
        }
    }
}

void nc_draw_text(WindowCtx *ctx, int y, int x, const char *text) {
    if (ctx && ctx->win && text) {
        mvwprintw(ctx->win, y, x, "%s", text);
    }
}

void nc_draw_text_align(WindowCtx *ctx, int y, TextAlign align, const char *text) {
    if (!ctx || !ctx->win || !text) return;

    int x;
    int width = ctx->bounds.width - (ctx->has_border ? 2 : 0);

    switch (align) {
        case ALIGN_LEFT:
            x = ctx->has_border ? 1 : 0;
            break;
        case ALIGN_CENTER:
            x = (width - strlen(text)) / 2;
            if (ctx->has_border) x++;
            break;
        case ALIGN_RIGHT:
            x = width - strlen(text);
            if (ctx->has_border) x--;
            break;
    }

    if (x < 0) x = 0;
    if (x >= ctx->bounds.width) x = ctx->bounds.width - 1;

    nc_draw_text(ctx, y, x, text);
}

void nc_draw_box(WindowCtx *ctx, Rect rect) {
    if (!ctx || !ctx->win) return;

    for (int y = rect.pos.y; y < rect.pos.y + rect.height; y++) {
        for (int x = rect.pos.x; x < rect.pos.x + rect.width; x++) {
            mvwaddch(ctx->win, y, x, ' ');
        }
    }

    for (int x = rect.pos.x; x < rect.pos.x + rect.width; x++) {
        mvwaddch(ctx->win, rect.pos.y, x, ACS_HLINE);
        mvwaddch(ctx->win, rect.pos.y + rect.height - 1, x, ACS_HLINE);
    }

    for (int y = rect.pos.y; y < rect.pos.y + rect.height; y++) {
        mvwaddch(ctx->win, y, rect.pos.x, ACS_VLINE);
        mvwaddch(ctx->win, y, rect.pos.x + rect.width - 1, ACS_VLINE);
    }

    mvwaddch(ctx->win, rect.pos.y, rect.pos.x, ACS_ULCORNER);
    mvwaddch(ctx->win, rect.pos.y, rect.pos.x + rect.width - 1, ACS_URCORNER);
    mvwaddch(ctx->win, rect.pos.y + rect.height - 1, rect.pos.x, ACS_LLCORNER);
    mvwaddch(ctx->win, rect.pos.y + rect.height - 1, rect.pos.x + rect.width - 1, ACS_LRCORNER);
}

void nc_draw_border(WindowCtx *ctx) {
    if (ctx && ctx->win) {
        box(ctx->win, 0, 0);
    }
}

int nc_get_char(WindowCtx *ctx) {
    if (ctx && ctx->win) {
        return wgetch(ctx->win);
    }
    return getch();
}

char *nc_get_input(WindowCtx *ctx, int y, int x, int max_len, bool echo) {
    static char buffer[MAX_INPUT_LEN];
    int ch;
    int pos = 0;

    if (max_len >= MAX_INPUT_LEN) max_len = MAX_INPUT_LEN - 1;

    buffer[0] = '\0';

    if (ctx && ctx->win) {
        wmove(ctx->win, y, x);
        wrefresh(ctx->win);
    } else {
        move(y, x);
        refresh();
    }

    curs_set(1);

    while (1) {
        if (ctx && ctx->win) {
            ch = wgetch(ctx->win);
        } else {
            ch = getch();
        }

        if (ch == KEY_ENTER_NL || ch == KEY_ENTER_CR) {
            break;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (pos > 0) {
                pos--;
                buffer[pos] = '\0';
                if (ctx && ctx->win) {
                    mvwaddch(ctx->win, y, x + pos, ' ');
                    wmove(ctx->win, y, x + pos);
                } else {
                    mvaddch(y, x + pos, ' ');
                    move(y, x + pos);
                }
            }
        } else if (pos < max_len && isprint(ch)) {
            buffer[pos++] = ch;
            buffer[pos] = '\0';
        }

        if (ctx && ctx->win) {
            wrefresh(ctx->win);
        } else {
            refresh();
        }
    }

    curs_set(0);
    noecho();

    return buffer;
}

int nc_get_int(WindowCtx *ctx, int y, int x, int max_digits) {
    char *input = nc_get_input(ctx, y, x, max_digits, true);
    return atoi(input);
}

int nc_menu(WindowCtx *ctx, const char **options, int count, const char *title) {
    if (!ctx || !ctx->win || !options || count <= 0) return -1;

    int selection = 0;
    int ch;
    int start_y = ctx->has_border ? 2 : 1;

    while (1) {
        nc_clear_window(ctx);

        if (title) {
            nc_draw_text_align(ctx, 0, ALIGN_CENTER, title);
        }

        for (int i = 0; i < count; i++) {
            if (i == selection) {
                wattron(ctx->win, A_REVERSE);
                nc_draw_text(ctx, start_y + i, 2, options[i]);
                wattroff(ctx->win, A_REVERSE);
            } else {
                nc_draw_text(ctx, start_y + i, 2, options[i]);
            }
        }

        nc_refresh_window(ctx);
        ch = nc_get_char(ctx);

        switch (ch) {
            case KEY_UP:
                if (selection > 0) selection--;
                break;
            case KEY_DOWN:
                if (selection < count - 1) selection++;
                break;
            case KEY_ENTER_NL:
            case KEY_ENTER_CR:
                return selection;
            case KEY_ESC:
                return -1;
        }
    }
}

bool nc_confirm(WindowCtx *ctx, const char *question) {
    if (!ctx || !ctx->win || !question) return false;

    int center_y = ctx->bounds.height / 2;
    int center_x = ctx->bounds.width / 2;

    nc_clear_window(ctx);
    nc_draw_text_align(ctx, center_y - 1, ALIGN_CENTER, question);
    nc_draw_text_align(ctx, center_y, ALIGN_CENTER, "[Y/N]");
    nc_refresh_window(ctx);

    while (1) {
        int ch = nc_get_char(ctx);
        if (ch == 'y' || ch == 'Y') return true;
        if (ch == 'n' || ch == 'N') return false;
    }
}

void nc_progress_bar(WindowCtx *ctx, int y, int x, int width, float progress) {
    if (!ctx || !ctx->win) return;

    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    int filled = (int)((width - 2) * progress);

    mvwaddch(ctx->win, y, x, '[');
    for (int i = 0; i < width - 2; i++) {
        if (i < filled) {
            mvwaddch(ctx->win, y, x + 1 + i, '#');
        } else {
            mvwaddch(ctx->win, y, x + 1 + i, '-');
        }
    }
    mvwaddch(ctx->win, y, x + width - 1, ']');
}

void nc_status_line(WindowCtx *ctx, const char *text) {
    if (!ctx || !ctx->win || !text) return;

    int y = ctx->bounds.height - 1;
    if (ctx->has_border) y--;

    for (int x = 0; x < ctx->bounds.width; x++) {
        mvwaddch(ctx->win, y, x, ' ');
    }

    nc_draw_text(ctx, y, 0, text);
}

void nc_center_text(char *dest, const char *src, int width) {
    if (!dest || !src) return;

    int len = strlen(src);
    int padding = (width - len) / 2;

    if (padding < 0) padding = 0;

    memset(dest, ' ', width);
    dest[width] = '\0';
    memcpy(dest + padding, src, len);
}

void nc_wait_key(const char *prompt) {
    if (prompt) {
        printw("%s", prompt);
        refresh();
    }
    getch();
}
