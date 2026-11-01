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

for test in guest_sky guest_animation; do
    ${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard \
        -Iplatform -Iplatform/psx -iquote game/include $(pkg-config --cflags sdl3) \
        "tools/bugtest/test_$test.c" $(pkg-config --libs sdl3) -o "$test_build/$test"
    "$test_build/$test"
done

# Port rendering changes must preserve the original effect segments and quad geometry.
for test in electric near_object; do
    gte_sources=
    if [ "$test" = near_object ]; then
        gte_sources="platform/psx/gte.c platform/psx/gte_tables.c"
    fi
    ${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations \
        -include platform/include/port.h -iquote game/include \
        "tools/bugtest/test_$test.c" $gte_sources -lm -o "$test_build/$test"
    "$test_build/$test"
done

${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -DPORT_VERSION='"0.0.6"' \
    -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) \
    tools/bugtest/test_updates.c $(pkg-config --libs sdl3) -o "$test_build/updates"
"$test_build/updates"

# The input tests drive a real SDL virtual controller and the game's button decoder.
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations -Wno-pointer-sign \
    -include platform/include/port.h -iquote game/include -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) \
    tools/bugtest/test_steering.c platform/psx/pad.c game/src/pad.c $(pkg-config --libs sdl3) -o "$test_build/steering"
"$test_build/steering"
for test in steering_target steering_ghost guest_motion guest_sign; do
    ${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations -Wno-pointer-sign \
        -include platform/include/port.h -iquote game/include "tools/bugtest/test_$test.c" -lm -o "$test_build/$test"
    "$test_build/$test"
done
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations \
    -include platform/include/port.h -iquote game/include tools/bugtest/test_guest_1995.c \
    game/src/skeleton.c platform/psx/libgte.c platform/psx/gte.c platform/psx/gte_tables.c \
    -lm -o "$test_build/guest_1995"
"$test_build/guest_1995"
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -iquote game/include \
    -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) tools/bugtest/test_steering_recording.c \
    $(pkg-config --libs sdl3) -o "$test_build/steering_recording"
"$test_build/steering_recording"

${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -iquote game/include \
    -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) tools/bugtest/test_link_timing.c \
    $(pkg-config --libs sdl3) -o "$test_build/link_timing"
"$test_build/link_timing"

${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Iplatform -Iplatform/psx \
    $(pkg-config --cflags sdl3) tools/bugtest/test_link_lifecycle.c $(pkg-config --libs sdl3) \
    -o "$test_build/link_lifecycle"
"$test_build/link_lifecycle"

# Optional disc images exercise full intro decoding and compare all five frame limits
# with the constants in the disc's player executables, then test skipping playback.
if [ "$#" -gt 0 ]; then
    ${CC:-cc} -O2 -Iplatform -Iplatform/psx -iquote game/include $(pkg-config --cflags sdl3) \
        tools/bugtest/test_movie_disc.c platform/psx/disc.c $(pkg-config --libs sdl3) -lm -o "$test_build/movie_disc"
    for disc in "$@"; do
        "$test_build/movie_disc" "$disc"
    done
fi
