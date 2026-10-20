#!/bin/sh
# Compile one game source with the host compiler, syntax only, with the diagnostics the port cares about.
# usage: tools/cc.sh game/src/FILE.c [more clang flags]
cd "$(dirname "$0")/.."
exec clang -std=gnu11 -fsigned-char -fno-strict-aliasing -fwrapv -fno-common -include platform/include/port.h -Igame/include \
    -fsyntax-only -Wno-pointer-sign -Wno-deprecated-declarations -Wpointer-to-int-cast -Wint-to-pointer-cast -Wshorten-64-to-32 "$@"
