#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
node tools/webtest/disc_storage.cjs
node tools/webtest/phone_chunks.cjs
node tools/webtest/viewport.cjs
python3 tools/webtest/test_phone_server.py
test_build=$(mktemp -d)
trap 'rm -rf "$test_build"' EXIT HUP INT TERM
node - "$test_build/disc.js" <<'JS'
const fs = require('node:fs'), html = fs.readFileSync('platform/web/shell.html', 'utf8');
fs.writeFileSync(process.argv[2], html.slice(html.indexOf('function audioOnlyFiles'), html.indexOf('// A track: its size')));
JS
emcc -O2 -Iplatform/psx tools/webtest/test_disc.c platform/psx/disc.c \
    --pre-js "$test_build/disc.js" -sENVIRONMENT=node -sEXPORTED_FUNCTIONS=_main,_malloc \
    -o "$test_build/disc.cjs"
node "$test_build/disc.cjs"
