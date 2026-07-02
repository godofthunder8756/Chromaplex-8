#!/usr/bin/env bash
# ╔═══════════════════════════════════════════════════════════════╗
# ║       CHROMAPLEX 8 — Cross-Compile & Package Helper           ║
# ╠═══════════════════════════════════════════════════════════════╣
# ║  Fetches dependencies and builds for all supported targets.   ║
# ║                                                               ║
# ║  Usage:                                                       ║
# ║    ./xbuild.sh deps          # download cross-compile deps   ║
# ║    ./xbuild.sh linux         # build for current Linux       ║
# ║    ./xbuild.sh win64         # cross-compile Windows x64     ║
# ║    ./xbuild.sh win32         # cross-compile Windows x86     ║
# ║    ./xbuild.sh all           # build everything              ║
# ║    ./xbuild.sh clean         # clean build directory         ║
# ╚═══════════════════════════════════════════════════════════════╝

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
DEPS="$ROOT/deps"
BUILD="$ROOT/build"

SDL2_VER="2.30.11"
LUA_VER="5.4.7"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC} $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}   $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
err()   { echo -e "${RED}[ERR]${NC}  $*" >&2; }

# ─── Check prerequisites ──────────────────────────────────────
check_tool() {
    if ! command -v "$1" &>/dev/null; then
        err "Required tool not found: $1"
        echo "  Install it with your package manager."
        return 1
    fi
}

# ─── Download dependencies ────────────────────────────────────
cmd_deps() {
    info "Downloading cross-compilation dependencies..."
    mkdir -p "$DEPS"

    # SDL2 MinGW development libraries
    if [ ! -d "$DEPS/SDL2-$SDL2_VER/x86_64-w64-mingw32" ]; then
        info "Downloading SDL2 $SDL2_VER (MinGW)..."
        local url="https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VER/SDL2-devel-$SDL2_VER-mingw.tar.gz"
        wget -q --show-progress -O "$DEPS/sdl2-mingw.tar.gz" "$url"
        tar xzf "$DEPS/sdl2-mingw.tar.gz" -C "$DEPS/"
        rm -f "$DEPS/sdl2-mingw.tar.gz"
        ok "SDL2 $SDL2_VER downloaded"
    else
        ok "SDL2 $SDL2_VER already present"
    fi

    # Lua source
    if [ ! -f "$DEPS/lua-$LUA_VER/src/lua.h" ]; then
        info "Downloading Lua $LUA_VER source..."
        wget -q --show-progress -O "$DEPS/lua.tar.gz" \
            "https://www.lua.org/ftp/lua-$LUA_VER.tar.gz"
        tar xzf "$DEPS/lua.tar.gz" -C "$DEPS/"
        rm -f "$DEPS/lua.tar.gz"
        ok "Lua $LUA_VER downloaded"
    else
        ok "Lua $LUA_VER already present"
    fi

    echo ""
    ok "All dependencies ready in $DEPS/"
    echo ""
    echo "  Cross-compile tools needed:"
    echo "    Arch:   sudo pacman -S mingw-w64-gcc"
    echo "    Ubuntu: sudo apt install mingw-w64"
    echo "    Fedora: sudo dnf install mingw64-gcc"
    echo ""
}

# ─── Native Linux build ──────────────────────────────────────
cmd_linux() {
    info "Building for Linux (native)..."
    check_tool gcc
    check_tool pkg-config
    make -C "$ROOT" linux
}

# ─── Cross-compile Windows x64 ───────────────────────────────
cmd_win64() {
    info "Cross-compiling for Windows x64..."
    check_tool x86_64-w64-mingw32-gcc

    if [ ! -d "$DEPS/SDL2-$SDL2_VER/x86_64-w64-mingw32" ]; then
        warn "SDL2 MinGW libs not found. Run: $0 deps"
        exit 1
    fi

    make -C "$ROOT" mingw-cross
}

# ─── Cross-compile Windows x86 ───────────────────────────────
cmd_win32() {
    info "Cross-compiling for Windows x86..."
    check_tool i686-w64-mingw32-gcc

    if [ ! -d "$DEPS/SDL2-$SDL2_VER/i686-w64-mingw32" ]; then
        warn "SDL2 MinGW libs not found. Run: $0 deps"
        exit 1
    fi

    make -C "$ROOT" mingw-cross-32
}

# ─── Build all targets ────────────────────────────────────────
cmd_all() {
    cmd_linux
    echo ""
    cmd_win64
    echo ""
    if command -v i686-w64-mingw32-gcc &>/dev/null; then
        cmd_win32
    else
        warn "Skipping win32 (i686-w64-mingw32-gcc not found)"
    fi
    echo ""
    ok "All builds complete!"
    echo "  Linux:  $BUILD/chromaplex8"
    echo "  Win64:  $BUILD/win64/chromaplex8.exe"
    [ -f "$BUILD/win32/chromaplex8.exe" ] && echo "  Win32:  $BUILD/win32/chromaplex8.exe"
}

# ─── Clean ────────────────────────────────────────────────────
cmd_clean() {
    info "Cleaning build directory..."
    rm -rf "$BUILD"
    ok "Done"
}

# ─── Main ─────────────────────────────────────────────────────
case "${1:-help}" in
    deps)    cmd_deps ;;
    linux)   cmd_linux ;;
    win64)   cmd_win64 ;;
    win32)   cmd_win32 ;;
    all)     cmd_all ;;
    clean)   cmd_clean ;;
    *)
        echo ""
        echo "  Chromaplex 8 Cross-Build Helper"
        echo "  ───────────────────────────────"
        echo "  Usage: $0 <command>"
        echo ""
        echo "  Commands:"
        echo "    deps     Download SDL2 + Lua for cross-compilation"
        echo "    linux    Build natively for Linux"
        echo "    win64    Cross-compile for Windows x64"
        echo "    win32    Cross-compile for Windows x86 (32-bit)"
        echo "    all      Build all available targets"
        echo "    clean    Remove build directory"
        echo ""
        ;;
esac
