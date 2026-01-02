#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <ncurses.h>
#include "../utils/log_message.h"


void show_configuration_menu(void) {

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, "src/installer/configs/config.lua") != LUA_OK) {
        log_message("Lua error: %s", lua_tostring(L, -1));
        lua_close(L);
        return;
    }

    lua_getglobal(L, "get_configurations_list");
    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        log_message("Lua error: %s", lua_tostring(L, -1));
        lua_close(L);
        return;
    }

    if (!lua_istable(L, -1)) {
        log_message("No configurations found");
        lua_close(L);
        return;
    }

    int config_count = luaL_len(L, -1);
    if (config_count == 0) {
        log_message("Configuration list is empty");
        lua_close(L);
        return;
    }

    int selected = 0;
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    while (1) {
        clear();

        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw(2, (max_x - 25) / 2, "SELECT CONFIGURATION");
        attroff(A_BOLD | COLOR_PAIR(1));

        mvprintw(4, 10, "Use UP/DOWN arrows to navigate, ENTER to select");

        for (int i = 0; i < config_count; i++) {
            lua_rawgeti(L, -1, i + 1);

            lua_getfield(L, -1, "name");
            const char *name = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "description");
            const char *desc = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "size");
            const char *size = lua_tostring(L, -1);
            lua_pop(L, 1);

            int y_pos = 6 + i * 3;

            if (i == selected) {
                attron(A_REVERSE | COLOR_PAIR(2));
                mvprintw(y_pos, 12, "> %-20s %-10s", name, size);
                attroff(A_REVERSE | COLOR_PAIR(2));

                // description elements
                attron(COLOR_PAIR(3));
                mvprintw(y_pos + 1, 15, "%s", desc);
                attroff(COLOR_PAIR(3));

                lua_getfield(L, -1, "features");
                if (lua_istable(L, -1)) {
                    int feature_y = y_pos + 2;
                    mvprintw(feature_y, 15, "Features: ");

                    int feature_count = luaL_len(L, -1);
                    for (int f = 0; f < feature_count && f < 3; f++) {
                        lua_rawgeti(L, -1, f + 1);
                        const char *feature = lua_tostring(L, -1);
                        if (f > 0) printw(", ");
                        printw("%s", feature);
                        lua_pop(L, 1);
                    }
                }
                lua_pop(L, 1);
            } else {
                attron(COLOR_PAIR(7));
                mvprintw(y_pos, 14, "%-20s %-10s", name, size);
                attroff(COLOR_PAIR(7));
            }

            lua_pop(L, 1);
        }

        // instruction
        attron(COLOR_PAIR(4));
        mvprintw(max_y - 3, 10, "ENTER: Select  ESC: Cancel");
        attroff(COLOR_PAIR(4));

        refresh();

        // callback input
        int ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected > 0) ? selected - 1 : config_count - 1;
                break;
            case KEY_DOWN:
                selected = (selected < config_count - 1) ? selected + 1 : 0;
                break;
            case 10: // Enter - choise
                {
                    // get UID selected configuration
                    lua_rawgeti(L, -1, selected + 1);
                    lua_getfield(L, -1, "id");
                    const char *selected_id = lua_tostring(L, -1);

                    // save choised cfg
                    log_message("Selected configuration: %s", selected_id);

                    // close Lua
                    lua_close(L);

                    // back or continue installation
                    return;
                }
                break;
            case 27: // ESC - close
                lua_close(L);
                return;
        }
    }

    lua_close(L);
}
