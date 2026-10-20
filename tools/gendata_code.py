"""What the machine code does with the globals tools/gendata.py defines.

`conformance` lays every access found by tools/mipsscan.py (address, width, stride) over the type
chosen for the symbol it falls in and reports the ones the type cannot explain; `evidence` prints
everything known about one name or address range (declarations, neighbours, accesses, uses in the C).
"""
import bisect
import collections
import glob
import os
import re

import mipsscan

GAME_CODE_END = 0x8007B94C       # the PsyQ libraries follow the game's code (README of the decompilation)
LIB_DATA = (0x80090D30, 0x80093DE4)   # .data of the libraries, between the game's .data and its small data


def leaf_at(ty, off):
    """[(start offset relative to ty, leaf type)] of the scalar leaves of `ty` that contain `off`."""
    if off < 0 or off >= max(ty.size, 1) and ty.kind != 'arr':
        return []
    k = ty.kind
    if k in ('int', 'float', 'ptr'):
        return [(0, ty)]
    if k == 'arr':
        e = ty.elem
        if not e.size:
            return []
        i, r = divmod(off, e.size)
        if ty.n is not None and i >= ty.n:
            return []
        return [(i * e.size + s, t) for s, t in leaf_at(e, r)]
    if k == 'rec':
        out = []
        for f in ty.fields:
            if f.offset <= off < f.offset + f.ty.size:
                out += [(f.offset + s, t) for s, t in leaf_at(f.ty, off - f.offset)]
        return out
    return []


def classify(ty, off, width, strides):
    """How a type explains one access: 'exact', 'narrow' (inside a bigger scalar), 'span' (covers several
    scalars inside the object: a block copy or clear), 'beyond' (reaches past the end of the object or
    into padding)."""
    size = ty.size
    if off + width > size:
        return 'beyond'
    leaves = leaf_at(ty, off)
    if not leaves:
        return 'beyond'
    best = 'span'
    for s, t in leaves:
        if s == off and t.size == width:
            return 'exact'
        if s <= off and off + width <= s + t.size:
            best = 'narrow'
    return best


def func_of(env):
    starts = sorted(env.funcs)

    def f(pc):
        i = bisect.bisect_right(starts, pc) - 1
        return env.funcs[starts[i]] if i >= 0 else '?'
    return f


def conformance(env, g, only=None, limit=3):
    """Lines describing accesses that the chosen types do not explain."""
    sc = env.code
    gen = [s for s in g['order'] if s.name in g['results'] and s.ty is not None]
    addrs = [s.addr for s in gen]
    where = func_of(env)
    per = collections.defaultdict(list)
    stats = collections.Counter()
    for a in sc.accesses:
        if a.pc >= GAME_CODE_END:
            continue
        i = bisect.bisect_right(addrs, a.address) - 1
        if i < 0:
            continue
        sy = gen[i]
        size = sy.size or sy.ty.size
        if a.address >= sy.addr + max(size, 1):
            # between two generated objects (an undeclared one, or padding)
            nxt = gen[i + 1].addr if i + 1 < len(gen) else None
            per['<gap>'].append((a, sy, 'gap'))
            stats['gap'] += 1
            continue
        c = classify(sy.ty, a.address - sy.addr, a.width, a.strides)
        stats[c] += 1
        if c == 'beyond':
            per[sy.name].append((a, sy, c))
    out = ['accesses by class: %s' % ', '.join('%s %d' % kv for kv in sorted(stats.items()))]
    for name, lst in sorted(per.items()):
        if only and name not in only:
            continue
        out.append('%s: %d unexplained' % (name, len(lst)))
        for a, sy, c in lst[:limit]:
            out.append('    %s %s+0x%x width %d%s%s in %s [%s %s]' % (
                'store' if a.store else 'load', sy.name, a.address - sy.addr, a.width,
                ' stride %s' % (a.strides,) if a.strides else '', ' gp' if a.base else '', where(a.pc), c, sy.ty))
    return '\n'.join(out)


def hexdump(b, n=48):
    return ' '.join('%02x' % x for x in b[:n]) + (' ...' if len(b) > n else '')


def evidence(env, g, names, span=None, show_lib=False):
    """Everything known about `names` (symbol names or 0xADDR) for a decision."""
    where = func_of(env)
    order = g['order']
    oaddrs = [s.addr for s in order]
    out = []
    for name in names:
        if name.startswith('0x'):
            addr = int(name, 16)
            sy = None
        else:
            sy = g['syms'].get(name)
            if sy is None:
                out.append('%s: unknown symbol' % name)
                continue
            addr = sy.addr
        size = span or (sy.size or (sy.ty.size if sy and sy.ty else 4) if sy else 16)
        out.append('=' * 78)
        out.append('%s at 0x%x, chosen %s (%d bytes), extent %s, raw extent %s' % (
            name, addr, sy.ty if sy else '-', size, getattr(sy, 'extent', '-'), getattr(sy, 'raw_extent', '-')))
        if sy:
            for d in sy.decls:
                out.append('  decl %s:%s  %s%s' % (d.tu, d.where[1], d.qual, '  [tentative definition]' if not d.extern else ''))
        lo, hi = addr - 4, addr + max(size, getattr(sy, 'extent', 0) or 0, 8) + 4
        i = max(bisect_left(oaddrs, lo) - 1, 0)
        out.append('  labels:')
        while i < len(order) and order[i].addr < hi:
            s = order[i]
            out.append('    0x%x %s%s' % (s.addr, s.name, '' if s.name in g['results'] else '  (not generated)' if not s.lib else '  (library)'))
            i += 1
        out.append('  bytes: ' + hexdump(env.pe.read(addr, min(max(size, 16), 96)), 96))
        acc = collections.defaultdict(lambda: [0, set()])
        for a in env.code.accesses:
            if lo <= a.address < hi and (show_lib or a.pc < GAME_CODE_END):
                key = (a.address - addr, a.width, a.store, a.strides)
                acc[key][0] += 1
                acc[key][1].add(where(a.pc))
        out.append('  accesses (offset width load/store strides: count, functions):')
        for key in sorted(acc):
            n, fs = acc[key]
            out.append('    %+d w%d %s %s: %d  %s' % (key[0], key[1], 'st' if key[2] else 'ld', key[3] or '', n, ' '.join(sorted(fs)[:6])))
        leas = sorted({l.address - addr for l in env.code.leas if lo <= l.address < hi})
        if leas:
            out.append('  address taken at offsets: %s' % leas)
        uses = []
        for f in sorted(glob.glob(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'game', 'src', '*.c'))):
            for ln, line in enumerate(open(f, encoding='latin-1'), 1):
                if re.search(r'\b%s\b' % re.escape(name), line) and not line.lstrip().startswith('extern'):
                    uses.append('%s:%d: %s' % (os.path.basename(f), ln, line.strip()[:110]))
        out.append('  uses in game/src (%d):' % len(uses))
        out += ['    ' + u for u in uses[:14]]
    return '\n'.join(out)


from bisect import bisect_left  # noqa: E402


def native_hazards(g, scan, packs, ptr=8):
    """Declarations (in any source file) of a defined global whose size or element size natively (8-byte
    pointers) differs from the definition's although the 32-bit sizes agree or the declaration is bigger:
    code reading through such a declaration touches other bytes than it did on the console."""
    import gendata
    saved = (gendata.Ptr.size, gendata.Ptr.align)
    gendata.Ptr.size = gendata.Ptr.align = ptr
    try:
        tys = {tu: gendata.Types(nodes, packs) for tu, nodes in scan.items()}

        def elem(t):
            while t.kind == 'arr':
                t = t.elem
            return t

        out = []
        for sy in g['order']:
            if sy.name not in g['results'] or sy.decl is None:
                continue
            try:
                ndef = tys[sy.decl.tu].parse(sy.decl.qual)
            except Exception:       # noqa
                continue
            if sy.ty.kind == 'arr' and ndef.kind == 'arr':
                dsize = ndef.elem.size * (sy.ty.n or 1)
            else:
                dsize = ndef.size
            for d in sy.decls:
                try:
                    nd = tys[d.tu].parse(d.qual)
                except Exception:   # noqa
                    continue
                if nd.kind == 'opaque' or elem(nd).kind == 'opaque':
                    continue
                e32, e64 = elem(d.ty), elem(nd)
                reason = None
                if elem(ndef).size != e64.size and elem(sy.decl.ty).size == e32.size:
                    reason = 'element %d bytes natively, the definition\'s %d (same on the console)' % (e64.size, elem(ndef).size)
                elif nd.kind != 'arr' or nd.n is not None:
                    if nd.size > dsize:
                        reason = 'declares %d bytes natively, the definition has %d' % (nd.size, dsize)
                if reason:
                    out.append((sy.name, d.tu, d.where[1], d.qual, str(sy.decl.qual), reason))
        return out
    finally:
        gendata.Ptr.size, gendata.Ptr.align = saved
