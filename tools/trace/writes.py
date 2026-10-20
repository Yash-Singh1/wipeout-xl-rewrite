#!/usr/bin/env python3
"""Translate a recording's scripted writes into the port's (WIPEOUT_WRITES).

The recorder writes bytes at console addresses: a global's address, or an
offset into the player's ShipData or into ConfigData. The port's records have
the native layout, so each offset is mapped to the field the console had there
and from that to the field's native offset. Lines out: COUNT TARGET OFFSET
WIDTH VALUE, TARGET being ship, config or a global's name.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT.parent / 'wipeout-xl' / 'decomp'
TARGET32 = ['-target', 'armv7-unknown-linux-gnueabi', '-ffreestanding', '-fsigned-char',
            '-isystem', str(ROOT / 'tools' / 'gendata_stubs')]
RECORDS = {'ship': 'ShipData', 'config': 'ConfigData'}


def layout(record, extra):
    """{offset: field} of the record's top-level fields for one target."""
    with tempfile.NamedTemporaryFile('w', suffix='.c', delete=False) as f:
        f.write('#include "functions.h"\n#include "wipeout.h"\nint size = sizeof(%s);\n' % record)
    out = subprocess.run(['clang'] + extra + ['-w', '-fsigned-char', '-fsyntax-only',
                          '-include', str(ROOT / 'platform' / 'include' / 'port.h'),
                          '-I' + str(ROOT / 'game' / 'include'),
                          '-Xclang', '-fdump-record-layouts', f.name],
                         capture_output=True, text=True).stdout
    fields, inside = {}, False
    for line in out.splitlines():
        if re.match(r'\s+0 \| (struct|union) %s$' % record, line):
            inside = True
            continue
        if inside:
            if 'sizeof=' in line:
                break
            m = re.match(r'\s+(\d+) \|   (\S.*?) (\w+)$', line)   # top level only (3 spaces)
            if m:
                fields[int(m.group(1))] = m.group(3)
    return fields


def symbols():
    out = []
    for line in open(DECOMP / 'symbol_addrs.txt'):
        m = re.match(r'(\w+) = 0x([0-9A-Fa-f]+);', line)
        if m and 'type:func' not in line:
            out.append((int(m.group(2), 16), m.group(1)))
    return sorted(out)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('recording', type=Path)
    p.add_argument('output', type=Path)
    args = p.parse_args()
    writes = json.load(open(args.recording / 'scenario.json')).get('writes', [])
    console = {k: layout(r, TARGET32) for k, r in RECORDS.items()}
    native = {k: {v: o for o, v in layout(r, []).items()} for k, r in RECORDS.items()}
    table = symbols()
    lines = []
    for w in writes:
        offset = int(str(w['offset']), 0)
        if w['target'] in RECORDS:
            field = console[w['target']].get(offset)
            if field is None:
                raise SystemExit(f'{w}: no field of {RECORDS[w["target"]]} starts at {offset:#x}')
            lines.append(f'{w["input"]} {w["target"]} {native[w["target"]][field]} {w["width"]} {w["value"]}')
        elif w['target'] == 'absolute':
            address, name = max((a, n) for a, n in table if a <= offset)
            if address != offset:
                raise SystemExit(f'{w}: {offset:#x} is inside {name}; only whole globals are supported')
            lines.append(f'{w["input"]} {name} 0 {w["width"]} {w["value"]}')
        else:
            raise SystemExit(f'{w}: target {w["target"]} is not supported')
    args.output.write_text(''.join(l + '\n' for l in lines))
    print(f'{len(lines)} writes')


if __name__ == '__main__':
    main()
