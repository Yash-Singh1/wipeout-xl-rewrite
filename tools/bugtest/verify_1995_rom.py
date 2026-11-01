#!/usr/bin/env python3
"""Check the 1995 scenery fixtures by executing the USA retail PS1 routines.

Pass SCUS_943.01 extracted from your disc. Requires the optional unicorn package.
"""
import argparse
import hashlib
from pathlib import Path
import re
import struct
from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS32, UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import (
    UC_MIPS_REG_A0, UC_MIPS_REG_A1, UC_MIPS_REG_A2, UC_MIPS_REG_A3,
    UC_MIPS_REG_GP, UC_MIPS_REG_SP, UC_MIPS_REG_RA,
    UC_MIPS_REG_S2, UC_MIPS_REG_S4,
)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("executable", type=Path)
args = parser.parse_args()
exe = args.executable.read_bytes()
assert hashlib.sha256(exe).hexdigest() == "d18b54674d41a4d00f4b913192c6a5527b4b9a427df6450da94e0fcfe6d39f6f", "Expected USA SCUS_943.01"
cpu = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 | UC_MODE_LITTLE_ENDIAN)
cpu.mem_map(0, 0x200000)
cpu.mem_write(0x10000, exe[2048:])
gp = 0x8007deb0
cpu.reg_write(UC_MIPS_REG_GP, gp)


def write(address, fmt, *values):
    cpu.mem_write(address & 0x1fffff, struct.pack("<" + fmt, *values))


def read(address, fmt):
    return struct.unpack("<" + fmt, cpu.mem_read(address & 0x1fffff, struct.calcsize("<" + fmt)))


def call(address, *values):
    for register, value in zip((UC_MIPS_REG_A0, UC_MIPS_REG_A1, UC_MIPS_REG_A2, UC_MIPS_REG_A3), values):
        cpu.reg_write(register, value & 0xffffffff)
    cpu.reg_write(UC_MIPS_REG_SP, 0x801ff000)
    cpu.reg_write(UC_MIPS_REG_RA, 0x80000000)
    cpu.emu_start(address, 0x80000000, count=1000000)


# Objects and skeletons are synthetic; the animation code and trig tables are original.
write(0x80120024, "I", 0x80121000)
write(0x80120100, "2I", 0x80120200, 0x80120300)
write(0x80120230, "I", 0x80120400)
write(0x80120330, "I", 0x80120500)
obj, vertices, polygons = 0x80140000, 0x80141000, 0x80142000
write(obj + 16, "h", 16)
write(obj + 20, "I", vertices)
write(obj + 32, "h", 82)
write(obj + 36, "I", polygons)
for i in range(16):
    write(vertices + i * 8, "4h", 0, (-6500, -8000, -12000, -5000)[i // 4], 0, 0)
write(polygons, "h", 6)
for i in range(81):
    address = polygons + 36 + i * 44
    group = i % 3 if i < 80 else 3
    write(address, "2h", 8, 0)
    write(address + 4, "4h", *(group * 4 + k for k in range(4)))
    for k in range(4):
        write(address + 28 + k * 4, "4B", 10 + k, 20 + k, 30 + k, 0)
# Stub only printf, which sky initialization uses for diagnostic messages.
write(0x80076c98, "2I", 0x03e00008, 0)
call(0x80045508, obj)
assert read(0x80087f24, "3h") == (-1, 0, -2)
fixture = Path(__file__).with_name("guest_1995_frames.h").read_text().split("sceneryFrames[] = {", 1)[1]
values = list(map(int, re.findall(r"-?\d+", fixture)))
assert len(values) == 15 * 47
for offset in range(0, len(values), 47):
    tick, *expected = values[offset:offset + 47]
    write(gp + 0x184, "H", ((tick - 1) * 140) & 65535)
    write(gp + 0x190, "H", ((tick - 1) * 20) & 65535)
    write(gp + 0x1a8, "i", tick - 1)
    call(0x80043fe4, 0x80120000)  # warning beacon
    call(0x800442e0, 0x80120100)  # both oil pumps and their rotation matrices
    call(0x80045698, obj)         # all 80 aurora quads
    actual = [read(0x8012101c, "B")[0]] + list(read(0x80120400, "9h"))
    actual += [v for i in range(3) for k in range(4)
               for v in read(polygons + 36 + i * 44 + 28 + k * 4, "3B")]
    assert actual == expected, f"Scenery update {tick}: {actual} != {expected}"
    for i in range(80):
        colours = [v for k in range(4) for v in read(polygons + 36 + i * 44 + 28 + k * 4, "3B")]
        assert colours == expected[10 + i % 3 * 12:22 + i % 3 * 12]
    assert read(polygons + 36 + 80 * 44 + 28, "3B") == (10, 20, 30)
    # The retail sky renderer preserves all four colours in a Gouraud GPU packet.
    # Execute its packet-emission block after projection and facing tests.
    for i in range(3):
        face, packet = polygons + 36 + i * 44, 0x80180000
        cpu.reg_write(UC_MIPS_REG_S2, packet)
        cpu.reg_write(UC_MIPS_REG_S4, face)
        cpu.emu_start(0x80014518, 0x80014578, count=1000)
        assert read(packet + 3, "B")[0] == 12 and read(packet + 7, "B")[0] == 0x3c
        for k in range(4):
            assert read(packet + 4 + k * 12, "3B") == read(face + 28 + k * 4, "3B")
print("USA PS1: scenery, all 80 aurora quads and Gouraud sky GPU packets verified at all 15 fixture updates")
