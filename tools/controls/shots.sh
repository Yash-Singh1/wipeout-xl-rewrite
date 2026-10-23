#!/bin/sh
# Screenshots of the button glyphs for each input device, played into the port as a player would:
#
#   tools/controls/shots.sh OUTPUT_DIR [keyboard xbox switch ps5 generic]
#
# For every device it runs the game hidden and muted three times (menus, a paused race, the results of
# a race) and leaves OUTPUT_DIR/DEVICE-SCREEN.png: title, main menu, controller screen, pause menu,
# the results ("bad luck", then replay or menu). Keys and the virtual gamepad go through WIPEOUT_KEYS
# (platform/controls.c), so they take the path real input does; the destroyed ship of the results
# comes from a scripted write (platform/trace.c). Environment: WIPEOUT_BIN (default build/mac/wipeoutxl, build/linux/ on Linux),
# WIPEOUT_XL_DISC (the PS1 disc .cue).
# The timings are video fields at a 8x disc speed; they hold for the retail disc.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out=${1:?output directory}
shift
devices=${*:-keyboard xbox switch ps5 generic}
case $(uname -s) in Darwin) native=mac ;; *) native=linux ;; esac
bin=${WIPEOUT_BIN:-$root/build/$native/wipeoutxl}
: "${WIPEOUT_XL_DISC:?set WIPEOUT_XL_DISC to the PS1 disc image (.cue)}"
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/controls-shots.XXXXXX")
trap 'rm -rf "$work"' EXIT

# the player's ship (ShipData.combatAttr, byte 184 of the 64-bit layout) is destroyed at the 1500th pad read:
# the race ends with the results screens
echo "1500 ship 184 2 32768" > "$work/writes.txt"

# press KEY (or button) at FIELD for five fields; the device decides the names
press() { # field name
    case $device in
    keyboard) echo "$1 down $2"; echo "$(($1 + 5)) up $2" ;;
    *) echo "$1 pad down $2"; echo "$(($1 + 5)) pad up $2" ;;
    esac
}
script() { # part
    case $device in keyboard) accept=Return down=Down pause=Escape start=Return wake=Space ;;
    *) accept=a down=dpdown pause=start start=start wake=b; echo "300 pad attach $device" ;;
    esac
    press 400 $wake # until a device is used the console's glyphs and words stay
    press 800 $start # the title waits from about field 650
    case $1 in
    menus) # the options menu, then the controller screen
        press 1050 $down; press 1150 $accept; press 1350 $down; press 1450 $accept ;;
    pause) press 1150 $accept; press 2750 $pause ;;
    results) press 1150 $accept; press 4450 $accept ;;
    esac
}
run() { # part frames every
    script "$1" > "$work/keys.txt"
    rm -rf "$work/frames"; mkdir -p "$work/frames"
    WIPEOUT_HIDDEN=1 WIPEOUT_MUTE=1 WIPEOUT_FIXED_STEP=1 WIPEOUT_FRAMES=$2 WIPEOUT_CD_SPEED=8 \
    WIPEOUT_NO_GAMEPAD=1 WIPEOUT_KEYS="$work/keys.txt" WIPEOUT_WRITES="$work/writes.txt" \
    WIPEOUT_CAPTURE_DIR="$work/frames" WIPEOUT_CAPTURE_EVERY=100 \
        "$bin" --disc "$WIPEOUT_XL_DISC" --window 640x480 > "$work/log.txt" 2>&1
}
shot() { # field name
    sips -s format png "$work/frames/frame_$(printf %06d "$1").ppm" --out "$out/$device-$2.png" > /dev/null
}

for device in $devices; do
    run menus 2100; shot 700 title; shot 1000 main-menu; shot 2100 controller
    run pause 3000; shot 3000 pause
    run results 4600; shot 4300 results-destroyed; shot 4600 results-menu
    echo "$device: $(ls "$out" | grep -c "^$device-") screenshots"
done
