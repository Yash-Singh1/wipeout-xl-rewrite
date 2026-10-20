#!/usr/bin/env python3
"""Record tests: the game's widening of 32-bit file records against an independent reader.

For every relevant data file of the PlayStation game (all PRMs, standalone and inside every WAD, the track
sections, vertices and faces of every track, CONFIG.DAT, SAVEGAME.BIN and MENU.DAT) and for random
records, this builds with the layouts clang reports for the PlayStation (MIPS o32) target and the native
target:
  expected  = what the native record must contain, computed here from the two layout dumps
              (every file field moved to its native offset, 4-byte pointer slots zero-extended into the
              8-byte slots, every padding byte zero), without using the generated tables;
  actual    = what tools/recordtest/recordtest (built from the game's port_widen.c and port_records.c)
              writes.
They must be equal byte for byte. The C program also walks each widened PRM image with the real
native structs the way LoadPrmCommon does, and checks narrow(widen(x)) == x.

Usage: python3 tools/recordtest/run.py [--iso DIR] [--seed N] [--keep]
"""
import argparse
import os
import random
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import genrecords as gl  # noqa: E402

DEFAULT_ISO = os.path.join(os.path.dirname(ROOT), "wipeout-xl", "disc", "root", "WIPEOUT2")

SCALARS = {"char": 1, "signed char": 1, "unsigned char": 1, "u_char": 1, "u8": 1, "s8": 1,
           "short": 2, "unsigned short": 2, "u_short": 2, "u16": 2, "s16": 2,
           "int": 4, "unsigned int": 4, "u_int": 4, "u_long": 4, "u32": 4, "s32": 4, "float": 4}


AGGREGATES = {}     # aggregate types used as array elements: {name: {pointer size: sizeof}}


def type_size(type_string, pointer_size):
    """Size of a leaf field from the type string of a layout dump."""
    dims = [int(n) for n in re.findall(r"\[(\d+)\]", type_string)]
    base = type_string.split("[")[0].strip()
    base = re.sub(r"^(struct|union) ", "", base)
    count = 1
    for d in dims:
        count *= d
    if "*" in base:
        return count * pointer_size
    if base in AGGREGATES:
        return count * AGGREGATES[base][pointer_size]
    if base not in SCALARS:
        raise SystemExit("run.py: unknown field type %r" % type_string)
    return count * SCALARS[base]


class Layouts:
    """The 32-bit and native layouts of the record types, read from the clang dumps."""

    TYPES = {"functions.h": ["Object", "Skeleton", "TrackSection", "Face", "VECTOR", "SVECTOR"] + gl.PRIMITIVES}
    # Records that only occur as array elements inside the ones above (they hold no pointers).
    ELEMENTS = {"functions.h": ["CVECTOR"]}
    PRIM_SIZE = {}

    def __init__(self, workdir):
        self.native = gl.host_target()
        self.items = {}
        self.size32 = {}
        self.sizen = {}
        for source, types in self.TYPES.items():
            for e in self.ELEMENTS[source]:
                _, _, esizes = gl.analyse(source, [e], self.native, workdir)
                if esizes[e][gl.FILE_TARGET] != esizes[e][self.native]:
                    raise SystemExit("%s changes size and is used as an array element" % e)
                AGGREGATES[e] = {4: esizes[e][gl.FILE_TARGET], 8: esizes[e][self.native]}
            trees, _, sizes = gl.analyse(source, types, self.native, workdir)
            for t in types:
                self.items[t] = (trees[t][gl.FILE_TARGET], trees[t][self.native])
                self.size32[t] = sizes[t][gl.FILE_TARGET]
                self.sizen[t] = sizes[t][self.native]

    def expected(self, t, record):
        """The native bytes of one file record of type t."""
        out = bytearray(self.sizen[t])
        self._apply(self.items[t][0], self.items[t][1], record, out)
        return bytes(out)

    def _apply(self, items32, itemsn, record, out):
        assert len(items32) == len(itemsn)
        for a, b in zip(items32, itemsn):
            if isinstance(a, gl.Leaf):
                self._leaf(a, b, record, out)
                continue
            leaves32 = list(gl.leaves([x for m in a.members for x in m]))
            leavesn = list(gl.leaves([x for m in b.members for x in m]))
            s32 = max(type_size(l.node.type, 4) for l in leaves32)
            sn = max(type_size(l.node.type, 8) for l in leavesn)
            off32, offn = a.node.offset, b.node.offset
            if any(l.pointer for l in leaves32):
                assert s32 == 4 and sn == 8, "union with a pointer"
                out[offn:offn + 4] = record[off32:off32 + 4]
            else:
                assert s32 == sn
                out[offn:offn + sn] = record[off32:off32 + s32]

    def _leaf(self, a, b, record, out):
        s32 = type_size(a.node.type, 4)
        sn = type_size(b.node.type, 8)
        off32, offn = a.node.offset, b.node.offset
        if a.pointer:
            assert sn == s32 * 2
            for k in range(s32 // 4):
                out[offn + 8 * k:offn + 8 * k + 4] = record[off32 + 4 * k:off32 + 4 * k + 4]
        else:
            assert s32 == sn, "%s changes size" % a.path
            out[offn:offn + sn] = record[off32:off32 + s32]

    def pointer_slots(self, t):
        """(file offset, byte count) of every pointer field, for statistics."""
        result = []
        for it in self.items[t][0]:
            for l in (gl.leaves([it]) if isinstance(it, (gl.Leaf, gl.UnionItem)) else []):
                if l.pointer:
                    result.append((l.node.offset, type_size(l.node.type, 4), l.path))
        return result


def align8(n):
    return (n + 7) & ~7


def be16(data, pos):
    return struct.unpack_from(">h", data, pos)[0]


class Failure(Exception):
    pass


class Runner:
    def __init__(self, args, layouts, workdir):
        self.args = args
        self.layouts = layouts
        self.workdir = workdir
        self.exe = os.path.join(workdir, "recordtest")
        self.checks = 0
        self.failures = []
        self.stats = {}
        self.pointer_values = {}
        self.pointer_records = {}

    # -- building ----------------------------------------------------------------------------
    def build(self):
        resource = subprocess.check_output(["clang", "-print-resource-dir"], text=True).strip()
        cmd = ["clang", "-std=gnu11", "-fsigned-char", "-fno-strict-aliasing", "-fwrapv", "-g", "-O1",
               "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
               "-include", os.path.join(ROOT, "platform/include/port.h"),
               "-I" + os.path.join(ROOT, "game/include"),
               "-Wno-incompatible-pointer-types", "-Wno-int-conversion", "-Wno-pointer-sign",
               "-Wno-implicit-function-declaration", "-w",
               os.path.join(HERE, "recordtest.c"), os.path.join(ROOT, "game/src/port_widen.c"),
               os.path.join(ROOT, "game/src/port_records.c"), os.path.join(ROOT, "game/src/malloc.c"),
               "-o", self.exe]
        subprocess.run(cmd, check=True)

    def c(self, *argv):
        proc = subprocess.run([self.exe] + list(argv), capture_output=True, text=True)
        return proc.returncode, (proc.stdout + proc.stderr).strip()

    # -- checking ----------------------------------------------------------------------------
    def fail(self, what):
        self.failures.append(what)
        print("FAIL " + what)

    def ok(self):
        self.checks += 1

    def stat(self, key, n=1):
        self.stats[key] = self.stats.get(key, 0) + n

    def check_records(self, label, t, data):
        """Widen data (an array of 32-bit records of type t) with the C code and compare."""
        L = self.layouts
        size = L.size32[t]
        if len(data) % size:
            self.fail("%s: %d bytes is not a whole number of %s records (%d)" % (label, len(data), t, size))
            return
        src = os.path.join(self.workdir, "in.bin")
        dst = os.path.join(self.workdir, "out.bin")
        open(src, "wb").write(data)
        code, text = self.c("widen-records", t, src, dst)
        if code:
            self.fail("%s: %s" % (label, text))
            return
        actual = open(dst, "rb").read()
        count = len(data) // size
        expected = b"".join(L.expected(t, data[i * size:(i + 1) * size]) for i in range(count))
        if actual != expected:
            at = next(i for i in range(len(expected)) if i >= len(actual) or actual[i] != expected[i])
            self.fail("%s: widened %s records differ from the expected layout (record %d, byte %d)"
                      % (label, t, at // L.sizen[t], at % L.sizen[t]))
            return
        if not label.startswith("random"):
            self.count_pointer_slots(t, [data[i * size:(i + 1) * size] for i in range(count)])
        code, text = self.c("roundtrip", t, src)
        if code:
            self.fail("%s: %s" % (label, text))
            return
        self.ok()
        if not label.startswith("random"):
            self.stat("records of the game's files: %s" % t, count)

    def count_pointer_slots(self, t, records):
        """Counts, per pointer field, the file records in which the slot holds a value."""
        for off, n, path in self.layouts.pointer_slots(t):
            self.pointer_values[(t, path)] = self.pointer_values.get((t, path), 0) + sum(1 for r in records if any(r[off:off + n]))
        self.pointer_records[t] = self.pointer_records.get(t, 0) + len(records)

    def expected_prm(self, data, real=False):
        """The widened image of a PRM file, computed with the layouts and a walk of the file written here."""
        L = self.layouts
        sizes = {t: L.size32[n] for t, n in PRIM_TYPES.items()}
        out = bytearray()
        pos = 0
        objects = 0
        osz, ssz = L.size32["Object"], L.size32["Skeleton"]
        while pos < len(data):
            if len(data) - pos < osz + ssz:
                break
            vertices, prims = be16(data, pos + 16), be16(data, pos + 32)
            end = pos + osz + ssz + vertices * L.size32["SVECTOR"]
            if vertices < 0 or prims < 0 or end > len(data):
                break
            bad = False
            for _ in range(prims):
                if end + 2 > len(data) or be16(data, end) not in sizes or end + sizes[be16(data, end)] > len(data):
                    bad = True
                    break
                end += sizes[be16(data, end)]
            if bad:
                break
            while len(out) % 8:
                out.append(0)
            if real:
                self.count_pointer_slots("Object", [data[pos:pos + osz]])
                self.count_pointer_slots("Skeleton", [data[pos + osz:pos + osz + ssz]])
            out += L.expected("Object", data[pos:pos + osz])
            out += L.expected("Skeleton", data[pos + osz:pos + osz + ssz])
            out += data[pos + osz + ssz:end]
            objects += 1
            pos = end
        out += data[pos:]
        return bytes(out), objects, pos == len(data) and objects > 0

    def check_prm(self, label, data):
        src = os.path.join(self.workdir, "in.prm")
        dst = os.path.join(self.workdir, "out.prm")
        open(src, "wb").write(data)
        expected, objects, complete = self.expected_prm(data, not label.startswith("random"))
        code, text = self.c("widen-prm", src, dst)
        if code and complete:
            self.fail("%s: %s" % (label, text))
            return None
        if code and not complete and not text.startswith("prm file"):
            self.fail("%s: %s" % (label, text))
            return None
        if not complete and not label.startswith("random"):
            self.stat("files that are not PRM images (copied or only partly widened, never loaded by the game)")
        actual = open(dst, "rb").read()
        if actual != expected:
            at = next((i for i in range(min(len(actual), len(expected))) if actual[i] != expected[i]),
                      min(len(actual), len(expected)))
            self.fail("%s: widened PRM image differs from the expected one (lengths %d and %d, first difference at %d)"
                      % (label, len(actual), len(expected), at))
            return None
        code, text2 = self.c("roundtrip-prm", src)
        if code:
            self.fail("%s: %s" % (label, text2))
            return None
        self.ok()
        if not label.startswith("random"):
            self.stat("PRM objects in the game's files", objects)
            self.stat("PRM bytes in the game's files", len(data))
            self.stat("PRM bytes widened", len(actual))
        return objects, len(data), len(actual), text, text2


# The tracks the game races (raceTracks in menus.c); TRACK04 is on the disc but never loaded.
RACED_TRACKS = (1, 2, 6, 7, 8, 13, 17, 20)

PRIM_TYPES = {1: "F3", 2: "FT3", 3: "F4", 4: "FT4", 5: "G3", 6: "GT3", 7: "G4", 8: "GT4", 10: "SPR", 11: "SPR",
              12: "LSF3", 13: "LSFT3", 14: "LSF4", 15: "LSFT4", 16: "LSG3", 17: "LSGT3", 18: "LSG4", 19: "LSGT4",
              20: "Spline", 21: "InfiniteLight", 22: "PointLight", 23: "SpotLight"}


def read_wad(path):
    """The directory entries (name, bytes) of a WAD file, as openwadfile reads them."""
    d = open(path, "rb").read()
    n = struct.unpack_from("<H", d, 0)[0]
    pos = 2
    entries = []
    for _ in range(n):
        name = d[pos:pos + 16].split(b"\0")[0].decode()
        size = struct.unpack_from("<I", d, pos + 20)[0]
        entries.append((name, size))
        pos += 25
    out = []
    for name, size in entries:
        out.append((name, d[pos:pos + size]))
        pos += size
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--iso", default=DEFAULT_ISO, help="directory with the PlayStation game's WIPEOUT2 files")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--fuzz", type=int, default=40, help="random records per type")
    parser.add_argument("--keep", action="store_true", help="keep the work directory")
    args = parser.parse_args()
    if not os.path.isdir(args.iso):
        raise SystemExit("no game data in %s (use --iso)" % args.iso)
    subprocess.run([sys.executable, os.path.join(ROOT, "tools/genrecords.py"), "--check"], check=True)
    with tempfile.TemporaryDirectory() as workdir:
        layouts = Layouts(workdir)
        runner = Runner(args, layouts, workdir)
        runner.build()
        for t, n in PRIM_TYPES.items():
            Layouts.PRIM_SIZE[t] = layouts.size32[n]
        for t in ("Object", "Skeleton", "TrackSection"):
            print("%-13s file %3d bytes, native %3d bytes" % (t, layouts.size32[t], layouts.sizen[t]))

        # PRMs: the standalone files and the ones inside every WAD.
        prm_rows = []
        for path in sorted(os.listdir(os.path.join(args.iso, "COMMON"))):
            if path.upper().endswith(".PRM"):
                data = open(os.path.join(args.iso, "COMMON", path), "rb").read()
                row = runner.check_prm("COMMON/" + path, data)
                if row:
                    prm_rows.append(("COMMON/" + path,) + row[:3])
        tracks = sorted(d for d in os.listdir(args.iso) if re.match(r"TRACK\d\d$", d))
        track_rows = []
        for track in tracks:
            wad = os.path.join(args.iso, track, "TRACK.WAD")
            if not os.path.exists(wad):
                continue
            entries = read_wad(wad)
            row = {"track": track}
            for name, data in entries:
                label = "%s/TRACK.WAD:%s" % (track, name)
                if name.endswith(".prm"):
                    r = runner.check_prm(label, data)
                    if r:
                        row[name] = r[:3]
                elif name == "track.trs":
                    runner.check_records(label, "TrackSection", data)
                    row[name] = (len(data) // layouts.size32["TrackSection"], len(data),
                                 len(data) // layouts.size32["TrackSection"] * layouts.sizen["TrackSection"])
                elif name == "track.trv":
                    runner.check_records(label, "VECTOR", data)
                elif name == "track.trf":
                    runner.check_records(label, "Face", data)
            track_rows.append(row)
        # Random records of every table type, pointer slots and padding included.
        rng = random.Random(args.seed)
        for t in sorted(layouts.items):
            data = bytes(rng.randrange(256) for _ in range(layouts.size32[t] * args.fuzz))
            runner.check_records("random %s" % t, t, data)
        # Random PRM images: valid objects with random contents.
        for n in range(args.fuzz // 4):
            runner.check_prm("random PRM %d" % n, random_prm(rng, layouts))
        # Damaged files must still widen without reading out of bounds.
        for n in range(args.fuzz):
            data = bytearray(random_prm(rng, layouts))
            for _ in range(rng.randrange(1, 6)):
                data[rng.randrange(len(data))] = rng.randrange(256)
            data = bytes(data[:rng.randrange(len(data) + 1)] if rng.random() < 0.3 else data)
            code, text = runner.c("widen-prm", write(workdir, "bad.prm", data), os.path.join(workdir, "bad.out"))
            if "AddressSanitizer" in text or "runtime error" in text:
                runner.fail("damaged PRM %d: %s" % (n, text.splitlines()[0]))
            else:
                runner.ok()
        check_loaders(runner, args, workdir)
        report(runner, prm_rows, track_rows, layouts)
        if args.keep:
            print("work directory:", workdir)
        if runner.failures:
            print("%d of %d checks FAILED" % (len(runner.failures), runner.checks + len(runner.failures)))
            sys.exit(1)
        print("recordtest: %d checks passed" % runner.checks)


def s16(data, pos):
    return struct.unpack_from(">h", data, pos)[0]


def s32(data, pos):
    return struct.unpack_from(">i", data, pos)[0]


def expected_dump(entries, track, sky):
    """The lines loadtest writes for a track, computed from the WAD's files by reading the 32-bit records."""
    by_name = dict(entries)
    sections = by_name["track.trs"]
    vertices = by_name["track.trv"]
    faces = by_name["track.trf"]
    lines = ["T %d sky %d" % (track, sky)]
    lines.append("N sections %d vertices %d faces %d" % (len(sections) // 156, len(vertices) // 16, len(faces) // 20))
    total = 0
    for i in range(len(sections) // 156):
        r = sections[i * 156:(i + 1) * 156]
        counts = [[s16(r, base + 2 * j) for j in range(3)] for base in (96, 102, 108, 114, 120)]
        views = [[0] * 3 for _ in range(5)]
        for j in range(3):
            for g in range(5):
                views[g][j] = total
                total += counts[g][j]
        line = "S %d version %d pad %d centre %d %d %d" % (i, s16(r, 24), s16(r, 26), s32(r, 12), s32(r, 16), s32(r, 20))
        line += " prev %d next %d junction %d" % (s32(r, 4), s32(r, 8), s32(r, 0))
        line += " face %d %d radius %d %d flags %d secNo %d bump %d snow %d" % (
            s16(r, 142), s16(r, 144), s16(r, 146), s16(r, 148), s16(r, 150), s16(r, 152),
            struct.unpack_from("b", r, 154)[0], struct.unpack_from("b", r, 155)[0])
        line += " counts " + " ".join(str(c) for g in counts for c in g)
        line += " views " + " ".join(str(v) for g in views for v in g)
        line += " high %s med %s" % (" ".join(str(s16(r, 126 + 2 * k)) for k in range(4)),
                                      " ".join(str(s16(r, 134 + 2 * k)) for k in range(4)))
        lines.append(line)
    for i in range(len(vertices) // 16):
        lines.append("V %d %d %d %d" % ((i,) + struct.unpack_from(">iii", vertices, i * 16)))
    for i in range(len(faces) // 20):
        r = faces[i * 20:(i + 1) * 20]
        v = struct.unpack_from(">4h", r, 0)
        n = struct.unpack_from(">3h", r, 8)
        texture, flags = struct.unpack_from("bb", r, 14)
        red, green, blue = r[16], r[17], r[18]
        lines.append("F %d %d %d %d %d n %d %d %d tex %d flags %d rgb %d %d %d copy %d %d %d" % (
            (i,) + v + n + (texture, flags, red, green, blue, red, green, blue)))
    for index, name in enumerate(("scene.prm", "sky.prm")):
        d = by_name[name]
        pos = 0
        while pos < len(d):
            vertices_n, prims = s16(d, pos + 16), s16(d, pos + 32)
            label = d[pos:pos + 16].split(b"\0")[0].decode("latin-1")
            line = "O %d %s extent %d primitives %d rel" % (index, label, s32(d, pos + 52), prims)
            line += "".join(" %d" % s16(d, pos + 64 + 2 * k) for k in range(9))
            # WADInitScene moves the sky model to (0, skyPos[track], 0): the value is the game's table, printed in the header
            relative_t = "0 %d 0" % sky if index == 1 and pos == 0 else " ".join(str(s32(d, pos + 64 + 20 + 4 * k)) for k in range(3))
            line += " t %s abs %s update %d" % (relative_t,
                                                " ".join(str(s32(d, pos + 64 + 52 + 4 * k)) for k in range(3)),
                                                s16(d, pos + 64 + 64))
            lines.append(line)
            end = pos + 144 + vertices_n * 8
            for _ in range(prims):
                end += Layouts.PRIM_SIZE[s16(d, end)]
            pos = end
    return lines


LOADTEST_HEADERS = ["functions.h", "track.h", "hud.h", "ayrace.h", "hiscores.h", "effects.h", "weapon.h", "sparks.h", "trails.h", "dynam.h"]

# Findings in the game itself that the sanitizers report and that are documented (docs/RECORDS.md).
# (kind, function that must be on the stack, what it is)
KNOWN_FINDINGS = [
    ("stack-buffer-overflow", "loadShipData",
     "ships.c loadShipData: strcat appends .cmp/.prm to a 24-character path in the 25-character ShipPath union (28 bytes): "
     "the terminating zero lands one byte after the variable (also read by the test's file name conversion)"),
]


def sanitizer_reports(text):
    """Distinct sanitizer reports of a run: (kind, frames, detail)."""
    reports = []
    seen = set()
    lines = text.split("\n")
    i = 0
    while i < len(lines):
        m = re.search(r"ERROR: AddressSanitizer: (\S+)", lines[i])
        if m:
            frames = []
            j = i + 1
            while j < len(lines) and len(frames) < 8:
                f = re.match(r"\s+#\d+ 0x[0-9a-f]+ in (\S+)", lines[j])
                if f:
                    frames.append(f.group(1).split("+")[0])
                elif frames and not lines[j].startswith(" "):
                    break
                j += 1
            key = (m.group(1), tuple(frames[:4]))
            if key not in seen:
                seen.add(key)
                reports.append((m.group(1), frames, lines[i]))
        m = re.search(r"([\w/\.\-]+:\d+:\d+): runtime error: (.*)", lines[i])
        if m:
            key = ("ubsan", m.group(1).split("/")[-1], m.group(2)[:60])
            if key not in seen:
                seen.add(key)
                reports.append(("runtime error", [m.group(1).split("/")[-1]], m.group(2)))
        i += 1
    return reports


def ps1_sizes(types, workdir):
    """Writes the sizes file of loadtest: name, PlayStation size, native size."""
    path = os.path.join(workdir, "loadtest_sizes.c")
    with open(path, "w") as f:
        for h in LOADTEST_HEADERS:
            f.write('#include "%s"\n' % h)
        f.write("const unsigned PortProbe[] = {\n")
        for t in types:
            f.write("    sizeof(%s),\n" % t)
        f.write("};\n")
    values = {}
    for target in (gl.FILE_TARGET, gl.host_target()):
        asm = gl.run_clang(target, path, ["-S", "-o", "-", "-O0"])
        nums = gl.asm_values(asm)
        if len(nums) != len(types):
            raise SystemExit("sizes of the loadtest types: expected %d values, got %d" % (len(types), len(nums)))
        values[target] = nums
    out = os.path.join(workdir, "loadtest_sizes.txt")
    with open(out, "w") as f:
        for k, t in enumerate(types):
            f.write("%s %d %d\n" % (t, values[gl.FILE_TARGET][k], values[gl.host_target()][k]))
    return out


def check_site_table(runner, sites_text):
    """Every DAlloc in game/src is in loadtest's table (which says how the original's size is derived)."""
    sites = [tuple(l.split("\t")) for l in sites_text.strip().split("\n")]
    uncovered = []
    count = 0
    for fn in sorted(os.listdir(os.path.join(ROOT, "game/src"))):
        if not fn.endswith(".c") or fn == "malloc.c":
            continue
        for n, line in enumerate(open(os.path.join(ROOT, "game/src", fn), errors="replace"), 1):
            code = line.split("/*")[0]
            if "DAlloc(" not in code or code.strip().startswith(("//", "*")):
                continue
            count += 1
            if not any(f == fn and t in line for f, t in sites):
                uncovered.append("%s:%d: %s" % (fn, n, line.strip()))
    if uncovered:
        runner.fail("heap calls that loadtest.c's site table does not know (it must say how the original's size follows from the native one):\n  " + "\n  ".join(uncovered))
    else:
        runner.ok()
        runner.stat("DAlloc calls in game/src, all classified in the load test", count)


def check_loaders(runner, args, workdir):
    """Runs the game's own start-up, menu and race loading (tools/recordtest/loadtest) and compares what it built."""
    exe = os.path.join(workdir, "loadtest")
    build = subprocess.run([sys.executable, os.path.join(HERE, "build_loadtest.py"), exe], capture_output=True, text=True)
    if build.returncode:
        runner.fail("loadtest could not be built:\n" + build.stderr[-3000:] + build.stdout[-1000:])
        return
    env = dict(os.environ, LOADTEST_GAME_SRC=os.path.join(ROOT, "game/src"), ASAN_OPTIONS="detect_leaks=0:halt_on_error=0",
               UBSAN_OPTIONS="print_stacktrace=0")
    sites = subprocess.run([exe, "--list-sites"], capture_output=True, text=True, env=env).stdout
    check_site_table(runner, sites)
    types = subprocess.run([exe, "--list-types"], capture_output=True, text=True, env=env).stdout.split()
    sizes = ps1_sizes(types, workdir)

    # the sanitizers must catch what the heap poisoning is for
    selftest = subprocess.run([exe, args.iso, "--sizes", sizes, "--selftest"], capture_output=True, text=True, env=env)
    expected_after = ["write one byte after the block", "write to a freed block", "write before the block (into its header)"]
    got = [m.group(1) for m in re.finditer(r"selftest: (write [^\n]*)\n==\d+==ERROR: AddressSanitizer: use-after-poison", selftest.stderr)]
    inside = re.search(r"selftest: write inside the block\nselftest: write one byte", selftest.stderr)
    if got == expected_after and inside:
        runner.ok()
    else:
        runner.fail("loadtest --selftest: the heap poisoning did not report exactly the writes outside the block (got %r)" % got)

    # the run: once as it is, once with every new block filled with 0xCD: the loaders must not read bytes they did not write
    outputs = {}
    for label, extra in (("plain", []), ("filled", ["--fill", "CD"])):
        dump = os.path.join(workdir, "dump_" + label)
        os.mkdir(dump)
        proc = subprocess.run([exe, args.iso, "--sizes", sizes, "--dump", dump] + extra, capture_output=True, text=True, env=env)
        outputs[label] = (proc, dump)
        if label == "plain":
            print()
            print(proc.stdout.rstrip())
        if proc.returncode:
            runner.fail("loadtest (%s): the loaders failed, the heap model disagreed or the heaps are too small (exit %d):\n%s" % (
                label, proc.returncode, "\n".join(l for l in proc.stdout.split("\n") if l.startswith(("PROBLEM", "NOTE")) or "FAILED" in l)))
        else:
            runner.ok()
    # sanitizer reports of the plain run: only the documented findings are allowed
    proc = outputs["plain"][0]
    found = []
    for kind, frames, detail in sanitizer_reports(proc.stderr):
        known = [what for k, fn, what in KNOWN_FINDINGS if kind == k and fn in frames]
        if known:
            found.append(known[0])
        else:
            runner.fail("sanitizer report in the game's loading: %s in %s: %s" % (kind, " < ".join(frames[:4]), detail))
    runner.stat("distinct documented findings the sanitizers report in the game's loading", len(set(found)))
    if not any(l.startswith("PROBLEM") for l in proc.stdout.split("\n")):
        runner.ok()
    # what the loaders built, against the files
    for track in sorted(d for d in os.listdir(args.iso) if re.match(r"TRACK\d\d$", d)):
        number = int(track[5:])
        if number not in RACED_TRACKS:
            continue
        entries = read_wad(os.path.join(args.iso, track, "TRACK.WAD"))
        for label in ("plain", "filled"):
            path = os.path.join(outputs[label][1], "track%02d.txt" % number)
            if not os.path.exists(path):
                runner.fail("%s: loadtest (%s) wrote no dump" % (track, label))
                continue
            actual = open(path, encoding="latin-1").read().split("\n")[:-1]
            sky = int(actual[0].split()[3]) if actual and actual[0].startswith("T ") else 0
            expected = expected_dump(entries, number, sky)
            if actual != expected:
                at = next((i for i in range(min(len(actual), len(expected))) if actual[i] != expected[i]), min(len(actual), len(expected)))
                runner.fail("%s (%s): loaded track differs from the file at line %d\n  loaded:   %s\n  expected: %s" % (
                    track, label, at, actual[at] if at < len(actual) else "(missing)", expected[at] if at < len(expected) else "(missing)"))
            else:
                runner.ok()
                if label == "plain":
                    runner.stat("loaded sections, vertices, faces and objects compared with the files", len(actual) - 2)


def write(workdir, name, data):
    path = os.path.join(workdir, name)
    open(path, "wb").write(data)
    return path


def random_prm(rng, layouts):
    osz, ssz = layouts.size32["Object"], layouts.size32["Skeleton"]
    out = bytearray()
    for _ in range(rng.randrange(1, 5)):
        obj = bytearray(rng.randrange(256) for _ in range(osz + ssz))
        vertices = rng.randrange(0, 6)
        types = sorted(PRIM_TYPES)
        prims = []
        for _ in range(rng.randrange(0, 6)):
            t = rng.choice(types)
            prims.append(struct.pack(">h", t) + bytes(rng.randrange(256) for _ in range(layouts.size32[PRIM_TYPES[t]] - 2)))
        obj[16:18] = struct.pack(">h", vertices)
        obj[32:34] = struct.pack(">h", len(prims))
        out += obj
        out += bytes(rng.randrange(256) for _ in range(vertices * 8))
        for p in prims:
            out += p
    return bytes(out)


def report(runner, prm_rows, track_rows, layouts):
    print()
    print("%-28s %8s %10s %10s %8s" % ("PRM", "objects", "file", "widened", "growth"))
    for label, objects, file_len, wide in prm_rows:
        print("%-28s %8d %10d %10d %+8d" % (label, objects, file_len, wide, wide - file_len))
    print()
    print("%-9s %-10s %8s %10s %10s %8s" % ("track", "entry", "records", "file", "widened", "growth"))
    for row in track_rows:
        for name in ("scene.prm", "sky.prm", "track.trs"):
            if name in row:
                a, b, c = row[name]
                print("%-9s %-10s %8d %10d %10d %+8d" % (row["track"], name, a, b, c, c - b))
    print()
    for k in sorted(runner.stats):
        print("%-48s %d" % (k, runner.stats[k]))
    print()
    print("pointer slots of the game's files that hold a value (the loaders overwrite or never read them)")
    for (t, path), n in sorted(runner.pointer_values.items()):
        if n:
            print("  %-26s %5d of %d records" % (t + "." + path, n, runner.pointer_records[t]))


if __name__ == "__main__":
    main()
