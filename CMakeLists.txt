cmake_minimum_required(VERSION 3.16)
project(chromaplex8 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ─── Options ───────────────────────────────────────────────────
option(CX8_USE_PICKUP "Build with PickUp language support" OFF)

# ─── Dependencies ──────────────────────────────────────────────
# SDL2
find_package(SDL2 REQUIRED)

# Lua 5.4
find_package(Lua REQUIRED)

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

# Math library on Unix
if(UNIX)
    target_link_libraries(chromaplex8 PRIVATE m)
endif()

# ─── Install ───────────────────────────────────────────────────
install(TARGETS chromaplex8 DESTINATION bin)
install(DIRECTORY carts/ DESTINATION share/chromaplex8/carts)
install(DIRECTORY modules/ DESTINATION share/chromaplex8/modules)

# ─── Copy runtime assets next to binary ───────────────────────
add_custom_command(TARGET chromaplex8 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/carts"
        "$<TARGET_FILE_DIR:chromaplex8>/carts"
    COMMENT "Copying cartridges to build directory"
)
