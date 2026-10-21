#!/bin/sh
# Plays back a session recorded with --record (or the debug console's "Record every session"):
#
#   tools/replay.sh REPLAY_DIR [OUTPUT_DIR] [FROM_FRAME] [EVERY]
#
# The game runs hidden, muted and in fixed-step time with the recorded buttons, scanline counter reads
# (frame timing and random seeds) and loading lengths, from a copy of the recorded memory card, and saves
# every EVERY-th shown frame from FROM_FRAME on (default: all) to OUTPUT_DIR (default /tmp/wipeoutxl-replay).
# Environment: WIPEOUT_BIN (default build/wipeoutxl), WIPEOUT_XL_DISC (the PS1 disc .cue), WIPEOUT_WINDOW
# (default 1280x960), and the settings' variables (WIPEOUT_FAR_VIEW, WIPEOUT_FADE, WIPEOUT_DEPTH) as the
# recorded wipeoutxl.ini had them unless set.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
replay=$(cd "$1" && pwd)
out=${2:-/tmp/wipeoutxl-replay}
from=${3:-0}
every=${4:-1}
bin=${WIPEOUT_BIN:-$root/build/wipeoutxl}
: "${WIPEOUT_XL_DISC:?set WIPEOUT_XL_DISC to the PS1 disc image (.cue)}"

mkdir -p "$out/frames"
rm -rf "$out/save"
mkdir -p "$out/save"
[ -f "$replay/memcard0.mcr" ] && cp "$replay/memcard0.mcr" "$out/save/"

# the recorded settings that change the picture
setting() { [ -f "$replay/wipeoutxl.ini" ] && sed -n "s/^$1 *= *//p" "$replay/wipeoutxl.ini" | head -1; }
far=$(setting far_view); fade=$(setting fade); near=$(setting fade_near); farfade=$(setting fade_far); depth=$(setting depth_test)
: "${WIPEOUT_FAR_VIEW:=${far:-1}}"
if [ -z "${WIPEOUT_FADE:-}" ]; then
    if [ "${fade:-1}" = 0 ]; then WIPEOUT_FADE=0; else WIPEOUT_FADE=${near:-64000},${farfade:-96000}; fi
fi
: "${WIPEOUT_DEPTH:=${depth:-0}}"

WIPEOUT_HIDDEN=1 WIPEOUT_MUTE=1 WIPEOUT_FIXED_STEP=1 WIPEOUT_NO_GAMEPAD=1 \
WIPEOUT_FAR_VIEW=$WIPEOUT_FAR_VIEW WIPEOUT_FADE=$WIPEOUT_FADE WIPEOUT_DEPTH=$WIPEOUT_DEPTH \
WIPEOUT_SAVE_DIR="$out/save/" \
WIPEOUT_PADS="$replay/pads.txt" \
WIPEOUT_VCOUNTS="$replay/vcounts.txt" \
WIPEOUT_LOADS="$replay/loads.txt" \
WIPEOUT_CAPTURE_DIR="$out/frames" WIPEOUT_CAPTURE_EVERY="$every" WIPEOUT_CAPTURE_FROM="$from" \
    "$bin" --disc "$WIPEOUT_XL_DISC" --window "${WIPEOUT_WINDOW:-1280x960}" > "$out/run.log" 2>&1 || true
echo "$(ls "$out/frames" | wc -l | tr -d ' ') frames in $out/frames"
