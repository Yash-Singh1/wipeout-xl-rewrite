#!/bin/sh
# Small regression tests using synthetic geometry and the movie program table.
set -eu
cd "$(dirname "$0")/../.."
test_build=$(mktemp -d)
trap 'rm -rf "$test_build"' EXIT HUP INT TERM
case $(uname -s) in Darwin) discard='-Wl,-dead_strip';; *) discard='-Wl,--gc-sections';; esac
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations \
    -std=gnu11 -include platform/include/port.h -iquote game/include \
    tools/bugtest/test_clearance.c -o "$test_build/clearance"
"$test_build/clearance"
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard \
    -Iplatform -Iplatform/psx -iquote game/include $(pkg-config --cflags sdl3) \
    tools/bugtest/test_movie.c $(pkg-config --libs sdl3) -o "$test_build/movie"
"$test_build/movie"

# Optional disc images exercise full intro decoding and compare all five frame limits
# with the constants in the disc's player executables, then test skipping playback.
if [ "$#" -gt 0 ]; then
    ${CC:-cc} -O2 -Iplatform -Iplatform/psx -iquote game/include $(pkg-config --cflags sdl3) \
        tools/bugtest/test_movie_disc.c platform/psx/disc.c $(pkg-config --libs sdl3) -lm -o "$test_build/movie_disc"
    for disc in "$@"; do
        "$test_build/movie_disc" "$disc"
    done
fi
