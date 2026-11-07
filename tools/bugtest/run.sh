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
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations -Wno-pointer-sign \
    -std=gnu11 -include platform/include/port.h -iquote game/include \
    tools/bugtest/test_hud_fill.c -o "$test_build/hud_fill"
"$test_build/hud_fill"
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard \
    -Iplatform -Iplatform/psx -iquote game/include $(pkg-config --cflags sdl3) \
    tools/bugtest/test_movie.c $(pkg-config --libs sdl3) -o "$test_build/movie"
"$test_build/movie"

${CC:-cc} -O2 -Iplatform -Iplatform/psx -iquote game/include $(pkg-config --cflags sdl3) \
    tools/bugtest/test_glyph_strokes.c platform/keyglyphs.c $(pkg-config --libs sdl3) -o "$test_build/glyph_strokes"
"$test_build/glyph_strokes"

${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations \
    -include platform/include/port.h -iquote game/include tools/bugtest/test_lobby.c \
    game/data/menus_data.c -o "$test_build/lobby"
"$test_build/lobby"

${CC:-cc} -O2 -Iplatform $(pkg-config --cflags sdl3) tools/bugtest/test_second_exit.c \
    $(pkg-config --libs sdl3) -o "$test_build/second_exit"
"$test_build/second_exit"

${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard \
    -Iplatform -Iplatform/psx -iquote game/include $(pkg-config --cflags sdl3) tools/bugtest/test_ordering_cycle.c \
    $(pkg-config --libs sdl3) -o "$test_build/ordering_cycle"
"$test_build/ordering_cycle"

${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard \
    -Iplatform -Iplatform/psx -iquote game/include $(pkg-config --cflags sdl3) tools/bugtest/test_menu_reuse.c \
    $(pkg-config --libs sdl3) -o "$test_build/menu_reuse"
"$test_build/menu_reuse"

${CC:-cc} -O2 -ffunction-sections -fdata-sections $discard \
    tools/bugtest/test_projection_cache.c -lm -o "$test_build/projection_cache"
"$test_build/projection_cache"

${CC:-cc} -O1 -Iplatform $(pkg-config --cflags sdl3) tools/bugtest/test_downsample_settings.c \
    $(pkg-config --libs sdl3) -o "$test_build/downsample_settings"
"$test_build/downsample_settings"

${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations \
    -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) tools/bugtest/test_framecap_storage.c \
    $(pkg-config --libs sdl3) -o "$test_build/framecap_storage"

${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard \
    -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) tools/bugtest/test_rumble_stop.c \
    $(pkg-config --libs sdl3) -o "$test_build/rumble_stop"
"$test_build/rumble_stop"
"$test_build/framecap_storage"

# Keep the portable encoder covered, plus the optional SIMD codec when installed.
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations \
    -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) tools/bugtest/test_framecap_codec.c \
    $(pkg-config --libs sdl3) -o "$test_build/framecap_codec"
"$test_build/framecap_codec"
if pkg-config --exists libturbojpeg; then
    for test in framecap_storage framecap_codec; do
        ${CC:-cc} -O1 -DPORT_HAVE_TURBOJPEG -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations \
            -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3 libturbojpeg) "tools/bugtest/test_$test.c" \
            $(pkg-config --libs sdl3 libturbojpeg) -o "$test_build/${test}_turbo"
        "$test_build/${test}_turbo"
    done
fi

${CC:-cc} -O1 -Iplatform $(pkg-config --cflags sdl3) tools/bugtest/test_render_switch.c \
    $(pkg-config --libs sdl3) -lm -o "$test_build/render_switch"
"$test_build/render_switch"
${CC:-cc} -O2 -D_FORTIFY_SOURCE=2 -include platform/include/port.h -iquote game/include \
    tools/bugtest/test_card_label.c -o "$test_build/card_label"
"$test_build/card_label" TESTSAVE

# Optional real GPU checks, for a desktop session with an OpenGL driver.
if [ "${WIPEOUT_TEST_GL:-0}" = 1 ]; then
    case $(uname -s) in Darwin) gl_libs='-framework OpenGL';; *) gl_libs='-lGL';; esac
    gpu_defines=''
    gpu_objects=''
    if [ "${WIPEOUT_TEST_METAL:-0}" = 1 ]; then
        python3 tools/embed_shader.py platform/render/shaders_metal.metal "$test_build/shaders_metal.h"
        ${CC:-cc} -c -fobjc-arc -Wno-deprecated-declarations -Iplatform -I"$test_build" $(pkg-config --cflags sdl3) \
            platform/render/device_metal.m -o "$test_build/metal.o"
        gpu_defines='-DPORT_HAVE_METAL'
        gpu_objects="$test_build/metal.o"
        gl_libs="$gl_libs -framework Metal -framework QuartzCore -framework AppKit"
    fi
    ${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard \
        $gpu_defines -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) tools/bugtest/test_gpu_hud.c platform/render/state.c platform/render/device_gl.c \
        $gpu_objects $(pkg-config --libs sdl3) $gl_libs -lm -o "$test_build/gpu_hud"
    "$test_build/gpu_hud"
    ${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard \
        $gpu_defines -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) tools/bugtest/test_gpu_mesh.c platform/render/state.c platform/render/device_gl.c \
        $gpu_objects $(pkg-config --libs sdl3) $gl_libs -lm -o "$test_build/gpu_mesh"
    "$test_build/gpu_mesh"
    ${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard \
        $gpu_defines -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) tools/bugtest/test_gpu_downsample.c platform/render/state.c platform/render/device_gl.c \
        $gpu_objects $(pkg-config --libs sdl3) $gl_libs -lm -o "$test_build/gpu_downsample"
    "$test_build/gpu_downsample"
    ${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard \
        $gpu_defines -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) tools/bugtest/test_gpu_passes.c platform/render/state.c platform/render/device_gl.c \
        $gpu_objects $(pkg-config --libs sdl3) $gl_libs -lm -o "$test_build/gpu_passes"
    "$test_build/gpu_passes"
    # WebGL's float metadata must reproduce the desktop flat interface exactly.
    # Exercise it on a real GL driver even when the other cases selected Metal.
    env -u WIPEOUT_TEST_METAL "$test_build/gpu_passes" "$test_build/flat.bin"
    ${CC:-cc} -O1 -DPORT_TEST_WEBGL_METADATA -DPORT_TEST_WEBGL_UPLOADS -ffunction-sections -fdata-sections $discard \
        $gpu_defines -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) tools/bugtest/test_gpu_passes.c platform/render/state.c platform/render/device_gl.c \
        $gpu_objects $(pkg-config --libs sdl3) $gl_libs -lm -o "$test_build/gpu_metadata"
    env -u WIPEOUT_TEST_METAL "$test_build/gpu_metadata" "$test_build/float.bin"
    cmp "$test_build/flat.bin" "$test_build/float.bin"
    echo 'WebGL float metadata and direct indexed uploads preserve all colour/depth hashes: OK'
    env -u WIPEOUT_TEST_METAL WIPEOUT_DECODED_CACHE=0 "$test_build/gpu_metadata" "$test_build/raw.bin"
    cmp "$test_build/flat.bin" "$test_build/raw.bin"
    echo 'WebGL raw VRAM sampler preserves all cached colour/depth hashes: OK'
fi

${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations \
    -include platform/include/port.h -Iplatform -Iplatform/psx -iquote game/include $(pkg-config --cflags sdl3) \
    tools/bugtest/test_far_lifetime.c platform/psx/gte.c platform/psx/gte_tables.c \
    game/src/skeleton.c platform/psx/libgte.c game/src/utils.c $(pkg-config --libs sdl3) \
    -lm -o "$test_build/far_lifetime"
"$test_build/far_lifetime"

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
${CC:-cc} -O1 -Iplatform -Iplatform/psx $(pkg-config --cflags sdl3) \
    tools/bugtest/test_updates_web.c $(pkg-config --libs sdl3) -o "$test_build/updates_web"

"$test_build/updates_web"

# Exercise the real alert and SDL controller navigation without a GPU or network.
imgui=platform/third_party/imgui
${CXX:-c++} -std=c++17 -O1 -ffunction-sections -fdata-sections $discard -DPORT_VERSION='"0.0.7"' \
    -Iplatform -Iplatform/psx -I"$imgui" $(pkg-config --cflags sdl3) \
    tools/bugtest/test_update_navigation.cpp "$imgui/imgui.cpp" "$imgui/imgui_draw.cpp" \
    "$imgui/imgui_tables.cpp" "$imgui/imgui_widgets.cpp" "$imgui/backends/imgui_impl_sdl3.cpp" \
    $(pkg-config --libs sdl3) -o "$test_build/update_navigation"
"$test_build/update_navigation"

# Run track reaction on a steep guest descent with the production integer helpers.
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Wno-deprecated-declarations \
    -include platform/include/port.h -iquote game/include tools/bugtest/test_guest_descent.c \
    game/src/utils.c game/data/ships_data.c platform/psx/libgte.c \
    platform/psx/gte.c platform/psx/gte_tables.c -lm -o "$test_build/guest_descent"
"$test_build/guest_descent"

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

${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Iplatform $(pkg-config --cflags sdl3) \
    tools/bugtest/test_saturn_viewport.c $(pkg-config --libs sdl3) -o "$test_build/saturn_viewport"
"$test_build/saturn_viewport"
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Iplatform \
    -include platform/include/port.h -iquote game/include $(pkg-config --cflags sdl3) \
    tools/bugtest/test_saturn_font.c game/data/ships_data.c $(pkg-config --libs sdl3) -o "$test_build/saturn_font"
"$test_build/saturn_font"
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Iplatform -Iplatform/psx -iquote game/include \
    $(pkg-config --cflags sdl3) tools/bugtest/test_saturn_text.c platform/psx/gte.c platform/psx/gte_tables.c \
    $(pkg-config --libs sdl3) -lm -o "$test_build/saturn_text"
"$test_build/saturn_text"
${CC:-cc} -O1 -ffunction-sections -fdata-sections $discard -Iplatform -iquote game/include $(pkg-config --cflags sdl3) \
    tools/bugtest/test_saturn_art.c $(pkg-config --libs sdl3) -o "$test_build/saturn_art"
"$test_build/saturn_art"

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
