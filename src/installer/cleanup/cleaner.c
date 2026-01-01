#include <stdio.h>
#include <ncurses.h>

#include "../system_utils/log_message.h"
#include "../system_utils/run_command.h"
#include "cleaner.h"

// Emergency cleanup
void emergency_cleanup() {
    log_message("Performing emergency cleanup...");

    // Unmount anything mounted under /mnt
    run_command("umount -R /mnt 2>/dev/null || true", 0);
    run_command("swapoff -a 2>/dev/null || true", 0);

    // Remove temporary files
    run_command("rm -f /tmp/lainux-*.tmp 2>/dev/null || true", 0);
    run_command("rm -f /mnt/root/core.pkg.tar.zst 2>/dev/null || true", 0);
}

// Cleanup function
void cleanup_ncurses() {
    if (log_win) delwin(log_win);
    if (status_win) delwin(status_win);
    endwin();
}
