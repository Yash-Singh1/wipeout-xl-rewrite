#!/usr/bin/env python3
"""Generate the C definitions of the PlayStation game's global data from the retail executable.

  tools/gendata.py            write game/data/*_data.c
  tools/gendata.py --check    compile them for a 32-bit little-endian target with the MIPS o32
                              struct layout, compare every symbol with the executable, and build
                              and run the alias tests natively
  tools/gendata.py --report   print the conflicts / adjacency / literal reports

See tools/GENDATA.md for what it does and how to regenerate.
"""
import argparse
import bisect
import collections
import glob
import json
import os
import pickle
import re
import struct
import subprocess
import sys
import shutil
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DECOMP = os.path.join(os.path.dirname(ROOT), 'wipeout-xl', 'decomp')
CACHE = os.environ.get('GENDATA_CACHE', '/tmp/gendata-cache-ps1')

import gendata_decisions as D

# The PlayStation CPU is a 32-bit little-endian MIPS (o32 ABI): 4-byte pointers and long, doubles and
# long longs aligned to 8, char signed (PsyQ GCC). armv7 Linux has the same record layout rules.
TARGET32 = ['-target', 'armv7-unknown-linux-gnueabi', '-ffreestanding', '-fsigned-char']
STUBS = os.path.join(ROOT, 'tools', 'gendata_stubs')


# --------------------------------------------------------------------------
# inputs: the executable and the splat output

IMAGE_BASE = 0x80010000       # load address of the PS-X EXE text (file offset 0x800)
FILE_BIAS = 0x800
RAM_END = 0x80200000


class Section:
    def __init__(self, name, lo, hi):
        self.name, self.va, self.end = name, lo, hi


class Exe:
    """The PS-X EXE: bytes of everything but the zero-initialised data."""

    def __init__(self, path, sections):
        self.d = open(path, 'rb').read()
        assert self.d[:8] == b'PS-X EXE'
        self.t_addr, self.t_size = struct.unpack_from('<II', self.d, 0x18)
        assert self.t_addr == IMAGE_BASE
        self.sections = sections
        self.filled_end = self.t_addr + self.t_size

    def section_of(self, a):
        for s in self.sections:
            if s.va <= a < s.end:
                return s
        return None

    def read(self, a, n):
        out = bytearray()
        for i in range(n):
            x = a + i
            if IMAGE_BASE <= x < self.filled_end:
                out.append(self.d[x - IMAGE_BASE + FILE_BIAS])
            else:
                out.append(0)
        return bytes(out)

    def u32(self, a):
        return struct.unpack('<I', self.read(a, 4))[0]

    def cstr(self, a, limit=512):
        b = self.read(a, limit)
        n = b.find(b'\0')
        return b if n < 0 else b[:n]

    def in_image(self, v):
        return IMAGE_BASE <= v < RAM_END


WORD_RE = re.compile(r'\s*/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s+[0-9A-Fa-f]{8}\s*\*/\s*\.word\s+(\S+)')


class Env:
    def __init__(self, decomp):
        self.decomp = decomp
        yaml = open(os.path.join(decomp, 'scus_943.51.yaml')).read()
        main_vram = int(re.search(r'name: main\s+type: code\s+start: 0x[0-9A-Fa-f]+\s+vram: (0x[0-9A-Fa-f]+)', yaml).group(1), 16)
        main_off = int(re.search(r'name: main\s+type: code\s+start: (0x[0-9A-Fa-f]+)', yaml).group(1), 16)
        text_off = int(re.search(r'\[(0x[0-9A-Fa-f]+), c, main\]', yaml).group(1), 16)
        data_off = int(re.search(r'\[(0x[0-9A-Fa-f]+), data\]', yaml).group(1), 16)
        bss_vram = int(re.search(r'type: bss, vram: (0x[0-9A-Fa-f]+)', yaml).group(1), 16)
        bss_size = int(re.search(r'bss_size: (0x[0-9A-Fa-f]+)', yaml).group(1), 16)
        text_va = main_vram + text_off - main_off
        data_va = main_vram + data_off - main_off
        # the assembler output of rodata that is not literals of a C file: [start, end) in the image
        subs = [(int(a, 16), k) for a, k in re.findall(r'^\s*- \[(0x[0-9A-Fa-f]+), ([^\]]*)\]', yaml, re.M)]
        subs.sort()
        self.asm_rodata = []
        for i, (off, kind) in enumerate(subs):
            if kind.strip() == 'rodata' and i + 1 < len(subs):
                self.asm_rodata.append((main_vram + off - main_off, main_vram + subs[i + 1][0] - main_off))
        self.sections = [Section('.pre', IMAGE_BASE, main_vram), Section('.rodata', main_vram, text_va),
                         Section('.text', text_va, data_va), Section('.data', data_va, bss_vram),
                         Section('.bss', bss_vram, bss_vram + bss_size)]
        self.pe = Exe(os.path.join(os.path.dirname(decomp), 'extracted', 'SCUS_943.51'), self.sections)
        self.rows = []        # (addr, name, kind): kind 'data' (labelled in asm/data), 'func'
        self.ptr_name = {}    # address of a pointer word -> operand the assembly gives (name or 0x8...)
        self.label_sec = {}   # data label -> section of the .s file
        self.label_size = {}  # bss label -> size
        self.labels = []
        for path in sorted(glob.glob(os.path.join(decomp, 'asm', 'data', '*.s'))):
            sec = None
            cur = None
            size = None
            for line in open(path):
                m = re.match(r'\.section (\.\w+)', line)
                if m:
                    sec = m.group(1)
                    continue
                m = re.match(r'nonmatching (\S+?)(?:, (0x[0-9a-fA-F]+))?\s*$', line)
                if m:
                    size = int(m.group(2), 16) if m.group(2) else None
                    continue
                m = re.match(r'dlabel (\S+)', line)
                if m:
                    cur = [m.group(1), None, sec, size]
                    self.labels.append(cur)
                    continue
                if cur is not None and cur[1] is None:
                    m = re.match(r'\s*/\*\s*(?:[0-9A-Fa-f]+\s+)?([0-9A-Fa-f]{8})\b', line)
                    if m:
                        cur[1] = int(m.group(1), 16)
                m = WORD_RE.match(line)
                if m:
                    self.ptr_name[int(m.group(1), 16)] = m.group(2)
        for name, addr, sec, size in self.labels:
            self.rows.append((addr, name, 'data'))
            self.label_sec[name] = sec
            if size is not None:
                self.label_size[name] = size
        self.funcs = {}
        self.sym_addrs = {}
        for line in open(os.path.join(decomp, 'symbol_addrs.txt')):
            m = re.match(r'(\w+)\s*=\s*(0x[0-9A-Fa-f]+);\s*(?://\s*(.*))?', line)
            if not m:
                continue
            name, addr, attrs = m.group(1), int(m.group(2), 16), m.group(3) or ''
            self.sym_addrs[name] = addr
            if 'type:func' in attrs:
                self.funcs[addr] = name
        for path in glob.glob(os.path.join(decomp, 'asm', '*.s')) + glob.glob(os.path.join(decomp, 'asm', 'nonmatchings', '*', '*.s')):
            lines = open(path).read().split('\n')
            for k, l in enumerate(lines):
                m = re.match(r'glabel (\S+)', l)
                if m and not m.group(1).startswith('.'):
                    for l2 in lines[k + 1:k + 4]:
                        mm = re.match(r'\s*/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})', l2)
                        if mm:
                            self.funcs.setdefault(int(mm.group(1), 16), m.group(1))
                            break
        # Names of symbol_addrs.txt that the assembler output has no label for: they lie inside a bigger
        # label's range (a string with a short variable in its padding...).
        have = {n for n, *_ in self.labels}
        self.extra_labels = []
        for name, addr in sorted(self.sym_addrs.items(), key=lambda kv: kv[1]):
            sec = self.pe.section_of(addr)
            if name in have or addr in self.funcs or sec is None or sec.name not in ('.rodata', '.data', '.bss'):
                continue
            self.extra_labels.append((name, addr, sec.name))
            self.rows.append((addr, name, 'data'))
            self.label_sec[name] = sec.name
        # Globals the sources declare under a name the console's tables do not have: one object that was
        # several names there (D.RENAMED: new name -> the first of those names).
        for new, (anchor, _why) in D.RENAMED.items():
            addr = next(a for a, n, *_ in self.rows if n == anchor)
            self.rows.append((addr, new, 'data'))
            self.label_sec[new] = self.label_sec[anchor]
        for a, n in sorted(self.funcs.items()):
            sec = self.pe.section_of(a)
            if sec is None or sec.name not in ('.pre', '.text'):
                del self.funcs[a]       # symbol_addrs.txt marks a few data labels `type:func`
                continue
            self.rows.append((a, n, 'func'))
        self.rows.sort()
        self.by_name = {n: (a, k) for a, n, k in self.rows}
        self.relocs = set()     # addresses of words that hold a pointer, per the assembly
        for a, v in self.ptr_name.items():
            if v.startswith('.L'):
                continue
            if v.startswith('0x'):
                if not (IMAGE_BASE <= int(v, 16) < RAM_END):
                    continue
            self.relocs.add(a)
        self.data = sorted((a, n) for a, n, k in self.rows if k == 'data')
        self.rows.sort()


# --------------------------------------------------------------------------
# clang AST scan


def clang_cmd(extra):
    return (['clang'] + TARGET32 + ['-w', '-ferror-limit=0', '-isystem', STUBS,
            '-include', os.path.join(ROOT, 'platform', 'include', 'port.h'),
            '-I' + os.path.join(ROOT, 'game', 'include')] + extra)


def resolve_locs(root):
    """Fill in the file and line clang's JSON omits when unchanged (document order)."""
    state = {'file': None, 'line': 0}

    def walk(n):
        if isinstance(n, dict):
            if 'offset' in n and 'tokLen' in n:
                if 'file' in n:
                    state['file'] = n['file']
                if 'line' in n:
                    state['line'] = n['line']
                n['_file'], n['_line'] = state['file'], state['line']
                return
            for v in n.values():
                if isinstance(v, (dict, list)):
                    walk(v)
        else:
            for v in n:
                if isinstance(v, (dict, list)):
                    walk(v)
    walk(root)


def loc_of(n):
    loc = n.get('loc', {})
    if 'expansionLoc' in loc:
        loc = loc['expansionLoc']
    return loc.get('_file'), loc.get('_line')


def slim(n):
    """Drop positions and ids that nobody needs, keep the structure."""
    if isinstance(n, dict):
        out = {}
        for k, v in n.items():
            if k in ('range',):
                continue
            out[k] = slim(v)
        return out
    if isinstance(n, list):
        return [slim(x) for x in n]
    return n


def scan_tu(path):
    r = subprocess.run(clang_cmd(['-Xclang', '-ast-dump=json', '-fsyntax-only', path]),
                       capture_output=True, text=True, cwd=ROOT)
    tree = json.loads(r.stdout)
    resolve_locs(tree)
    keep = []
    for n in tree['inner']:
        k = n['kind']
        if k in ('VarDecl', 'TypedefDecl', 'RecordDecl', 'EnumDecl'):
            f, line = loc_of(n)
            rng = n.get('range')
            span = None
            if k in ('TypedefDecl', 'RecordDecl') and rng:
                b, e = rng['begin'], rng['end']
                if 'expansionLoc' in b:
                    b = b['expansionLoc']
                if 'expansionLoc' in e:
                    e = e['expansionLoc']
                if '_file' in b and '_file' in e:
                    span = (b['_file'], b['offset'], e['offset'] + e['tokLen'])
            n = slim(n)
            n['_where'] = (f, line)
            n['_span'] = span
            keep.append(n)
        elif k == 'FunctionDecl':
            f, line = loc_of(n)
            has_body = any(c['kind'] == 'CompoundStmt' for c in n.get('inner', []))
            keep.append({'kind': 'FunctionDecl', 'name': n.get('name'), 'type': n['type'],
                         'storageClass': n.get('storageClass'), 'body': has_body, '_where': (f, line)})
    return keep


def newest_input(path):
    files = [path, __file__, os.path.join(ROOT, 'platform', 'include', 'port.h')]
    files += glob.glob(os.path.join(ROOT, 'game', 'include', '*.h'))
    files += glob.glob(os.path.join(STUBS, '*.h'))
    return max(os.path.getmtime(f) for f in files)


def scan_all(force=False):
    os.makedirs(CACHE, exist_ok=True)
    out = {}
    for path in sorted(glob.glob(os.path.join(ROOT, 'game', 'src', '*.c'))):
        name = os.path.basename(path)
        cp = os.path.join(CACHE, name + '.pkl')
        if not force and os.path.exists(cp) and os.path.getmtime(cp) > newest_input(path):
            out[name] = pickle.load(open(cp, 'rb'))
            continue
        out[name] = scan_tu(path)
        pickle.dump(out[name], open(cp, 'wb'))
    return out



# --------------------------------------------------------------------------
# type model (MSVC i686 layout)


class Ty:
    kind = '?'
    size = 0
    align = 1


class Int(Ty):
    kind = 'int'

    def __init__(self, size, signed, spelling):
        self.size, self.align, self.signed, self.spelling = size, size, signed, spelling

    def __repr__(self):
        return self.spelling


class Flt(Ty):
    kind = 'float'

    def __init__(self, size):
        self.size = self.align = size

    def __repr__(self):
        return 'float' if self.size == 4 else 'double'


class Void(Ty):
    kind = 'void'
    size = 1

    def __repr__(self):
        return 'void'


class Ptr(Ty):
    kind = 'ptr'
    size = align = 4

    def __init__(self, to):
        self.to = to

    def __repr__(self):
        return '%r *' % (self.to,)


class Func(Ty):
    kind = 'func'
    size = 1

    def __init__(self, ret, params):
        self.ret, self.params = ret, params

    def __repr__(self):
        return '%r (%s)' % (self.ret, self.params)


class Arr(Ty):
    kind = 'arr'

    def __init__(self, elem, n):
        self.elem, self.n = elem, n

    @property
    def size(self):
        return self.elem.size * (self.n or 0)

    @property
    def align(self):
        return self.elem.align

    def __repr__(self):
        return '%r[%s]' % (self.elem, '' if self.n is None else self.n)


class Field:
    def __init__(self, name, ty, offset, bits=None, bitpos=0):
        self.name, self.ty, self.offset, self.bits, self.bitpos = name, ty, offset, bits, bitpos


class Rec(Ty):
    kind = 'rec'

    def __init__(self, tag, union, name):
        self.tag, self.union, self.name = tag, union, name
        self.fields = []
        self.size = 0
        self.align = 1
        self.complete = False

    def __repr__(self):
        return ('union ' if self.union else 'struct ') + (self.name or '<anon>')


class Opaque(Ty):
    """A type the declarations name but the headers on hand do not define."""
    kind = 'opaque'

    def __init__(self, spelling):
        self.spelling = spelling

    def __repr__(self):
        return self.spelling


BUILTIN = {
    'char': Int(1, True, 'char'), 'signed char': Int(1, True, 'signed char'),
    'unsigned char': Int(1, False, 'unsigned char'),
    'short': Int(2, True, 'short'), 'unsigned short': Int(2, False, 'unsigned short'),
    'int': Int(4, True, 'int'), 'unsigned int': Int(4, False, 'unsigned int'),
    'long': Int(4, True, 'long'), 'unsigned long': Int(4, False, 'unsigned long'),
    'long long': Int(8, True, 'long long'), 'unsigned long long': Int(8, False, 'unsigned long long'),
    'float': Flt(4), 'double': Flt(8), 'long double': Flt(8), 'void': Void(), '_Bool': Int(1, False, '_Bool'),
}
BUILTIN_WORDS = {'char', 'short', 'int', 'long', 'float', 'double', 'void', 'unsigned', 'signed', '_Bool', '__int64'}
ANON_RE = re.compile(r'(?:\w+::)*\((?:unnamed|anonymous)[^)]*\)')
TOK_RE = re.compile(r'\s*(\.\.\.|[A-Za-z_]\w*|\d+|[*()\[\],])')


def builtin_of(words):
    unsigned = 'unsigned' in words
    if '__int64' in words or words.count('long') == 2:
        return BUILTIN['unsigned long long' if unsigned else 'long long']
    if 'double' in words:
        return BUILTIN['long double' if 'long' in words else 'double']
    if 'float' in words:
        return BUILTIN['float']
    if 'void' in words:
        return BUILTIN['void']
    if '_Bool' in words:
        return BUILTIN['_Bool']
    if 'char' in words:
        return BUILTIN['unsigned char' if unsigned else 'signed char' if 'signed' in words else 'char']
    if 'short' in words:
        return BUILTIN['unsigned short' if unsigned else 'short']
    if 'long' in words:
        return BUILTIN['unsigned long' if unsigned else 'long']
    return BUILTIN['unsigned int' if unsigned else 'int']


class Types:
    """Type environment of one translation unit."""

    def __init__(self, nodes, packs):
        self.typedefs = {}      # name -> qualType string
        self.typedef_node = {}
        self.records = {}       # ('struct'|'union', name) -> RecordDecl node (complete preferred)
        self.enums = set()
        self.anon = {}          # 'file:line:col' -> RecordDecl node
        self.cache = {}         # id(record node) -> Rec
        self.packs = packs
        owned = {}
        for n in nodes:
            if n['kind'] == 'TypedefDecl':
                prev = self.typedef_node.get(n['name'])
                if prev is not None and '/include/' in (prev['_where'][0] or '') \
                        and '/include/' not in (n['_where'][0] or ''):
                    continue            # a .c file repeating a header's typedef
                self.typedefs[n['name']] = n['type']['qualType']
                self.typedef_node[n['name']] = n
                for c in n.get('inner', []):
                    if c['kind'] == 'ElaboratedType' and 'ownedTagDecl' in c:
                        owned[c['ownedTagDecl']['id']] = n['name']
        queue = []
        for n in nodes:
            if n['kind'] == 'RecordDecl':
                name = n.get('name') or owned.get(n['id'])
                if name:
                    key = (n['tagUsed'], name)
                    if n.get('completeDefinition') or key not in self.records:
                        self.records[key] = n
                else:
                    queue.append(n)
            elif n['kind'] == 'EnumDecl':
                if n.get('name'):
                    self.enums.add(n['name'])
            elif n['kind'] == 'VarDecl':
                m = re.search(r'\((?:unnamed|anonymous)[^)]*? at ([^)]*)\)', n['type']['qualType'])
                if m and m.group(1) not in self.anon and queue:
                    self.anon[m.group(1)] = queue.pop(0)
        self.owned = owned
        self.uses = set()
        self.use_nodes = {}

    # -- parsing of clang's qualType spellings

    def parse(self, s):
        s = s.strip()
        anons = []

        def sub(m):
            anons.append(m.group(0))
            return 'ANON%d' % (len(anons) - 1)
        s = ANON_RE.sub(sub, s)
        toks = TOK_RE.findall(s)
        self.anons = anons
        ty, pos = self.declarator_full(toks, 0, len(toks))
        return ty

    def declarator_full(self, toks, pos, end):
        base, pos = self.specifiers(toks, pos, end)
        return self.declarator(toks, pos, end, base), pos

    def specifiers(self, toks, pos, end):
        words = []
        base = None
        while pos < end:
            t = toks[pos]
            if t in ('const', 'volatile', 'restrict', '__restrict'):
                pos += 1
            elif t in BUILTIN_WORDS:
                words.append(t)
                pos += 1
            elif t in ('struct', 'union', 'enum') and base is None:
                name = toks[pos + 1]
                pos += 2
                if t == 'enum':
                    base = BUILTIN['int']
                elif name.startswith('ANON'):
                    base = self.record_of_anon(self.anons[int(name[4:])], t)
                else:
                    base = self.record(t, name)
            elif not words and base is None and re.match(r'[A-Za-z_]\w*$', t):
                pos += 1
                base = self.typedef(t)
            else:
                break
        if base is None:
            base = builtin_of(words) if words else BUILTIN['int']
        return base, pos

    def typedef(self, name):
        if name in self.typedefs:
            self.uses.add(self.typedef_node[name]['_where'][0])
            self.use_nodes[self.typedef_node[name]['id']] = self.typedef_node[name]
            return self.parse_sub(self.typedefs[name])
        if name in self.enums:
            return BUILTIN['int']
        return Opaque(name)

    def parse_sub(self, s):
        saved = getattr(self, 'anons', None)
        ty = self.parse(s)
        self.anons = saved
        return ty

    def declarator(self, toks, pos, end, base):
        t = base
        while pos < end and toks[pos] == '*':
            pos += 1
            while pos < end and toks[pos] in ('const', 'volatile', 'restrict'):
                pos += 1
            t = Ptr(t)
        inner = None
        if pos < end and toks[pos] == '(' and pos + 1 < end and toks[pos + 1] in ('*', '('):
            depth = 0
            q = pos
            while True:
                if toks[q] == '(':
                    depth += 1
                elif toks[q] == ')':
                    depth -= 1
                    if depth == 0:
                        break
                q += 1
            inner = (pos + 1, q)
            pos = q + 1
        suffixes = []
        while pos < end:
            if toks[pos] == '[':
                if toks[pos + 1] == ']':
                    suffixes.append(('a', None))
                    pos += 2
                else:
                    suffixes.append(('a', int(toks[pos + 1])))
                    pos += 3
            elif toks[pos] == '(':
                depth = 0
                q = pos
                while True:
                    if toks[q] == '(':
                        depth += 1
                    elif toks[q] == ')':
                        depth -= 1
                        if depth == 0:
                            break
                    q += 1
                self.note_params(toks[pos + 1:q])
                suffixes.append(('f', ' '.join(toks[pos + 1:q])))
                pos = q + 1
            else:
                break
        for kind, v in reversed(suffixes):
            t = Arr(t, v) if kind == 'a' else Func(t, v)
        if inner:
            t = self.declarator(toks, inner[0], inner[1], t)
        return t

    def note_params(self, toks):
        """Names in a parameter list count as uses of their declarations."""
        for i, t in enumerate(toks):
            if t in ('struct', 'union') and i + 1 < len(toks):
                self.record(t, toks[i + 1])
            elif t in self.typedefs and (i == 0 or toks[i - 1] not in ('struct', 'union', 'enum')):
                self.typedef(t)

    # -- records

    def record(self, tag, name):
        node = self.records.get((tag, name))
        if node is None:
            return Opaque('%s %s' % (tag, name))
        self.uses.add(node['_where'][0])
        self.use_nodes[node['id']] = node
        return self.build(node, name)

    def record_of_anon(self, text, tag):
        m = re.search(r' at (.*)\)$', text)
        node = self.anon.get(m.group(1)) if m else None
        if node is None:
            return Opaque(text)
        return self.build(node, None)

    def build(self, node, name):
        key = node['id']
        if key in self.cache:
            c = self.cache[key]
            self.use_nodes.update(getattr(c, 'deps', {}))
            self.uses |= getattr(c, 'dep_files', set())
            return c
        rec = Rec(node['tagUsed'], node['tagUsed'] == 'union', name or node.get('name'))
        self.cache[key] = rec
        rec.node = node
        if node.get('name'):
            rec.spell = '%s %s' % (node['tagUsed'], node['name'])
        elif node['id'] in self.owned:
            rec.spell = self.owned[node['id']]
        else:
            rec.spell = None
        if not node.get('completeDefinition'):
            return rec
        outer_nodes, outer_files = self.use_nodes, self.uses
        self.use_nodes, self.uses = {}, set()
        try:
            self.build_fields(rec, node)
        finally:
            rec.deps, rec.dep_files = self.use_nodes, self.uses
            outer_nodes.update(self.use_nodes)
            outer_files |= self.uses
            self.use_nodes, self.uses = outer_nodes, outer_files
        return rec

    def build_fields(self, rec, node):
        pack = None
        for c in node.get('inner', []):
            if c['kind'] == 'MaxFieldAlignmentAttr':
                f, line = node['_where']
                pack = self.packs.get(f, lambda l: None)(line)
        queue = []
        for c in node.get('inner', []):
            if c['kind'] == 'RecordDecl' and not c.get('name'):
                queue.append(c)
            elif c['kind'] == 'FieldDecl':
                qt = c['type']['qualType']
                m = re.search(r'\((?:unnamed|anonymous)[^)]*? at ([^)]*)\)', qt)
                if m and m.group(1) not in self.anon and queue:
                    self.anon[m.group(1)] = queue.pop(0)
        off = 0
        align = 1
        for c in node.get('inner', []):
            if c['kind'] != 'FieldDecl':
                continue
            ty = self.parse(c['type']['qualType'])
            if c.get('isBitfield'):
                raise NotImplementedError('bitfield in %r' % rec)
            fa = min(ty.align, pack) if pack else ty.align
            if rec.union:
                foff = 0
                off = max(off, ty.size)
            else:
                off = (off + fa - 1) // fa * fa
                foff = off
                off += ty.size
            align = max(align, fa)
            rec.fields.append(Field(c.get('name'), ty, foff))
        rec.align = align
        rec.size = (off + align - 1) // align * align
        rec.complete = True
        return rec


def pack_table():
    """file -> function(line) giving the #pragma pack value in force, for the game headers."""
    out = {}
    paths = glob.glob(os.path.join(ROOT, 'game', 'include', '*.h')) + glob.glob(os.path.join(ROOT, 'game', 'src', '*.c'))
    paths += glob.glob(os.path.join(STUBS, '*.h'))
    for p in paths:
        spans = []
        stack = []
        for i, line in enumerate(open(p, encoding='latin-1'), 1):
            m = re.match(r'\s*#\s*pragma\s+pack\s*\((.*)\)', line)
            if not m:
                continue
            a = [x.strip() for x in m.group(1).split(',')]
            if a[0] == 'push':
                stack.append((i, int(a[1]) if len(a) > 1 else None))
            elif a[0] == 'pop':
                if stack:
                    start, v = stack.pop()
                    spans.append((start, i, v))
        if spans:
            out[p] = (lambda sp: (lambda l: next((v for a, b, v in sp if a < l < b), None)))(spans)
    return out




# --------------------------------------------------------------------------
# symbols: declarations, extents, types


CONTAINERS = {a.container for a in D.ALIASES}
ALIAS_NAMES = {a.name for a in D.ALIASES}


def type_key(t):
    """Structural spelling used to tell declarations apart."""
    if t.kind == 'ptr':
        return type_key(t.to) + ' *'
    if t.kind == 'func':
        return '%s(%s)' % (type_key(t.ret), re.sub(r'\s+', ' ', t.params))
    if t.kind == 'arr':
        return '%s[%s]' % (type_key(t.elem), '' if t.n is None else t.n)
    if t.kind == 'rec':
        return repr(t) if t.name else 'struct <anon@%s>' % t.node['id']
    return repr(t)


def has_opaque(t):
    if t.kind == 'opaque':
        return True
    if t.kind == 'ptr':
        return False
    if t.kind == 'arr':
        return has_opaque(t.elem)
    if t.kind == 'rec':
        return any(has_opaque(f.ty) for f in t.fields)
    return False


class Decl:
    """One declaration of a global in one translation unit."""

    def __init__(self, tu, node, ty, tys):
        self.tu, self.node, self.ty, self.tys = tu, node, ty, tys
        self.qual = node['type']['qualType']
        self.where = node['_where']
        self.extern = node.get('storageClass') == 'extern' and 'init' not in node
        self.static = node.get('storageClass') == 'static'
        self.in_header = '/include/' in (self.where[0] or '')
        self.invalid = node.get('isInvalid', False)
        self.via_label = node.get('mangledName') not in (None, node.get('name'))
        self.has_init = 'init' in node
        self.aligned = 0
        for c in node.get('inner', []):
            if c.get('kind') == 'AlignedAttr':
                for e in c.get('inner', []):
                    if e.get('value', '').isdigit():
                        self.aligned = max(self.aligned, int(e['value']))

    def spell(self):
        return self.qual


class Sym:
    def __init__(self, addr, name):
        self.addr, self.name = addr, name
        self.cname = D.RENAMES.get(name, name)
        self.alias = False
        self.synthetic = None
        self.decls = []
        self.defined_in_game = False
        self.tentative = []       # tentative definitions in game/src
        self.static_in = None
        self.kind = None          # 'rdata' 'data' 'bss' 'text'
        self.extent = 0
        self.ty = None            # chosen type
        self.qual = None          # chosen spelling
        self.decl = None
        self.size = 0
        self.module = None
        self.conflicts = []
        self.notes = []
        self.inferred = False
        self.lib = False


LIB_DATA_STARTS = (0x80090D30, 0x80093DE4)       # the libraries' .data lies between the game's .data and its small data
AUTO_RE = re.compile(r'^(D_[0-9A-F]{8}|func_[0-9A-F]{8}|jtbl_[0-9A-F]{8}|L[0-9A-F]{8})$')


def build_symbols(env, scan, tys):
    syms = {}
    order = []
    for a, n in env.data:
        sy = Sym(a, n)
        syms[n] = sy
        order.append(sy)
    kinds = {'.rodata': 'rdata', '.data': 'data', '.bss': 'bss'}
    for i, sy in enumerate(order):
        sec = env.pe.section_of(sy.addr)
        sy.kind = kinds.get(env.label_sec[sy.name], 'text')
        sy.section = sec
        sy.auto = bool(AUTO_RE.match(sy.name))
    bycname = {sy.cname: sy for sy in order}
    for tu, nodes in scan.items():
        for n in nodes:
            # `type name __asm__("symbol")` declares the symbol under another identifier
            link = n.get('mangledName') or n.get('name')
            if n['kind'] != 'VarDecl' or link not in bycname:
                continue
            sy = bycname[link]
            tys[tu].uses = set()
            tys[tu].use_nodes = {}
            try:
                ty = tys[tu].parse(n['type']['qualType'])
            except Exception as e:     # noqa
                ty = Opaque('?' + n['type']['qualType'])
            d = Decl(tu, n, ty, tys[tu])
            d.uses = set(tys[tu].uses)
            d.use_nodes = dict(tys[tu].use_nodes)
            sy.decls.append(d)
            if not d.extern and not d.invalid:
                sy.defined_in_game = True
                sy.tentative.append(d)
                if d.static:
                    sy.static_in = tu
    # The assembler labels every address the code mentions (D_8xxxxxxx), and the decompilation named
    # many of them: most are fields or elements inside a bigger object. Only the names the game's C
    # declares end an object (and the names of the decisions table that stand for a part of another
    # one do not). sy.extent runs to the next such name, sy.raw_extent to the next label of any kind.
    ends = [i for i, sy in enumerate(order)
            if (sy.decls and sy.name not in ALIAS_NAMES) or sy.addr in LIB_DATA_STARTS or sy.name.startswith('_')]
    for i, sy in enumerate(order):
        sec = sy.section
        end = sec.end if sec else sy.addr
        if sy.kind == 'rdata':
            # an object of an assembler rodata file stops at the end of the file: what follows are the
            # literals and jump tables of a C file's functions, not data
            for lo_, hi_ in env.asm_rodata:
                if lo_ <= sy.addr < hi_:
                    end = hi_
        k = bisect.bisect_right(ends, i)
        nxt = order[ends[k]].addr if k < len(ends) else end
        nraw = order[i + 1].addr if i + 1 < len(order) else end
        sy.extent = min(nxt, end) - sy.addr
        sy.raw_extent = min(nraw, end) - sy.addr
    return syms, order


def code_ranges(env):
    s = {x.name: x for x in env.sections}
    return [(s['.pre'].va, s['.pre'].end), (s['.text'].va, s['.text'].end)]


def scan_code(env):
    """Every access the machine code makes to a constant address (tools/mipsscan.py)."""
    import mipsscan
    pre = next(x for x in env.sections if x.name == '.pre')
    sc = mipsscan.Scanner(env.pe, code_ranges(env), 0x80093DE4, no_gp=[(pre.va, pre.end)])
    sc.run()
    env.code = sc
    return sc


def access_widths(env):
    """address -> set of operand widths the code uses when it reads or writes exactly that address."""
    out = collections.defaultdict(set)
    for a in scan_code(env).accesses:
        out[a.address].add(a.width)
    return out


def choose_types(syms, order, report, widths=None):
    """Pick the declaration each undefined symbol is generated from."""
    widths = widths or {}
    for sy in order:
        ds = list(sy.decls)
        good = [d for d in ds if not has_opaque(d.ty)]
        # a declaration under another identifier (`int fooWord __asm__("foo")`, a word-wise view the
        # decompiler used to get word copies) says how foo is accessed, not what it is
        direct = [d for d in good if not d.via_label]
        if direct:
            # ... but a record or array view that fits the extent says how big the object is
            good = direct + [d for d in good if d.via_label and d.ty.kind in ('arr', 'rec')
                             and not (d.ty.kind == 'arr' and d.ty.n is None) and d.ty.size <= sy.extent]
        if sy.name in D.PREFER:
            good = [d for d in good if d.qual == D.PREFER[sy.name]] or good
        if not good:
            continue
        # distinct structural types
        groups = collections.OrderedDict()
        for d in good:
            groups.setdefault(type_key(d.ty), []).append(d)
        sy.conflicts = [(k, [(d.tu, d.where[1]) for d in v]) for k, v in groups.items()]

        def elem_of(t):
            while t.kind == 'arr':
                t = t.elem
            return t

        def agrees(d):
            # a scalar view narrower than an access the code makes at the address cannot be the object
            seen = widths.get(sy.addr)
            t = d.ty
            if seen and t.kind in ('int', 'float', 'ptr'):
                return 1 if t.size >= max(seen) else 0
            return 1

        def score(d):
            t = d.ty
            e = elem_of(t)
            incomplete = t.kind == 'arr' and t.n is None
            private = any(u and '/game/src/' in u for u in d.uses)
            # arrays and records stand for more of the object than scalars do: the scalar declarations
            # are the decompiler's word-wise views (`u16 linkPadHead` for the head of a ControlInput)
            rank = 2 if (t.kind in ('arr', 'rec') or e.kind == 'rec') else 1
            # natively a pointer is 8 bytes: a declaration with pointers wins over a same-size view without
            has_ptr = bool(pointer_slots(e)) or e.kind == 'ptr'
            return (agrees(d), has_ptr, rank, e.kind == 'rec', e.size, 0 if incomplete else t.size, not private,
                    d.in_header, len(groups[type_key(d.ty)]))
        pool = list(good)
        pool.sort(key=score, reverse=True)
        sy.decl = pool[0]
        sy.ty = sy.decl.ty
        sy.qual = sy.decl.qual


def spell_type(t):
    """C spelling of a parsed type (no declarator); None when it cannot be written."""
    if t.kind == 'int':
        return t.spelling
    if t.kind == 'float':
        return 'float' if t.size == 4 else 'double'
    if t.kind == 'void':
        return 'void'
    if t.kind == 'opaque':
        return t.spelling
    if t.kind == 'rec':
        return getattr(t, 'spell', None)
    if t.kind == 'ptr':
        if t.to.kind == 'func':
            return spell_func_ptr(t.to)
        inner = spell_type(t.to)
        if inner is None:
            return None
        return inner + (' *' if not inner.endswith('*') else '*')
    return None


def spell_func_ptr(f):
    r = spell_type(f.ret)
    return None if r is None else '%s (*)(%s)' % (r, f.params)


def c_string(b):
    out = []
    for i, c in enumerate(b):
        ch = chr(c)
        if ch == '"':
            out.append('\\"')
        elif ch == '\\':
            out.append('\\\\')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\t':
            out.append('\\t')
        elif ch == '\r':
            out.append('\\r')
        elif 32 <= c < 127:
            out.append(ch)
        else:
            out.append('\\%03o' % c)
    return '"' + ''.join(out) + '"'


def fmt_float(raw, size):
    """Shortest decimal literal that round-trips exactly, or None (inf/nan)."""
    if size == 4:
        v = struct.unpack('<f', raw)[0]
        if v != v or v in (float('inf'), float('-inf')):
            return None
        if v == 0:
            return '-0.0f' if raw[3] & 0x80 else '0.0f'
        for p in range(1, 10):
            t = '%.*g' % (p, v)
            if struct.pack('<f', float(t)) == raw:
                break
        if not any(c in t for c in '.eE'):
            t += '.0'
        return t + 'f'
    v = struct.unpack('<d', raw)[0]
    if v != v or v in (float('inf'), float('-inf')):
        return None
    if v == 0:
        return '-0.0' if raw[7] & 0x80 else '0.0'
    t = repr(v)
    if not any(c in t for c in '.eE') and 'n' not in t:
        t += '.0'
    return t


def fmt_int(v, ty):
    if ty.signed:
        if v < 0:
            if ty.size == 4 and v == -2 ** 31:
                return '(-2147483647 - 1)'
            return str(v)
        return str(v) if v < 100000 else '0x%x' % v
    return str(v) if v < 10 else ('%d' % v if v < 1000 else '0x%x' % v)


class Emitter:
    """Turn bytes of the executable into a C initialiser of a given type."""

    def __init__(self, env, syms, order, funcdecls):
        self.env = env
        self.pe = env.pe
        self.syms = syms
        self.order = order
        self.addrs = [s.addr for s in order]
        self.funcs = {a: n for a, n, k in env.rows if k in ('func', 'lib')}
        self.funcdecls = funcdecls          # name -> parsed Func type key
        self.literals = []                  # (symbol, address, text)
        self.problems = []                  # (symbol, text)
        self.cur = None
        self.used_syms = set()
        self.sym_refs = {}                   # symbol -> names of the objects its initialiser points into
        self.func_refs = {}                  # symbol -> names of the functions its initialiser points to
        self.objs = []
        self.obj_addrs = []
        self.absolute = []                  # (symbol, console address) written as 0: D.ABSOLUTE
        self.limit = None                   # bytes from here on are not the symbol's (declared bigger than it is)
        self.partial = {}                   # symbol -> [(start address, end address)] not representable

    # -- pointer targets

    def sym_at(self, t):
        i = bisect.bisect_right(self.addrs, t) - 1
        return self.order[i] if i >= 0 else None

    def path(self, ty, off):
        """('.f[2]', type) reaching exactly `off` inside ty, or None."""
        if off == 0 and ty.kind != 'rec' and not (ty.kind == 'arr'):
            return '', ty
        if ty.kind == 'arr':
            if not ty.elem.size:
                return None
            i, r = divmod(off, ty.elem.size)
            if ty.n is not None and i >= ty.n:
                return None
            sub = self.path(ty.elem, r)
            if sub is None:
                return None
            return '[%d]%s' % (i, sub[0]), sub[1]
        if ty.kind == 'rec':
            for f in ty.fields:
                if f.offset <= off < f.offset + f.ty.size or (ty.union is False and off == f.offset and f.ty.size == 0):
                    if f.name is None:
                        sub = self.path(f.ty, off - f.offset)
                        return sub
                    sub = self.path(f.ty, off - f.offset)
                    if sub is None:
                        return None
                    return '.%s%s' % (f.name, sub[0]), sub[1]
            return None
        return None

    def literal_at(self, t):
        b = self.pe.read(t, 512)
        n = b.find(b'\0')
        if n < 0:
            return None
        return b[:n]

    def set_objects(self, objs):
        """The symbols the generated files define (they have a chosen type and size)."""
        self.objs = sorted(objs, key=lambda x: x.addr)
        self.obj_addrs = [x.addr for x in self.objs]

    def object_at(self, t):
        """(symbol, offset) of the defined object containing address t, or None."""
        i = bisect.bisect_right(self.obj_addrs, t) - 1
        while i >= 0:
            sy = self.objs[i]
            if t < sy.addr + self.decl_size(sy):
                return sy, t - sy.addr
            if sy.addr + self.decl_size(sy) <= t and i > 0 and self.objs[i - 1].addr + self.decl_size(self.objs[i - 1]) <= sy.addr:
                break
            i -= 1
        return None

    def pointer(self, t, pty):
        """Expression for the address `t` stored in a field of pointer type pty."""
        spell = spell_type(pty) if pty is not None else None
        if t in self.funcs:
            name = self.funcs[t]
            self.used_syms.add(name)
            self.func_refs.setdefault(self.cur, set()).add(name)
            if pty is not None and pty.to.kind == 'func':
                have = self.funcdecls.get(name)
                if have is not None and have == type_key(pty.to):
                    return name
                return '(%s)%s' % (spell, name) if spell else name
            return '(%s)%s' % (spell, name) if spell else name
        hit = self.object_at(t)
        if hit is not None:
            sy, off = hit
            self.used_syms.add(sy.name)
            self.sym_refs.setdefault(self.cur, set()).add(sy.name)
            if sy.alias:
                self.problems.append((self.cur, 'pointer into %s, which is a name inside another object' % sy.name))
            if sy.ty is not None:
                pr = self.path(sy.ty, off) if off else ('', sy.ty)
                if pr is not None:
                    suf, fin = pr
                    expr, ety = self.address_of(sy, suf, fin)
                    return self.cast_to(expr, ety, pty)
            if off == 0:
                return '&' + sy.cname
            return '(char *)&%s + %d' % (sy.cname, off)
        # string literal or unknown data
        sec = self.pe.section_of(t)
        if sec is not None and sec.name in ('.data', '.rodata'):
            lit = self.literal_at(t)
            if lit is not None and all(32 <= c < 127 or c in (9, 10, 13) for c in lit):
                self.literals.append((self.cur, t, lit))
                return c_string(lit)
        # one past the end of an object
        i = bisect.bisect_left(self.obj_addrs, t) - 1
        if i >= 0 and self.objs[i].addr + self.decl_size(self.objs[i]) == t:
            sy = self.objs[i]
            self.sym_refs.setdefault(self.cur, set()).add(sy.name)
            self.problems.append((self.cur, 'pointer one past the end of %s (0x%x)' % (sy.name, t)))
            return '(%s)((char *)&%s + %d)' % (spell or 'void *', sy.cname, self.decl_size(sy))
        if self.cur in D.ABSOLUTE:
            self.absolute.append((self.cur, t))
            return '0'
        self.problems.append((self.cur, 'pointer to 0x%x outside any defined object' % t))
        return '(%s)0x%x' % (spell or 'void *', t)

    def decl_size(self, sy):
        if sy.ty is not None:
            if sy.size:
                return sy.size
            return sy.ty.size if not (sy.ty.kind == 'arr' and sy.ty.n is None) else sy.extent
        return sy.extent

    def address_of(self, sy, suf, fin):
        name = sy.cname + suf
        if fin.kind == 'arr':
            return name, fin          # array decays to element pointer
        return '&' + name, fin

    def cast_to(self, expr, ety, pty):
        if pty is None:
            return expr
        pt = pty.to
        e = ety.elem if ety.kind == 'arr' else ety
        if pt.kind == 'void' or type_key(pt) == type_key(e):
            return expr
        spell = spell_type(pty)
        if spell is None:
            return expr
        return '(%s)%s' % (spell, expr)

    # -- values

    def raw(self, addr, n):
        b = self.pe.read(addr, n)
        if self.limit is not None and addr + n > self.limit:
            b = b[:max(self.limit - addr, 0)] + b'\0' * (addr + n - max(self.limit, addr))
        return b

    def has_reloc(self, addr, n):
        return any(a in self.env.relocs for a in range(addr - 3, addr + n))

    def value(self, ty, addr, ind):
        k = ty.kind
        if k == 'int':
            v = int.from_bytes(self.raw(addr, ty.size), 'little', signed=ty.signed)
            if addr in self.env.relocs and ty.size == 4:
                tgt = self.pe.u32(addr)
                self.problems.append((self.cur, 'integer field at +0x%x holds the address 0x%x' % (addr, tgt)))
            return fmt_int(v, ty)
        if k == 'float':
            r = self.raw(addr, ty.size)
            f = fmt_float(r, ty.size)
            if f is None:
                self.problems.append((self.cur, 'non-finite float %s at +0x%x' % (r.hex(), addr)))
                return '0.0'
            return f
        if k == 'ptr':
            t = self.pe.u32(addr)
            if self.limit is not None and addr >= self.limit:
                return '0'
            if addr in self.env.relocs or (t != 0 and self.pe.in_image(t)):
                return self.pointer(t, ty)
            if t == 0:
                return '0'
            self.problems.append((self.cur, 'pointer field holds %#x, which is no address of the image' % t))
            return '(%s)0x%x' % (spell_type(ty) or 'void *', t)
        if k == 'arr':
            return self.array(ty, addr, ind)
        if k == 'rec':
            return self.record(ty, addr, ind)
        raise ValueError('cannot emit %r' % ty)

    def array(self, ty, addr, ind):
        n = ty.n
        e = ty.elem
        raw = self.raw(addr, n * e.size)
        if e.kind == 'int' and e.spelling in ('char', 'unsigned char') and not any(a in self.env.relocs for a in range(addr, addr + len(raw))):
            z = raw.find(b'\0')
            body = raw if z < 0 else raw[:z]
            if z != 0 and body and all(32 <= c < 127 or c in (9, 10, 13) for c in body) \
                    and (z < 0 or not any(raw[z:])) and (len(body) >= 3 or z >= 0 and len(body) >= 1 and len(raw) <= 16):
                return c_string(body)
        if not any(raw) and not any(a in self.env.relocs for a in range(addr, addr + len(raw))):
            return '{ 0 }' if e.kind != 'int' or True else '0'
        items = [self.value(e, addr + i * e.size, ind + 1) for i in range(n)]
        return self.layout_items(items, e, ind)

    def layout_items(self, items, e, ind):
        pad = '    ' * (ind + 1)
        one = '{ ' + ', '.join(items) + ' }'
        if len(one) + len(pad) <= 100 and '\n' not in one:
            return one
        lines = []
        if e.kind in ('int', 'float', 'ptr') and not any('\n' in x for x in items):
            cur = pad
            for it in items:
                piece = it + ', '
                if len(cur) + len(piece) > 100 and cur.strip():
                    lines.append(cur.rstrip())
                    cur = pad
                cur += piece
            if cur.strip():
                lines.append(cur.rstrip())
        else:
            for it in items:
                lines.append(pad + it + ',')
        if lines and lines[-1].endswith(','):
            lines[-1] = lines[-1][:-1] if e.kind not in ('int', 'float', 'ptr') else lines[-1]
        return '{\n' + '\n'.join(lines) + '\n' + '    ' * ind + '}'

    def record(self, ty, addr, ind):
        if not ty.complete:
            raise ValueError('incomplete %r' % ty)
        if ty.union:
            f0 = ty.fields[0]
            raw = self.raw(addr, ty.size)
            body = self.value(f0.ty, addr, ind + 1)
            first = f0.ty.size
            if any(raw[first:]) or self.has_reloc_range(addr + first, ty.size - first):
                self.problems.append((self.cur, 'union %r holds bytes beyond its first member' % ty))
                self.partial.setdefault(self.cur, []).append((addr + first, addr + ty.size))
            return '{ ' + body + ' }' if not body.startswith('{') else '{ ' + body + ' }'
        items = []
        for f in ty.fields:
            items.append(self.value(f.ty, addr + f.offset, ind + 1))
        if not items:
            return '{ 0 }'
        raw = self.raw(addr, ty.size)
        if not any(raw) and not self.has_reloc_range(addr, ty.size):
            return '{ 0 }'
        return self.layout_items(items, ty, ind)

    def has_reloc_range(self, addr, n):
        return any(a in self.env.relocs for a in range(addr, addr + n))



# --------------------------------------------------------------------------
# sizes, declarations text, attribution to source files


def decl_text(qual, name, count, tys=None):
    """`qual` is clang's spelling of a type; return the declaration of `name` of that type."""
    if tys is not None:
        def anon(m):
            node = tys.anon.get(m.group(1))
            nm = tys.owned.get(node['id']) if node is not None else None
            if nm is None and node is not None and node.get('_span'):
                f, b, e = node['_span']
                body = open(f, 'rb').read()[b:e].decode('latin-1')
                return re.sub(r'\s+', ' ', body)
            return nm or m.group(0)
        qual = re.sub(r'(?:struct|union) \((?:unnamed|anonymous)[^)]*? at ([^)]*)\)', anon, qual)
    if count is not None:
        qual = qual.replace('[]', '[%d]' % count, 1)
    if '(*' in qual:
        i = qual.index('(*') + 2
        return qual[:i] + name + qual[i:]
    m = re.search(r'\[', qual)
    if m:
        head, tail = qual[:m.start()], qual[m.start():]
    else:
        head, tail = qual, ''
    head = head.rstrip()
    sep = '' if head.endswith('*') else ' '
    return head + sep + name + tail


def printable_string(b):
    z = b.find(b'\0')
    if z <= 0:
        return None
    body = b[:z]
    if all(32 <= c < 127 or c in (9, 10, 13) for c in body):
        return body
    return None


def sibling_decl(sy, syms):
    """Undeclared `fooN`: reuse the declaration of a declared `fooM`."""
    m = re.match(r'^(.*?)(\d+)$', sy.name)
    if not m:
        return None
    for other in syms.values():
        if other is not sy and other.decl is not None and re.match(r'^%s\d+$' % re.escape(m.group(1)), other.name):
            return other
    return None


def constant_indices():
    """name -> {index: (file, line)} for `name[<literal>]` outside declarations in game/src."""
    found = collections.defaultdict(dict)
    pat = re.compile(r'\b([A-Za-z_]\w*)\[(\d+)\]')
    for f in sorted(glob.glob(os.path.join(ROOT, 'game', 'src', '*.c'))):
        for ln, line in enumerate(open(f, encoding='latin-1'), 1):
            for m in pat.finditer(line):
                before = line[:m.start()]
                if re.match(r'^\s*(?:(?:extern|static|const|unsigned|signed|struct|union)\s+)*[A-Za-z_][\w ]*[\s\*]*$', before) \
                        and before.strip():
                    continue            # a declaration
                if before.lstrip().startswith('extern') or before.rstrip().endswith('&'):
                    continue            # declarator lists, one-past-the-end addresses
                found[m.group(1)].setdefault(int(m.group(2)), (os.path.basename(f), ln))
    return found


def finalize(env, syms, order):
    pe = env.pe
    const_idx = constant_indices()
    for sy in order:
        lib = (sy.kind == 'text' or sy.name.startswith('_'))
        if lib:
            sy.lib = True
            continue
        if sy.ty is None:
            sib = sibling_decl(sy, syms)
            if sib is not None:
                sy.decl, sy.ty, sy.qual = None, sib.ty, sib.qual
                sy.sibling = sib
                sy.notes.append('type taken from %s (no declaration)' % sib.name)
            else:
                sy.inferred = True
                if sy.kind == 'rdata' and sy.extent in (4, 8):
                    sy.ty = Flt(sy.extent)
                    sy.qual = 'const float' if sy.extent == 4 else 'const double'
                elif sy.extent in (1, 2, 4):
                    sy.ty = BUILTIN[{1: 'unsigned char', 2: 'unsigned short', 4: 'unsigned int'}[sy.extent]]
                    sy.qual = sy.ty.spelling
                else:
                    sy.ty = Arr(BUILTIN['unsigned char'], sy.extent)
                    sy.qual = 'unsigned char[%d]' % sy.extent
                continue
        t = sy.ty
        if sy.name in D.SIZES and t.kind != 'arr':
            # a scalar the decisions table makes an array (its other elements have names of their own)
            n = D.SIZES[sy.name][0]
            sy.ty = Arr(t, n)
            sy.qual = sy.qual + '[]'
            sy.size, sy.count = n * t.size, n
            sy.notes.append('%d entries: %s' % (n, D.SIZES[sy.name][1]))
            continue
        if sy.name in D.SIZES and t.kind == 'arr' and t.n is not None:
            # a complete array the decisions table makes longer (names that are parts of it follow)
            n = D.SIZES[sy.name][0]
            sy.ty = Arr(t.elem, n)
            sy.qual = re.sub(r'\[\d+\]', '[%d]' % n, sy.qual, count=1)
            sy.size, sy.count = n * t.elem.size, None
            sy.notes.append('%d entries: %s' % (n, D.SIZES[sy.name][1]))
            continue
        # array whose length comes from the extent
        if t.kind == 'arr' and t.n is None:
            e = t.elem
            n = sy.extent // e.size if e.size else 0
            if e.kind == 'int' and e.spelling in ('char', 'unsigned char') and sy.kind in ('data', 'rdata'):
                s = printable_string(pe.read(sy.addr, min(sy.extent, 4096)))
                if s is not None:
                    tail = pe.read(sy.addr + len(s) + 1, max(sy.extent - len(s) - 1, 0))
                    if len(tail) >= 4 and not any(tail):
                        sy.notes.append('buffer holding a string')      # written by the game: keep the extent
                    else:
                        n = len(s) + 1
                        while n % 4 and n < sy.extent and pe.read(sy.addr + n, 1) == b'\0':
                            n += 1                 # the zero padding up to the next word belongs to it
                        sy.notes.append('string')
            if e.kind == 'arr' and e.n:
                n = sy.extent // e.size
            if sy.kind in ('data', 'rdata') and e.kind in ('rec', 'int', 'float', 'arr') and n > 0:
                k = valid_prefix(env, e, sy.addr, n)
                if k < n:
                    sy.notes.append('table of %d entries; the rest of the extent holds other data' % k)
                    n = max(k, 1)
            if e.kind == 'ptr' and sy.kind in ('data', 'rdata'):
                k = 0
                while k < n and (sy.addr + 4 * k in env.relocs or pe.u32(sy.addr + 4 * k) == 0):
                    k += 1
                while k > 1 and pe.u32(sy.addr + 4 * (k - 1)) == 0 and sy.addr + 4 * (k - 1) not in env.relocs:
                    k -= 1
                if k < n:
                    sy.notes.append('table of %d pointers; the rest of the extent holds other data' % k)
                    n = k
            if sy.name in D.SIZES:
                n = D.SIZES[sy.name][0]
                sy.notes.append('%d entries: %s' % (n, D.SIZES[sy.name][1]))
            elif n <= 0:
                n = 1
                sy.notes.append('extent smaller than one element')
            hi = max(const_idx.get(sy.name, {-1}))
            if hi >= n and not (e.kind == 'int' and e.spelling == 'char'):
                f, ln = const_idx[sy.name][hi]
                sy.notes.append('indexed up to [%d] at %s:%d' % (hi, f, ln))
                sy.extended = hi + 1
                n = hi + 1
            sy.ty = Arr(e, n)
            sy.size = n * e.size
            sy.count = n
        else:
            sy.size = t.size
            sy.count = None


def pointer_slots(ty, base=0):
    """Offsets of the pointer members of a type."""
    if ty.kind == 'ptr':
        return {base}
    if ty.kind == 'arr':
        out = set()
        for i in range(ty.n or 0):
            out |= pointer_slots(ty.elem, base + i * ty.elem.size)
        return out
    if ty.kind == 'rec':
        out = set()
        for f in ty.fields:
            out |= pointer_slots(f.ty, base + f.offset)
        return out
    return set()


def valid_prefix(env, e, addr, n):
    """How many leading elements look like values of type e (pointers are relocations or null)."""
    slots = pointer_slots(e)
    pe = env.pe
    for i in range(n):
        a = addr + i * e.size
        for off in range(0, e.size):
            r = (a + off) in env.relocs
            if off in slots:
                if not r and pe.u32(a + off) != 0:
                    return i
            elif r:
                return i
    return n




def lnds(seq):
    """Indices of a longest non-decreasing subsequence of the list of ints."""
    tails = []        # index of the smallest tail for each length
    prev = [-1] * len(seq)
    for i, v in enumerate(seq):
        lo, hi = 0, len(tails)
        while lo < hi:
            mid = (lo + hi) // 2
            if seq[tails[mid]] <= v:
                lo = mid + 1
            else:
                hi = mid
        if lo > 0:
            prev[i] = tails[lo - 1]
        if lo == len(tails):
            tails.append(i)
        else:
            tails[lo] = i
    out = []
    i = tails[-1] if tails else -1
    while i >= 0:
        out.append(i)
        i = prev[i]
    return set(out)


def link_order(env, section):
    """Source modules in the order the linker script places their `section` (.rodata/.data/.bss)."""
    out = []
    for line in open(os.path.join(env.decomp, 'SCUS_943.51.ld')):
        m = re.search(r'build/src/(\w+)\.c\.o\(%s\)' % re.escape(section), line)
        if m:
            out.append(m.group(1))
    return out


def source_words():
    """module -> set of identifiers the file's text mentions."""
    out = {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'game', 'src', '*.c'))):
        out[os.path.splitext(os.path.basename(f))[0]] = set(re.findall(r'[A-Za-z_]\w*', open(f, encoding='latin-1').read()))
    return out


def attribute(env, scan, syms, order):
    """Assign each symbol to the source file whose object file defined it."""
    words = source_words()
    refs_of = {}
    for sy in order:
        refs_of[sy.name] = {m for m, w in words.items() if sy.cname in w}
    regions = {'rdata': link_order(env, '.rodata'), 'data': link_order(env, '.data'), 'bss': link_order(env, '.bss')}
    for region in ('rdata', 'data', 'bss'):
        mods = regions[region]
        index = {m: i for i, m in enumerate(mods)}
        members = [s for s in order if s.kind == region]
        refs = {s.name: {index[m] for m in refs_of[s.name] if m in index} for s in members}
        if region == 'bss':
            # the uninitialised data is linked in an order of its own: take the assignment with the
            # fewest changes of file that agrees with who uses what
            user = [refs[s.name] for s in members]
            cost = {m: 0 for m in range(len(mods))}
            back = []
            for i, s in enumerate(members):
                new = {}
                choice = {}
                best_prev = min(cost, key=lambda m: cost[m])
                for m in cost:
                    mism = 0 if (not user[i] or m in user[i]) else 3
                    stay = cost[m]
                    move = cost[best_prev] + 1
                    if stay <= move:
                        new[m], choice[m] = stay + mism, m
                    else:
                        new[m], choice[m] = move + mism, best_prev
                back.append(choice)
                cost = new
            m = min(cost, key=lambda k: cost[k])
            for i in range(len(members) - 1, -1, -1):
                members[i].run = m
                m = back[i][m]
        else:
            anchors = [(i, next(iter(refs[s.name]))) for i, s in enumerate(members) if len(refs[s.name]) == 1]
            good = lnds([r for i, r in anchors])
            good_idx = {anchors[k][0]: anchors[k][1] for k in good}
            gi = sorted(good_idx)
            for i, s in enumerate(members):
                if i in good_idx:
                    s.run = good_idx[i]
                    continue
                k = bisect.bisect_left(gi, i)
                rp = good_idx[gi[k - 1]] if k > 0 else None
                rn = good_idx[gi[k]] if k < len(gi) else None
                if rp is None:
                    rp = rn
                if rn is None:
                    rn = rp
                r = refs[s.name]
                if rp in r:
                    s.run = rp
                elif rn in r:
                    s.run = rn
                elif k > 0 and k < len(gi) and (s.addr - members[gi[k - 1]].addr) > (members[gi[k]].addr - s.addr):
                    s.run = rn
                else:
                    s.run = rp
        for s in members:
            s.module = mods[s.run] if mods else 'main'
            s.refs = sorted(refs_of[s.name])
    return regions


# --------------------------------------------------------------------------
# output


_closure_memo = {}


def includes_of(header):
    """Set of normalised paths a header pulls in (itself included)."""
    if header not in _closure_memo:
        r = subprocess.run(['clang', '-M', '-w', '-isystem', STUBS, '-include', os.path.join(ROOT, 'platform', 'include', 'port.h'),
                            '-I' + os.path.join(ROOT, 'game', 'include'), '-x', 'c', header] + TARGET32,
                           capture_output=True, text=True)
        _closure_memo[header] = {os.path.normpath(t) for t in r.stdout.replace('\\\n', ' ').split()[1:]}
    return _closure_memo[header]


def header_closure():
    """Headers that functions.h pulls in."""
    return includes_of(os.path.join(ROOT, 'game', 'include', 'functions.h'))


def symbol_comment(sy, em):
    out = []
    if sy.synthetic:
        out.append('Port: %s' % sy.synthetic)
    if sy.name in D.ARENAS:
        out.append('Port: %s; the original size is for 4-byte pointers.' % D.ARENAS[sy.name])
    if sy.name in D.TRUNCATE:
        out.append('Port: %s' % D.TRUNCATE[sy.name])
    if sy.name in D.ALIGN:
        out.append('Port: %s' % D.ALIGN[sy.name][1])
    if sy.name in D.SIZES:
        out.append('Port: %s' % D.SIZES[sy.name][1])
    if sy.name in D.ABSOLUTE:
        out.append('Port: %s' % D.ABSOLUTE[sy.name])
    if sy.name in em.partial:
        out.append('C89 initialises only the first member of the union; the executable holds more data after it.')
    if sy.inferred and sy.decl is None:
        out.append('No declaration in the game sources: type inferred from the executable.')
    return ''.join('/* %s */\n' % c for c in out)


def symbol_text(sy, em, tys):
    """The definition of one symbol."""
    em.cur = sy.name
    decl_tys = sy.decl.tys if sy.decl is not None else None
    count = getattr(sy, 'count', None)
    if sy.synthetic:
        count = sy.count
    if 'string' in sy.notes:
        lit = printable_string(em.pe.read(sy.addr, 4096))
        if lit is not None and count == len(lit) + 1:
            count = None            # the compiler counts the NUL
    text = decl_text(sy.qual, sy.cname, count, decl_tys)
    if sy.name in D.ARENAS:
        text = text.replace('[%d]' % count, '[%d * (sizeof(void *) / 4)]' % count)
    align = max([d.aligned for d in sy.decls] + [D.ALIGN[sy.name][0] if sy.name in D.ALIGN else 0])
    if align:
        text += ' __attribute__((aligned(%d)))' % align
    if sy.kind == 'bss':
        return symbol_comment(sy, em) + text + ';'
    em.limit = sy.addr + sy.extent if sy.name in D.TRUNCATE else None
    try:
        val = em.value(sy.ty, sy.addr, 0)
    finally:
        em.limit = None
    note = symbol_comment(sy, em)
    return '%s%s = %s;' % (note, text, val)


def generate(env, scan, quiet=False):
    packs = pack_table()
    tys = {tu: Types(nodes, packs) for tu, nodes in scan.items()}
    syms, order = build_symbols(env, scan, tys)
    choose_types(syms, order, None, access_widths(env))
    finalize(env, syms, order)
    runs = attribute(env, scan, syms, order)
    funcdecls = {}
    for tu, nodes in scan.items():
        for n in nodes:
            if n['kind'] == 'FunctionDecl' and n['name'] not in funcdecls:
                try:
                    t = tys[tu].parse(n['type']['qualType'])
                    funcdecls[n['name']] = type_key(t)
                except Exception:      # noqa
                    pass
    em = Emitter(env, syms, order, funcdecls)
    modules = collections.OrderedDict()
    results = {}
    g_synth = {}
    skipped = {'defined': [], 'lib': [], 'undeclared': [], 'alias': []}
    for sy in order:
        sy.alias = sy.name in ALIAS_NAMES
    gen = []
    for sy in order:
        if sy.alias:
            skipped['alias'].append(sy)
            continue
        if sy.lib:
            skipped['lib'].append(sy)
            continue
        if sy.decl is None:
            skipped['undeclared'].append(sy)
            continue
        if any(d.has_init and not d.static for d in sy.decls):
            skipped.setdefault('initialised', []).append(sy)     # game/src defines it with its data
            continue
        if sy.defined_in_game:
            skipped['defined'].append(sy)       # still defined here: tools/gendata.py --externalize drops the game's
        gen.append(sy)
    em.set_objects(gen)
    for sy in gen:
        try:
            text = symbol_text(sy, em, tys)
        except Exception as e:           # noqa
            em.problems.append((sy.name, 'cannot generate: %s' % e))
            continue
        uses = set(sy.decl.uses) if sy.decl is not None else set()
        sy.uses = uses
        sy.use_nodes = dict(sy.decl.use_nodes) if sy.decl is not None else {}
        results[sy.name] = text
        modules.setdefault(sy.module, []).append(sy)
    for name, addr, ctype, count, module, why in D.SYNTHETIC:
        sy = Sym(addr, name)
        sy.kind, sy.module, sy.extent = 'data', module, count
        sy.ty = Arr(BUILTIN[ctype], count)
        sy.size, sy.count, sy.qual = sy.ty.size, count, '%s[]' % ctype
        sy.uses, sy.use_nodes, sy.synthetic = set(), {}, why
        em.cur = name
        results[name] = symbol_text(sy, em, tys)
        modules.setdefault(module, []).append(sy)
        g_synth[name] = sy
    header_typedefs = {n['name'] for nodes in scan.values() for n in nodes
                       if n['kind'] == 'TypedefDecl' and '/include/' in (n['_where'][0] or '')}
    return dict(syms=syms, order=order, em=em, modules=modules, results=results, runs=runs, tys=tys,
                skipped=skipped, header_typedefs=header_typedefs, synthetic=g_synth, funcdecls=funcdecls, scan=scan)


def source_includes(module):
    """The #include lines of game/src/<module>.c."""
    out = []
    for line in open(os.path.join(ROOT, 'game', 'src', module + '.c'), encoding='latin-1'):
        if re.match(r'\s*#\s*include\b', line):
            out.append(line.strip())
        elif line.strip() and not line.lstrip().startswith(('/*', '*', '//')) and out:
            break
    return out


def private_text(nodes, header_typedefs=()):
    """Verbatim source of the type definitions that only a .c file provides."""
    spans = {}
    for n in nodes.values():
        f = n['_where'][0]
        if n['kind'] == 'TypedefDecl' and n['name'] in header_typedefs:
            continue                    # the header's typedef is already visible
        if f and '/game/src/' in f and n.get('_span'):
            spans[n['id']] = n['_span']
    items = sorted(set(spans.values()))
    keep = []
    for sp in items:
        if not any(o != sp and o[0] == sp[0] and o[1] <= sp[1] and sp[2] <= o[2] for o in items):
            keep.append(sp)
    out = []
    for f, b, e in keep:
        src = open(f, 'rb').read()
        text = src[b:e].decode('latin-1')
        if not text.rstrip().endswith(';'):
            text += ';'
        out.append(text)
    return out


def write_files(g, outdir):
    os.makedirs(outdir, exist_ok=True)
    for f in glob.glob(os.path.join(outdir, '*_data.c')):
        os.remove(f)
    closure = header_closure()
    written = []
    for module, members in sorted(g['modules'].items()):
        members.sort(key=lambda s: s.addr)
        name = '%s_data.c' % module
        header_lines = ['#include "functions.h"']
        covered = closure
        extra = []
        privnodes = {}
        for sy in members:
            privnodes.update(sy.use_nodes)
            for u in sy.uses:
                if not u:
                    continue
                if '/game/include/' in u and os.path.normpath(u) not in covered:
                    extra.append(os.path.relpath(u, os.path.join(ROOT, 'game', 'include')))
        have = ' '.join(header_lines)
        lines = ['/* Global data of %s.c, generated from the executable by tools/gendata.py. */' % module]
        lines += header_lines
        # objects and functions the initialisers point to
        em = g['em']
        local = set()
        fwd = []
        fwd_funcs = set()
        for sy in members:
            for rn in sorted(em.sym_refs.get(sy.name, ())):
                tgt = g['syms'][rn]
                if rn not in local and tgt.decl is not None and not tgt.synthetic and not (tgt.module == module and tgt.addr < sy.addr):
                    fwd.append(tgt)
                    privnodes.update(tgt.decl.use_nodes)
                    for u in tgt.decl.uses:
                        if u and '/game/include/' in u and os.path.normpath(u) not in covered:
                            extra.append(os.path.relpath(u, os.path.join(ROOT, 'game', 'include')))
            for fn in sorted(em.func_refs.get(sy.name, ())):
                if fn not in g['funcdecls']:
                    fwd_funcs.add(fn)
            local.add(sy.name)
        for h in sorted(set(extra)):
            if '"%s"' % h not in have:
                lines.append('#include "%s"' % h)
                have += ' "%s"' % h
        priv = private_text(privnodes, g['header_typedefs'])
        if priv:
            lines.append('')
            lines.append('/* Types that %s.c declares privately. */' % module)
            lines += priv
        decls = []
        seen_f = set()
        for tgt in fwd:
            if tgt.name in seen_f:
                continue
            seen_f.add(tgt.name)
            decls.append('extern ' + decl_text(tgt.qual, tgt.cname, getattr(tgt, 'count', None), tgt.decl.tys) + ';')
        for fn in sorted(fwd_funcs):
            decls.append('extern void %s();' % fn)
        if decls:
            lines.append('')
            lines += decls
        lines.append('')
        for sy in members:
            lines.append(g['results'][sy.name])
        open(os.path.join(outdir, name), 'w').write('\n'.join(lines) + '\n')
        written.append(name)
    return written


# --------------------------------------------------------------------------
# verification: compile, then compare object bytes and relocations with the executable


class Elf:
    """Sections, symbols and REL relocations of a 32-bit little-endian ELF object file."""

    def __init__(self, path):
        d = self.d = open(path, 'rb').read()
        assert d[:4] == b'\x7fELF' and d[4] == 1 and d[5] == 1, 'not a 32-bit little-endian ELF object'
        shoff, = struct.unpack_from('<I', d, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2e)
        raw = []
        for i in range(shnum):
            name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from('<IIIIIIIIII', d, shoff + i * shentsize)
            raw.append((name, typ, flags, addr, off, size, link, info, align, entsize))
        shstr = d[raw[shstrndx][4]:raw[shstrndx][4] + raw[shstrndx][5]]
        self.sections = []
        for name, typ, flags, addr, off, size, link, info, align, entsize in raw:
            nm = shstr[name:shstr.index(b'\0', name)].decode('latin-1')
            data = b'\0' * size if typ == 8 else d[off:off + size]       # SHT_NOBITS
            self.sections.append(dict(name=nm, type=typ, flags=flags, size=size, data=data, link=link, info=info,
                                      entsize=entsize, relocs=[], align=align))
        self.symbols = []       # index -> (name, value, shndx, binding, type, size)
        for sec in self.sections:
            if sec['type'] == 2:        # SHT_SYMTAB
                strtab = self.sections[sec['link']]['data']
                for k in range(sec['size'] // 16):
                    nm, val, sz, info, other, shndx = struct.unpack_from('<IIIBBH', sec['data'], 16 * k)
                    name = strtab[nm:strtab.index(b'\0', nm)].decode('latin-1')
                    self.symbols.append((name, val, shndx, info >> 4, info & 15, sz))
        for sec in self.sections:
            if sec['type'] == 9:        # SHT_REL
                tgt = self.sections[sec['info']]
                for k in range(sec['size'] // 8):
                    off, info = struct.unpack_from('<II', sec['data'], 8 * k)
                    tgt['relocs'].append((off, info >> 8, info & 255))


def native_cmd(path, out):
    return ['clang', '-c', '-w', '-fsigned-char', '-include', os.path.join(ROOT, 'platform', 'include', 'port.h'),
            '-I' + os.path.join(ROOT, 'game', 'include'), '-o', out, path]


def compile_cmd(path, out, native):
    if native:
        return native_cmd(path, out)
    return (['clang'] + TARGET32 + ['-c', '-w', '-fdata-sections', '-isystem', STUBS,
            '-include', os.path.join(ROOT, 'platform', 'include', 'port.h'),
            '-I' + os.path.join(ROOT, 'game', 'include'), '-o', out, path])


R_ARM_ABS32 = 2


def compare_symbol(env, g, res, bad, co, name, sec, start, size, tag, lim=None):
    """Bytes and relocations of one defined symbol of an object against the executable."""
    pe = env.pe
    names = env.names
    addr = names[name][0]
    lim = size if lim is None else lim         # bytes from here on belong to the next global (TRUNCATE)
    res['%s: symbols checked' % tag] += 1
    res['%s: bytes compared' % tag] += size
    exp = pe.read(addr, size)
    got = sec['data'][start:start + size]
    masked = set()
    my = {}
    for off_, si, ty in sec['relocs']:
        if not start <= off_ < start + size:
            continue
        if ty != R_ARM_ABS32:
            bad.append('%s (%s): relocation type %d' % (name, tag, ty))
            continue
        off = off_ - start
        addend = struct.unpack_from('<I', sec['data'], off_)[0]
        my[off] = (co.symbols[si], addend)
        masked.update(range(off, off + 4))
    for off in range(0, lim - 3):
        if (addr + off) in env.relocs and off not in my and name not in D.ABSOLUTE:
            bad.append('%s+0x%x (%s): executable has a pointer (value 0x%x), the object has none' % (
                name, off, tag, pe.u32(addr + off)))
    skip = set()
    if name in D.ABSOLUTE:
        for off in range(size - 3):
            if (addr + off) in env.relocs:
                skip.update(range(off, off + 4))
                res['%s: pointer words holding console RAM addresses (ABSOLUTE)' % tag] += 1
    for lo, hi in g['em'].partial.get(name, []):
        skip.update(range(lo - addr, hi - addr))
    res['%s: bytes not representable in C89 (union members)' % tag] += 0
    for lo_, hi_ in g['em'].partial.get(name, []):
        res['%s: bytes not representable in C89 (union members)' % tag] += hi_ - lo_
    for off in range(size):
        if off in masked or off in skip:
            continue
        if off >= lim:
            if got[off]:
                bad.append('%s+0x%x (%s): byte 0x%02x beyond the extent of a truncated definition' % (name, off, tag, got[off]))
                break
            continue
        if exp[off] != got[off]:
            bad.append('%s+0x%x (%s): byte 0x%02x, executable 0x%02x' % (name, off, tag, got[off], exp[off]))
            break
    for off, (tsym, addend) in sorted(my.items()):
        tname, tval, tshndx, tbind, ttype, tsize = tsym
        t = pe.u32(addr + off)
        if (addr + off) not in env.relocs and not (pe.in_image(t) and t != 0):
            bad.append('%s+0x%x (%s): object has a pointer, executable has plain value 0x%x' % (
                name, off, tag, t))
            continue
        if ttype == 3 or tname.startswith(('.L', '__const', '.str')) or (tbind == 0 and ttype in (0, 1) and tshndx):
            # a string literal (or another compiler-made constant): compare the bytes it denotes
            lit = co.sections[tshndx]['data']
            s0 = lit[tval + addend:]
            s0 = s0[:s0.index(b'\0') + 1] if b'\0' in s0 else s0
            ex = pe.read(t, len(s0))
            if ex != s0:
                bad.append('%s+0x%x (%s): string literal %r, executable has %r' % (name, off, tag, s0, ex))
            else:
                res['%s: pointers to string literals' % tag] += 1
            continue
        if tname not in names:
            bad.append('%s+0x%x (%s): pointer to %s, unknown to the symbol tables' % (name, off, tag, tname))
            continue
        want = names[tname][0] + addend
        if want != t:
            bad.append('%s+0x%x (%s): pointer to %s+%d = 0x%x, executable 0x%x' % (name, off, tag, tname, addend, want, t))
        else:
            res['%s: pointers to symbols' % tag] += 1


ALIAS_DIR = os.path.join(ROOT, 'tools', 'aliastest')


def alias_includes(a):
    return ['#include "%s"' % h for h in a.includes]


def write_alias_tests(env, g):
    """tools/aliastest/alias_*.c: for every alias, a function that checks (natively) that the alias
    storage lies inside its container, that paired elements are the same object and that the
    executable's initial bytes are visible through the alias; and the 32-bit offset assertions."""
    os.makedirs(ALIAS_DIR, exist_ok=True)
    for f in glob.glob(os.path.join(ALIAS_DIR, '*.c')):
        os.remove(f)
    asserts = []
    names = []
    for a in D.ALIASES:
        aa, ca = env.by_name[a.name][0], env.by_name[a.container][0]
        off = aa - ca
        sy = g['syms'][a.name]
        exp = env.pe.read(aa, min(max(sy.extent, 1), 16)) if sy.kind in ('data', 'rdata') else b''
        csy = g['syms'][a.container]
        cdecl = 'extern ' + decl_text(csy.qual, csy.cname, getattr(csy, 'count', None), csy.decl.tys if csy.decl else None) + ';'
        head = ['/* Generated by tools/gendata.py from tools/gendata_decisions.py: alias %s. */' % a.name,
                '#include <string.h>', '#include "functions.h"'] + alias_includes(a) + \
            [d for d in a.decls if not d.startswith('extern ')] + [cdecl] + ['#define %s %s' % (a.name, a.macro)]
        snippet32 = head + [
            '/* In the executable %s is at %s + 0x%x. */' % (a.name, a.container, off),
            'typedef char alias_offset_%s[((char *)&(%s) - (char *)&(%s) == 0x%x) ? 1 : -1];' % (
                a.name, a.expr, a.container, off), '']
        body = head + ['', 'int alias_%s(void)' % a.name, '{',
                       '    char *alias = (char *)&(%s);' % a.expr,
                       '    char *container = (char *)&(%s);' % a.container,
                       '    if (alias < container || alias + sizeof(%s) > container + sizeof(%s))' % (a.expr, a.container),
                       '        return 1;']
        if a.pair:
            body += ['    if ((char *)&(%s) != (char *)&(%s))' % a.pair, '        return 2;']
        if any(exp):
            body += ['    {', '        static const unsigned char expected[] = { %s };' % ', '.join(str(x) for x in exp),
                     '        unsigned int n = sizeof(%s) < sizeof(expected) ? sizeof(%s) : sizeof(expected);' % (a.expr, a.expr),
                     '        if (memcmp(alias, expected, n) != 0)', '            return 3;', '    }']
        body += ['    return 0;', '}', '']
        open(os.path.join(ALIAS_DIR, 'alias_%s.c' % a.name), 'w').write('\n'.join(body))
        asserts.append((a, snippet32))
        names.append(a.name)
    main = ['/* Generated by tools/gendata.py: runs the alias checks. Build and run with `tools/gendata.py --check`. */',
            '#include <stdio.h>', '']
    main += ['int alias_%s(void);' % n for n in names]
    main += ['', 'int main(void)', '{', '    int failures = 0, r;']
    for n in names:
        main += ['    r = alias_%s();' % n,
                 '    if (r) { printf("alias %s failed (%%d)\\n", r); failures++; }' % n]
    main += ['    printf("%d alias checks, %d failed\\n", ' + str(len(names)) + ', failures);', '    return failures != 0;', '}', '']
    open(os.path.join(ALIAS_DIR, 'alias_main.c'), 'w').write('\n'.join(main))
    return asserts


def mask_code(text):
    """`text` with comments and string/character literals blanked (same length)."""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
        elif text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            j += 1
        else:
            i += 1
            continue
        for k in range(i, min(j, n)):
            if out[k] != '\n':
                out[k] = ' '
        i = j
    return ''.join(out)


def matching_bracket(masked, i):
    depth = 0
    for j in range(i, len(masked)):
        if masked[j] == '[':
            depth += 1
        elif masked[j] == ']':
            depth -= 1
            if depth == 0:
                return j
    return -1


def rewrite_uses(text, aliases):
    """Replace the uses of the alias names in the code of one file (see Alias.use in the decisions)."""
    masked = mask_code(text)
    edits = []
    for a in aliases:
        use = a.use
        if isinstance(use, tuple) and use[0] == 'rewrite':
            continue
        if isinstance(use, tuple) and use[0] == 'index':
            for m in re.finditer(r'\b%s\s*\[' % re.escape(a.name), masked):
                lb = m.end() - 1
                rb = matching_bracket(masked, lb)
                inner = text[lb + 1:rb].strip()
                simple = re.fullmatch(r'\w+', inner) is not None
                n = use[2]
                idx = inner if not n else ('%d + %s' % (n, inner if simple else '(%s)' % inner))
                edits.append((m.start(), rb + 1, '%s[%s]' % (use[1], idx)))
        else:
            for m in re.finditer(r'(?<![\w.>])%s\b' % re.escape(a.name), masked):
                edits.append((m.start(), m.end(), use))
    for start, end, new in sorted(edits, reverse=True):
        text = text[:start] + new + text[end:]
    return text, len(edits)


def apply_aliases(dry=False):
    """Make the alias names of the decisions go away from game/src: their declarations are removed (the
    container is declared where a file needs it) and their uses become the container's fields or elements."""
    report = []
    header_text = {h: open(h, encoding='latin-1').read() for h in glob.glob(os.path.join(ROOT, 'game', 'include', '*.h'))}
    seen_by_all = header_closure()          # what functions.h includes
    for f in sorted(glob.glob(os.path.join(ROOT, 'game', 'src', '*.c'))):
        base = os.path.basename(f)
        text = open(f, encoding='latin-1').read()
        lines = text.split('\n')
        drop = {}
        for a in D.ALIASES:
            pat = re.compile(r'^(?!\s)(?!#)[^;{}()=]*\b%s\b(?:\[[^\]]*\])*\s*;\s*(?:/\*.*\*/)?\s*$' % re.escape(a.name))
            for i, line in enumerate(lines):
                if pat.match(line) and not line.lstrip().startswith(('return', 'if', 'else')):
                    drop[i] = a
        rest = '\n'.join(l for i, l in enumerate(lines) if i not in drop)
        used = [a for a in D.ALIASES if re.search(r'\b%s\b' % re.escape(a.name), mask_code(rest))]
        inserted = set()
        noted = set()
        out = []
        for i, line in enumerate(lines):
            if i not in drop:
                out.append(line)
                continue
            a = drop[i]
            if a.container not in noted:
                noted.add(a.container)
                names = sorted({u.name for u in used if u.container == a.container})
                if names:
                    out.append('/* Port: %s %s part%s of %s; the uses read %s directly. */' % (
                        ', '.join(names), 'is' if len(names) == 1 else 'are', '' if len(names) == 1 else 's', a.container,
                        'it' if len(names) == 1 else 'them'))
            declared = re.search(r'\b%s\b' % re.escape(a.container), mask_code(rest))
            for h, ht in header_text.items():
                if re.search(r'\bextern\b[^;]*\b%s\b' % re.escape(a.container), ht) and \
                        (os.path.normpath(h) in seen_by_all or '"%s"' % os.path.basename(h) in rest):
                    declared = True
            if a.container_decl and a.name in [u.name for u in used] and a.container not in inserted and not declared:
                inserted.add(a.container)
                out.append(a.container_decl)
                report.append('%s: %s -> %s' % (base, line.strip(), a.container_decl.replace('\n', ' ')))
            else:
                report.append('%s: removed %s' % (base, line.strip()))
        new = '\n'.join(out)
        new, n = rewrite_uses(new, used)
        if n:
            report.append('%s: %d uses rewritten (%s)' % (base, n, ' '.join(sorted({u.name for u in used}))))
        if new != text and not dry:
            open(f, 'w', encoding='latin-1').write(new)
    for a in D.ALIASES:
        if isinstance(a.use, tuple) and a.use[0] == 'rewrite':
            rules = a.use[1]
        else:
            continue
        for fname, old, new in rules:
            path = os.path.join(ROOT, 'game', 'src', fname)
            text = open(path, encoding='latin-1').read()
            if old in text:
                report.append('%s: %s -> %s' % (fname, old.strip(), new.strip()))
                if not dry:
                    open(path, 'w', encoding='latin-1').write(text.replace(old, new))
    for fname, old, new in D.REWRITES:
        path = os.path.join(ROOT, 'game', 'src', fname)
        text = open(path, encoding='latin-1').read()
        if old in text:
            report.append('%s: %s -> %s' % (fname, old.strip(), new.strip()))
            if not dry:
                open(path, 'w', encoding='latin-1').write(text.replace(old, new, 1))
    return report


def externalize(g, dry=False, root=ROOT):
    """Turn the tentative definitions of game/src that the data files define into extern declarations
    (without this the program has two definitions of each, and the game's would be zero-filled).
    Returns (changed lines, lines it could not handle)."""
    changed, problems = [], []
    byfile = collections.defaultdict(list)
    for sy in g['order']:
        if sy.name not in g['results']:
            continue
        for d in sy.decls:
            if not d.extern and not d.static and not d.invalid:
                byfile[d.tu].append((d.where[1], sy, d))
    for tu, items in sorted(byfile.items()):
        path = os.path.join(root, 'game', 'src', tu)
        lines = open(path, encoding='latin-1').read().split('\n')
        for ln, sy, d in sorted(items):
            line = lines[ln - 1]
            body = line.split('/*')[0].rstrip()
            if re.match(r'^\s*extern\b', line):
                continue
            if not body.endswith(';') or ',' in re.sub(r'\(.*?\)', '', body) or line.startswith((' ', '\t')) \
                    or '{' in body or '=' in body or ln != d.node['loc'].get('line', ln) and False:
                problems.append('%s:%d: %s' % (tu, ln, line.strip()))
                continue
            lines[ln - 1] = 'extern ' + line
            changed.append('%s:%d: %s' % (tu, ln, line.strip()))
        if not dry:
            open(path, 'w', encoding='latin-1').write('\n'.join(lines))
    return changed, problems


def decisions_markdown(env, g):
    """docs/GENDATA_DECISIONS.md: the decisions table, with the addresses of the executable."""
    def addr(n):
        return '0x%08X' % env.by_name[n][0] if n in env.by_name else '-'
    out = ['# Decisions about the executable\'s globals', '',
           'Generated by `tools/gendata.py --decisions` from `tools/gendata_decisions.py`; the evidence is what the',
           'machine code does with the addresses (`tools/gendata.py --evidence NAME` prints every access).', '',
           '## Names that stand for a part of another global (aliases)', '',
           'Their declarations are removed from `game/src` and their uses read the container directly',
           '(`tools/gendata.py --apply-aliases`).', '',
           '| name | address | container | address | offset | the same storage | evidence |', '|---|---|---|---|---|---|---|']
    for a in D.ALIASES:
        off = env.by_name[a.name][0] - env.by_name[a.container][0]
        out.append('| `%s` | %s | `%s` | %s | +%d | `%s` | %s |' % (a.name, addr(a.name), a.container, addr(a.container), off,
                                                                 a.use if isinstance(a.use, str) else a.macro, a.evidence))
    out += ['', '## Names the sources have since rewritten (no longer declared)', '',
            '| name | address | container | evidence |', '|---|---|---|---|']
    for n, (c, e) in D.RESOLVED.items():
        out.append('| `%s` | %s | `%s` | %s |' % (n, addr(n), c, e))
    out += ['', '## Declared bigger than the object (`TRUNCATE`)', '', '| name | address | extent | why |', '|---|---|---|---|']
    for n, why in D.TRUNCATE.items():
        out.append('| `%s` | %s | %d | %s |' % (n, addr(n), g['syms'][n].extent, why))
    out += ['', '## Lengths the extents cannot give (`SIZES`)', '', '| name | address | entries | why |', '|---|---|---|---|']
    for n, (k, why) in D.SIZES.items():
        out.append('| `%s` | %s | %d | %s |' % (n, addr(n), k, why))
    out += ['', '## Globals under a new name (`RENAMED`)', '', '| name | starts at | why |', '|---|---|---|']
    for n, (anchor, why) in D.RENAMED.items():
        out.append('| `%s` | `%s` %s | %s |' % (n, anchor, addr(anchor), why))
    out += ['', '## Preferred declarations (`PREFER`)', '']
    out += ['* `%s`: `%s`' % kv for kv in D.PREFER.items()]
    return '\n'.join(out) + '\n'


def link_stubs(objs, tmp):
    """C source defining every symbol the objects use but none defines: the game's and the library's
    functions that data points to."""
    r = subprocess.run(['nm', '-u'] + objs, capture_output=True, text=True)
    undefined = set()
    for line in r.stdout.splitlines():
        line = line.strip()
        if not line or line.endswith(':'):
            continue
        name = line.split()[-1]
        if name.startswith('_') and sys.platform == 'darwin':
            name = name[1:]
        undefined.add(name)
    defined = set()
    r = subprocess.run(['nm', '-g', '--defined-only'] + objs, capture_output=True, text=True)
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3:
            n = parts[2]
            defined.add(n[1:] if n.startswith('_') and sys.platform == 'darwin' else n)
    path = os.path.join(tmp, 'link_stubs.c')
    with open(path, 'w') as f:
        for n in sorted(undefined - defined):
            if n in ('memcpy', 'memset', 'memcmp', 'strlen', 'strcpy', 'printf', 'sprintf', 'puts', 'abort', 'dyld_stub_binder') or n.startswith('__'):
                continue
            f.write('void %s(void) {}\n' % n)
    return path


def check_aliases(env, g, tmp, res, bad):
    asserts = write_alias_tests(env, g)
    for a, snippet in asserts:
        path = os.path.join(tmp, 'offset32_%s.c' % a.name)
        open(path, 'w').write('\n'.join(snippet))
        r = subprocess.run(compile_cmd(path, os.path.join(tmp, 'offset32_%s.o' % a.name), False),
                           capture_output=True, text=True)
        if r.returncode:
            bad.append('alias %s (%s): not at %s + the executable offset in the 32-bit layout:\n%s' % (
                a.name, a.expr, a.container, r.stderr[:600]))
        else:
            res['aliases at the executable offset (32-bit layout)'] += 1
    # natively: compile the checks, link them with the data files and run
    objs = []
    for f in sorted(glob.glob(os.path.join(ALIAS_DIR, '*.c'))) + sorted(glob.glob(os.path.join(ROOT, 'game', 'data', '*_data.c'))):
        o = os.path.join(tmp, 'alias-' + os.path.basename(f) + '.o')
        r = subprocess.run(compile_cmd(f, o, True), capture_output=True, text=True)
        if r.returncode:
            bad.append('%s: native compile failed:\n%s' % (os.path.basename(f), r.stderr[:800]))
            return
        objs.append(o)
    stubs = link_stubs(objs, tmp)
    so = os.path.join(tmp, 'link_stubs.o')
    r = subprocess.run(compile_cmd(stubs, so, True), capture_output=True, text=True)
    if r.returncode:
        bad.append('link stubs do not compile:\n' + r.stderr[:800])
        return
    exe = os.path.join(tmp, 'alias_test')
    r = subprocess.run(['clang', '-o', exe] + objs + [so], capture_output=True, text=True)
    if r.returncode:
        bad.append('alias test does not link:\n' + r.stderr[:1500])
        return
    r = subprocess.run([exe], capture_output=True, text=True)
    res['aliases checked natively'] += len(D.ALIASES) if r.returncode == 0 else 0
    if r.returncode:
        bad.append('alias test failed:\n' + r.stdout[:1500])


def nm_symbols(objs, flags):
    """Names the object files define (flags 'B', 'D', 'C'...) or leave undefined ('U')."""
    r = subprocess.run(['nm', '-g'] + objs, capture_output=True, text=True)
    out = collections.defaultdict(set)
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[-2] in flags:
            n = parts[-1]
            out[parts[-2]].add(n[1:] if n.startswith('_') and sys.platform == 'darwin' else n)
    return out


def link_check(env, g, tmp, data_files, res, bad):
    """The program's globals as the linker sees them: compile the game sources (a copy in which the tentative
    definitions the data files define are made extern: --externalize) and the data files, then look for
    globals defined twice and variables used but defined nowhere."""
    work = os.path.join(tmp, 'tree')
    shutil.copytree(os.path.join(ROOT, 'game'), os.path.join(work, 'game'), ignore=shutil.ignore_patterns('data'))
    done, problems = externalize(g, False, work)
    for p_ in problems:
        bad.append('--externalize cannot handle %s' % p_)
    res['game sources: tentative definitions made extern (--externalize)'] += len(done)
    plat = os.path.join(ROOT, 'platform', 'include')
    objs = []
    for f in sorted(glob.glob(os.path.join(work, 'game', 'src', '*.c'))):
        o = os.path.join(tmp, 'link-' + os.path.basename(f) + '.o')
        r = subprocess.run(['clang', '-c', '-w', '-fsigned-char', '-fno-strict-aliasing', '-fwrapv', '-D_FORTIFY_SOURCE=0',
                            '-std=gnu11', '-include', os.path.join(plat, 'port.h'), '-I' + os.path.join(work, 'game', 'include'),
                            '-I' + plat, '-o', o, f], capture_output=True, text=True)
        if r.returncode:
            res['game sources that do not compile natively (skipped by the link check)'] += 1
            continue
        objs.append(o)
        res['game sources compiled natively'] += 1
    dobjs = []
    for f in data_files:
        o = os.path.join(tmp, 'link-' + os.path.basename(f) + '.o')
        r = subprocess.run(compile_cmd(f, o, True), capture_output=True, text=True)
        if r.returncode == 0:
            dobjs.append(o)
    game = nm_symbols(objs, 'BDCRSTbdcrst')
    data = nm_symbols(dobjs, 'BDCRSTbdcrst')
    defined_game = set().union(*(game[k] for k in game if k != 'U')) if game else set()
    defined_data = set().union(*(data[k] for k in data if k != 'U')) if data else set()
    for n_ in sorted(defined_game & defined_data):
        bad.append('%s is defined by the data files and by a game source' % n_)
    variables = {n['name'] for tu_ in g['scan'].values() for n in tu_ if n['kind'] == 'VarDecl' and n.get('storageClass') != 'static'}
    undefined = (game.get('U', set()) | data.get('U', set())) - defined_game - defined_data
    missing = sorted(n_ for n_ in undefined if n_ in variables)
    res['variables used but defined by neither the data files nor the game sources'] += len(missing)
    g['undefined_variables'] = missing
    for n_ in missing:
        print('UNDEFINED VARIABLE', n_)


def check(env, g, outdir):
    res = collections.Counter()
    bad = []
    seen = set()
    sizes = {}
    names = {n: (a, k) for a, n, k in env.rows}
    bycname = {}
    for s_ in g['order']:
        names[s_.cname] = names[s_.name]
        bycname[s_.cname] = s_
    for n_, sy_ in g['synthetic'].items():
        names[n_] = (sy_.addr, 'data')
        bycname[n_] = sy_
    env.names = names
    tmp = tempfile.mkdtemp(prefix='gendata-')
    files = sorted(glob.glob(os.path.join(outdir, '*_data.c')))
    # 1. clang, 32-bit little-endian target: one section per symbol
    for f in files:
        obj = os.path.join(tmp, os.path.basename(f) + '.o')
        r = subprocess.run(compile_cmd(f, obj, False), capture_output=True, text=True)
        if r.returncode:
            bad.append('%s: 32-bit compile failed:\n%s' % (os.path.basename(f), r.stderr[:2000]))
            continue
        res['clang: files compiled (32-bit little-endian, o32 layout)'] += 1
        co = Elf(obj)
        for sym in co.symbols:
            name, val, shndx, bind, typ, symsize = sym
            if shndx in (0, 0xfff1, 0xfff2) or bind != 1 or typ not in (1, 2) or shndx >= len(co.sections):
                continue
            sec = co.sections[shndx]
            if name not in names or names[name][1] != 'data':
                bad.append('%s defines %s which is not a data symbol of the executable' % (os.path.basename(f), name))
                continue
            if name in seen:
                bad.append('%s is defined twice' % name)
            seen.add(name)
            sizes[name] = symsize
            sy = bycname[name]
            if real_size(sy) != symsize:
                bad.append('%s: layout model says %d bytes, clang %d' % (name, real_size(sy), symsize))
            compare_symbol(env, g, res, bad, co, name, sec, val, symsize, 'clang',
                           min(symsize, sy.extent) if name in D.TRUNCATE else None)
    for s_ in list(g['order']) + list(g['synthetic'].values()):
        if s_.name in g['results'] and s_.cname not in seen:
            bad.append('%s was generated but is not defined by any object' % s_.name)
    # 2. native
    native_bad = []
    for f in files:
        obj = os.path.join(tmp, os.path.basename(f) + '.native.o')
        r = subprocess.run(compile_cmd(f, obj, True), capture_output=True, text=True)
        if r.returncode:
            native_bad.append('%s:\n%s' % (os.path.basename(f), r.stderr[:1500]))
        else:
            res['clang: files compiled natively (64-bit)'] += 1
    # arrays of pointer-bearing elements are sized by the 32-bit element count, never by native bytes
    for sy_ in g['order']:
        if sy_.name not in g['results'] or sy_.ty is None or sy_.ty.kind != 'arr' or not sy_.decl:
            continue
        e_ = sy_.ty
        while e_.kind == 'arr':
            e_ = e_.elem
        if e_.kind != 'ptr' and not pointer_slots(e_):
            continue
        res['arrays of pointer-bearing elements (sized by 32-bit element count)'] += 1
        if getattr(sy_.decl.ty, 'n', 0) is None and sy_.ty.n != sy_.extent // sy_.ty.elem.size and not sy_.notes:
            bad.append('%s: %d elements but the extent holds %d of %d bytes' % (
                sy_.name, sy_.ty.n, sy_.extent // sy_.ty.elem.size, sy_.ty.elem.size))
    # data the game's own sources define with initialisers (ships.c: startStateTable): compare their bytes
    for sy_ in g['skipped'].get('initialised', []):
        for d_ in sy_.decls:
            if d_.has_init and not d_.static:
                src_ = os.path.join(ROOT, 'game', 'src', d_.tu)
                obj_ = os.path.join(tmp, 'game-' + d_.tu + '.o')
                r_ = subprocess.run(compile_cmd(src_, obj_, False), capture_output=True, text=True)
                if r_.returncode:
                    bad.append('%s: %s defines %s with data but does not compile for the 32-bit layout:\n%s' % (
                        d_.tu, d_.tu, sy_.name, r_.stderr[:800]))
                    break
                co_ = Elf(obj_)
                found_ = False
                for sym_ in co_.symbols:
                    if sym_[0] == sy_.cname and sym_[2] not in (0, 0xfff1, 0xfff2) and sym_[3] == 1:
                        compare_symbol(env, g, res, bad, co_, sy_.cname, co_.sections[sym_[2]], sym_[1], sym_[5], 'game')
                        found_ = True
                if not found_:
                    bad.append('%s: %s not found in the object of %s' % (sy_.name, sy_.cname, d_.tu))
                break
    for sy_, inner in overlaps(g):
        if sy_.name in D.TRUNCATE:
            res['declared bigger than the object (TRUNCATE)'] += 1
            continue
        bad.append('%s (%d bytes, extent %d) contains the declared names %s: needs an alias or TRUNCATE decision' % (
            sy_.name, real_size(sy_), sy_.extent, ' '.join('%s@+%d' % (o.name, o.addr - sy_.addr) for o in inner)))
    link_check(env, g, tmp, files, res, bad)
    # decisions the sources no longer need
    stale = []
    live_overlap = {sy_.name for sy_, _ in overlaps(g)}
    for n_ in D.TRUNCATE:
        if n_ not in live_overlap:
            stale.append('TRUNCATE %s: no declared name inside it any more' % n_)
    for n_ in list(D.SIZES) + list(D.ABSOLUTE) + list(D.ALIGN):
        if n_ not in g['results']:
            stale.append('%s: not a defined global any more' % n_)
    for a_ in D.ALIASES:
        if g['syms'][a_.name].decls:
            bad.append('alias %s is still declared in game/src: run tools/gendata.py --apply-aliases' % a_.name)
        if a_.container not in g['results']:
            stale.append('alias %s: container %s is not defined' % (a_.name, a_.container))
    for line_ in stale:
        print('STALE', line_)
    check_aliases(env, g, tmp, res, bad)
    return res, bad, native_bad, tmp


# --------------------------------------------------------------------------
# report


def overlaps(g):
    """Defined objects bigger than the space up to the next name the game declares, with the names
    inside them. Each needs a decision (an alias, or TRUNCATE) in tools/gendata_decisions.py."""
    order = g['order']
    idx = {s.name: i for i, s in enumerate(order)}
    out = []
    for sy in order:
        if sy.name not in g['results'] or sy.ty is None:
            continue
        sz = real_size(sy)
        if sz <= sy.extent:
            continue
        inner = [o for o in order[idx[sy.name] + 1:] if o.addr < sy.addr + sz and o.decls and o.name not in ALIAS_NAMES]
        if inner:
            out.append((sy, inner))
    return out


def real_size(sy):
    if sy.ty is None:
        return sy.size
    return sy.size or (sy.ty.size if not (sy.ty.kind == 'arr' and sy.ty.n is None) else 0)


def report(env, g):
    order, em = g['order'], g['em']
    gen = [s for s in order if s.name in g['results']]
    out = []
    P = out.append
    P('== counts')
    P('labelled data symbols (asm/data/*.s and symbol_addrs.txt): %d' % len(order))
    P('  game/src defines them with their data itself: %d (%s)' % (
        len(g['skipped'].get('initialised', [])), ' '.join(s.name for s in g['skipped'].get('initialised', []))))
    P('  names that stand for a part of another global (aliases): %d' % len(g['skipped']['alias']))
    P('  runtime/library data left out: %d (%s)' % (len(g['skipped']['lib']), ' '.join(s.name for s in g['skipped']['lib'])))
    P('  not declared anywhere in game/ (not generated): %d' % len(g['skipped']['undeclared']))
    P('  generated: %d' % len(gen))
    for kind in ('rdata', 'data', 'bss'):
        P('    %-6s %d' % (kind, sum(1 for s in gen if s.kind == kind)))
    P('  of which game/src has a tentative definition (--externalize makes it extern): %d' % len(g['skipped']['defined']))
    P('  files: %d' % len(g['modules']))
    P('  decisions: %d aliases, %d truncated declarations, %d sizes, %d alignments, %d absolute pointers, %d use rewrites' % (
        len(D.ALIASES), len(D.TRUNCATE), len(D.SIZES), len(D.ALIGN), len(D.ABSOLUTE), len(D.REWRITES)))
    P('')
    P('== declarations that disagree')
    kinds = collections.Counter()
    for sy in gen:
        if len(sy.conflicts) < 2:
            continue
        chosen = type_key(sy.decl.ty) if sy.decl else '-'
        alts = {}
        for d in sy.decls:
            if not has_opaque(d.ty):
                alts.setdefault(type_key(d.ty), d)
        c = sy.decl.ty

        def classify(t):
            if t.kind == 'int' and c.kind == 'int' and t.size == c.size:
                return 'signedness'
            if t.kind == 'ptr' and c.kind == 'ptr':
                return 'pointer target'
            if t.kind == 'func' or c.kind == 'func':
                return 'function type'
            if t.kind == 'arr' and c.kind == 'arr' and t.n is None or c.kind == 'arr' and c.n is None and t.kind == 'arr':
                return 'incomplete array'
            if t.kind == 'arr' and c.kind == 'arr':
                return 'array length or element'
            if t.kind == 'rec' and c.kind == 'rec' and t.size == c.size:
                return 'same-size record'
            if t.size == c.size:
                return 'same size, other type'
            return 'SIZE DIFFERS'
        labels = sorted({classify(d.ty) for k, d in alts.items() if k != chosen})
        for l in labels:
            kinds[l] += 1
        parts = []
        for key, where in sy.conflicts:
            tus = ','.join(sorted({w[0] for w in where}))
            parts.append('%s [%s]%s' % (key, tus if len(tus) < 40 else '%d files' % len(where), ' <= chosen' if key == chosen else ''))
        P('%s (extent %d) {%s}: %s' % (sy.name, sy.extent, ', '.join(labels), ' | '.join(parts)))
    P('summary: %s' % ', '.join('%s %d' % kv for kv in sorted(kinds.items())))
    P('')
    P('== unions that C89 cannot initialise completely')
    for name, rngs in em.partial.items():
        P('%s: %d bytes' % (name, sum(h - l for l, h in rngs)))
    P('')
    P('== pointers to string literals (an equal literal is emitted; identity with the literal in the function is lost)')
    by = collections.defaultdict(list)
    for sy, addr, lit in em.literals:
        by[sy].append(lit)
    for sy, lits in by.items():
        P('%s: %d  %s' % (sy, len(lits), ' | '.join(l.decode('latin-1')[:24] for l in lits[:3])))
    shared = collections.defaultdict(list)
    for sy, addr, lit in em.literals:
        shared[addr].append(sy)
    multi = {a: v for a, v in shared.items() if len(v) > 1}
    P('literals the executable shares between several pointers (the data files give each an equal literal; '
      'the compiler may or may not pool them): %d' % len(multi))
    for a, v in sorted(multi.items()):
        P('  0x%x %s: %s' % (a, em.pe.cstr(a)[:30], ', '.join(sorted(set(v)))))
    P('')
    P('== other problems')
    for sy, msg in em.problems:
        P('%s: %s' % (sy, msg))
    P('')
    P('== types the game declares privately in a .c file (repeated in the data file)')
    for module, members in sorted(g['modules'].items()):
        names = sorted({n['name'] or n['id'] for sy in members for n in sy.use_nodes.values()
                        if n['_where'][0] and '/game/src/' in n['_where'][0] and n.get('name')})
        if names:
            P('%s: %s' % (module, ' '.join(names)))
    P('')
    P('== adjacency: declared type larger than the space up to the next symbol')
    idx = {s.name: i for i, s in enumerate(order)}
    for sy in gen:
        sz = real_size(sy)
        if sz and sz > sy.extent:
            inner = [o.name for o in order[idx[sy.name] + 1:] if o.addr < sy.addr + sz]
            P('%s: declared %d bytes, extent %d; overlaps %s' % (sy.name, sz, sy.extent, ' '.join(inner[:8])))
    P('')
    P('== adjacency: declared array smaller than its extent (beyond 3 bytes of padding)')
    for sy in gen:
        d = sy.decl.ty if sy.decl else None
        if d is not None and d.kind == 'arr' and d.n is not None and d.size + 3 < sy.extent:
            nxt_sym = order[idx[sy.name] + 1] if idx[sy.name] + 1 < len(order) else None
            pad = sy.extent - d.size
            if sy.kind == 'bss' and pad < 16 and nxt_sym is not None and nxt_sym.addr % 16 == 0:
                continue                # the linker aligns the next communal variable to 16 bytes
            nxt = nxt_sym.name if nxt_sym else '-'
            P('%s: declared %s = %d bytes, extent %d (next symbol %s)' % (sy.name, d, d.size, sy.extent, nxt))
    P('')
    P('== adjacency: incomplete arrays sized from the extent')
    plain = 0
    for sy in gen:
        d = sy.decl.ty if sy.decl else None
        if d is not None and d.kind == 'arr' and d.n is None:
            notes = [n for n in sy.notes if n not in ('string', 'buffer holding a string')]
            if notes or (getattr(sy, 'count', 0) or 0) <= 1:
                P('%s: %s -> %d elements (extent %d)%s' % (sy.name, d, getattr(sy, 'count', 0) or 0, sy.extent,
                                                           '  ' + '; '.join(notes) if notes else ''))
            else:
                plain += 1
    P('(%d more incomplete arrays take their length from the extent without anything unusual)' % plain)
    P('')
    P('== adjacency: constant indices at or beyond the declared length')
    ci = constant_indices()
    for sy in gen:
        d = sy.decl.ty if sy.decl else None
        if d is None or d.kind != 'arr' or not d.n:
            continue
        for idx, (f, ln) in sorted(ci.get(sy.name, {}).items()):
            if idx >= d.n:
                P('%s[%d] with declared length %d at %s:%d' % (sy.name, idx, d.n, f, ln))
    P('')
    P('== adjacency: comments in the sources and notes that mention it')
    names = '|'.join(re.escape(s.name) for s in gen)
    hint = re.compile(r'past the|bytes past|follows it in memory|adjacent|overrun|writes index|reads entries|beyond the end|next global', re.I)
    files = glob.glob(os.path.join(ROOT, 'game', 'src', '*.c')) + glob.glob(os.path.join(ROOT, 'game', 'include', '*.h'))
    files += glob.glob(os.path.join(env.decomp, 'notes', '*.md'))
    for f in sorted(files):
        for ln, line in enumerate(open(f, encoding='latin-1'), 1):
            if hint.search(line) and re.search(r'\b(%s)\b' % names, line):
                P('%s:%d: %s' % (os.path.relpath(f, ROOT) if f.startswith(ROOT) else 'decomp-pc/notes/' + os.path.basename(f), ln, line.strip()[:200]))
    P('')
    P('== notes per symbol')
    for sy in gen:
        for n in sy.notes:
            if n not in ('string',):
                P('%s: %s' % (sy.name, n))
    return '\n'.join(out)

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--decomp', default=DEFAULT_DECOMP)
    ap.add_argument('--rescan', action='store_true')
    ap.add_argument('--out', default=os.path.join(ROOT, 'game', 'data'))
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--report', action='store_true')
    ap.add_argument('--apply-aliases', action='store_true', help='drop the declarations of alias names from game/src')
    ap.add_argument('--externalize', action='store_true', help='make the tentative definitions of game/src that the data files define extern')
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--conform', action='store_true', help='lay the code\'s accesses over the chosen types')
    ap.add_argument('--only', nargs='*', default=None)
    ap.add_argument('--decisions', action='store_true', help='write docs/GENDATA_DECISIONS.md')
    ap.add_argument('--hazards', action='store_true', help='declarations whose native size differs from the definition')
    ap.add_argument('--evidence', nargs='+', default=None, help='names or 0xADDRESSes: declarations, labels, accesses, uses')
    ap.add_argument('--span', type=int, default=None)
    a = ap.parse_args()
    if a.apply_aliases:
        for line in apply_aliases(a.dry_run):
            print(line)
        return 0
    env = Env(a.decomp)
    scan = scan_all(a.rescan)
    g = generate(env, scan)
    files = write_files(g, a.out)
    print('wrote %d files' % len(files))
    if a.report:
        print(report(env, g))
        return 0
    if a.decisions:
        open(os.path.join(ROOT, 'docs', 'GENDATA_DECISIONS.md'), 'w').write(decisions_markdown(env, g))
        return 0
    if a.externalize:
        done, bad = externalize(g, a.dry_run)
        print('%d declarations made extern, %d not handled' % (len(done), len(bad)))
        for b in bad:
            print('NOT HANDLED', b)
        return 0
    if a.hazards:
        import gendata_code
        for ptr in (4, 8):
            print('== pointers of %d bytes' % ptr)
            for h in gendata_code.native_hazards(g, scan, pack_table(), ptr):
                print('%s: %s:%d declares %s (defined %s): %s' % h)
        return 0
    if a.evidence:
        import gendata_code
        print(gendata_code.evidence(env, g, a.evidence, a.span))
        return 0
    if a.conform:
        import gendata_code
        print(gendata_code.conformance(env, g, a.only))
        return 0
    if a.check:
        res, bad, nbad, tmp = check(env, g, a.out)
        for k, v in sorted(res.items()):
            print('%-45s %d' % (k, v))
        for b in bad:
            print('MISMATCH', b)
        for b in nbad:
            print('NATIVE', b)
        print('mismatches: %d, native compile failures: %d' % (len(bad), len(nbad)))
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)
        return 1 if bad else 0
    for sy, msg in g['em'].problems[:50]:
        print('PROBLEM', sy, msg)


if __name__ == '__main__':
    main()
