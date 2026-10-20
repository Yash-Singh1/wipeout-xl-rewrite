#!/bin/sh
# usage: tools/asmtest/build.sh OUT   -- builds the differential render test
cd "$(dirname "$0")/../.."
OUT=${1:-/tmp/render_test}
clang -std=gnu11 -O1 -g -fno-strict-aliasing -fwrapv -fsigned-char -fsanitize=address,undefined -fno-sanitize=shift-base \
    -include platform/include/port.h -Igame/include -Iplatform/psx -w \
    tools/asmtest/render_test.c tools/asmtest/mips.c \
    game/src/trackdraw.c game/src/skeleton.c game/src/malloc.c game/src/port_widen.c game/src/port_records.c \
    platform/psx/gte.c platform/psx/gte_tables.c platform/psx/libgte.c platform/psx/libgte_more.c \
    -o "$OUT"
