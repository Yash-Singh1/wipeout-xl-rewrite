#!/usr/bin/env python3
"""Write the number of loading-screen frames of each load in a console
recording (WIPEOUT_LOADS): the runs of CallbackSwap boundaries."""
import argparse
from pathlib import Path

from reader import events

CALLBACK_SWAP = 0x800686E8


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('console', type=Path, help='recording directory or its trace.bin(.gz)')
    p.add_argument('output', type=Path)
    args = p.parse_args()
    trace = args.console / 'trace.bin.gz' if args.console.is_dir() else args.console
    runs, current = [], 0
    for e in events(trace, kinds={'boundary'}):
        if e['words'][0] == CALLBACK_SWAP:
            current += 1
        elif current:
            runs.append(current)
            current = 0
    if current:
        runs.append(current)
    args.output.write_text(''.join(f'{n}\n' for n in runs))
    print(f'{len(runs)} loads: {runs}')


if __name__ == '__main__':
    main()
