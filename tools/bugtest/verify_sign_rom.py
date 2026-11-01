#!/usr/bin/env python3
"""Verify the checkpoint fixtures against a supplied USA Wipeout 64 ROM.

Requires the optional Python unicorn package. No emulator state or ROM data is
stored in the repository. The regular native regression test uses these fixtures.
"""
import argparse
import hashlib
import math
from pathlib import Path
import re
import struct

from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS32, UC_MODE_BIG_ENDIAN
from unicorn.mips_const import (
    UC_MIPS_REG_A0, UC_MIPS_REG_A1, UC_MIPS_REG_A2, UC_MIPS_REG_A3,
    UC_MIPS_REG_RA, UC_MIPS_REG_S0, UC_MIPS_REG_S1, UC_MIPS_REG_S4,
    UC_MIPS_REG_SP, UC_MIPS_REG_V0,
)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("rom", type=Path)
args = parser.parse_args()
rom = args.rom.read_bytes()
assert hashlib.sha256(rom).hexdigest() == "fdcf4ae70d90abef4f299b04664e009a967e55f97a065b43f596b749dd7d6b15", "Expected the USA big-endian .z64 ROM"
cpu = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 | UC_MODE_BIG_ENDIAN)
cpu.mem_map(0, 0x800000)
# The boot code loads ROM offset 0x1000 at virtual address 0x80000400.
cpu.mem_write(0x400, rom[0x1000:])


def write(address, fmt, *values):
    cpu.mem_write(address & 0x7fffff, struct.pack(">" + fmt, *values))


def read(address, fmt):
    return struct.unpack(">" + fmt, cpu.mem_read(address & 0x7fffff, struct.calcsize(">" + fmt)))


def call(address, *values):
    for register, value in zip((UC_MIPS_REG_A0, UC_MIPS_REG_A1, UC_MIPS_REG_A2, UC_MIPS_REG_A3), values):
        cpu.reg_write(register, value & 0xffffffff)
    cpu.reg_write(UC_MIPS_REG_SP, 0x807ff000)
    cpu.reg_write(UC_MIPS_REG_RA, 0x80000000)
    cpu.emu_start(address, 0x80000000, count=100000)
    return cpu.reg_read(UC_MIPS_REG_V0)


# Check every entry of both lookup tables that the native reconstruction uses.
for index in range(4097):
    expected = int(math.atan(index / 4096) * 65536 / math.tau + 0.5)
    assert read(0x8008d6b4 + index * 2, "H")[0] == expected
for index in range(4096):
    value = math.sin(index * math.tau / 4096) * 16384
    expected = int(math.copysign(math.floor(abs(value) + 0.5), value))
    assert read(0x8008aeb0 + index * 2, "h")[0] == expected

fixture = Path(__file__).with_name("guest_sign_placements.h").read_text().split("signPlacements[] = {", 1)[1]
values = list(map(int, re.findall(r"-?\d+", fixture)))
assert len(values) == 18 * 26
for offset in range(0, len(values), 26):
    track, section, *data = values[offset:offset + 26]
    gate, following, previous = 0x80400000, 0x80400100, 0x80400200
    record, body, panel = 0x80600000, 0x80610000, 0x80610200
    write(gate + 4, "2I", previous, following)
    write(following + 4, "I", gate)
    for address, coords in zip((gate, following, previous), (data[0:3], data[3:6], data[6:9])):
        write(address + 12, "3i", *coords)
    write(record, "10i", 6, *([0] * 9))
    for register, value in ((UC_MIPS_REG_S0, gate), (UC_MIPS_REG_S1, record),
                            (UC_MIPS_REG_S4, 0), (UC_MIPS_REG_SP, 0x807ff000)):
        cpu.reg_write(register, value)
    # Original placement block, including its call to the original atan2.
    cpu.emu_start(0x80045d38, 0x80045e28, count=100000)
    call(0x800199b0, body, read(record + 32, "i")[0], 0, 0)
    xyz = read(record + 8, "3i")
    call(0x80019808, body, *xyz)
    write(body + 0x44, "I", 0)
    call(0x800199b0, panel, 0, 0, 0)
    call(0x80019808, panel, 0, 0, -600)
    write(panel + 0x44, "I", body)
    # Original parent composition, preserving its integer rounding.
    absolute = call(0x80019ce0, panel)
    result = list(read(body, "9h")) + [v * 2 for v in xyz] + [v * 2 for v in read(absolute + 20, "3i")]
    assert result == data[9:], f"Track {track}, checkpoint section {section}: {result} != {data[9:]}"
print("USA ROM: 18 checkpoint transforms and all 8193 trig lookup entries verified")
