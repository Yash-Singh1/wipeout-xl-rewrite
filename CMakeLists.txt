cmake_minimum_required(VERSION 3.20)
project(wipeoutxl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- The PlayStation game: game/src/*.c is the decompilation's C, compiled natively. ----
file(GLOB GAME_SOURCES ${CMAKE_SOURCE_DIR}/game/src/*.c)
list(SORT GAME_SOURCES)
add_library(game STATIC ${GAME_SOURCES})
target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/game/include)
# The console's compiler had signed char, no strict-aliasing assumptions and wrapping signed
# overflow.  Pointer/int conversions and 64->32 truncations stay visible: the port removed every
# one of them (tools/diag.py), so any new one is a bug.
target_compile_options(game PRIVATE
    -std=gnu11
    -include ${CMAKE_SOURCE_DIR}/platform/include/port.h
    -fsigned-char -fno-strict-aliasing -fwrapv
    -Wno-pointer-sign -Wno-deprecated-declarations
    -Wpointer-to-int-cast -Wint-to-pointer-cast -Wshorten-64-to-32
    -Werror=implicit-function-declaration -Werror=int-conversion)

# ---- Data generated from the console executable (game/data, the data worker) ----
file(GLOB GAME_DATA_SOURCES ${CMAKE_SOURCE_DIR}/game/data/*.c)
if(GAME_DATA_SOURCES)
    add_library(gamedata STATIC ${GAME_DATA_SOURCES})
    target_include_directories(gamedata PRIVATE ${CMAKE_SOURCE_DIR}/game/include)
    target_compile_options(gamedata PRIVATE -std=gnu11 -include ${CMAKE_SOURCE_DIR}/platform/include/port.h
        -fsigned-char -fno-strict-aliasing -fwrapv -Wno-pointer-sign)
endif()

# ---- Platform: the PsyQ libraries and the program entry (platform/*.c, platform/*/*.c) ----
file(GLOB_RECURSE PLATFORM_SOURCES ${CMAKE_SOURCE_DIR}/platform/*.c)
list(SORT PLATFORM_SOURCES)
if(PLATFORM_SOURCES)
    find_package(SDL3 CONFIG)
    add_executable(wipeoutxl ${PLATFORM_SOURCES})
    target_include_directories(wipeoutxl PRIVATE ${CMAKE_SOURCE_DIR}/platform ${CMAKE_SOURCE_DIR}/platform/psx
        ${CMAKE_SOURCE_DIR}/game/include)
    target_compile_options(wipeoutxl PRIVATE -fsigned-char -fno-strict-aliasing)
    target_link_libraries(wipeoutxl PRIVATE game m)
    if(GAME_DATA_SOURCES)
        target_link_libraries(wipeoutxl PRIVATE gamedata)
    endif()
    if(SDL3_FOUND)
        target_link_libraries(wipeoutxl PRIVATE SDL3::SDL3)
    endif()
    if(APPLE)
        target_link_libraries(wipeoutxl PRIVATE "-framework OpenGL")
    elseif(WIN32)
        target_link_libraries(wipeoutxl PRIVATE opengl32)
    else()
        find_package(OpenGL)
        if(OpenGL_FOUND)
            target_link_libraries(wipeoutxl PRIVATE OpenGL::GL)
        endif()
    endif()
endif()

# ---- Link check: which symbols does the game still need from the platform and the data? ----
# tools/linkstub.py writes build/linkstub.c: weak definitions for every symbol that the game
# (plus whatever platform and data sources exist) leaves undefined, and prints the list.  The
# `linkcheck` executable links with those stubs, so the toolchain's link step itself proves what
# is left.  Build it with:  cmake --build build --target linkcheck
add_custom_target(linkstub
    COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/tools/linkstub.py
            --build ${CMAKE_BINARY_DIR} --source ${CMAKE_SOURCE_DIR}
    DEPENDS game
    COMMENT "Generating the weak link stubs"
    VERBATIM)

if(PLATFORM_SOURCES AND EXISTS ${CMAKE_SOURCE_DIR}/tools/linkstub.c)
    add_executable(linkcheck EXCLUDE_FROM_ALL ${PLATFORM_SOURCES} ${CMAKE_SOURCE_DIR}/tools/linkstub.c)
    target_include_directories(linkcheck PRIVATE ${CMAKE_SOURCE_DIR}/platform ${CMAKE_SOURCE_DIR}/platform/psx
        ${CMAKE_SOURCE_DIR}/game/include)
    target_link_libraries(linkcheck PRIVATE game m)
    if(GAME_DATA_SOURCES)
        target_link_libraries(linkcheck PRIVATE gamedata)
    endif()
    if(SDL3_FOUND)
        target_link_libraries(linkcheck PRIVATE SDL3::SDL3)
    endif()
    if(APPLE)
        target_link_libraries(linkcheck PRIVATE "-framework OpenGL")
    endif()
endif()
