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
