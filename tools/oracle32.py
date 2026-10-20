#!/usr/bin/env python3
"""32-bit oracle: do the port edits change what the game computes on the PlayStation's own ABI?

The port rewrites declarations and casts so the game is correct on a 64-bit host.  Every such
edit must leave the program unchanged where `int`, `long` and pointers are all 32 bits.  This
tool compiles every game source for MIPS o32 (little endian, the PlayStation's ABI) with clang
before and after, and compares the disassembly function by function.  A function whose code
changed is listed with a diff; the edit is then reviewed (the usual reasons for a change:
a prototype now converts an argument, a type was widened/narrowed on purpose).

  tools/oracle32.py snapshot REF [NAME]   compile the tree at git REF (default HEAD) -> /tmp/oracle32/NAME
  tools/oracle32.py diff NAME [-v] [FILES]  compile the working tree and compare with snapshot NAME

Requires Homebrew LLVM (clang with the MIPS backend and llvm-objdump).
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
import difflib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LLVM = Path('/opt/homebrew/opt/llvm/bin')
OUT = Path('/tmp/oracle32')
NOSP = bool(os.environ.get('ORACLE_NOSP'))   # ORACLE_NOSP=1: ignore stack slot numbers (locals were retyped)
OPT = os.environ.get('ORACLE_OPT', '-O2')   # ORACLE_OPT=-O0 compares unoptimised code (no scheduling noise)
FLAGS = ['-target', 'mipsel-unknown-none-elf', '-march=mips1', '-mabi=o32', OPT, '-fno-pic',
         '-mno-abicalls', '-msoft-float', '-ffreestanding', '-nostdinc', '-fsigned-char',
         '-fno-strict-aliasing', '-fwrapv', '-std=gnu11', '-w', '-Wno-error',
         # the sources are compiled as the PlayStation compiler accepted them
         '-Wno-implicit-function-declaration', '-Wno-int-conversion', '-Wno-incompatible-pointer-types',
         '-Wno-return-mismatch', '-Wno-incompatible-function-pointer-types', '-Wno-implicit-int']


def compile_tree(src_root, out_dir, only=None):
    out_dir.mkdir(parents=True, exist_ok=True)
    files = sorted((src_root / 'game/src').glob('*.c'))
    if only:
        files = [f for f in files if f.name in only]
    # the port header is for the host; the oracle uses a 32-bit libc prototype stub instead
    def one(f):
        obj = out_dir / (f.stem + '.o')
        cmd = [str(LLVM / 'clang')] + FLAGS + ['-include', str(ROOT / 'tools/oracle32/o32.h'),
                                               '-isystem', str(ROOT / 'tools/oracle32/include'), '-I' + str(src_root / 'game/include'), '-c', str(f), '-o', str(obj)]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            print('COMPILE FAIL', f.name, r.stderr[:2000])
            return f.stem, None
        dis = subprocess.run([str(LLVM / 'llvm-objdump'), '-dr', '--no-show-raw-insn', str(obj)],
                             capture_output=True, text=True).stdout
        (out_dir / (f.stem + '.dis')).write_text(dis)
        return f.stem, split_functions(dis)
    with ThreadPoolExecutor(max_workers=2) as ex:
        return dict(ex.map(one, files))


def split_functions(dis):
    """{function: [normalised instruction lines]}.  Addresses are made function-relative."""
    funcs = {}
    cur = None
    start = 0
    for line in dis.split('\n'):
        m = re.match(r'^([0-9a-f]+) <(\S+)>:$', line)
        if m:
            cur = m.group(2)
            start = int(m.group(1), 16)
            funcs[cur] = []
            continue
        if cur is None or not line.strip():
            continue
        if line.startswith('Disassembly of section'):
            cur = None
            continue
        line = re.sub(r'^\s*[0-9a-f]+:\s*', '', line)       # drop the address
        line = re.sub(r'<[^>]*>', '', line)                  # symbol annotations of immediates
        if re.match(r'\s*jal\s', line):                    # calls: the relocation line names the callee
            line = re.sub(r'0x[0-9a-f]+\s*$', '@call', line)
        elif re.match(r'\s*(j|b\w*)\s', line):            # branch targets: relative to the function
            line = re.sub(r'0x([0-9a-f]+)\s*$', lambda mm: '@%x' % (int(mm.group(1), 16) - start), line)
        line = re.sub(r'\s+', ' ', line.strip())
        if NOSP:                                            # ORACLE_NOSP=1: ignore stack slot numbers and the frame size
            line = re.sub(r'-?0x[0-9a-f]+\(\$(sp|fp)\)', r'SLOT($\1)', line)
            line = re.sub(r'(^|\s)@[0-9a-f]+$', r'\1@', line)
            line = re.sub(r'addiu \$sp, \$sp, -?0x[0-9a-f]+', 'addiu $sp, $sp, FRAME', line)
            line = re.sub(r'addiu (\$\w+), \$sp, -?0x[0-9a-f]+', r'addiu \1, $sp, SLOT', line)
        funcs[cur].append(line)
    return funcs


def reloc_lines(dis):
    return dis


def snapshot(ref, name):
    d = Path(tempfile.mkdtemp(prefix='oracle32-src-'))
    arch = subprocess.run(['git', '-C', str(ROOT), 'archive', ref, 'game', 'tools'], capture_output=True).stdout
    subprocess.run(['tar', '-x', '-C', str(d)], input=arch)
    out = OUT / name
    funcs = compile_tree(d, out)
    subprocess.run(['rm', '-rf', str(d)])
    print('snapshot %s: %d files' % (name, len(funcs)))


def diff(name, verbose, only):
    base_dir = OUT / name
    cur_dir = OUT / ('cur-' + name)
    cur = compile_tree(ROOT, cur_dir, only)
    changed = 0
    for stem, funcs in sorted(cur.items()):
        base_dis = base_dir / (stem + '.dis')
        if funcs is None or not base_dis.exists():
            print('%s: no baseline or compile failure' % stem)
            continue
        base = split_functions(base_dis.read_text())
        for fn in sorted(set(funcs) | set(base)):
            a, b = base.get(fn), funcs.get(fn)
            if a == b:
                continue
            changed += 1
            if a is None:
                print('%s: %s  NEW' % (stem, fn))
            elif b is None:
                print('%s: %s  REMOVED' % (stem, fn))
            else:
                print('%s: %s  CHANGED (%d -> %d lines)' % (stem, fn, len(a), len(b)))
                if verbose:
                    for l in difflib.unified_diff(a, b, lineterm='', n=1):
                        print('    ' + l)
    print('%d function(s) differ from %s' % (changed, name))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('snapshot')
    s.add_argument('ref', nargs='?', default='HEAD')
    s.add_argument('name', nargs='?')
    d = sub.add_parser('diff')
    d.add_argument('name')
    d.add_argument('-v', action='store_true')
    d.add_argument('files', nargs='*')
    args = ap.parse_args()
    if args.cmd == 'snapshot':
        snapshot(args.ref, args.name or args.ref)
    else:
        diff(args.name, args.v, set(args.files))


if __name__ == '__main__':
    main()
