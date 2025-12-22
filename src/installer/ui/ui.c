#include <ncurses.h>
#include <string.h>
#include "system/system.h"

/**
 * Draws a box with ncurses line drawing characters
 * Creates a visual border for menus and dialogs
 */
void draw_box(int starty, int startx, int height, int width) {
    // Draw four corners
    mvaddch(starty, startx, ACS_ULCORNER);
    mvaddch(starty, startx + width, ACS_URCORNER);
    mvaddch(starty + height, startx, ACS_LLCORNER);
    mvaddch(starty + height, startx + width, ACS_LRCORNER);
    
    // Draw horizontal lines (top and bottom)
    for (int i = 1; i < width; i++) {
        mvaddch(starty, startx + i, ACS_HLINE);
        mvaddch(starty + height, startx + i, ACS_HLINE);
    }
    
    // Draw vertical lines (left and right)
    for (int i = 1; i < height; i++) {
        mvaddch(starty + i, startx, ACS_VLINE);
        mvaddch(starty + i, startx + width, ACS_VLINE);
    }
}

/**
 * Displays the welcome screen with Lainux branding
 * First screen shown when installer starts
 */
void show_welcome_screen() {
    clear();
    int y, x;
    getmaxyx(stdscr, y, x); // Get terminal dimensions

    // Display Lainux logo and title
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(y/2 - 2, (x-13)/2, "L A I N U X");
    mvprintw(y/2 - 1, (x-30)/2, "System Laboratory Deployment");
    attroff(COLOR_PAIR(1) | A_BOLD);

    // Prompt to continue
    mvprintw(y/2 + 2, (x-25)/2, "Press any key to begin...");
    refresh();
    getch(); // Wait for any key press
}

/**
 * Displays the main menu with all installer options
 * Returns selected menu option index
 */
int show_main_menu() {
    clear();
    int y, x;
    getmaxyx(stdscr, y, x);

    int height = 12, width = 40;
    int starty = (y - height) / 2;
    int startx = (x - width) / 2;

    // Main menu options
    const char *choices[] = {
        "  Turbo Install Lainux    ",
        "  Partitioning Menu       ",
        "  Install Toolchain       ",
        "  User Configuration      ",
        "  DEPLOY SYSTEM           ",
        "  Exit                    "
    };

    int highlight = 0;
    while(1) {
        draw_box(starty, startx, height, width);
        
        // Menu title
        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw(starty - 1, startx + 2, " [ Main Control Unit ] ");
        attroff(A_BOLD | COLOR_PAIR(1));

        // Display menu options with highlighting
        for(int i = 0; i < 6; i++) {
            if(i == highlight) {
                attron(A_REVERSE | COLOR_PAIR(1)); // Highlight selected option
                mvprintw(starty + 2 + i, startx + 2, "%s", choices[i]);
                attroff(A_REVERSE | COLOR_PAIR(1));
            } else {
                mvprintw(starty + 2 + i, startx + 2, "%s", choices[i]);
            }
        }
        
        // Handle keyboard input
        int c = getch();
        switch(c) {
            case KEY_UP: highlight = (highlight == 0) ? 5 : highlight - 1; break;
            case KEY_DOWN: highlight = (highlight == 5) ? 0 : highlight + 1; break;
            case 10: return highlight; // Enter key selects option
        }
    }
}

/**
 * Shows disk selection menu for installation target
 * Returns index of selected disk or -1 for cancel
 */
int show_disk_picker(DiskInfo disks[], int count) {
    int highlight = 0;
    int y, x;
    getmaxyx(stdscr, y, x);

    while(1) {
        clear();
        draw_box((y-15)/2, (x-50)/2, 12, 50);
        
        // Menu title
        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw((y-15)/2 - 1, (x-50)/2 + 2, " [ Select Target Drive ] ");
        attroff(A_BOLD | COLOR_PAIR(1));

        // Display list of available disks
        for(int i = 0; i < count; i++) {
            if(i == highlight) attron(A_REVERSE | COLOR_PAIR(1));
            mvprintw((y-15)/2 + 3 + i, (x-50)/2 + 4, "/dev/%-10s %15s", 
                     disks[i].name, disks[i].size);
            if(i == highlight) attroff(A_REVERSE | COLOR_PAIR(1));
        }

        // User instructions
        mvprintw((y-15)/2 + 10, (x-50)/2 + 4, "Press ENTER to confirm selection");

        // Handle keyboard input
        int c = getch();
        switch(c) {
            case KEY_UP: highlight = (highlight == 0) ? count - 1 : highlight - 1; break;
            case KEY_DOWN: highlight = (highlight == count - 1) ? 0 : highlight + 1; break;
            case 10: return highlight; // Enter key selects disk
            case 27: return -1; // ESC key cancels/returns
        }
    }
}

/**
 * Displays installation progress screen with progress bar
 * Shows current step, percentage, and animated spinner
 */
void show_progress(const char *message, int step, int total_steps) {
    int y, x;
    getmaxyx(stdscr, y, x);
    
    clear();
    
    // Draw progress window border
    draw_box((y-10)/2, (x-60)/2, 8, 60);
    
    // Display current operation message
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw((y-10)/2 + 1, (x-strlen(message))/2, "%s", message);
    attroff(COLOR_PAIR(1) | A_BOLD);
    
    // Show step counter (e.g., "Step 2 of 8")
    char step_text[64];
    snprintf(step_text, sizeof(step_text), "Step %d of %d", step, total_steps);
    mvprintw((y-10)/2 + 2, (x-strlen(step_text))/2, "%s", step_text);
    
    // Progress bar dimensions and position
    int bar_width = 50;
    int bar_start_x = (x - bar_width) / 2;
    int bar_y = (y-10)/2 + 4;
    
    // Calculate and display percentage
    int percent = (step * 100) / total_steps;
    char percent_text[16];
    snprintf(percent_text, sizeof(percent_text), "%d%%", percent);
    mvprintw(bar_y - 1, (x - strlen(percent_text)) / 2, "%s", percent_text);
    
    // Draw progress bar brackets
    mvaddch(bar_y, bar_start_x - 1, '[');
    mvaddch(bar_y, bar_start_x + bar_width, ']');
    
    // Fill progress bar based on completion
    int filled = (step * bar_width) / total_steps;
    for(int i = 0; i < bar_width; i++) {
        if(i < filled) {
            attron(COLOR_PAIR(1)); // Use color for filled portion
            mvaddch(bar_y, bar_start_x + i, '=');
            attroff(COLOR_PAIR(1));
        } else {
            mvaddch(bar_y, bar_start_x + i, ' '); // Empty space
        }
    }
    
    // Animated spinner to show activity
    const char *spinners[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    static int spinner_index = 0;
    
    mvprintw(bar_y + 2, (x - 20)/2, "Please wait %s", spinners[spinner_index]);
    spinner_index = (spinner_index + 1) % 10; // Cycle through spinner frames
    
    refresh();
}

/**
 * Shows desktop environment selection menu
 * Returns index of selected desktop option
 */
int show_desktop_menu(void) {
    int y, x;
    getmaxyx(stdscr, y, x);
    int choice = 0;
    
    // Available desktop environments
    const char *desktops[] = {
        "  XFCE (Recommended)  ",
        "  GNOME              ",
        "  KDE Plasma         ",
        "  Minimal (No DE)    ",
        "  Skip Desktop       "
    };
    
    while (1) {
        clear();
        draw_box((y-12)/2, (x-50)/2, 10, 50);
        
        // Menu title
        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw((y-12)/2 - 1, (x-50)/2 + 2, " [ Desktop Environment ] ");
        attroff(A_BOLD | COLOR_PAIR(1));
        
        // Instructions
        mvprintw((y-12)/2 + 1, (x-50)/2 + 4, "Choose desktop environment:");
        
        // Display desktop options
        for (int i = 0; i < 5; i++) {
            if (i == choice) attron(A_REVERSE | COLOR_PAIR(1));
            mvprintw((y-12)/2 + 3 + i, (x-50)/2 + 4, "%s", desktops[i]);
            if (i == choice) attroff(A_REVERSE | COLOR_PAIR(1));
        }
        
        // Handle keyboard input
        int c = getch();
        switch (c) {
            case KEY_UP: choice = (choice == 0) ? 4 : choice - 1; break;
            case KEY_DOWN: choice = (choice == 4) ? 0 : choice + 1; break;
            case 10: // Enter key selects option
                return choice;
            case 27: // ESC key selects "Skip Desktop"
                return 4;
        }
    }
}

/**
 * Specialized menu for Turbo Install mode
 * Shows disk selection with detailed Turbo Install information
 */
int show_turbo_install_menu(DiskInfo disks[], int count) {
    int highlight = 0;
    int y, x;
    getmaxyx(stdscr, y, x);

    while(1) {
        clear();
        draw_box((y-18)/2, (x-60)/2, 16, 60);
        
        // Turbo Install title
        attron(A_BOLD | COLOR_PAIR(1));
        mvprintw((y-18)/2 - 1, (x-60)/2 + 2, " [ Turbo Install Lainux ] ");
        attroff(A_BOLD | COLOR_PAIR(1));

        // Explain Turbo Install features
        mvprintw((y-18)/2 + 1, (x-60)/2 + 4, "Automatic one-click installation:");
        mvprintw((y-18)/2 + 2, (x-60)/2 + 6, "• Full disk partitioning");
        mvprintw((y-18)/2 + 3, (x-60)/2 + 6, "• Auto network detection");
        mvprintw((y-18)/2 + 4, (x-60)/2 + 6, "• Timezone configuration");
        mvprintw((y-18)/2 + 5, (x-60)/2 + 6, "• Essential packages");
        mvprintw((y-18)/2 + 6, (x-60)/2 + 6, "• User creation (lainux:lainux)");
        mvprintw((y-18)/2 + 7, (x-60)/2 + 6, "• System optimization");

        // Disk selection prompt
        mvprintw((y-18)/2 + 9, (x-60)/2 + 4, "Select target disk:");

        // Display available disks (max 4 shown)
        for(int i = 0; i < count && i < 4; i++) {
            if(i == highlight) attron(A_REVERSE | COLOR_PAIR(1));
            mvprintw((y-18)/2 + 11 + i, (x-60)/2 + 6, 
                     "/dev/%-8s %12s  %-20s", 
                     disks[i].name, disks[i].size, disks[i].model);
            if(i == highlight) attroff(A_REVERSE | COLOR_PAIR(1));
        }

        // Control instructions
        mvprintw((y-18)/2 + 15, (x-60)/2 + 4, 
                 "ENTER: Install  ESC: Back  SPACE: View details");

        // Handle keyboard input
        int c = getch();
        switch(c) {
            case KEY_UP: 
                highlight = (highlight == 0) ? (count-1 < 4 ? count-1 : 3) : highlight - 1; 
                break;
            case KEY_DOWN: 
                highlight = (highlight == (count-1 < 4 ? count-1 : 3)) ? 0 : highlight + 1; 
                break;
            case 10: // Enter - start installation
                return highlight;
            case 27: // ESC - return to main menu
                return -1;
            case ' ': // Space - show detailed disk information
                if (highlight >= 0 && highlight < count) {
                    clear();
                    attron(A_BOLD | COLOR_PAIR(1));
                    mvprintw(5, 10, "Disk Information");
                    attroff(A_BOLD | COLOR_PAIR(1));
                    mvprintw(7, 10, "Device:    /dev/%s", disks[highlight].name);
                    mvprintw(8, 10, "Size:      %s", disks[highlight].size);
                    mvprintw(9, 10, "Model:     %s", disks[highlight].model);
                    mvprintw(11, 10, "Warning: All data on this disk will be permanently");
                    mvprintw(12, 10, "erased during installation.");
                    mvprintw(14, 10, "Press any key to continue...");
                    getch();
                }
                break;
        }
    }
}

/**
 * Enhanced progress screen for Turbo Install mode
 * Shows detailed step-by-step progress with checkmarks
 */
void show_turbo_progress(const char *phase, int step, int total) {
    int y, x;
    getmaxyx(stdscr, y, x);
    
    clear();
    draw_box((y-12)/2, (x-60)/2, 10, 60);
    
    // Turbo Install title
    attron(A_BOLD | COLOR_PAIR(1));
    mvprintw((y-12)/2 - 1, (x-60)/2 + 2, " [ Turbo Installation ] ");
    attroff(A_BOLD | COLOR_PAIR(1));
    
    // Current phase
    mvprintw((y-12)/2 + 1, (x-strlen(phase))/2, "%s", phase);
    
    // List of installation steps
    const char *steps[] = {
        "Initializing",
        "Partitioning disk",
        "Formatting filesystems", 
        "Mounting partitions",
        "Installing base system",
        "Configuring system",
        "Setting up bootloader",
        "Creating user account",
        "Optimizing performance",
        "Finalizing installation"
    };
    
    // Display current and recent steps with status indicators
    for (int i = 0; i < 5 && (step - 2 + i) >= 0 && (step - 2 + i) < total; i++) {
        int idx = step - 2 + i;
        if (idx == step - 1) {
            // Current step - highlighted with arrow
            attron(COLOR_PAIR(1));
            mvprintw((y-12)/2 + 3 + i, (x-60)/2 + 8, "➤ %s", steps[idx]);
            attroff(COLOR_PAIR(1));
        } else if (idx < step - 1) {
            // Completed step - shown with checkmark
            attron(COLOR_PAIR(2));
            mvprintw((y-12)/2 + 3 + i, (x-60)/2 + 8, "✓ %s", steps[idx]);
            attroff(COLOR_PAIR(2));
        } else {
            // Future step - normal display
            mvprintw((y-12)/2 + 3 + i, (x-60)/2 + 8, "  %s", steps[idx]);
        }
    }
    
    // Progress bar
    int bar_width = 40;
    int bar_x = (x - bar_width) / 2;
    int bar_y = (y-12)/2 + 9;
    
    mvprintw(bar_y - 1, (x-10)/2, "Progress");
    
    // Draw progress bar brackets
    mvaddch(bar_y, bar_x - 1, '[');
    mvaddch(bar_y, bar_x + bar_width, ']');
    
    // Fill progress bar
    int filled = (step * bar_width) / total;
    for(int i = 0; i < bar_width; i++) {
        if(i < filled) {
            attron(COLOR_PAIR(1));
            mvaddch(bar_y, bar_x + i, '=');
            attroff(COLOR_PAIR(1));
        } else {
            mvaddch(bar_y, bar_x + i, ' ');
        }
    }
    
    // Display completion percentage
    int percent = (step * 100) / total;
    mvprintw(bar_y + 1, (x-10)/2, "%d%% Complete", percent);
    
    refresh();
}