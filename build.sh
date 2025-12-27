#!/bin/bash

set -e

BINARY="installer.lain"
SRC_DIR="src/installer"
UTILS_DIR="src/utils/network_connection"
UI_DIR="src/installer/ui"

# sources
SOURCES=(
    "$SRC_DIR/installer.c"
    "$UTILS_DIR/network_sniffer.c"
    "$UTILS_DIR/network_state.c"
    "$UI_DIR/logo.c"
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
    log "Compiling $BINARY..."
    gcc \
        "${SOURCES[@]}" \
        -o "$BINARY" \
        -lncurses \
        -lcurl \
        -Wextra \
        -O2 \
        -std=c11 \
        -lcrypto \
        -D_DEFAULT_SOURCE
    log "${GREEN}Build successful!${NC}"
    log "Binary saved as './$BINARY'"
}


log "Lainux Installer Build Script"

for src in "${SOURCES[@]}"; do
    if [[ ! -f "$src" ]]; then
        error "Source file not found: $src"
    fi
done

read -p "Build installer? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log "Build cancelled."
    exit 0
fi

build
