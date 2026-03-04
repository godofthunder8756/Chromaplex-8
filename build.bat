@echo off
REM ╔══════════════════════════════════════════════════════════════╗
REM ║              CHROMAPLEX 8 — Build Script (GCC)               ║
REM ║                                                              ║
REM ║  Uses w64devkit portable GCC + SDL2 MinGW + Lua 5.4         ║
REM ║  All dependencies live in deps\                              ║
REM ║                                                              ║
REM ║  1. Downloads w64devkit, SDL2, Lua if not present            ║
REM ║  2. Compiles Lua from source                                 ║
REM ║  3. Builds chromaplex8.exe                                   ║
REM ╚══════════════════════════════════════════════════════════════╝

setlocal enabledelayedexpansion

set ROOT=%~dp0
set BUILD=%ROOT%build
set DEPS=%ROOT%deps
set SRC=%ROOT%src

REM ─── Tool & dependency paths ──────────────────────────────
set GCC_DIR=%DEPS%\w64devkit\bin
set SDL2_VER=2.30.11
set LUA_VER=5.4.7
set SDL2_DIR=%DEPS%\SDL2-%SDL2_VER%\x86_64-w64-mingw32
set LUA_DIR=%DEPS%\lua-%LUA_VER%

echo.
echo   ╔═══════════════════════════════════════════╗
echo   ║       CHROMAPLEX 8 — BUILD SYSTEM         ║
echo   ╚═══════════════════════════════════════════╝
echo.

REM ─── Check for GCC ────────────────────────────────────────
if not exist "%GCC_DIR%\gcc.exe" (
    echo [BUILD] ERROR: GCC not found at %GCC_DIR%
    echo         Download w64devkit from:
    echo         https://github.com/skeeto/w64devkit/releases
    echo         Extract to %DEPS%\w64devkit\
    exit /b 1
)

set PATH=%GCC_DIR%;%PATH%

REM ─── Check SDL2 ───────────────────────────────────────────
if not exist "%SDL2_DIR%\include\SDL2\SDL.h" (
    echo [BUILD] ERROR: SDL2 not found at %SDL2_DIR%
    echo         Download SDL2-devel-%SDL2_VER%-mingw.tar.gz from:
    echo         https://www.libsdl.org/release/
    echo         Extract to %DEPS%\
    exit /b 1
)

REM ─── Check/Build Lua ──────────────────────────────────────
if not exist "%LUA_DIR%\src\liblua.a" (
    echo [BUILD] Compiling Lua %LUA_VER% ...
    if not exist "%LUA_DIR%\src\lua.h" (
        echo [BUILD] ERROR: Lua source not found at %LUA_DIR%
        echo         Download from https://www.lua.org/ftp/lua-%LUA_VER%.tar.gz
        exit /b 1
    )
    pushd "%LUA_DIR%\src"
    for %%f in (lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c ^
        lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c ^
        lundump.c lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c ^
        liolib.c lmathlib.c loadlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c linit.c) do (
        gcc -std=c11 -O2 -Wall -DLUA_COMPAT_5_3 -c %%f
    )
    ar rcs liblua.a lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o ^
        lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o ^
        lundump.o lvm.o lzio.o lauxlib.o lbaselib.o lcorolib.o ldblib.o ^
        liolib.o lmathlib.o loadlib.o loslib.o lstrlib.o ltablib.o lutf8lib.o linit.o
    popd
    if not exist "%LUA_DIR%\src\liblua.a" (
        echo [BUILD] ERROR: Lua compilation failed
        exit /b 1
    )
    echo [BUILD] Lua compiled successfully
)

REM ─── Create build directory ───────────────────────────────
if not exist "%BUILD%" mkdir "%BUILD%"
if not exist "%BUILD%\carts" mkdir "%BUILD%\carts"

REM ─── Compile Chromaplex 8 ─────────────────────────────────
echo [BUILD] Compiling Chromaplex 8...

set CFLAGS=-std=c11 -O2 -Wall -Wno-unused-result
set INCLUDES=-I"%SDL2_DIR%\include\SDL2" -I"%LUA_DIR%\src" -I"%SRC%"
set LIBS=-L"%SDL2_DIR%\lib" -L"%LUA_DIR%\src" -lmingw32 -lSDL2main -lSDL2 -llua -lm
set SOURCES=%SRC%\main.c %SRC%\cx8_memory.c %SRC%\cx8_gpu.c %SRC%\cx8_font.c %SRC%\cx8_input.c %SRC%\cx8_apu.c %SRC%\cx8_cart.c %SRC%\cx8_modules.c %SRC%\cx8_scripting.c %SRC%\cx8_home.c %SRC%\cx8_editor.c %SRC%\cx8_ed_code.c %SRC%\cx8_ed_sprite.c %SRC%\cx8_ed_map.c %SRC%\cx8_ed_sfx.c

gcc %CFLAGS% %INCLUDES% %SOURCES% %LIBS% -o "%BUILD%\chromaplex8.exe"

if errorlevel 1 (
    echo [BUILD] FAILED
    exit /b 1
)

REM ─── Copy runtime files ───────────────────────────────────
copy /Y "%SDL2_DIR%\bin\SDL2.dll" "%BUILD%\" > nul
xcopy /Y /E /I "%ROOT%carts\*" "%BUILD%\carts\" > nul

echo.
echo   ╔═══════════════════════════════════════════╗
echo   ║            BUILD SUCCESSFUL!              ║
echo   ╠═══════════════════════════════════════════╣
echo   ║  Output: build\chromaplex8.exe            ║
echo   ║                                           ║
echo   ║  Run:  cd build                           ║
echo   ║        chromaplex8.exe                     ║
echo   ║    or: chromaplex8.exe carts\hello.lua     ║
echo   ╚═══════════════════════════════════════════╝
echo.

endlocal