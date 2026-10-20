#!/usr/bin/env python3
"""List exactly which symbols the game still needs, and generate weak stubs so that it links.

The game library calls the PsyQ libraries (platform/), uses data that was in the executable's
data sections (game/data) and a few routines that were hand-written assembly.  This tool takes the
object files that CMake built (the `game` library, plus the platform and data libraries once they
exist), computes the undefined symbols and writes:

  tools/linkstub.c       weak definitions of every undefined symbol (a function that reports its name
                         and aborts; data as zeroed storage of the declared size) so that
                         `cmake --build build --target linkcheck` links an executable
  docs/LINKSTUB.md       the list, with the declaration the game uses for each symbol

Symbols that the other sources define are not stubbed, so the list shrinks as the platform and data
workers deliver.  Usage:  tools/linkstub.py [--build build] [--source .]
"""
import argparse
import platform
import re
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import xdecl  # noqa: E402


def object_files(build):
    objs = []
    for p in sorted(build.rglob('*.o')) + sorted(build.rglob('*.obj')):
        if 'linkstub' in p.name or 'CMakeFiles/CMake' in str(p) or 'CompilerId' in str(p):
            continue
        objs.append(p)
    return objs


def nm(objs):
    """(defined, undefined) symbol name sets."""
    strip = platform.system() == 'Darwin'
    defined, undefined = set(), set()
    for o in objs:
        out = subprocess.run(['nm', '-g', str(o)], capture_output=True, text=True).stdout
        for line in out.split('\n'):
            m = re.match(r'^\s*(?:[0-9a-fA-F]+\s+)?([A-Za-z])\s+(\S+)$', line)
            if not m:
                continue
            kind, name = m.groups()
            if strip and name.startswith('_'):
                name = name[1:]
            if kind == 'U':
                undefined.add(name)
            elif kind in 'TtDdBbRrSsCcAa' or kind == 'W' or kind == 'V':
                defined.add(name)
    return defined, undefined


def declaration_text(decl):
    """The source text of a declaration (first statement starting at its line)."""
    f = decl['file']
    try:
        lines = Path(f).read_text(encoding='latin-1').split('\n')
    except OSError:
        return ''
    i = decl['line'] - 1
    text = []
    while i < len(lines):
        text.append(lines[i].strip())
        if re.search(r'[;{]\s*$', lines[i]) or ';' in lines[i]:
            break
        i += 1
        if len(text) > 6:
            break
    t = ' '.join(text)
    t = re.sub(r'\s*\{.*$', '', t)
    return t


def sizeof_probe(tu, names):
    """sizeof() of each named variable as seen from translation unit tu, via the compiler."""
    src = ['#include "%s"' % (ROOT / 'game/src' / tu)]
    for i, n in enumerate(names):
        src.append('const unsigned long Q_%d = sizeof(%s);' % (i, n))
    with tempfile.NamedTemporaryFile('w', suffix='.c', delete=False, dir='/tmp') as f:
        f.write('\n'.join(src) + '\n')
        path = f.name
    cmd = ['clang', '-std=gnu11', '-fsigned-char', '-include', str(ROOT / 'platform/include/port.h'),
           '-I' + str(ROOT / 'game/include'), '-S', '-o', '-', '-O0', '-w', path]
    asm = subprocess.run(cmd, capture_output=True, text=True).stdout
    Path(path).unlink()
    sizes = {}
    for i, n in enumerate(names):
        m = re.search(r'^_?Q_%d:\s*\n\s*\.quad\s+(\d+)' % i, asm, re.M)
        if m:
            sizes[n] = int(m.group(1))
    return sizes


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--build', default='build')
    ap.add_argument('--source', default='.')
    ap.add_argument('--out', default=str(ROOT / 'tools/linkstub.c'))
    ap.add_argument('--report', default=str(ROOT / 'docs/LINKSTUB.md'))
    args = ap.parse_args()
    build = Path(args.build)
    if not build.is_absolute():
        build = ROOT / build
    objs = object_files(build)
    if not objs:
        sys.exit('no object files under %s: build the game first (cmake --build %s)' % (build, build))
    defined, undefined = nm(objs)
    missing = sorted(undefined - defined)
    # Standard C library and system symbols are not ours to provide.
    system = set()
    out = subprocess.run(['nm', '-g', '-U', '/usr/lib/libSystem.B.dylib'], capture_output=True, text=True).stdout \
        if platform.system() == 'Darwin' else ''
    decls = xdecl.collect(sorted((ROOT / 'game/src').glob('*.c')))
    libc = {'printf', 'sprintf', 'memcpy', 'memset', 'strcpy', 'strcat', 'strlen', 'strncmp', 'strcmp', 'abs', 'exit',
            'srand', 'rand', 'toupper', 'malloc', 'free', 'memmove', 'puts', 'putchar'}
    funcs, data = [], []
    for name in missing:
        if name in libc or name.startswith(('__', 'dyld_stub')):
            continue
        ds = decls.get(name, [])
        if any(d['kind'] == 'VarDecl' for d in ds):
            data.append((name, ds))
        else:
            funcs.append((name, ds))
    # Sizes of the data symbols.
    sizes = {}
    by_tu = defaultdict(list)
    for name, ds in data:
        for d in ds:
            if d['kind'] == 'VarDecl' and not d['type'].endswith('[]'):
                by_tu[d['tu']].append(name)
                break
    for tu, names in by_tu.items():
        sizes.update(sizeof_probe(tu, names))

    c = ['/* Generated by tools/linkstub.py: weak stand-ins for what the game needs and nothing else provides.',
         '   Do not edit; see docs/LINKSTUB.md for the list. */',
         '#include <stdio.h>', '#include <stdlib.h>', '',
         'static void LinkStubCalled(const char *name)', '{',
         '    fprintf(stderr, "linkstub: %s called but not provided\\n", name);', '    abort();', '}', '']
    for name, ds in funcs:
        c.append('__attribute__((weak)) void %s(void) { LinkStubCalled("%s"); }' % (name, name))
    for name, ds in data:
        c.append('__attribute__((weak, aligned(16))) char %s[%d];' % (name, max(sizes.get(name, 65536), 1)))
    Path(args.out).write_text('\n'.join(c) + '\n')

    r = ['# Symbols the game still needs', '',
         'Generated by `tools/linkstub.py` from the object files of the last build. %d functions, %d data symbols '
         'have no definition anywhere in the build.' % (len(funcs), len(data)), '',
         '## Functions', '', 'Declaration as the game uses it (from `game/include/psyq.h` or the source file).', '']
    for name, ds in funcs:
        d = next((x for x in ds if x['proto'] or True), None) if ds else None
        txt = declaration_text(d) if d else '(implicit declaration)'
        loc = '%s:%d' % (Path(d['file']).name, d['line']) if d else ''
        r.append('- `%s`  %s' % (txt or name, loc))
    r += ['', '## Data', '', 'Declared type and size the game assumes.', '']
    for name, ds in data:
        d = next(x for x in ds if x['kind'] == 'VarDecl')
        r.append('- `%s` %s (%s bytes) %s:%d' % (name, d['type'], sizes.get(name, '?'), Path(d['file']).name, d['line']))
    Path(args.report).write_text('\n'.join(r) + '\n')
    print('%d functions, %d data symbols undefined; wrote %s and %s' % (len(funcs), len(data), args.out, args.report))


if __name__ == '__main__':
    main()
