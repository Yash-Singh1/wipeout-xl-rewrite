#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
node tools/webtest/disc_storage.cjs
node tools/webtest/saves.cjs
node tools/webtest/phone_chunks.cjs
node tools/webtest/music_stream.cjs
node tools/webtest/viewport.cjs
node tools/webtest/fullscreen.cjs
node tools/webtest/audio.cjs
node tools/webtest/audio_output.cjs
node tools/webtest/presentation.cjs
node tools/webtest/phone_diagnostics.cjs
python3 tools/webtest/test_phone_server.py
test_build=$(mktemp -d)
trap 'rm -rf "$test_build"' EXIT HUP INT TERM
node - "$test_build/disc.js" <<'JS'
const fs = require('node:fs'), html = fs.readFileSync('platform/web/shell.html', 'utf8');
fs.writeFileSync(process.argv[2], fs.readFileSync('platform/web/phone-loader.js', 'utf8') +
  html.slice(html.indexOf('function audioOnlyFiles'), html.indexOf('// A track: its size')));
JS
emcc -O2 -Iplatform/psx tools/webtest/test_disc.c platform/psx/disc.c platform/psx/cdrom.c platform/psx/spu.c \
    --pre-js "$test_build/disc.js" -sASYNCIFY -sALLOW_MEMORY_GROWTH -sENVIRONMENT=node -sEXPORTED_FUNCTIONS=_main,_malloc \
    -o "$test_build/disc.cjs"
node "$test_build/disc.cjs"
emcc -O2 --use-port=sdl3 -Iplatform/psx tools/webtest/test_audio_output.c platform/psx/audio_sdl.c \
    --pre-js platform/web/audio-output.js --pre-js tools/webtest/audio_fixture.js \
    -sALLOW_MEMORY_GROWTH -sENVIRONMENT=node -o "$test_build/audio.cjs"
node "$test_build/audio.cjs"
emcc -O3 -fno-inline-functions --use-port=sdl3 -Iplatform -Iplatform/psx -iquote game/include \
    tools/bugtest/test_glyph_strokes.c platform/keyglyphs.c \
    -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1 -o "$test_build/glyph_strokes.cjs"
node "$test_build/glyph_strokes.cjs"
