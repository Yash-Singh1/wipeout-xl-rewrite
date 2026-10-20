#!/usr/bin/env python3
"""Write the scanline-counter values the game read in a console recording.

The port replays them (WIPEOUT_VCOUNTS) so a lockstep run takes the console's
slow frames and seeds its random numbers from the same counts.
"""
import argparse
from pathlib import Path

from reader import events

GS_GET_VCOUNT = 0x80082090
GAME_LOW, GAME_HIGH = 0x80010000, 0x80119000


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('console', type=Path, help='recording directory or its trace.bin(.gz)')
    p.add_argument('output', type=Path)
    args = p.parse_args()
    trace = args.console / 'trace.bin.gz' if args.console.is_dir() else args.console
    count = 0
    with open(args.output, 'w') as out:
        for e in events(trace, kinds={'return'}):
            function, caller, value = e['words'][0], e['words'][1], e['words'][6]
            if function == GS_GET_VCOUNT and GAME_LOW <= caller < GAME_HIGH:
                out.write(f'{value}\n')
                count += 1
    print(f'{count} counter reads')


if __name__ == '__main__':
    main()
