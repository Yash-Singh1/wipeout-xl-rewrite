#!/usr/bin/env python3
"""Times the guest tracks' checkpoints, for platform/original.c's GateSeconds tables.

    WIPEOUT_XL_DISC=XL.cue WIPEOUT_ORIGINAL=WIPEOUT.7z WIPEOUT_64=WIPEOUT64.z64 \\
        tools/checkpoint_times.py [--ratio 1.5] [--tracks 0-20] [--game build/mac/wipeoutxl]

Races each guest (the class menu's WIPEOUT line, in its order: the original's tracks the copy has, then Wipeout
64's) in an arcade race, hidden, the player's ship on XL's autopilot (WIPEOUT_AUTOPILOT) and the clock kept from
running out (WIPEOUT_CHECKPOINT_LOG), for three laps. For each it prints the slowest time from each gate to the
next on the second and third laps, and the seconds to give each gate: that time --ratio times, rounded up.

The ratio: XL's own tracks give their gates about half as long again as the autopilot takes (each lap's gates
add 1.46 to 1.71 times the autopilot's lap at Venom and Rapier), so 1.5 makes a guest about as generous as
them. A run takes some 20 seconds.
"""
import argparse
import math
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def keys(track, path):
    """The pad presses from the title screen to an arcade race on the guest `track`."""
    events = ["100 pad attach ps5"]

    def press(frame, button):
        events.extend(["%d pad down %s" % (frame, button), "%d pad up %s" % (frame + 4, button)])

    press(1500, "start")
    frame = 1900
    for button in ["dpup", "dpright", "dpright", "a"]:  # to CLASS AND TRACK
        press(frame, button)
        frame += 60
    frame += 200
    for _ in range(4):  # down to the WIPEOUT line
        press(frame, "dpdown")
        frame += 40
    for _ in range(track):
        press(frame, "dpright")
        frame += 30
    frame += 100
    press(frame, "a")
    frame += 300
    press(frame, "dpdown")  # START
    frame += 60
    for _ in range(4):
        press(frame, "a")
        frame += 150
    with open(path, "w") as f:
        f.write("\n".join(sorted(events, key=lambda line: int(line.split()[0]))) + "\n")


def race(game, track, work):
    key_path = os.path.join(work, "keys.txt")
    log_path = os.path.join(work, "gates%d.txt" % track)
    keys(track, key_path)
    env = dict(os.environ, WIPEOUT_AUTOPILOT="all", WIPEOUT_CHECKPOINT_LOG=log_path, WIPEOUT_FIXED_STEP="1",
               WIPEOUT_SKIP_LOGOS="1", WIPEOUT_HIDDEN="1", WIPEOUT_MUTE="1", WIPEOUT_NO_GAMEPAD="1",
               WIPEOUT_CD_SPEED="8", WIPEOUT_FRAMES="60000", WIPEOUT_SAVE_DIR=os.path.join(work, "saves") + "/",
               WIPEOUT_KEYS=key_path)
    os.makedirs(os.path.join(work, "saves"), exist_ok=True)
    subprocess.run([game, "--window", "320x240"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                   timeout=900)
    passes = []
    if os.path.exists(log_path):
        for line in open(log_path):
            words = line.split()
            if words[0] == "gate":
                passes.append((int(words[1]), int(words[3]), int(words[5])))
    return passes


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--ratio", type=float, default=1.5, help="the seconds given for each second the autopilot takes")
    parser.add_argument("--tracks", default="0-20", help="the guests to time (their places on the WIPEOUT line)")
    parser.add_argument("--game", default=os.path.join(ROOT, "build", "mac", "wipeoutxl"))
    args = parser.parse_args()
    first, _, last = args.tracks.partition("-")
    with tempfile.TemporaryDirectory() as work:
        for track in range(int(first), int(last or first) + 1):
            passes = [(gate, frame) for gate, frame, lap in race(args.game, track, work) if lap >= 2]
            slowest = {}
            for (gate, frame), (_, after) in zip(passes, passes[1:]):
                slowest[gate] = max(slowest.get(gate, 0.0), (after - frame) / 30.0)
            if sorted(slowest) != [0, 1, 2]:
                print("guest %2d: a gate was never passed (%s)" % (track, passes))
                continue
            seconds = [math.ceil(args.ratio * slowest[gate]) for gate in range(3)]
            print("guest %2d: %s s  {{%d, %d, %d}}" % (track, " ".join("%4.1f" % slowest[g] for g in range(3)),
                                                      *seconds))
            sys.stdout.flush()


if __name__ == "__main__":
    main()
