// We extract the configuration file from the lua config file and display it interactively on the screen via SI in ncurses :))
// Wienton, the lead developer of the project, participated in its development.
// TODO: finish correct display of packages included in the config list

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <ncurses.h>
#include <string.h>
#include "../utils/log_message.h"

// Function for displaying configurations in ncurses
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
            lua_rawgeti(L, -1, i + 1); // push config table

            lua_getfield(L, -1, "name");
            const char *name = lua_tostring(L, -1) ?: "Unknown";
            lua_pop(L, 1);

            lua_getfield(L, -1, "description");
            const char *desc = lua_tostring(L, -1) ?: "No description";
            lua_pop(L, 1);

            lua_getfield(L, -1, "size");
            const char *size = lua_tostring(L, -1) ?: "~?";
            lua_pop(L, 1);

            int y_pos = 6 + i * 5; // больше вертикального отступа для деталей

            if (i == selected) {
                attron(A_REVERSE | COLOR_PAIR(2));
                mvprintw(y_pos, 12, "> %-20s %-10s", name, size);
                attroff(A_REVERSE | COLOR_PAIR(2));

                // Description
                attron(COLOR_PAIR(3));
                mvprintw(y_pos + 1, 15, "%s", desc);
                attroff(COLOR_PAIR(3));


                                // Packages
                mvprintw(y_pos + 2, 15, "Packages: ");
                lua_getfield(L, -1, "packages");
                if (lua_istable(L, -1)) {
                    // for debug print message with packages from lua
                    log_message("found packages from config lua: %s\n", name);

                    int pkg_line = y_pos + 2;
                    int pkg_col = 25;
                    int first = 1;


                    lua_pushnil(L);
                    while (lua_next(L, -2) != 0) {
                        if (lua_istable(L, -1)) {
                            int cat_len = luaL_len(L, -1);
                            for (int p = 1; p <= cat_len; p++) {
                                lua_rawgeti(L, -1, p);
                                const char *pkg = lua_tostring(L, -1);
                                if (pkg && strlen(pkg) > 0) {
                                    if (!first) {
                                        if (pkg_col + 2 >= max_x - 2) {
                                            pkg_line++;
                                            pkg_col = 25;
                                        } else {
                                            mvaddstr(pkg_line, pkg_col, ", ");
                                            pkg_col += 2;
                                        }
                                    }
                                    int len = strlen(pkg);
                                    if (pkg_col + len >= max_x - 2) {
                                        pkg_line++;
                                        pkg_col = 25;
                                    }
                                    mvaddstr(pkg_line, pkg_col, pkg);
                                    pkg_col += len;
                                    first = 0;
                                }
                                lua_pop(L, 1);
                            }
                        }
                        lua_pop(L, 1);
                    }

                    if (first) {
                        mvaddstr(y_pos + 2, 25, "none");
                    }
                } else {
                    mvaddstr(y_pos + 2, 25, "none");
                }
                lua_pop(L, 1); // pop "packages"

                // Features
                lua_getfield(L, -1, "features");
                if (lua_istable(L, -1)) {
                    int feat_line = y_pos + 3;
                    while (mvprintw(feat_line, 15, "Features: ") == ERR) feat_line++; // safeguard
                    int feat_col = 25;
                    int feat_count = luaL_len(L, -1);
                    int first_feat = 1;
                    for (int f = 1; f <= feat_count && f <= 5; f++) {
                        lua_rawgeti(L, -1, f);
                        const char *feat = lua_tostring(L, -1);
                        if (feat) {
                            if (!first_feat) {
                                if (feat_col + 2 >= max_x - 2) {
                                    feat_line++;
                                    feat_col = 25;
                                } else {
                                    mvaddstr(feat_line, feat_col, ", ");
                                    feat_col += 2;
                                }
                            }
                            int len = strlen(feat);
                            if (feat_col + len >= max_x - 2) {
                                feat_line++;
                                feat_col = 25;
                            }
                            mvaddstr(feat_line, feat_col, feat);
                            feat_col += len;
                            first_feat = 0;
                        }
                        lua_pop(L, 1);
                    }
                }
                lua_pop(L, 1); // pop "features"
            } else {
                attron(COLOR_PAIR(7));
                mvprintw(y_pos, 14, "%-20s %-10s", name, size);
                attroff(COLOR_PAIR(7));
            }

            lua_pop(L, 1); // pop config table
        }

        // Instructions
        attron(COLOR_PAIR(4));
        mvprintw(max_y - 3, 10, "ENTER: Select  ESC: Cancel");
        attroff(COLOR_PAIR(4));

        refresh();

        int ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected > 0) ? selected - 1 : config_count - 1;
                break;
            case KEY_DOWN:
                selected = (selected < config_count - 1) ? selected + 1 : 0;
                break;
            case 10: // Enter
            {
                lua_rawgeti(L, -1, selected + 1);
                lua_getfield(L, -1, "id");
                const char *selected_id = lua_tostring(L, -1);
                if (selected_id) {
                    log_message("Selected configuration: %s", selected_id);
                }
                lua_close(L);
                return;
            }
            break;
            case 27: // ESC
                lua_close(L);
                return;
        }
    }

    lua_close(L);
}
