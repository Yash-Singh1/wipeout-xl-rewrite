#!/usr/bin/env python3
"""Find every access the PlayStation code makes to a constant address.

A small forward analysis over the MIPS-I code of SCUS_943.51: each register holds an affine
expression `constant + sum(coefficient * opaque value)`; `lui`/`addiu`/`ori`/`addu`/`sll` build
addresses the way PsyQ GCC does (hi/lo pairs, gp-relative small data, `base + (index << k)`), and
every load or store through such a register is recorded as

    Access(pc, address, width, store, strides)

where `address` is the constant part (symbol + offset) and `strides` the coefficients of the
opaque values added to it (the element size of an indexed access). Registers are forgotten at
every possible branch target and, except for the callee-saved ones, after a call.

Also recorded: `Lea(pc, address)` whenever a full constant is formed (lui + addiu/ori) or an
address is passed on with an added offset: the code takes the address of something there.
"""
import collections
import struct

IMAGE_BASE = 0x80010000

Access = collections.namedtuple('Access', 'pc address width store strides base')
Lea = collections.namedtuple('Lea', 'pc address')

GP = 28
CALLER_SAVED = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31]

LOADS = {0x20: (1, 'lb'), 0x21: (2, 'lh'), 0x23: (4, 'lw'), 0x24: (1, 'lbu'), 0x25: (2, 'lhu'), 0x22: (4, 'lwl'),
         0x26: (4, 'lwr'), 0x32: (4, 'lwc2')}
STORES = {0x28: (1, 'sb'), 0x29: (2, 'sh'), 0x2b: (4, 'sw'), 0x2a: (4, 'swl'), 0x2e: (4, 'swr'), 0x3a: (4, 'swc2')}


class Val:
    """c + sum(k * id) with ids opaque."""
    __slots__ = ('c', 'terms')

    def __init__(self, c=None, terms=None):
        self.c = c
        self.terms = terms or {}

    def add(self, other):
        if self.c is None or other.c is None:
            return None
        t = dict(self.terms)
        for k, v in other.terms.items():
            t[k] = t.get(k, 0) + v
            if t[k] == 0:
                del t[k]
        return Val((self.c + other.c) & 0xffffffff, t)

    def neg(self):
        return Val((-self.c) & 0xffffffff, {k: -v for k, v in self.terms.items()})

    def shl(self, n):
        return Val((self.c << n) & 0xffffffff, {k: v << n for k, v in self.terms.items()})


def s16(x):
    return x - 0x10000 if x & 0x8000 else x


class Scanner:
    def __init__(self, exe, text_ranges, gp, jump_targets=(), no_gp=()):
        self.exe = exe
        self.ranges = text_ranges
        self.no_gp = tuple(no_gp)       # ranges of hand-written code that uses $gp as a scratch register
        self.use_gp = True
        self.gp = gp
        self.accesses = []
        self.leas = []
        self.targets = set(jump_targets)
        self.nextid = 1

    def words(self, lo, hi):
        data = self.exe.read(lo, hi - lo)
        return struct.unpack('<%dI' % ((hi - lo) // 4), data)

    def find_targets(self):
        for lo, hi in self.ranges:
            for i, w in enumerate(self.words(lo, hi)):
                pc = lo + 4 * i
                op = w >> 26
                if op in (4, 5, 6, 7) or (op == 1 and ((w >> 16) & 0x1f) in (0, 1, 16, 17)):
                    self.targets.add(pc + 4 + 4 * s16(w & 0xffff))
                elif op in (2, 3):
                    self.targets.add((pc & 0xf0000000) | ((w & 0x3ffffff) << 2))

    def fresh(self):
        self.nextid += 1
        return Val(0, {self.nextid: 1})

    def run(self):
        self.find_targets()
        for lo, hi in self.ranges:
            self.use_gp = not any(a <= lo < b for a, b in self.no_gp)
            self.scan(lo, hi)

    def reset(self):
        regs = [None] * 32
        regs[0] = Val(0, {})
        regs[GP] = Val(self.gp, {}) if self.use_gp else None
        return regs

    def scan(self, lo, hi):
        ws = self.words(lo, hi)
        regs = self.reset()
        pending = None              # kind of control transfer whose delay slot is the next instruction
        for i, w in enumerate(ws):
            pc = lo + 4 * i
            slot, pending = pending, None
            if pc in self.targets and slot is None:
                regs = self.reset()
            op = w >> 26
            rs, rt, rd = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
            imm = w & 0xffff
            sa = (w >> 6) & 31
            fn = w & 63

            def get(r):
                v = regs[r]
                if v is None:
                    v = regs[r] = self.fresh()
                return v

            def put(r, v):
                if r:
                    regs[r] = v

            if op == 0x0f:      # lui
                put(rt, Val((imm << 16) & 0xffffffff, {}))
            elif op in (0x09, 0x08):        # addiu / addi
                a = get(rs)
                v = a.add(Val(s16(imm) & 0xffffffff, {}))
                put(rt, v if v is not None else self.fresh())
                if v is not None and not v.terms and rs != GP and rs != 0 and not a.terms \
                        and self.exe.section_of(v.c) is not None and s16(imm) != 0:
                    self.leas.append(Lea(pc, v.c))
            elif op == 0x0d:        # ori
                a = get(rs)
                if not a.terms:
                    v = Val(a.c | imm, {})
                    put(rt, v)
                    if rs != 0 and self.exe.section_of(v.c) is not None:
                        self.leas.append(Lea(pc, v.c))
                else:
                    put(rt, self.fresh())
            elif op in LOADS or op in STORES:
                width = (LOADS.get(op) or STORES[op])[0]
                base = get(rs)
                store = op in STORES
                a = base.add(Val(s16(imm) & 0xffffffff, {}))
                if a is not None and op in (0x22, 0x2a):
                    a = a.add(Val(0xfffffffd, {}))      # lwl/swl name the most significant byte of the unaligned word
                if a is not None and a.c >= IMAGE_BASE and self.exe.section_of(a.c) is not None:
                    self.accesses.append(Access(pc, a.c, width, store, tuple(sorted(a.terms.values())), rs == GP))
                if not store and op != 0x32:
                    put(rt, self.fresh())
            elif op == 0:
                if fn in (0x21, 0x20):         # addu / add
                    v = get(rs).add(get(rt))
                    put(rd, v if v is not None else self.fresh())
                elif fn in (0x23, 0x22):       # subu / sub
                    v = get(rs).add(get(rt).neg())
                    put(rd, v if v is not None else self.fresh())
                elif fn == 0:                  # sll
                    if w:
                        put(rd, get(rt).shl(sa))
                elif fn == 0x25:               # or
                    a, b = get(rs), get(rt)
                    if rt == 0:
                        put(rd, a)
                    elif rs == 0:
                        put(rd, b)
                    elif not a.terms and not b.terms:
                        put(rd, Val(a.c | b.c, {}))
                    else:
                        put(rd, self.fresh())
                elif fn == 8:
                    pending = 'jump'
                elif fn == 9:
                    pending = 'call'
                    put(rd, self.fresh())
                elif fn in (0x10, 0x12):       # mfhi / mflo
                    put(rd, self.fresh())
                elif fn in (0x18, 0x19, 0x1a, 0x1b, 0x11, 0x13, 0x0c, 0x0d):
                    pass
                else:
                    put(rd, self.fresh())
            elif op == 2:
                pending = 'jump'
            elif op == 3:
                pending = 'call'
            elif op in (4, 5, 6, 7):
                pending = 'branch'
            elif op == 1:
                pending = 'call' if ((w >> 16) & 0x1f) in (16, 17) else 'branch'
            elif op in (0x0a, 0x0b, 0x0c, 0x0e):   # slti sltiu andi xori
                put(rt, self.fresh())
            elif op in (0x10, 0x12):      # cop0 / cop2
                if rs in (0, 2):          # mfc / cfc
                    put(rt, self.fresh())
            if slot == 'call':
                for r in CALLER_SAVED:
                    regs[r] = None
                regs[0] = Val(0, {})
            elif slot == 'jump':
                regs = self.reset()
