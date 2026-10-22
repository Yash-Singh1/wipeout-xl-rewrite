cmake_minimum_required(VERSION 3.20)
project(wipeoutxl C CXX)

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
    -Wpointer-to-int-cast -Wint-to-pointer-cast
    -Werror=implicit-function-declaration -Werror=int-conversion)
include(CheckCCompilerFlag)
check_c_compiler_flag(-Wshorten-64-to-32 HAVE_WARN_SHORTEN)   # Clang's; GCC has no such warning
if(HAVE_WARN_SHORTEN)
    target_compile_options(game PRIVATE -Wshorten-64-to-32)
endif()
# GCC would take the game's own overruns (a loop reading one element past an array, as the console
# did) as a promise that the loop ends sooner, and cut it short.
check_c_compiler_flag(-fno-aggressive-loop-optimizations HAVE_NO_AGGRESSIVE_LOOPS)
if(HAVE_NO_AGGRESSIVE_LOOPS)
    target_compile_options(game PRIVATE -fno-aggressive-loop-optimizations)
endif()
# Locals the game reads before writing them: every reachable one is resolved to what the console
# reads (docs/LOCKSTEP.md); any one still unknown reads zero, as the console's untouched stack, and
# never host garbage.
check_c_compiler_flag(-ftrivial-auto-var-init=zero HAVE_AUTO_VAR_INIT_ZERO)
if(HAVE_AUTO_VAR_INIT_ZERO)
    target_compile_options(game PRIVATE -ftrivial-auto-var-init=zero)
endif()

# ---- Data generated from the console executable (game/data, the data worker) ----
file(GLOB GAME_DATA_SOURCES ${CMAKE_SOURCE_DIR}/game/data/*.c)
if(GAME_DATA_SOURCES)
    add_library(gamedata STATIC ${GAME_DATA_SOURCES})
    target_include_directories(gamedata PRIVATE ${CMAKE_SOURCE_DIR}/game/include)
    target_compile_options(gamedata PRIVATE -std=gnu11 -include ${CMAKE_SOURCE_DIR}/platform/include/port.h
        -fsigned-char -fno-strict-aliasing -fwrapv -fno-common -Wno-pointer-sign)
endif()

# ---- SDL3: one platform layer for macOS, Linux, Windows and the web ----
# The web (Emscripten) uses Emscripten's own SDL3 port. Elsewhere the system's SDL3 is used when
# CMake finds it; otherwise (Windows, a Linux without SDL3 packages, or WIPEOUT_BUILD_SDL=ON) SDL3
# is downloaded and built into the program.
if(EMSCRIPTEN)
    add_library(SDL3::SDL3 INTERFACE IMPORTED)
    target_compile_options(SDL3::SDL3 INTERFACE --use-port=sdl3)
    target_link_options(SDL3::SDL3 INTERFACE --use-port=sdl3)
else()
    option(WIPEOUT_BUILD_SDL "Build SDL3 from source instead of using the system's" OFF)
    if(NOT WIPEOUT_BUILD_SDL)
        find_package(SDL3 CONFIG QUIET)
    endif()
    if(NOT TARGET SDL3::SDL3)
        include(FetchContent)
        message(STATUS "SDL3 not found: building it from source")
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL3
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
            URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3)
        FetchContent_MakeAvailable(SDL3)
    endif()
endif()

# ---- Platform: the PsyQ libraries and the program entry ----
# platform/*.c: entry point, window, movies, traces.  platform/psx/: the libraries (gte, gpu, etc here;
# the sound, disc, controller and memory card libraries are the psxio target of platform/psx/CMakeLists.txt).
set(PSXIO_SDL ON CACHE BOOL "Audio output and input through SDL3")
add_subdirectory(platform/psx psxio)
file(GLOB PLATFORM_TOP_SOURCES ${CMAKE_SOURCE_DIR}/platform/*.c)
set(PLATFORM_PSX_SOURCES
    platform/psx/gte.c platform/psx/gte_tables.c platform/psx/gpu_gl.c
    platform/psx/libetc.c platform/psx/libgpu.c platform/psx/libgte.c platform/psx/libgte_more.c
    platform/psx/framecap.c)
list(TRANSFORM PLATFORM_PSX_SOURCES PREPEND ${CMAKE_SOURCE_DIR}/)
# The debug console: Dear ImGui (platform/third_party/imgui, MIT) over SDL3 and OpenGL 3.
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/platform/third_party/imgui)
set(DEBUG_UI_SOURCES ${CMAKE_SOURCE_DIR}/platform/debug_ui.cpp
    ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
set(PLATFORM_SOURCES ${PLATFORM_TOP_SOURCES} ${PLATFORM_PSX_SOURCES} ${DEBUG_UI_SOURCES})
# Disc images in .7z archives (platform/archive.c): the LZMA SDK's 7z decoder (platform/third_party/lzma, public
# domain).
file(GLOB LZMA_SOURCES ${CMAKE_SOURCE_DIR}/platform/third_party/lzma/*.c)
add_library(lzma7z STATIC ${LZMA_SOURCES})
list(SORT PLATFORM_SOURCES)
if(NOT EMSCRIPTEN)
    find_package(ZLIB QUIET)
endif()   # optional: compressed lockstep traces (platform/trace.c)

function(port_platform_target name)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/platform ${CMAKE_SOURCE_DIR}/platform/psx
        ${CMAKE_SOURCE_DIR}/game/include ${IMGUI_DIR})
    set_source_files_properties(${DEBUG_UI_SOURCES} PROPERTIES COMPILE_OPTIONS "-std=c++17")
    target_compile_options(${name} PRIVATE -fsigned-char -fno-strict-aliasing)
    target_link_libraries(${name} PRIVATE game psxio lzma7z m)
    if(GAME_DATA_SOURCES)
        target_link_libraries(${name} PRIVATE gamedata)
    endif()
    target_link_libraries(${name} PRIVATE SDL3::SDL3)
    if(ZLIB_FOUND)
        target_link_libraries(${name} PRIVATE ZLIB::ZLIB)
        target_compile_definitions(${name} PRIVATE PORT_HAVE_ZLIB)
    endif()
    if(EMSCRIPTEN)
        # WebGL 2 through Emscripten's OpenGL ES 3
    elseif(APPLE)
        target_link_libraries(${name} PRIVATE "-framework OpenGL" objc) # objc: App Nap off for the link (link.c)
    elseif(WIN32)
        target_link_libraries(${name} PRIVATE opengl32)
        # one .exe, no MinGW runtime DLLs beside it, no console window; the test traces look the
        # game's globals up in the executable's exports (platform/trace.c)
        if(MINGW)
            target_link_options(${name} PRIVATE -static -Wl,--export-all-symbols)
        endif()
        set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE ON)
    else()
        find_package(OpenGL)
        if(OpenGL_FOUND)
            target_link_libraries(${name} PRIVATE OpenGL::GL)
        endif()
    endif()
    if(NOT WIN32 AND NOT EMSCRIPTEN)
        set_target_properties(${name} PROPERTIES ENABLE_EXPORTS ON) # the traces' dlsym (platform/trace.c)
    endif()
endfunction()

add_executable(wipeoutxl ${PLATFORM_SOURCES})
port_platform_target(wipeoutxl)

# ---- The web: wipeoutxl.html, .js and .wasm (emcmake cmake; README.md) ----
# The game's main loop runs as on the desktop: Asyncify lets its waits (the field pacing, the CD, each
# picture's wait for the display) give the browser its turn. The page reads the player's disc into memory
# (up to the 4 GB WebAssembly allows) and keeps the settings and memory cards in the browser's storage.
if(EMSCRIPTEN)
    set_target_properties(wipeoutxl PROPERTIES SUFFIX ".html")
    set(WEB_SHELL ${CMAKE_SOURCE_DIR}/platform/web/shell.html)
    set_property(TARGET wipeoutxl APPEND PROPERTY LINK_DEPENDS ${WEB_SHELL})
    target_link_options(wipeoutxl PRIVATE
        --shell-file ${WEB_SHELL}
        -sENVIRONMENT=web
        -sASYNCIFY -sASYNCIFY_STACK_SIZE=1048576
        -sSTACK_SIZE=8388608
        -sINITIAL_MEMORY=268435456 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=4294967296
        -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1
        -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM=1 -lidbfs.js
        "-sEXPORTED_FUNCTIONS=_main,_malloc,_free"
        "-sEXPORTED_RUNTIME_METHODS=callMain,FS,IDBFS,HEAPU8")

    # The disc the page loads by itself, as wipeout-rewrite's page has its data: the host's own copy, given
    # here (-DWIPEOUT_WEB_DISC=path/to/Wipeout XL (USA).cue). Its .cue and .bin tracks are linked into disc/
    # beside the page (copy the folder with the links followed to put it on a server: cp -RL) and
    # wipeoutxl-data.json names the .cue; the page reads that first, else tries disc/Wipeout XL (USA).cue,
    # and asks for the player's own files only when neither is there.
    set(WIPEOUT_WEB_DISC "" CACHE FILEPATH "The disc image (.cue) the web page loads by itself")
    if(WIPEOUT_WEB_DISC)
        get_filename_component(webDiscDir "${WIPEOUT_WEB_DISC}" DIRECTORY)
        get_filename_component(webDiscCue "${WIPEOUT_WEB_DISC}" NAME)
        file(STRINGS "${WIPEOUT_WEB_DISC}" webDiscLines REGEX "^[ \t]*FILE")
        set(webDiscFiles "${webDiscCue}")
        foreach(line IN LISTS webDiscLines)
            string(REGEX REPLACE "^[ \t]*FILE[ \t]+\"([^\"]+)\".*" "\\1" track "${line}")
            list(APPEND webDiscFiles "${track}")
        endforeach()
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/disc)
        foreach(name IN LISTS webDiscFiles)
            if(NOT EXISTS "${webDiscDir}/${name}")
                message(FATAL_ERROR "WIPEOUT_WEB_DISC: ${webDiscDir}/${name} is missing")
            endif()
            file(CREATE_LINK "${webDiscDir}/${name}" "${CMAKE_BINARY_DIR}/disc/${name}" SYMBOLIC COPY_ON_ERROR)
        endforeach()
        file(WRITE ${CMAKE_BINARY_DIR}/wipeoutxl-data.json "{ \"disc\": \"disc/${webDiscCue}\" }\n")
        message(STATUS "The web page loads ${webDiscCue} by itself (disc/)")
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

if(EXISTS ${CMAKE_SOURCE_DIR}/tools/linkstub.c)
    add_executable(linkcheck EXCLUDE_FROM_ALL ${PLATFORM_SOURCES} ${CMAKE_SOURCE_DIR}/tools/linkstub.c)
    port_platform_target(linkcheck)
endif()
