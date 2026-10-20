#!/usr/bin/env python3
"""Record layouts of every game type: the PlayStation's (MIPS o32, 32-bit pointers) against the host's.

The PlayStation compiler laid structs out with 4-byte pointers.  Natively pointers are 8 bytes, so
every struct that holds one changes size and the offset of every later field.  This tool asks clang
for the layout of each named struct/union/typedef of the game, once per target, and reports where
they differ.  Used to audit the code (which structs grew), and as the source of the file-record tables.

  tools/layouts.py audit            list the types whose size or field offsets differ
  tools/layouts.py show TYPE...     print both layouts of the named types (typedef names or `struct X`)
  tools/layouts.py dump FILE.json   write all layouts as JSON
Needs Homebrew LLVM (MIPS backend).
"""
import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLANG = '/opt/homebrew/opt/llvm/bin/clang'
COMMON = ['-std=gnu11', '-fsigned-char', '-w', '-fsyntax-only', '-ferror-limit=0',
          '-Wno-error', '-I' + str(ROOT / 'game/include')]
TARGETS = {
    'ps1': ['-target', 'mipsel-unknown-none-elf', '-march=mips1', '-mabi=o32', '-ffreestanding', '-nostdinc',
            '-include', str(ROOT / 'tools/oracle32/o32.h')],
    'host': ['-include', str(ROOT / 'platform/include/port.h')],
}


def type_names(source):
    """Names of the record types a source file (with its headers) defines."""
    cmd = ['clang', '-std=gnu11', '-fsigned-char', '-w', '-fsyntax-only', '-Xclang', '-ast-dump=json',
           '-include', str(ROOT / 'platform/include/port.h'), '-I' + str(ROOT / 'game/include'), str(source)]
    data = json.loads(subprocess.run(cmd, capture_output=True, text=True).stdout)
    names = []
    state = {'file': ''}
    for node in data['inner']:
        loc = node.get('loc', {})
        if 'file' in loc:
            state['file'] = loc['file']
        rng = node.get('range', {}).get('begin', {})
        if 'file' in rng:
            state['file'] = rng['file']
        f = state['file']
        if f.startswith(('/Applications', '/usr', '/opt', '/Library')):
            continue
        kind = node.get('kind')
        if kind == 'RecordDecl' and node.get('completeDefinition') and node.get('name'):
            names.append('%s %s' % (node['tagUsed'], node['name']))
        elif kind == 'TypedefDecl':
            q = node['type'].get('desugaredQualType') or node['type']['qualType']
            if re.match(r'^(struct|union) ', q) or 'unnamed' in q:
                names.append(node['name'])
    return names


def probe(source, names, target):
    lines = ['#include "%s"' % source]
    for i, n in enumerate(names):
        lines.append('struct ProbeWrap_%d { %s v; };' % (i, n))
    with tempfile.NamedTemporaryFile('w', suffix='.c', delete=False, dir='/tmp') as f:
        f.write('\n'.join(lines) + '\n')
        path = f.name
    cmd = [CLANG] + TARGETS[target] + COMMON + ['-Xclang', '-fdump-record-layouts-complete', path]
    out = subprocess.run(cmd, capture_output=True, text=True).stdout
    Path(path).unlink()
    return parse_dump(out)


def parse_dump(text):
    """{ProbeWrap_N: node}; node = dict(offset, text, children, sizeof, align)."""
    records = {}
    stack = None
    for raw in text.split('\n'):
        if raw.startswith('*** Dumping'):
            stack = None
            continue
        m = re.match(r'^\s*\| \[sizeof=(\d+)(?:, dsize=\d+)?, align=(\d+)', raw)
        if m:
            if stack:
                stack[0]['sizeof'] = int(m.group(1))
                stack[0]['align'] = int(m.group(2))
            continue
        m = re.match(r'^\s*(\d+)(:\d+-\d+)? \| ( *)(.*)$', raw)
        if not m:
            continue
        depth = len(m.group(3)) // 2
        node = {'offset': int(m.group(1)), 'text': m.group(4), 'children': [], 'bits': m.group(2)}
        if depth == 0:
            stack = [node]
            w = re.match(r'^(?:struct|union) (ProbeWrap_\d+)$', node['text'])
            if w:
                records[w.group(1)] = node
            else:
                stack = None
            continue
        if stack is None:
            continue
        del stack[depth:]
        stack[-1]['children'].append(node)
        stack.append(node)
    return records


def leaves(node, path=''):
    """Flatten to [(path, offset, type text)]; nested aggregates are expanded."""
    out = []
    for c in node['children']:
        t = c['text']
        m = re.match(r'^(.*?)([A-Za-z_][A-Za-z_0-9]*)$', t)
        name = m.group(2) if m and not t.endswith(' ') else ''
        typ = m.group(1).strip() if m and not t.endswith(' ') else t.strip()
        p = path + ('.' if path and name else '') + name
        if c['children']:
            out.extend(leaves(c, p))
        else:
            out.append((p, c['offset'], typ))
    return out


def gather(files):
    result = {}
    for f in files:
        names = type_names(f)
        names = [n for n in dict.fromkeys(names)]
        per = {t: probe(f, names, t) for t in TARGETS}
        for i, n in enumerate(names):
            key = 'ProbeWrap_%d' % i
            if n in result:
                continue
            if key not in per['ps1'] or key not in per['host']:
                continue
            entry = {'file': Path(f).name}
            for t in TARGETS:
                rec = per[t][key]
                v = rec['children'][0] if rec['children'] else rec
                entry[t] = {'size': rec.get('sizeof'), 'align': rec.get('align'),
                            'leaves': leaves(v) if v is not rec else [], 'v': v}
            result[n] = entry
    return result


def summarize(entry):
    a, b = entry['ps1'], entry['host']
    la = [(p, o) for p, o, _ in a['leaves']]
    lb = [(p, o) for p, o, _ in b['leaves']]
    has_ptr = any('*' in t for _, _, t in a['leaves'])
    return a['size'], b['size'], la != lb or a['size'] != b['size'], has_ptr


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=['audit', 'show', 'dump'])
    ap.add_argument('args', nargs='*')
    a = ap.parse_args()
    files = sorted((ROOT / 'game/src').glob('*.c'))
    if a.cmd == 'show':
        files = sorted((ROOT / 'game/src').glob('*.c'))
    data = gather(files)
    if a.cmd == 'audit':
        n = 0
        for name, e in sorted(data.items()):
            s32, s64, differs, ptr = summarize(e)
            if differs:
                n += 1
                print('%-34s ps1 %5d  host %5d  %s  (%s)' % (name, s32, s64, 'pointers' if ptr else 'layout', e['file']))
        print('%d of %d types differ' % (n, len(data)))
    elif a.cmd == 'show':
        for name in a.args:
            e = data.get(name)
            if not e:
                print('%s: not found' % name)
                continue
            print('%s (%s): ps1 size %s align %s, host size %s align %s' % (name, e['file'], e['ps1']['size'], e['ps1']['align'], e['host']['size'], e['host']['align']))
            hl = {p: (o, t) for p, o, t in e['host']['leaves']}
            for p, o, t in e['ps1']['leaves']:
                ho = hl.get(p, (None, ''))[0]
                print('   %-28s ps1 %5d  host %5s  %s%s' % (p, o, ho, t, '' if o == ho else '   <-- moves'))
    else:
        out = {n: {t: {k: v for k, v in e[t].items() if k != 'v'} for t in TARGETS} | {'file': e['file']} for n, e in data.items()}
        Path(a.args[0]).write_text(json.dumps(out, indent=1))


if __name__ == '__main__':
    main()
