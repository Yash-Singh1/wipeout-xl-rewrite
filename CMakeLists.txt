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
    -fsigned-char -fno-strict-aliasing -fwrapv -fno-common
    -Wno-pointer-sign -Wno-deprecated-declarations
    -Wpointer-to-int-cast -Wint-to-pointer-cast -Wshorten-64-to-32
    -Werror=implicit-function-declaration -Werror=int-conversion)

# ---- Data generated from the console executable (game/data, the data worker) ----
file(GLOB GAME_DATA_SOURCES ${CMAKE_SOURCE_DIR}/game/data/*.c)
if(GAME_DATA_SOURCES)
    add_library(gamedata STATIC ${GAME_DATA_SOURCES})
    target_include_directories(gamedata PRIVATE ${CMAKE_SOURCE_DIR}/game/include)
    target_compile_options(gamedata PRIVATE -std=gnu11 -include ${CMAKE_SOURCE_DIR}/platform/include/port.h
        -fsigned-char -fno-strict-aliasing -fwrapv -fno-common -Wno-pointer-sign)
endif()

# ---- Platform: the PsyQ libraries and the program entry ----
# platform/*.c: entry point, window, movies, traces.  platform/psx/: the libraries (gte, gpu, etc here;
# the sound, disc, controller and memory card libraries are the psxio target of platform/psx/CMakeLists.txt).
set(PSXIO_SDL ON CACHE BOOL "Audio output and input through SDL3")
add_subdirectory(platform/psx psxio)
file(GLOB PLATFORM_TOP_SOURCES ${CMAKE_SOURCE_DIR}/platform/*.c)
set(PLATFORM_PSX_SOURCES
    platform/psx/gte.c platform/psx/gte_tables.c platform/psx/gpu_gl.c
    platform/psx/libetc.c platform/psx/libgpu.c platform/psx/libgte.c platform/psx/libgte_more.c)
list(TRANSFORM PLATFORM_PSX_SOURCES PREPEND ${CMAKE_SOURCE_DIR}/)
set(PLATFORM_SOURCES ${PLATFORM_TOP_SOURCES} ${PLATFORM_PSX_SOURCES})
list(SORT PLATFORM_SOURCES)
find_package(SDL3 CONFIG)
find_package(ZLIB QUIET)   # optional: compressed lockstep traces (platform/trace.c)

function(port_platform_target name)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/platform ${CMAKE_SOURCE_DIR}/platform/psx
        ${CMAKE_SOURCE_DIR}/game/include)
    target_compile_options(${name} PRIVATE -fsigned-char -fno-strict-aliasing)
    target_link_libraries(${name} PRIVATE game psxio m)
    if(GAME_DATA_SOURCES)
        target_link_libraries(${name} PRIVATE gamedata)
    endif()
    if(SDL3_FOUND)
        target_link_libraries(${name} PRIVATE SDL3::SDL3)
    endif()
    if(ZLIB_FOUND)
        target_link_libraries(${name} PRIVATE ZLIB::ZLIB)
        target_compile_definitions(${name} PRIVATE PORT_HAVE_ZLIB)
    endif()
    if(APPLE)
        target_link_libraries(${name} PRIVATE "-framework OpenGL")
    elseif(WIN32)
        target_link_libraries(${name} PRIVATE opengl32)
    else()
        find_package(OpenGL)
        if(OpenGL_FOUND)
            target_link_libraries(${name} PRIVATE OpenGL::GL)
        endif()
    endif()
endfunction()

option(PORT_ASM_STUBS "Temporary: stand-ins for the renderer's assembly routines that are not ported yet" OFF)
if(PORT_ASM_STUBS)
    list(APPEND PLATFORM_SOURCES ${CMAKE_SOURCE_DIR}/tools/asm_stubs.c)
endif()
add_executable(wipeoutxl ${PLATFORM_SOURCES})
port_platform_target(wipeoutxl)

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

if(EXISTS ${CMAKE_SOURCE_DIR}/tools/linkstub.c)
    add_executable(linkcheck EXCLUDE_FROM_ALL ${PLATFORM_SOURCES} ${CMAKE_SOURCE_DIR}/tools/linkstub.c)
    port_platform_target(linkcheck)
endif()
