#!/usr/bin/env python3
"""Compare the port's drawing with a console recording, frame by frame.

Both traces are WXLTRC01 files: the console one from the recorder, the port's
from WIPEOUT_TRACE. The GPU command streams are decoded into packets and cut
into frames at each drawing-environment change (PutDrawEnv's area packet).
Frames are compared packet by packet, so the report names the first frame
and the first packet in it where the port draws something else.

GPU traffic from the console's movie player (code loaded at 0x80119000) is
left out: the port plays the movies itself. Game drawing that
the console's DMA delivers after the player has been started still counts.
"""
import argparse
import hashlib
import json
from pathlib import Path

from reader import events
from gp0 import Decoder

MOVIE_LOW, MOVIE_HIGH = 0x80119000, 0x80140000
ROM_LOW = 0xBFC00000  # the boot ROM's own writes (around Exec), not the game's
META = {'iteration', 'frame', 'tick', 'address', 'order', 'words'}


def foreign(pc):
    return MOVIE_LOW <= pc < MOVIE_HIGH


def normalise(packet):
    """The packet's values the GPU uses. Raw-textured primitives ignore their
    colour, which the game often leaves uninitialised."""
    p = {k: v for k, v in packet.items() if k not in META}
    if p.get('raw_texture'):
        p.pop('colour', None)
        for v in p.get('vertices', ()):
            v.pop('colour', None)
    return p


def frames(path):
    """Yield (iteration, packets) per frame; packets are decoded values."""
    decoder = Decoder()
    current, start, started = [], None, False
    dma = False
    for e in events(path, kinds={'gp0', 'gp1', 'dma'}):
        if e['kind'] == 'dma':
            dma = True
            continue
        delivered, dma = dma, False
        if foreign(e['pc']) or (e['pc'] >= ROM_LOW and not delivered):
            continue
        if e['kind'] == 'gp1':
            if e['words'][0] >> 24 == 0:
                decoder.buffer, decoder.context = [], []
                decoder.mode, decoder.offset, decoder.window = 0, [0, 0], 0
            continue
        for p in decoder.feed(e['words'], e):
            if p['type'] == 'draw_area_min':
                if current:
                    yield start, current
                current = []
                started = True
            elif not started:
                continue  # the console's boot ROM draws before the game starts
            if not current:
                start = p['iteration']
            current.append(normalise(p))
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
    left, right = frames(console), frames(args.port)
    count = same = 0
    mismatched = []
    first = None
    while args.frames is None or count < args.frames:
        a, b = next(left, None), next(right, None)
        if a is None:
            if b is not None:
                print('(the port ran on past the end of the console recording)')
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
