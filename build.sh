#!/bin/bash

set -e

# general directory
BINARY="installer.lain"                              # name binary file, exctension .lain :)
SRC_DIR="src/installer"                              # general directory for making binary file
VM_DIR="src/installer/vm"                            # working with virtual machine in our situation qemu
SYSTEM_UTILS_DIR="src/installer/system_utils"        # system utils
UTILS_DIR="src/utils/network_connection"             # working with network(sniffer and checker)
UI_DIR="src/installer/ui"                            # TUI interface
DISK_UTILS_DIR="src/installer/disk_utils"            # disk utils
LOCAL_DIR="src/installer/locale"                     # directory for localization(language)
SETTING_DIR="src/installer/settings"                 # settings directory with files
CLEANS_DIR="src/installer/cleanup"                   # cleanup dir
# drivers source
DRIVER_MAIN="lainux-driver"

# sources
SOURCES=(
    "$SRC_DIR/installer.c"
    "$UTILS_DIR/network_sniffer.c"
    "$UTILS_DIR/network_state.c"
    "$UI_DIR/logo.c"
    "$VM_DIR/vm.c"
    "$SYSTEM_UTILS_DIR/log_message.c"
    "$SYSTEM_UTILS_DIR/run_command.c"
    "$DISK_UTILS_DIR/disk_info.c"
    "$LOCAL_DIR/lang.c"
    "$LOCAL_DIR/ru.c"
    "$LOCAL_DIR/en.c"
    "$SETTING_DIR/settings.c"
    "$CLEANS_DIR/cleaner.c"
)

# dependencies
DEPS=("gcc" "ncurses" "curl")

# colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[$(date +'%H:%M:%S')]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
    exit 1
}

build_drivers() {
    log "Compiling Drivers"
}

build() {
    log "compiling $BINARY..."
    gcc \
        "${SOURCES[@]}" \
        -o bin/"$BINARY" \
        -lncurses \
        -lcurl \
        -Wextra \
        -O2 \
        -std=c11 \
        -lcrypto \
        -D_DEFAULT_SOURCE
    log "${GREEN}build successful${NC}"
    log "bianry saved as './$BINARY'"
}


log "**** || lainux bash script for make binary files || ****"

for src in "${SOURCES[@]}"; do
    if [[ ! -f "$src" ]]; then
        error "Source file not found: $src"
    fi
done

read -p "build installer? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log "build cancelled."
    exit 0
fi

build
