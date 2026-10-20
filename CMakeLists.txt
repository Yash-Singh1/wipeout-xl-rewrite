cmake_minimum_required(VERSION 3.20)
project(wipeoutxl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The PS1 game, compiled natively: game/src/*.c is the decompilation's C.
file(GLOB GAME_SOURCES ${CMAKE_SOURCE_DIR}/game/src/*.c)
list(SORT GAME_SOURCES)

add_library(game STATIC ${GAME_SOURCES})
target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/game/include)
# The PlayStation compiler's char is signed, it did not assume strict aliasing,
# and signed overflow wrapped.
target_compile_options(game PRIVATE
    -std=gnu11
    -include ${CMAKE_SOURCE_DIR}/platform/include/port.h
    -fsigned-char -fno-strict-aliasing -fwrapv
    -Wno-pointer-sign
    -Wpointer-to-int-cast -Wint-to-pointer-cast -Wshorten-64-to-32)
