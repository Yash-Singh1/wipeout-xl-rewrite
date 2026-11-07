cmake_minimum_required(VERSION 3.20)
project(wipeoutxl C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- The PlayStation game: game/src/*.c is the decompilation's C, compiled natively. ----
file(GLOB GAME_SOURCES ${CMAKE_SOURCE_DIR}/game/src/*.c ${CMAKE_SOURCE_DIR}/game/src/pc/*.c)
list(SORT GAME_SOURCES)
add_library(game STATIC ${GAME_SOURCES})
# (its headers for its own quoted includes only: its malloc.h is not the C library's, which Android's stdlib.h
# includes)
target_compile_options(game PRIVATE "SHELL:-iquote ${CMAKE_SOURCE_DIR}/game/include")
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
    target_compile_options(gamedata PRIVATE "SHELL:-iquote ${CMAKE_SOURCE_DIR}/game/include")
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
    if(NOT WIPEOUT_BUILD_SDL AND NOT IOS AND NOT ANDROID) # (a phone's: always built for it)
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

# Full-resolution rolling recording must keep JPEG compression off the render path.
# Releases bundle the static SIMD codec; minimal builds retain the stb fallback.
option(WIPEOUT_FAST_JPEG "Use libjpeg-turbo for recorder compression when available" ON)
option(WIPEOUT_REQUIRE_FAST_JPEG "Fail if the recorder's fast JPEG codec is unavailable" OFF)
if(WIPEOUT_FAST_JPEG AND NOT EMSCRIPTEN)
    find_package(libjpeg-turbo CONFIG QUIET)
endif()
if(WIPEOUT_REQUIRE_FAST_JPEG AND NOT TARGET libjpeg-turbo::turbojpeg-static)
    message(FATAL_ERROR "The release requires the static libjpeg-turbo library")
endif()

# ---- Platform: the PsyQ libraries and the program entry ----
# platform/*.c: entry point, window, movies, traces.  platform/psx/: the libraries (gte, gpu, etc here;
# the sound, disc, controller and memory card libraries are the psxio target of platform/psx/CMakeLists.txt).
set(PSXIO_SDL ON CACHE BOOL "Audio output and input through SDL3")
add_subdirectory(platform/psx psxio)
file(GLOB PLATFORM_TOP_SOURCES ${CMAKE_SOURCE_DIR}/platform/*.c)
if(EMSCRIPTEN)
    list(REMOVE_ITEM PLATFORM_TOP_SOURCES ${CMAKE_SOURCE_DIR}/platform/update_http.c)
endif()
if(APPLE)
    enable_language(OBJC)
    list(APPEND PLATFORM_TOP_SOURCES ${CMAKE_SOURCE_DIR}/platform/update_http_apple.m)
    set_source_files_properties(platform/update_http_apple.m PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
elseif(NOT WIN32 AND NOT ANDROID AND NOT EMSCRIPTEN)
    find_package(CURL REQUIRED)
endif()
# The native driver is shared by both game instances. All mutable device state
# belongs to RenderDevice, so the second game's symbol rewriting excludes it.
if(APPLE AND NOT IOS)
    option(WIPEOUT_METALLIB "Compile macOS Metal shaders ahead of time (requires Xcode Metal tools)" OFF)
    if(WIPEOUT_METALLIB)
        find_program(XCRUN_EXECUTABLE xcrun REQUIRED)
        set(METAL_LIBRARY ${CMAKE_BINARY_DIR}/generated/wipeout.metallib)
        set(METAL_SHADER_HEADER ${CMAKE_BINARY_DIR}/generated/shaders_metallib.h)
        set(METAL_MINIMUM "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        if(NOT METAL_MINIMUM)
            set(METAL_MINIMUM "11.0")
        endif()
        add_custom_command(OUTPUT ${METAL_LIBRARY} ${METAL_SHADER_HEADER}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated
            COMMAND ${XCRUN_EXECUTABLE} -sdk macosx metal -std=macos-metal2.3 -fno-fast-math
                -mmacosx-version-min=${METAL_MINIMUM} -c ${CMAKE_SOURCE_DIR}/platform/render/shaders_metal.metal
                -o ${CMAKE_BINARY_DIR}/generated/wipeout.air
            COMMAND ${XCRUN_EXECUTABLE} -sdk macosx metallib ${CMAKE_BINARY_DIR}/generated/wipeout.air -o ${METAL_LIBRARY}
            COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/embed_metallib.py ${METAL_LIBRARY} ${METAL_SHADER_HEADER}
            DEPENDS tools/embed_metallib.py platform/render/shaders_metal.metal VERBATIM)
    else()
        set(METAL_SHADER_HEADER ${CMAKE_BINARY_DIR}/generated/shaders_metal.h)
        add_custom_command(OUTPUT ${METAL_SHADER_HEADER}
            COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/embed_shader.py
                ${CMAKE_SOURCE_DIR}/platform/render/shaders_metal.metal ${METAL_SHADER_HEADER}
            DEPENDS tools/embed_shader.py platform/render/shaders_metal.metal VERBATIM)
    endif()
    add_library(render_metal STATIC platform/render/device_metal.m ${METAL_SHADER_HEADER})
    target_include_directories(render_metal PRIVATE ${CMAKE_BINARY_DIR}/generated)
    target_compile_options(render_metal PRIVATE -fobjc-arc)
    if(WIPEOUT_METALLIB)
        target_compile_definitions(render_metal PRIVATE PORT_PRECOMPILED_METAL)
    endif()
    target_link_libraries(render_metal PRIVATE SDL3::SDL3 "-framework Metal" "-framework QuartzCore" "-framework AppKit")
    add_compile_definitions(PORT_HAVE_METAL)
endif()
set(PLATFORM_PSX_SOURCES
    platform/psx/gte.c platform/psx/gte_tables.c platform/psx/gpu_renderer.c
    platform/render/state.c platform/render/device_gl.c
    platform/psx/libetc.c platform/psx/libgpu.c platform/psx/libgte.c platform/psx/libgte_more.c
    platform/psx/framecap.c)
list(TRANSFORM PLATFORM_PSX_SOURCES PREPEND ${CMAKE_SOURCE_DIR}/)
# The debug console uses SDL3 input and the shared graphics device interface.
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/platform/third_party/imgui)
set(IMGUI_SOURCES ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp)
set(DEBUG_UI_SOURCES ${CMAKE_SOURCE_DIR}/platform/render/imgui.cpp ${CMAKE_SOURCE_DIR}/platform/debug_ui.cpp ${CMAKE_SOURCE_DIR}/platform/touch.cpp ${IMGUI_SOURCES})
# (the port's own: what the second player's game has a copy of, platform/second/)
set(PLATFORM_OWN_SOURCES ${CMAKE_SOURCE_DIR}/platform/render/imgui.cpp ${PLATFORM_TOP_SOURCES} ${PLATFORM_PSX_SOURCES} ${CMAKE_SOURCE_DIR}/platform/debug_ui.cpp
    ${CMAKE_SOURCE_DIR}/platform/touch.cpp)
list(SORT PLATFORM_OWN_SOURCES)
set(PLATFORM_SOURCES ${PLATFORM_TOP_SOURCES} ${PLATFORM_PSX_SOURCES} ${DEBUG_UI_SOURCES})
# Disc images in .7z archives (platform/archive.c): the LZMA SDK's 7z decoder (platform/third_party/lzma, public
# domain).
file(GLOB LZMA_SOURCES ${CMAKE_SOURCE_DIR}/platform/third_party/lzma/*.c)
add_library(lzma7z STATIC ${LZMA_SOURCES})
# The other archives (zip, rar, tar, gzip, bzip2, xz, zstd): libarchive (BSD), the system's when CMake finds it
# (Homebrew's is keg-only: looked for there too), otherwise (or with WIPEOUT_BUILD_LIBARCHIVE=ON) downloaded and
# built into the program with the compression libraries the system has. The web has no archives.
if(NOT EMSCRIPTEN)
    option(WIPEOUT_BUILD_LIBARCHIVE "Build libarchive from source instead of using the system's" OFF)
    if(NOT WIPEOUT_BUILD_LIBARCHIVE AND NOT IOS AND NOT ANDROID)
        if(APPLE)
            list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/libarchive /usr/local/opt/libarchive)
        endif()
        find_package(LibArchive QUIET)
    endif()
    if(LibArchive_FOUND)
        add_library(port_archive INTERFACE)
        target_include_directories(port_archive INTERFACE ${LibArchive_INCLUDE_DIRS})
        target_link_libraries(port_archive INTERFACE ${LibArchive_LIBRARIES})
    else()
        include(FetchContent)
        message(STATUS "libarchive not found: building it from source")
        foreach(off ENABLE_TEST ENABLE_TAR ENABLE_CPIO ENABLE_CAT ENABLE_UNZIP ENABLE_INSTALL ENABLE_OPENSSL
                    ENABLE_MBEDTLS ENABLE_NETTLE ENABLE_LIBXML2 ENABLE_EXPAT ENABLE_ACL ENABLE_XATTR ENABLE_ICONV
                    ENABLE_LIBB2 ENABLE_CNG ENABLE_PCREPOSIX ENABLE_PCRE2POSIX ENABLE_LZO ENABLE_LIBGCC
                    ENABLE_WERROR ENABLE_COVERAGE)
            set(${off} OFF CACHE BOOL "" FORCE)
        endforeach()
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        # no regular expressions (its tools' only): without a POSIX regex library (MinGW) libarchive goes on to
        # look for PCRE, which stops its configuration while ENABLE_LIBGCC is off ("libgcc not found")
        set(POSIX_REGEX_LIB NONE CACHE STRING "" FORCE)
        FetchContent_Declare(libarchive
            URL https://github.com/libarchive/libarchive/releases/download/v3.8.10/libarchive-3.8.10.tar.xz
            URL_HASH SHA256=56cad765459d5ee25a3b761f90c47d99379d28297fbfb72d908880d6b875fb96)
        FetchContent_MakeAvailable(libarchive)
        add_library(port_archive INTERFACE)
        target_include_directories(port_archive INTERFACE ${libarchive_SOURCE_DIR}/libarchive)
        target_compile_definitions(port_archive INTERFACE LIBARCHIVE_STATIC)
        target_link_libraries(port_archive INTERFACE archive_static)
    endif()
endif()
list(SORT PLATFORM_SOURCES)
if(NOT EMSCRIPTEN)
    find_package(ZLIB QUIET)
endif()   # optional: compressed lockstep traces (platform/trace.c)

# The version the debug console shows: the release's (tools/release.py sets WIPEOUT_VERSION), else what git describe
# says when CMake configures (the last tag, and the commit when it is not that tag's); none without git.
set(WIPEOUT_VERSION "" CACHE STRING "The release's version, shown in the debug console (default: git describe)")
if(WIPEOUT_VERSION)
    set(PORT_VERSION "${WIPEOUT_VERSION}")
else()
    # Recompute the version after commits, branch switches or newly fetched tags.
    # A normal build otherwise keeps the version from the last configure.
    execute_process(COMMAND git symbolic-ref -q HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    OUTPUT_VARIABLE version_branch OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    foreach(version_ref HEAD packed-refs refs/tags ${version_branch})
        execute_process(COMMAND git rev-parse --git-path ${version_ref} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                        OUTPUT_VARIABLE version_path OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(version_path)
            get_filename_component(version_path "${version_path}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
            if(NOT EXISTS "${version_path}")
                get_filename_component(version_path "${version_path}" DIRECTORY)
            endif()
            if(EXISTS "${version_path}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${version_path}")
            endif()
        endif()
    endforeach()
    execute_process(COMMAND git describe --tags --always --dirty WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    OUTPUT_VARIABLE PORT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
string(REGEX REPLACE "^v" "" PORT_VERSION "${PORT_VERSION}")
string(REGEX REPLACE "-[0-9]+-g([0-9a-f]+)" "-\\1" PORT_VERSION "${PORT_VERSION}") # (the tag and the commit, not the count)

function(port_platform_target name)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/platform ${CMAKE_SOURCE_DIR}/platform/psx
        ${IMGUI_DIR})
    target_compile_options(${name} PRIVATE "SHELL:-iquote ${CMAKE_SOURCE_DIR}/game/include")
    set_source_files_properties(${DEBUG_UI_SOURCES} PROPERTIES COMPILE_OPTIONS "-std=c++17")
    target_compile_options(${name} PRIVATE -fsigned-char -fno-strict-aliasing)
    target_compile_definitions(${name} PRIVATE "PORT_VERSION=\"${PORT_VERSION}\"")
    target_link_libraries(${name} PRIVATE game psxio lzma7z m)
    if(GAME_DATA_SOURCES)
        target_link_libraries(${name} PRIVATE gamedata)
    endif()
    target_link_libraries(${name} PRIVATE SDL3::SDL3)
    if(WIPEOUT_FAST_JPEG AND TARGET libjpeg-turbo::turbojpeg-static)
        target_link_libraries(${name} PRIVATE libjpeg-turbo::turbojpeg-static)
        target_compile_definitions(${name} PRIVATE PORT_HAVE_TURBOJPEG)
    endif()
    if(TARGET port_archive)
        target_link_libraries(${name} PRIVATE port_archive)
        target_compile_definitions(${name} PRIVATE PORT_HAVE_LIBARCHIVE)
    endif()
    if(APPLE AND NOT IOS)
        target_link_libraries(${name} PRIVATE render_metal)
    endif()
    if(APPLE)
        target_link_libraries(${name} PRIVATE "-framework Foundation")
    elseif(WIN32)
        target_link_libraries(${name} PRIVATE winhttp)
    elseif(NOT ANDROID AND NOT EMSCRIPTEN)
        target_link_libraries(${name} PRIVATE CURL::libcurl)
        target_compile_definitions(${name} PRIVATE PORT_HAVE_CURL)
    endif()
    if(ZLIB_FOUND)
        target_link_libraries(${name} PRIVATE ZLIB::ZLIB)
        target_compile_definitions(${name} PRIVATE PORT_HAVE_ZLIB)
    endif()
    if(EMSCRIPTEN)
        # WebGL 2 through Emscripten's OpenGL ES 3
    elseif(IOS)
        target_link_libraries(${name} PRIVATE "-framework OpenGLES")
    elseif(ANDROID)
        target_link_libraries(${name} PRIVATE GLESv3 android log)
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
    if(NOT WIN32 AND NOT EMSCRIPTEN AND NOT IOS AND NOT ANDROID)
        set_target_properties(${name} PROPERTIES ENABLE_EXPORTS ON) # the traces' dlsym (platform/trace.c)
    endif()
endfunction()

# Two players in one program (platform/second/): on the desktop the program holds a second copy of the game for
# the second player. The web and phones have none.
if(NOT EMSCRIPTEN AND NOT IOS AND NOT ANDROID)
    set(PORT_SECOND_GAME ON)
    # the first game's platform layer, as objects: the second game's copy is compiled under their names' own
    add_library(platform OBJECT ${PLATFORM_OWN_SOURCES})
    port_platform_target(platform)
    target_compile_definitions(platform PRIVATE PORT_HAVE_SECOND_GAME)
    add_library(imgui STATIC ${IMGUI_SOURCES})
    target_include_directories(imgui PRIVATE ${IMGUI_DIR})
    target_compile_options(imgui PRIVATE -std=c++17)
    target_link_libraries(imgui PRIVATE SDL3::SDL3)
    set(PROGRAM_SOURCES $<TARGET_OBJECTS:platform> ${CMAKE_SOURCE_DIR}/platform/second/host.c
        ${CMAKE_SOURCE_DIR}/platform/second/entry.c)
else()
    set(PORT_SECOND_GAME OFF)
    set(PROGRAM_SOURCES ${PLATFORM_SOURCES})
endif()
if(ANDROID)
    # Android: the library SDL's activity loads (platform/android: the app around it, built with Gradle)
    add_library(wipeoutxl SHARED ${PROGRAM_SOURCES})
    set_target_properties(wipeoutxl PROPERTIES OUTPUT_NAME main)
elseif(WIN32)
    # the PC version's icon (platform/windows)
    enable_language(RC)
    add_executable(wipeoutxl ${PROGRAM_SOURCES} ${CMAKE_SOURCE_DIR}/platform/windows/wipeoutxl.rc)
else()
    add_executable(wipeoutxl ${PROGRAM_SOURCES})
endif()
port_platform_target(wipeoutxl)
if(PORT_SECOND_GAME)
    target_link_libraries(wipeoutxl PRIVATE imgui)
    include(cmake/second_game.cmake)
endif()

# ---- macOS: Wipeout XL.app beside the program, after each build ----
# The program with the Mac version's icon, and the libraries it uses from Homebrew inside (it runs on a Mac
# without them). The program stays beside it as well (the tests run it).
if(APPLE AND NOT IOS)
    set(MAC_APP "${CMAKE_BINARY_DIR}/Wipeout XL.app")
    add_custom_command(OUTPUT "${MAC_APP}/Contents/Info.plist"
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/macos_app.py $<TARGET_FILE:wipeoutxl> "${MAC_APP}"
                ${CMAKE_SOURCE_DIR}/platform/macos/Info.plist ${CMAKE_SOURCE_DIR}/platform/macos/wipeoutxl.icns ${METAL_LIBRARY}
        DEPENDS wipeoutxl ${CMAKE_SOURCE_DIR}/tools/macos_app.py ${CMAKE_SOURCE_DIR}/platform/macos/Info.plist
                ${CMAKE_SOURCE_DIR}/platform/macos/wipeoutxl.icns ${METAL_LIBRARY}
                ${CMAKE_SOURCE_DIR}/docs/licenses/libjpeg-turbo.txt
        COMMENT "Wipeout XL.app"
        VERBATIM)
    add_custom_target(macos_app ALL DEPENDS "${MAC_APP}/Contents/Info.plist")
endif()

# ---- iOS: Wipeout XL.app (cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS; README.md) ----
# Landscape and full screen; its Documents folder is in the Files app ("On My iPhone"), where the player puts
# the disc image. A device build is signed with the player's team (WIPEOUT_IOS_TEAM).
if(IOS)
    set(WIPEOUT_IOS_BUNDLE_ID "com.example.wipeoutxl" CACHE STRING "The iOS app's bundle identifier")
    set(WIPEOUT_IOS_TEAM "" CACHE STRING "The Apple development team that signs the iOS app")
    set_target_properties(wipeoutxl PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Wipeout XL"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/platform/ios/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER ${WIPEOUT_IOS_BUNDLE_ID}
        MACOSX_BUNDLE_BUNDLE_NAME "Wipeout XL"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${WIPEOUT_IOS_BUNDLE_ID}
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${WIPEOUT_IOS_TEAM}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic)
endif()

# ---- The web: wipeoutxl.html, .js and .wasm (emcmake cmake; README.md) ----
# The game's main loop runs as on the desktop: Asyncify lets its waits (the field pacing, the CD, each
# picture's wait for the display) give the browser its turn. The page reads the player's disc into memory
# (up to the 4 GB WebAssembly allows) and keeps the settings and memory cards in the browser's storage.
if(EMSCRIPTEN)
    set_target_properties(wipeoutxl PROPERTIES SUFFIX ".html")
    # Let Binaryen inline small hot helpers within the bound below instead of
    # inheriting LLVM's large single-caller merges before Asyncify runs.
    foreach(web_target wipeoutxl game psxio)
        target_compile_options(${web_target} PRIVATE -fno-inline-functions)
    endforeach()
    set(WEB_SHELL ${CMAKE_SOURCE_DIR}/platform/web/shell.html)
    set(WEB_VIEWPORT ${CMAKE_SOURCE_DIR}/platform/web/viewport.js)
    set(WEB_FULLSCREEN ${CMAKE_SOURCE_DIR}/platform/web/fullscreen.js)
    set(WEB_AUDIO ${CMAKE_SOURCE_DIR}/platform/web/audio.js)
    set(WEB_AUDIO_OUTPUT ${CMAKE_SOURCE_DIR}/platform/web/audio-output.js)
    set(WEB_PRESENTATION ${CMAKE_SOURCE_DIR}/platform/web/presentation.js)
    set(WEB_SAVES ${CMAKE_SOURCE_DIR}/platform/web/saves.js)
    set_property(TARGET wipeoutxl APPEND PROPERTY LINK_DEPENDS ${WEB_SHELL} ${WEB_VIEWPORT} ${WEB_FULLSCREEN} ${WEB_AUDIO} ${WEB_AUDIO_OUTPUT} ${WEB_PRESENTATION} ${WEB_SAVES})
    target_link_options(wipeoutxl PRIVATE
        --emit-symbol-map
        --shell-file ${WEB_SHELL}
        "SHELL:--pre-js '${WEB_VIEWPORT}'"
        "SHELL:--pre-js '${WEB_FULLSCREEN}'"
        "SHELL:--pre-js '${WEB_AUDIO}'"
        "SHELL:--pre-js '${WEB_AUDIO_OUTPUT}'"
        "SHELL:--pre-js '${WEB_PRESENTATION}'"
        "SHELL:--pre-js '${WEB_SAVES}'"
        -sENVIRONMENT=web
        -sASYNCIFY -sASYNCIFY_STACK_SIZE=1048576
        # Binaryen otherwise merges single-caller functions into very large
        # Asyncify bodies. Older WebKit register allocators exhaust process
        # memory while compiling them. Keep small-function optimization while
        # bounding those merges; suspension coverage remains conservative.
        "-sBINARYEN_EXTRA_PASSES=--inline-max-combined-binary-size=8192"
        -sSTACK_SIZE=8388608
        -sINITIAL_MEMORY=167772160 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=4294967296
        -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
        -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM=1 -lidbfs.js
        "-sEXPORTED_FUNCTIONS=_main,_malloc,_free"
        "-sEXPORTED_RUNTIME_METHODS=callMain,FS,IDBFS,HEAPU8")

    # The disc the page loads by itself, as wipeout-rewrite's page has its data: the host's own copy, given
    # here (-DWIPEOUT_WEB_DISC=path/to/Wipeout XL (USA).cue). Its .cue and .bin tracks are linked into disc/
    # beside the page (copy the folder with the links followed to put it on a server: cp -RL) and
    # wipeoutxl-data.json names the .cue; the page reads that first, else tries disc/Wipeout XL (USA).cue,
    # and asks for the player's own files only when neither is there.
    set(WIPEOUT_WEB_DISC "" CACHE FILEPATH "The disc image (.cue) the web page loads by itself")
    # Without the music (OFF): the data track alone (86 MB of the disc's 680), with a .cue of its own; the game plays
    # it with silent music tracks (platform/psx/disc.c)
    option(WIPEOUT_WEB_DISC_MUSIC "The web page's disc with its music tracks" ON)
    if(WIPEOUT_WEB_DISC)
        get_filename_component(webDiscDir "${WIPEOUT_WEB_DISC}" DIRECTORY)
        get_filename_component(webDiscCue "${WIPEOUT_WEB_DISC}" NAME)
        file(STRINGS "${WIPEOUT_WEB_DISC}" webDiscLines REGEX "^[ \t]*FILE")
        if(WIPEOUT_WEB_DISC_MUSIC)
            set(webDiscFiles "${webDiscCue}")
        else()
            list(GET webDiscLines 0 webDiscLines)
            set(webDiscFiles "")
        endif()
        foreach(line IN LISTS webDiscLines)
            string(REGEX REPLACE "^[ \t]*FILE[ \t]+\"([^\"]+)\".*" "\\1" track "${line}")
            list(APPEND webDiscFiles "${track}")
        endforeach()
        file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/disc) # (another configuration's links)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/disc)
        if(NOT WIPEOUT_WEB_DISC_MUSIC)
            # the .cue's first track: its FILE line and what follows up to the next FILE
            file(STRINGS "${WIPEOUT_WEB_DISC}" cueLines)
            set(webCue "")
            set(files 0)
            foreach(line IN LISTS cueLines)
                if(line MATCHES "^[ \t]*FILE")
                    math(EXPR files "${files} + 1")
                endif()
                if(files EQUAL 1)
                    string(APPEND webCue "${line}\n")
                endif()
            endforeach()
            file(WRITE "${CMAKE_BINARY_DIR}/disc/${webDiscCue}" "${webCue}")
        endif()
        foreach(name IN LISTS webDiscFiles)
            if(NOT EXISTS "${webDiscDir}/${name}")
                message(FATAL_ERROR "WIPEOUT_WEB_DISC: ${webDiscDir}/${name} is missing")
            endif()
            file(CREATE_LINK "${webDiscDir}/${name}" "${CMAKE_BINARY_DIR}/disc/${name}" SYMBOLIC COPY_ON_ERROR)
        endforeach()
        file(WRITE ${CMAKE_BINARY_DIR}/wipeoutxl-data.json "{ \"disc\": \"disc/${webDiscCue}\" }\n")
        if(WIPEOUT_WEB_DISC_MUSIC)
            message(STATUS "The web page loads ${webDiscCue} by itself (disc/)")
        else()
            message(STATUS "The web page loads ${webDiscCue}'s data track by itself (disc/), without the music")
        endif()
    endif()
endif()

# ---- Link check: which symbols does the game still need from the platform and the data? ----
# tools/linkstub.py writes linkstub.c in the build folder: weak definitions for every symbol that the game
# (plus whatever platform and data sources exist) leaves undefined, and prints the list.  The
# `linkcheck` executable links with those stubs, so the toolchain's link step itself proves what
# is left.  Build it with:  cmake --build build/mac --target linkcheck
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
