# ╔═══════════════════════════════════════════════════════════════╗
# ║              CHROMAPLEX 8 — Cross-Platform Makefile            ║
# ╠═══════════════════════════════════════════════════════════════╣
# ║  Targets: linux, linux-static, arch, macos, mingw-cross       ║
# ║                                                               ║
# ║  Usage:                                                       ║
# ║    make                    # native build (auto-detect OS)    ║
# ║    make linux              # Linux with system SDL2/Lua       ║
# ║    make linux-static       # Linux static (portable binary)   ║
# ║    make arch               # Arch Linux (pacman deps)         ║
# ║    make macos              # macOS with Homebrew deps         ║
# ║    make mingw-cross        # Cross-compile Win64 from Linux   ║
# ║    make clean                                                 ║
# ╚═══════════════════════════════════════════════════════════════╝

# ─── Project ──────────────────────────────────────────────────
NAME     := chromaplex8
VERSION  := 1.0.0
BUILD    := build
SRC      := src

SOURCES  := $(wildcard $(SRC)/*.c)
OBJECTS  := $(SOURCES:$(SRC)/%.c=$(BUILD)/%.o)

# ─── Compiler defaults ────────────────────────────────────────
CC       ?= gcc
CFLAGS   := -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-result
CFLAGS   += -I$(SRC)
LDFLAGS  :=

# ─── OS detection ─────────────────────────────────────────────
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME_S),Linux)
    DETECTED_OS := linux
else ifeq ($(UNAME_S),Darwin)
    DETECTED_OS := macos
else
    DETECTED_OS := windows
endif

# ═══════════════════════════════════════════════════════════════
#  PLATFORM TARGETS
# ═══════════════════════════════════════════════════════════════

# ─── Default: auto-detect ─────────────────────────────────────
.PHONY: all
all: $(DETECTED_OS)

# ─── Linux (Debian/Ubuntu/Fedora/generic) ─────────────────────
# Deps: sudo apt install libsdl2-dev liblua5.4-dev
#    or: sudo dnf install SDL2-devel lua-devel
.PHONY: linux
linux: CFLAGS  += $(shell pkg-config --cflags sdl2 lua5.4 2>/dev/null || pkg-config --cflags sdl2 lua)
linux: LDFLAGS += $(shell pkg-config --libs sdl2 lua5.4 2>/dev/null || pkg-config --libs sdl2 lua)
linux: LDFLAGS += -lm -ldl -lpthread
linux: $(BUILD)/$(NAME)
	@cp -r carts $(BUILD)/carts 2>/dev/null || true
	@echo ""
	@echo "  ✓ Built: $(BUILD)/$(NAME) (Linux)"
	@echo ""

# ─── Linux static (AppImage-friendly portable binary) ─────────
# Requires static SDL2: build SDL2 with --enable-static
.PHONY: linux-static
linux-static: CFLAGS  += $(shell pkg-config --cflags sdl2 lua5.4 2>/dev/null || pkg-config --cflags sdl2 lua)
linux-static: LDFLAGS += $(shell pkg-config --libs --static sdl2 lua5.4 2>/dev/null || pkg-config --libs --static sdl2 lua)
linux-static: LDFLAGS += -lm -ldl -lpthread -static-libgcc
linux-static: $(BUILD)/$(NAME)
	@strip $(BUILD)/$(NAME)
	@cp -r carts $(BUILD)/carts 2>/dev/null || true
	@echo ""
	@echo "  ✓ Built: $(BUILD)/$(NAME) (Linux, stripped)"
	@echo ""

# ─── Arch Linux ───────────────────────────────────────────────
# Deps: sudo pacman -S sdl2 lua
.PHONY: arch
arch: CFLAGS  += $(shell pkg-config --cflags sdl2 lua)
arch: LDFLAGS += $(shell pkg-config --libs sdl2 lua)
arch: LDFLAGS += -lm -ldl -lpthread
arch: $(BUILD)/$(NAME)
	@cp -r carts $(BUILD)/carts 2>/dev/null || true
	@echo ""
	@echo "  ✓ Built: $(BUILD)/$(NAME) (Arch Linux)"
	@echo "  Install: sudo install -Dm755 $(BUILD)/$(NAME) /usr/local/bin/$(NAME)"
	@echo ""

# ─── macOS (Homebrew) ─────────────────────────────────────────
# Deps: brew install sdl2 lua
.PHONY: macos
macos: CFLAGS  += $(shell pkg-config --cflags sdl2 lua)
macos: LDFLAGS += $(shell pkg-config --libs sdl2 lua)
macos: LDFLAGS += -framework Cocoa -framework IOKit -framework CoreAudio \
                  -framework CoreVideo -framework AudioToolbox -framework Carbon
macos: $(BUILD)/$(NAME)
	@cp -r carts $(BUILD)/carts 2>/dev/null || true
	@echo ""
	@echo "  ✓ Built: $(BUILD)/$(NAME) (macOS)"
	@echo ""

# ─── Cross-compile Windows x64 from Linux ─────────────────────
# Deps: sudo apt install mingw-w64
#       Download SDL2-devel-*-mingw.tar.gz → deps/SDL2-2.30.11/
#       Ensure deps/lua-5.4.7/src/ has Lua source
MINGW_CC     := x86_64-w64-mingw32-gcc
MINGW_STRIP  := x86_64-w64-mingw32-strip
MINGW_AR     := x86_64-w64-mingw32-ar
MINGW_SDL2   := deps/SDL2-2.30.11/x86_64-w64-mingw32
MINGW_LUA    := deps/lua-5.4.7/src
MINGW_BUILD  := $(BUILD)/win64

# Lua sources for cross-compile bundling
LUA_SRCS := lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c \
            llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c \
            ltable.c ltm.c lundump.c lvm.c lzio.c lauxlib.c lbaselib.c \
            lcorolib.c ldblib.c liolib.c lmathlib.c loadlib.c loslib.c \
            lstrlib.c ltablib.c lutf8lib.c linit.c

.PHONY: mingw-cross
mingw-cross: $(MINGW_BUILD)/$(NAME).exe
	@echo ""
	@echo "  ✓ Built: $(MINGW_BUILD)/$(NAME).exe (Windows x64, cross-compiled)"
	@echo ""

$(MINGW_BUILD)/$(NAME).exe: $(SOURCES) | $(MINGW_BUILD)/liblua.a
	@mkdir -p $(MINGW_BUILD)/carts
	$(MINGW_CC) -std=c11 -O2 -Wall -Wno-unused-result \
		-I$(MINGW_SDL2)/include/SDL2 \
		-I$(MINGW_LUA) \
		-I$(SRC) \
		$(SOURCES) \
		-L$(MINGW_SDL2)/lib \
		-L$(MINGW_BUILD) \
		-lmingw32 -lSDL2main -lSDL2 -llua -lm -lws2_32 \
		-o $@
	$(MINGW_STRIP) $@
	@cp $(MINGW_SDL2)/bin/SDL2.dll $(MINGW_BUILD)/
	@cp -r carts/* $(MINGW_BUILD)/carts/

$(MINGW_BUILD)/liblua.a:
	@mkdir -p $(MINGW_BUILD)
	@echo "[LUA] Cross-compiling Lua 5.4 for Windows x64..."
	@for f in $(LUA_SRCS); do \
		$(MINGW_CC) -std=c11 -O2 -DLUA_COMPAT_5_3 -c $(MINGW_LUA)/$$f \
			-o $(MINGW_BUILD)/$${f%.c}.o; \
	done
	$(MINGW_AR) rcs $@ $(MINGW_BUILD)/*.o
	@rm -f $(MINGW_BUILD)/*.o

# ─── Cross-compile Windows i686 (32-bit) from Linux ────────────
MINGW32_CC     := i686-w64-mingw32-gcc
MINGW32_STRIP  := i686-w64-mingw32-strip
MINGW32_AR     := i686-w64-mingw32-ar
MINGW32_SDL2   := deps/SDL2-2.30.11/i686-w64-mingw32
MINGW32_BUILD  := $(BUILD)/win32

.PHONY: mingw-cross-32
mingw-cross-32: $(MINGW32_BUILD)/$(NAME).exe
	@echo ""
	@echo "  ✓ Built: $(MINGW32_BUILD)/$(NAME).exe (Windows x86, cross-compiled)"
	@echo ""

$(MINGW32_BUILD)/$(NAME).exe: $(SOURCES) | $(MINGW32_BUILD)/liblua.a
	@mkdir -p $(MINGW32_BUILD)/carts
	$(MINGW32_CC) -std=c11 -O2 -Wall -Wno-unused-result \
		-I$(MINGW32_SDL2)/include/SDL2 \
		-I$(MINGW_LUA) \
		-I$(SRC) \
		$(SOURCES) \
		-L$(MINGW32_SDL2)/lib \
		-L$(MINGW32_BUILD) \
		-lmingw32 -lSDL2main -lSDL2 -llua -lm -lws2_32 \
		-o $@
	$(MINGW32_STRIP) $@
	@cp $(MINGW32_SDL2)/bin/SDL2.dll $(MINGW32_BUILD)/
	@cp -r carts/* $(MINGW32_BUILD)/carts/

$(MINGW32_BUILD)/liblua.a:
	@mkdir -p $(MINGW32_BUILD)
	@echo "[LUA] Cross-compiling Lua 5.4 for Windows x86..."
	@for f in $(LUA_SRCS); do \
		$(MINGW32_CC) -std=c11 -O2 -DLUA_COMPAT_5_3 -c $(MINGW_LUA)/$$f \
			-o $(MINGW32_BUILD)/$${f%.c}.o; \
	done
	$(MINGW32_AR) rcs $@ $(MINGW32_BUILD)/*.o
	@rm -f $(MINGW32_BUILD)/*.o

# ═══════════════════════════════════════════════════════════════
#  COMMON BUILD RULES
# ═══════════════════════════════════════════════════════════════

$(BUILD)/$(NAME): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	@mkdir -p $(BUILD)

# ═══════════════════════════════════════════════════════════════
#  PACKAGING
# ═══════════════════════════════════════════════════════════════

.PHONY: dist-linux
dist-linux: linux
	@mkdir -p $(BUILD)/dist/$(NAME)-$(VERSION)-linux-x64
	@cp $(BUILD)/$(NAME) $(BUILD)/dist/$(NAME)-$(VERSION)-linux-x64/
	@cp -r carts $(BUILD)/dist/$(NAME)-$(VERSION)-linux-x64/
	@cp README.md $(BUILD)/dist/$(NAME)-$(VERSION)-linux-x64/ 2>/dev/null || true
	@cd $(BUILD)/dist && tar czf $(NAME)-$(VERSION)-linux-x64.tar.gz $(NAME)-$(VERSION)-linux-x64/
	@echo "  ✓ Package: $(BUILD)/dist/$(NAME)-$(VERSION)-linux-x64.tar.gz"

.PHONY: dist-win64
dist-win64: mingw-cross
	@mkdir -p $(BUILD)/dist/$(NAME)-$(VERSION)-win64
	@cp $(MINGW_BUILD)/$(NAME).exe $(BUILD)/dist/$(NAME)-$(VERSION)-win64/
	@cp $(MINGW_BUILD)/SDL2.dll $(BUILD)/dist/$(NAME)-$(VERSION)-win64/
	@cp -r carts $(BUILD)/dist/$(NAME)-$(VERSION)-win64/
	@cp README.md $(BUILD)/dist/$(NAME)-$(VERSION)-win64/ 2>/dev/null || true
	@cd $(BUILD)/dist && zip -r $(NAME)-$(VERSION)-win64.zip $(NAME)-$(VERSION)-win64/
	@echo "  ✓ Package: $(BUILD)/dist/$(NAME)-$(VERSION)-win64.zip"

.PHONY: dist-all
dist-all: dist-linux dist-win64

# ═══════════════════════════════════════════════════════════════
#  UTILITY
# ═══════════════════════════════════════════════════════════════

.PHONY: clean
clean:
	rm -rf $(BUILD)

.PHONY: install
install: $(BUILD)/$(NAME)
	install -Dm755 $(BUILD)/$(NAME) $(DESTDIR)/usr/local/bin/$(NAME)
	install -dm755 $(DESTDIR)/usr/local/share/$(NAME)/carts
	cp -r carts/* $(DESTDIR)/usr/local/share/$(NAME)/carts/

.PHONY: help
help:
	@echo ""
	@echo "  Chromaplex 8 Build System"
	@echo "  ─────────────────────────"
	@echo "  make              Auto-detect OS and build natively"
	@echo "  make linux        Linux (system SDL2 + Lua via pkg-config)"
	@echo "  make linux-static Linux with static linking"
	@echo "  make arch         Arch Linux (pacman package names)"
	@echo "  make macos        macOS (Homebrew SDL2 + Lua)"
	@echo "  make mingw-cross  Cross-compile Windows x64 from Linux"
	@echo "  make mingw-cross-32  Cross-compile Windows x86 from Linux"
	@echo "  make dist-linux   Package Linux build as .tar.gz"
	@echo "  make dist-win64   Package Windows x64 build as .zip"
	@echo "  make dist-all     Package all targets"
	@echo "  make install      Install to /usr/local (Linux/macOS)"
	@echo "  make clean        Remove build directory"
	@echo ""
