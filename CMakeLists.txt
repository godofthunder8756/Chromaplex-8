cmake_minimum_required(VERSION 3.16)
project(chromaplex8 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ─── Options ───────────────────────────────────────────────────
option(CX8_USE_PICKUP   "Build with PickUp language support" OFF)
option(CX8_USE_BUNDLED  "Use bundled deps from deps/ (auto-detected on Windows)" OFF)
option(CX8_STATIC       "Link statically where possible" OFF)

# Auto-detect bundled deps on Windows when deps/ exists
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/deps/SDL2-2.30.11")
    set(CX8_USE_BUNDLED ON)
endif()

# ─── Dependencies ──────────────────────────────────────────────

if(CX8_USE_BUNDLED)
    # ── Bundled SDL2 (MinGW x86_64 from deps/) ─────────────────
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL2_BUNDLED_DIR "${CMAKE_SOURCE_DIR}/deps/SDL2-2.30.11/x86_64-w64-mingw32")
    else()
        set(SDL2_BUNDLED_DIR "${CMAKE_SOURCE_DIR}/deps/SDL2-2.30.11/i686-w64-mingw32")
    endif()

    set(SDL2_INCLUDE_DIRS "${SDL2_BUNDLED_DIR}/include/SDL2")
    set(SDL2_LIBRARIES
        "${SDL2_BUNDLED_DIR}/lib/libSDL2main.a"
        "${SDL2_BUNDLED_DIR}/lib/libSDL2.dll.a"
    )
    set(SDL2_DLL "${SDL2_BUNDLED_DIR}/bin/SDL2.dll")

    # ── Bundled Lua 5.4 (compiled from source in deps/) ────────
    set(LUA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/deps/lua-5.4.7/src")
    set(LUA_SRC_DIR "${CMAKE_SOURCE_DIR}/deps/lua-5.4.7/src")

    # Build Lua as a static library from source
    file(GLOB LUA_CORE_SRCS
        "${LUA_SRC_DIR}/lapi.c" "${LUA_SRC_DIR}/lcode.c" "${LUA_SRC_DIR}/lctype.c"
        "${LUA_SRC_DIR}/ldebug.c" "${LUA_SRC_DIR}/ldo.c" "${LUA_SRC_DIR}/ldump.c"
        "${LUA_SRC_DIR}/lfunc.c" "${LUA_SRC_DIR}/lgc.c" "${LUA_SRC_DIR}/llex.c"
        "${LUA_SRC_DIR}/lmem.c" "${LUA_SRC_DIR}/lobject.c" "${LUA_SRC_DIR}/lopcodes.c"
        "${LUA_SRC_DIR}/lparser.c" "${LUA_SRC_DIR}/lstate.c" "${LUA_SRC_DIR}/lstring.c"
        "${LUA_SRC_DIR}/ltable.c" "${LUA_SRC_DIR}/ltm.c" "${LUA_SRC_DIR}/lundump.c"
        "${LUA_SRC_DIR}/lvm.c" "${LUA_SRC_DIR}/lzio.c"
        "${LUA_SRC_DIR}/lauxlib.c" "${LUA_SRC_DIR}/lbaselib.c" "${LUA_SRC_DIR}/lcorolib.c"
        "${LUA_SRC_DIR}/ldblib.c" "${LUA_SRC_DIR}/liolib.c" "${LUA_SRC_DIR}/lmathlib.c"
        "${LUA_SRC_DIR}/loadlib.c" "${LUA_SRC_DIR}/loslib.c" "${LUA_SRC_DIR}/lstrlib.c"
        "${LUA_SRC_DIR}/ltablib.c" "${LUA_SRC_DIR}/lutf8lib.c" "${LUA_SRC_DIR}/linit.c"
    )
    add_library(lua_bundled STATIC ${LUA_CORE_SRCS})
    target_include_directories(lua_bundled PUBLIC "${LUA_SRC_DIR}")
    target_compile_definitions(lua_bundled PRIVATE LUA_COMPAT_5_3)
    set(LUA_LIBRARIES lua_bundled)

    message(STATUS "[CX8] Using BUNDLED SDL2 from: ${SDL2_BUNDLED_DIR}")
    message(STATUS "[CX8] Using BUNDLED Lua from: ${LUA_SRC_DIR}")
else()
    # ── System SDL2 ─────────────────────────────────────────────
    find_package(SDL2 REQUIRED)
    # ── System Lua 5.4 ──────────────────────────────────────────
    find_package(Lua REQUIRED)
    message(STATUS "[CX8] Using SYSTEM SDL2: ${SDL2_INCLUDE_DIRS}")
    message(STATUS "[CX8] Using SYSTEM Lua: ${LUA_INCLUDE_DIR}")
endif()

# ─── Source files ──────────────────────────────────────────────
set(CX8_SOURCES
    src/main.c
    src/cx8_memory.c
    src/cx8_gpu.c
    src/cx8_font.c
    src/cx8_input.c
    src/cx8_apu.c
    src/cx8_cart.c
    src/cx8_modules.c
    src/cx8_scripting.c
    src/cx8_home.c
    src/cx8_editor.c
    src/cx8_ed_code.c
    src/cx8_ed_sprite.c
    src/cx8_ed_map.c
    src/cx8_ed_sfx.c
    src/cx8_netlink.c
    src/cx8_pixstretch.c
    src/cx8_persist.c
    src/cx8_music.c
)

# ─── Executable ────────────────────────────────────────────────
add_executable(chromaplex8 ${CX8_SOURCES})

target_include_directories(chromaplex8 PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIR}
    src
)

target_link_libraries(chromaplex8 PRIVATE
    ${SDL2_LIBRARIES}
    ${LUA_LIBRARIES}
)

# Platform-specific link libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(chromaplex8 PRIVATE m pthread dl)
elseif(APPLE)
    target_link_libraries(chromaplex8 PRIVATE m
        "-framework Cocoa" "-framework IOKit" "-framework CoreAudio"
        "-framework CoreVideo" "-framework AudioToolbox" "-framework Carbon")
elseif(WIN32)
    target_link_libraries(chromaplex8 PRIVATE ws2_32 winmm imm32 setupapi version)
    if(CX8_USE_BUNDLED)
        target_link_libraries(chromaplex8 PRIVATE mingw32)
    endif()
endif()

# Static linking option
if(CX8_STATIC AND NOT APPLE)
    target_link_options(chromaplex8 PRIVATE -static-libgcc -static-libstdc++)
    if(UNIX)
        # On Linux, link SDL2 statically if available
        target_link_options(chromaplex8 PRIVATE -static)
    endif()
endif()

# ─── Install ───────────────────────────────────────────────────
install(TARGETS chromaplex8 DESTINATION bin)
install(DIRECTORY carts/ DESTINATION share/chromaplex8/carts)

# ─── Copy runtime assets next to binary ───────────────────────
add_custom_command(TARGET chromaplex8 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/carts"
        "$<TARGET_FILE_DIR:chromaplex8>/carts"
    COMMENT "Copying cartridges to build directory"
)

# Copy SDL2.dll on Windows with bundled deps
if(WIN32 AND CX8_USE_BUNDLED AND DEFINED SDL2_DLL)
    add_custom_command(TARGET chromaplex8 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_DLL}"
            "$<TARGET_FILE_DIR:chromaplex8>"
        COMMENT "Copying SDL2.dll to build directory"
    )
endif()
