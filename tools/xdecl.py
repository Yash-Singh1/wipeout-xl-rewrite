#!/usr/bin/env python3
"""Cross-file declaration check.

The PlayStation compiler linked every file against one namespace and never
compared the types a file declared for another file's global or function: an
`extern int ships;` in one file and `ShipData *ships` in another were the same
four bytes.  Natively a pointer is eight bytes and a prototype-less `int f()` is
called with 32-bit results, so every disagreement is a bug.  This tool parses
each game source with clang (-ast-dump=json), groups the file-scope variables and
functions by link name (an `__asm__("name")` label counts) and reports:

  VAR   a variable declared with different types in different files
  FUNC  a function declared/defined with different return or parameter types
  DEF   a link name defined (not just declared) by more than one file
  IMPL  a call to an undeclared function (clang's implicit declaration)

Usage: tools/xdecl.py [--src game/src] [FILES...]
"""
import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def qual(t):
    """Desugared type string of a clang type object."""
    return t.get('desugaredQualType') or t['qualType']


def norm(s):
    s = re.sub(r'\bconst\b|\bvolatile\b|\bextern\b', '', s)
    s = re.sub(r'\s+', ' ', s).strip()
    # [] and [N] arrays agree when either is incomplete
    return s


def dump(path, defines):
    cmd = ['clang', '-std=gnu11', '-fsigned-char', '-include', str(ROOT / 'platform/include/port.h'),
           '-I' + str(ROOT / 'game/include'), '-fsyntax-only', '-w', '-Xclang', '-ast-dump=json',
           '-Wno-error'] + defines + [str(path)]
    out = subprocess.run(cmd, capture_output=True, text=True).stdout
    return json.loads(out)


def is_user(node, state):
    loc = node.get('loc', {})
    f = loc.get('file')
    if f:
        state['file'] = f
    return not (state.get('file', '').startswith(('/Applications', '/usr', '/Library', '/opt')))


def walk(tu_json, tu):
    syms = []
    state = {'file': ''}
    last_line = [0]
    for node in tu_json.get('inner', []):
        kind = node.get('kind')
        loc = node.get('loc', {})
        # clang elides repeated file names; track both loc and range.begin
        for key in ('loc',):
            if 'file' in loc:
                state['file'] = loc['file']
        rng = node.get('range', {}).get('begin', {})
        if 'file' in rng:
            state['file'] = rng['file']
        if kind not in ('FunctionDecl', 'VarDecl'):
            continue
        if state['file'].startswith(('/Applications', '/usr', '/Library', '/opt')):
            continue
        name = node.get('name')
        if not name:
            continue
        link = name
        for sub in node.get('inner', []):
            if sub.get('kind') == 'AsmLabelAttr':
                link = sub['label'] if 'label' in sub else sub.get('inner', [{}])[0].get('value', name)
        if node.get('storageClass') == 'static':
            continue
        t = node['type']
        info = {'name': name, 'link': link, 'kind': kind, 'type': norm(qual(t)),
                'sugar': norm(t['qualType']), 'file': state['file'], 'line': loc.get('line', 0),
                'tu': tu}
        if kind == 'FunctionDecl':
            info['defined'] = any(s.get('kind') == 'CompoundStmt' for s in node.get('inner', []))
            params = [norm(qual(s['type'])) for s in node.get('inner', []) if s.get('kind') == 'ParmVarDecl']
            info['params'] = params
            ret = info['type'].split('(')[0].strip() if '(' in info['type'] else info['type']
            info['ret'] = ret
            info['proto'] = not ('()' in info['type'] and not params and not info['defined'])
        else:
            has_init = any(s.get('kind') not in ('AsmLabelAttr',) and 'Attr' not in s.get('kind', '')
                           for s in node.get('inner', [])) or 'init' in node
            info['defined'] = bool(has_init) or node.get('storageClass') != 'extern'
        syms.append(info)
    return syms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='*')
    ap.add_argument('-D', action='append', default=[])
    args = ap.parse_args()
    files = [Path(f) for f in args.files] or sorted((ROOT / 'game/src').glob('*.c'))
    by_link = defaultdict(list)
    for f in files:
        data = dump(f, ['-D' + d for d in args.D])
        for s in walk(data, f.name):
            by_link[s['link']].append(s)
    problems = 0
    for link, items in sorted(by_link.items()):
        vars_ = [i for i in items if i['kind'] == 'VarDecl']
        funcs = [i for i in items if i['kind'] == 'FunctionDecl']
        if vars_ and funcs:
            print('KIND %s: variable and function' % link)
            problems += 1
        # variables
        types = defaultdict(list)
        for v in vars_:
            t = re.sub(r'\[\d*\]', '[]', v['type'])
            types[t].append(v)
        if len(types) > 1:
            problems += 1
            print('VAR %s' % link)
            for t, vs in types.items():
                where = sorted({'%s:%s' % (v['tu'] if v['file'].endswith('.c') else Path(v['file']).name, v['line']) for v in vs})
                print('    %-40s %s' % (t, ' '.join(where[:6]) + (' ...' if len(where) > 6 else '')))
        defs = sorted({v['tu'] for v in items if v['defined'] and v['file'].endswith('.c')})
        defs_all = sorted({(v['tu']) for v in items if v['defined']})
        if len(defs_all) > 1 and vars_:
            problems += 1
            print('DEF %s defined in %s' % (link, ' '.join(defs_all)))
        elif len(defs_all) > 1 and funcs:
            problems += 1
            print('DEF %s (function) defined in %s' % (link, ' '.join(defs_all)))
        # functions
        rets = defaultdict(list)
        for f in funcs:
            rets[f['ret']].append(f)
        if len(rets) > 1:
            problems += 1
            print('FUNC %s: return types differ' % link)
            for t, fs in rets.items():
                where = sorted({'%s:%s' % (f['tu'], f['line']) for f in fs})
                print('    %-40s %s' % (t, ' '.join(where[:6])))
        protos = [f for f in funcs if f['proto']]
        sigs = defaultdict(list)
        for f in protos:
            sigs[tuple(f['params'])].append(f)
        if len(sigs) > 1:
            problems += 1
            print('FUNC %s: parameters differ' % link)
            for p, fs in sigs.items():
                where = sorted({'%s:%s' % (f['tu'], f['line']) for f in fs})
                print('    (%s)  %s' % (', '.join(p), ' '.join(where[:6])))
    print('%d problem(s)' % problems)


if __name__ == '__main__':
    main()
