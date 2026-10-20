#!/usr/bin/env python3
"""Compare the port's drawing with a console recording, frame by frame.

Both traces are WXLTRC01 files: the console one from the recorder, the port's
from WIPEOUT_TRACE. The GPU command streams are decoded into packets and cut
into frames at each drawing-environment change (PutDrawEnv's area packet).
Frames are compared packet by packet, so the report names the first frame
and the first packet in it where the port draws something else.

Iterations the console spent in the movie player (boundaries that are not
Swap or CallbackSwap) are left out: the port plays the movies itself.
"""
import argparse
import hashlib
import json
from pathlib import Path

from reader import events
from gp0 import Decoder

GAME_MARKERS = {0x800684C4, 0x800686E8}
META = {'iteration', 'frame', 'tick', 'address', 'order', 'words'}


def movie_iterations(path):
    """Iterations closed by a boundary that is not the game's own."""
    return {e['iteration'] for e in events(path, kinds={'boundary'})
            if e['words'][0] not in GAME_MARKERS}


def frames(path, skip=frozenset()):
    """Yield (iteration, packets) per frame; packets are decoded values."""
    decoder = Decoder()
    current, start = [], None
    for e in events(path, kinds={'gp0', 'gp1'}):
        if e['iteration'] in skip:
            continue
        if e['kind'] == 'gp1':
            if e['words'][0] >> 24 == 0:
                decoder.buffer, decoder.context = [], []
                decoder.mode, decoder.offset, decoder.window = 0, [0, 0], 0
            continue
        for p in decoder.feed(e['words'], e):
            if p['type'] == 'draw_area_min' and current:
                yield start, current
                current = []
            if not current:
                start = p['iteration']
            current.append({k: v for k, v in p.items() if k not in META})
    if current:
        yield start, current


def digest(packets):
    h = hashlib.sha256()
    for p in packets:
        h.update(json.dumps(p, sort_keys=True).encode())
    return h.hexdigest()


def first_difference(a, b):
    for i in range(max(len(a), len(b))):
        x = a[i] if i < len(a) else None
        y = b[i] if i < len(b) else None
        if x != y:
            return i, x, y
    return None


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('console', type=Path, help='recording directory or its trace.bin(.gz)')
    p.add_argument('port', type=Path, help="the port's trace")
    p.add_argument('--frames', type=int, help='compare at most this many frames')
    p.add_argument('--show', type=int, default=8, help='mismatched frames to list')
    p.add_argument('--context', type=int, default=3, help='packets shown before the first difference')
    args = p.parse_args()

    console = args.console / 'trace.bin.gz' if args.console.is_dir() else args.console
    left = frames(console, movie_iterations(console))
    right = frames(args.port, movie_iterations(args.port))
    count = same = 0
    mismatched = []
    first = None
    while args.frames is None or count < args.frames:
        a, b = next(left, None), next(right, None)
        if a is None and b is None:
            break
        count += 1
        if a is not None and b is not None and digest(a[1]) == digest(b[1]):
            same += 1
            continue
        mismatched.append((count - 1, a[0] if a else None, b[0] if b else None,
                           len(a[1]) if a else 0, len(b[1]) if b else 0))
        if first is None:
            first = count - 1, a, b
    print(f'frames compared: {count}, identical: {same}, different: {count - same}')
    for index, ci, pi, cn, pn in mismatched[:args.show]:
        print(f'  frame {index}: console iteration {ci} ({cn} packets), port iteration {pi} ({pn} packets)')
    if first:
        index, a, b = first
        print(f'\nfirst different frame: {index}')
        if a is None or b is None:
            print('  only the', 'port' if a is None else 'console', 'has this frame')
        else:
            i, x, y = first_difference(a[1], b[1])
            print(f'  first different packet: {i} of {len(a[1])} (console) / {len(b[1])} (port)')
            for j in range(max(0, i - args.context), i):
                print(f'  same    {json.dumps(a[1][j], sort_keys=True)}')
            print(f'  console {json.dumps(x, sort_keys=True)}')
            print(f'  port    {json.dumps(y, sort_keys=True)}')
    return 0 if first is None else 1


if __name__ == '__main__':
    raise SystemExit(main())
