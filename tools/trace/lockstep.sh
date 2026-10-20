#!/bin/sh
# Runs the port against a console recording and compares the drawing.
#
#   tools/trace/lockstep.sh RECORDING_DIR [OUTPUT_DIR]
#
# The port runs hidden, muted and in fixed-step time, with the recording's
# pad script, scripted writes, loading-screen lengths and scanline-counter reads, and writes its trace to OUTPUT_DIR
# (default /tmp/wipeoutxl-lockstep/NAME). Environment: WIPEOUT_BIN (the
# executable, default build/wipeoutxl), WIPEOUT_XL_DISC (the PS1 disc .cue),
# WIPEOUT_XL_PC_DATA, LOCKSTEP_FRAMES (frames to compare).
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
recording=$(cd "$1" && pwd)
name=$(basename "$recording")
out=${2:-/tmp/wipeoutxl-lockstep/$name}
bin=${WIPEOUT_BIN:-$root/build/wipeoutxl}
: "${WIPEOUT_XL_DISC:?set WIPEOUT_XL_DISC to the PS1 disc image (.cue)}"

mkdir -p "$out"
fields=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["frames"])' "$recording/scenario.json")
[ -s "$out/vcounts.txt" ] || python3 "$here/vcounts.py" "$recording" "$out/vcounts.txt"
python3 "$here/writes.py" "$recording" "$out/writes.txt" >/dev/null
[ -s "$out/loads.txt" ] || python3 "$here/loads.py" "$recording" "$out/loads.txt" >/dev/null

WIPEOUT_HIDDEN=1 WIPEOUT_MUTE=1 WIPEOUT_FIXED_STEP=1 \
WIPEOUT_FRAMES="$fields" \
WIPEOUT_PADS="$recording/pads.txt" \
WIPEOUT_VCOUNTS="$out/vcounts.txt" \
WIPEOUT_WRITES="$out/writes.txt" \
WIPEOUT_LOADS="$out/loads.txt" \
WIPEOUT_TRACE="$out/trace.bin.gz" \
    "$bin" > "$out/run.log" 2>&1 || echo "port exited with status $? (see $out/run.log)"

python3 "$here/lockstep.py" "$recording" "$out/trace.bin.gz" ${LOCKSTEP_FRAMES:+--frames "$LOCKSTEP_FRAMES"}
