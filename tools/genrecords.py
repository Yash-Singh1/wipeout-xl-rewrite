#!/usr/bin/env python3
"""Generate game/src/port_records.c and game/include/port_records.h.

The original game reads file contents straight into C structs (PRM objects and
skeletons, track sections, saved configuration...). The files hold the 32-bit
PlayStation layout of those structs; on a 64-bit target the structs that contain
pointers are larger. This script asks clang for both layouts of every file
record type listed in RECORDS and writes, for each record type, its size in
both layouts and its fields (offset and size in each layout, and whether the
field is a pointer slot). game/src/port_widen.c converts between the layouts
with these tables.

How it works:
  1. For each source file in RECORDS a probe translation unit includes that file
     and wraps every record type in `struct ProbeWrap_NAME { NAME value; }`.
  2. clang -fdump-record-layouts-complete is run on it for the file target
     (mipsel o32: the layout of the PlayStation executable) and for
     the native target. The dumps give the nesting, the names and the type
     strings of every field.
  3. A second probe turns each leaf field into sizeof/offsetof constants and
     clang -S evaluates them for both targets. They must agree with the dumps.
  4. Unions that hold a pointer next to an int alias of it (SectionPtr) become
     one pointer slot: the 32-bit file slot is widened by zero extension, so
     the int member still reads the file value.

Run from the repository root:  python3 tools/genrecords.py
Options: --check   do not write; fail if the generated files are out of date
         --cross   also check that a second 64-bit target (x86_64 Linux) has
                   the same native layout as the host target
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILE_TARGET = "mipsel-unknown-none-elf"   # the PlayStation (MIPS o32, little endian, 32-bit pointers)
LLVM_CLANG = "/opt/homebrew/opt/llvm/bin/clang"

PRIMITIVES = ["F3", "FT3", "F4", "FT4", "G3", "GT3", "G4", "GT4", "LSF3", "LSFT3", "LSF4", "LSFT4",
              "LSG3", "LSGT3", "LSG4", "LSGT4", "SPR", "Spline", "PointLight", "SpotLight", "InfiniteLight"]

# (header that defines the type, C type, role[, extra header to include]).  "functions.h" declares the shared types.
RECORDS = [
    ("functions.h", "Object", "PRM object record"),
    ("functions.h", "Skeleton", "PRM skeleton record"),
    ("functions.h", "TrackSection", "TRS track section record"),
    ("functions.h", "Face", "TRF track face record"),
    ("functions.h", "VECTOR", "TRV track vertex record"),
    ("functions.h", "SVECTOR", "PRM vertex record"),
] + [("functions.h", p, "PRM primitive record") for p in PRIMITIVES] + [
]
RECORDS = [r if len(r) == 4 else r + ("",) for r in RECORDS]


class Node:
    def __init__(self, offset, text, bitfield, depth):
        self.offset = offset
        self.bitfield = bitfield
        self.depth = depth
        self.children = []
        self.sizeof = None
        if text.endswith(" "):          # anonymous member: the line ends after the type
            self.name = ""
            self.type = text.strip()
        else:
            m = re.match(r"^(.*?)([A-Za-z_][A-Za-z_0-9]*)$", text)
            if m is None or text.endswith(")") and "(unnamed" in text.rsplit(" ", 1)[-1]:
                self.type = text.strip()
                self.name = ""
            else:
                self.type = m.group(1).strip()
                self.name = m.group(2)
        self.text = text


def parse_dump(text):
    """Return {wrapper record name: Node of the record} for the ProbeWrap_* records."""
    records = {}
    stack = None
    for raw in text.split("\n"):
        if raw.startswith("*** Dumping"):
            stack = None
            continue
        size = re.match(r"^\s*\| \[sizeof=(\d+)", raw)
        if size:
            if stack:
                stack[0].sizeof = int(size.group(1))
            continue
        m = re.match(r"^\s*(\d+)(:\d+-\d+)? \| ( *)(.*)$", raw)
        if not m:
            continue
        depth = len(m.group(3)) // 2
        node = Node(int(m.group(1)), m.group(4), m.group(2) is not None, depth)
        if depth == 0:
            stack = [node]
            wrap = re.match(r"^(?:struct|union) (ProbeWrap_\w+)$", node.text)
            if wrap:
                records[wrap.group(1)] = node
            else:
                stack = None
            continue
        if stack is None:
            continue
        del stack[depth:]
        stack[-1].children.append(node)
        stack.append(node)
    return records


def is_union(node):
    if node.type.startswith("union "):
        return True
    if node.type.startswith("struct ") or len(node.children) < 2:
        return False
    return all(c.offset == node.offset for c in node.children)


def is_pointer_type(type_string):
    return "*" in type_string


def join(path, name):
    if not name:
        return path
    return name if not path else path + "." + name


class Leaf:
    def __init__(self, path, node):
        self.path = path
        self.node = node
        self.pointer = is_pointer_type(node.type)


class UnionItem:
    def __init__(self, path, node, members):
        self.path = path
        self.node = node
        self.members = members      # list of lists of items


def flatten(node, path):
    if node.bitfield:
        raise SystemExit("bit-field %s: not supported by the record tables" % (path or node.text))
    if not node.children:
        return [Leaf(path, node)]
    if is_union(node):
        return [UnionItem(path, node, [flatten(c, join(path, c.name)) for c in node.children])]
    items = []
    for c in node.children:
        items.extend(flatten(c, join(path, c.name)))
    return items


def leaves(items):
    for it in items:
        if isinstance(it, Leaf):
            yield it
        else:
            for member in it.members:
                yield from leaves(member)


def clang_base(target):
    if target.startswith("mips"):
        return [LLVM_CLANG, "-target", target, "-march=mips1", "-mabi=o32", "-ffreestanding", "-nostdinc",
                "-std=gnu11", "-fsigned-char", "-include", os.path.join(ROOT, "tools/oracle32/o32.h"),
                "-I" + os.path.join(ROOT, "game/include"), "-I" + os.path.join(ROOT, "game/src"),
                "-w", "-fno-ident", "-Wno-error"]
    return ["clang", "-std=gnu11", "-fsigned-char", "-include", os.path.join(ROOT, "platform/include/port.h"),
            "-I" + os.path.join(ROOT, "game/include"), "-I" + os.path.join(ROOT, "game/src"),
            "-w", "-fno-ident", "-Wno-error"]


def run_clang(target, source_path, extra, tolerant=False):
    cmd = clang_base(target) + extra + [source_path]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0 and not (tolerant and proc.stdout):
        sys.stderr.write(proc.stderr)
        raise SystemExit("clang failed: " + " ".join(cmd))
    return proc.stdout


def host_target():
    return subprocess.check_output(["clang", "-print-target-triple"], text=True).strip()


def probe_source(source, types):
    lines = ['#include "%s"' % source]
    for t in types:
        lines.append("struct ProbeWrap_%s { %s value; };" % (t, t))
    return "\n".join(lines) + "\n"


def asm_values(asm):
    """The .long values of PortProbe, in order."""
    values = []
    inside = False
    for line in asm.split("\n"):
        if re.match(r"^_?PortProbe:", line):
            inside = True
            continue
        if inside:
            m = re.match(r"^\s*\.(?:long|4byte)\s+(-?\d+)", line)
            if m:
                values.append(int(m.group(1)) & 0xffffffff)
            elif re.match(r"^\s*\.(p2align|align|zero)", line):
                continue
            elif re.match(r"^\s*\.(size|section|globl|type|def|scl|endef|addrsig)", line) or re.match(r"^\w", line):
                if values:
                    break
    return values


def analyse(source, types, native, workdir):
    """Return {type: (Item list from the dump, probe path list)} for both targets."""
    src_path = os.path.join(workdir, "probe_%s.c" % os.path.splitext(source)[0])
    base = probe_source(source, types)
    open(src_path, "w").write(base)
    dumps = {}
    for target in (FILE_TARGET, native):
        text = run_clang(target, src_path, ["-Xclang", "-fdump-record-layouts-complete", "-fsyntax-only"])
        dumps[target] = parse_dump(text)
    result = {}
    sizeofs = {}
    all_paths = []
    for t in types:
        trees = {}
        for target in (FILE_TARGET, native):
            wrap = dumps[target].get("ProbeWrap_" + t)
            if wrap is None or len(wrap.children) != 1:
                raise SystemExit("no layout for %s in %s" % (t, source))
            trees[target] = flatten(wrap.children[0], "")
        # Both targets must have the same field structure.
        paths32 = [l.path for l in leaves(trees[FILE_TARGET])]
        pathsN = [l.path for l in leaves(trees[native])]
        if paths32 != pathsN:
            raise SystemExit("%s: field structure differs between targets" % t)
        result[t] = trees
        sizeofs[t] = {target: dumps[target]["ProbeWrap_" + t].sizeof for target in (FILE_TARGET, native)}
        all_paths.append((t, paths32))
    # Second probe: sizeof and offsetof of every leaf, evaluated by the compiler.
    probe = [base, "const unsigned PortProbe[] = {"]
    index = []
    for t, paths in all_paths:
        probe.append("    sizeof(%s)," % t)
        index.append((t, None, "size"))
        for p in paths:
            probe.append("    (unsigned)sizeof(((%s *)0)->%s), (unsigned)__builtin_offsetof(%s, %s)," % (t, p, t, p))
            index.append((t, p, "size"))
            index.append((t, p, "offset"))
    probe.append("};")
    open(src_path, "w").write("\n".join(probe) + "\n")
    values = {}
    for target in (FILE_TARGET, native):
        asm = run_clang(target, src_path, ["-S", "-o", "-", "-O0"])
        nums = asm_values(asm)
        if len(nums) != len(index):
            raise SystemExit("probe for %s: expected %d values, got %d" % (target, len(index), len(nums)))
        values[target] = {key: n for key, n in zip(index, nums)}
    return result, values, sizeofs


class Field:
    def __init__(self, name, off32, size32, offn, sizen, pointer):
        self.name = name
        self.off32 = off32
        self.size32 = size32
        self.offn = offn
        self.sizen = sizen
        self.pointer = pointer


def build_record(t, role, trees, values, native):
    v32 = values[FILE_TARGET]
    vn = values[native]
    fields = []

    def probe(path):
        return ((v32[(t, path, "offset")], v32[(t, path, "size")]),
                (vn[(t, path, "offset")], vn[(t, path, "size")]))

    def add_leaf(leaf, name):
        (o32, s32), (on, sn) = probe(leaf.path)
        if leaf.pointer:
            if s32 % 4 or sn != s32 * 2:
                raise SystemExit("%s.%s: pointer array of %d/%d bytes" % (t, name, s32, sn))
            fields.append(Field(name, o32, s32, on, sn, True))
        else:
            if s32 != sn:
                raise SystemExit("%s.%s: a non-pointer field changes size (%d -> %d); "
                                 "needs manual handling (pointer-sized integer?)" % (t, name, s32, sn))
            fields.append(Field(name, o32, s32, on, sn, False))

    def add_items(items):
        for it in items:
            if isinstance(it, Leaf):
                add_leaf(it, it.path)
                continue
            members = list(leaves([x for m in it.members for x in m]))
            if any(l.pointer for l in members):
                # A pointer slot with int aliases no wider than the 32-bit slot.
                ptr = [l for l in members if l.pointer]
                first = ptr[0]
                (o32, s32), (on, sn) = probe(first.path)
                for l in members:
                    (a32, b32), (an, bn) = probe(l.path)
                    if a32 != o32 or an != on:
                        raise SystemExit("%s.%s: union members at different offsets" % (t, it.path))
                    if l.pointer and (b32 != s32 or bn != sn):
                        raise SystemExit("%s.%s: union of pointers of different sizes" % (t, it.path))
                    if not l.pointer and b32 > s32:
                        raise SystemExit("%s.%s: union member %s is wider than the pointer slot" % (t, it.path, l.path))
                fields.append(Field(it.path or "/".join(l.path for l in members), o32, s32, on, sn, True))
            else:
                # Plain data: the bytes of the largest member.
                best = None
                for l in members:
                    (a32, b32), (an, bn) = probe(l.path)
                    if best is None or b32 > best[0][1]:
                        best = ((a32, b32), (an, bn), l)
                (o32, s32), (on, sn), l = best
                if s32 != sn:
                    raise SystemExit("%s.%s: union changes size" % (t, it.path))
                fields.append(Field(it.path or "/".join(m.path for m in members), o32, s32, on, sn, False))

    add_items(trees[native])
    # Cross-check against the dump offsets.
    for target, tree_key in ((FILE_TARGET, FILE_TARGET), (native, native)):
        dump_offsets = {l.path: l.node.offset for l in leaves(trees[tree_key])}
        vals = values[target]
        for path, off in dump_offsets.items():
            if vals[(t, path, "offset")] != off:
                raise SystemExit("%s.%s: dump offset %d, compiler %d (%s)" % (t, path, off, vals[(t, path, "offset")], target))
    fields.sort(key=lambda f: (f.offn, f.name))
    rec = {
        "name": t,
        "role": role,
        "size32": v32[(t, None, "size")],
        "sizen": vn[(t, None, "size")],
        "fields": fields,
    }
    rec["identical"] = (rec["size32"] == rec["sizen"] and
                        all(f.off32 == f.offn and f.size32 == f.sizen and not f.pointer for f in fields))
    return rec


def check_fields(rec):
    """Fields must not overlap and must fit in their record."""
    for key in ("32", "n"):
        spans = sorted((getattr(f, "off" + key), getattr(f, "size" + key), f.name) for f in rec["fields"])
        end = 0
        for off, size, name in spans:
            if off < end:
                raise SystemExit("%s: field %s overlaps the previous field in the %s layout" % (rec["name"], name, key))
            end = off + size
        if end > rec["size" + key]:
            raise SystemExit("%s: fields run past the %s record size" % (rec["name"], key))


HEADER = """/* Generated by tools/genrecords.py from clang's record layouts for the PlayStation
   (MIPS o32, 32-bit pointers: %s) and the native target (%s). Do not edit.

   The game reads and writes some file contents as C structs. For each of
   those record types the table lists the record size in both layouts and every
   field: where it is in the file layout and in the native layout, and whether it is
   a pointer slot. A pointer slot is 4 bytes in the file and 8 on a 64-bit target;
   conversion zero-extends it. Everything else is copied byte for byte. */
"""


def nat(native, file):
    """A layout number as written in the tables: plain when both layouts agree."""
    return str(native) if native == file else "PORT_N(%d, %d)" % (native, file)


def emit(records, native, visible):
    h = [HEADER % (FILE_TARGET, native),
         "#ifndef PORT_RECORDS_H", "#define PORT_RECORDS_H", "",
         "/* The native value of a layout number: on a target with 4-byte pointers (the PlayStation's 32-bit",
         "   build) the native layout is the file layout. */",
         "#if defined(_WIN64) || defined(__LP64__) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)",
         "#define PORT_N(native, file) (native)",
         "#else",
         "#define PORT_N(native, file) (file)",
         "#endif", "",
         "/* One field of a record. A pointer field covers fileSize / 4 pointer slots. */",
         "typedef struct PortRecordField {",
         "    const char *name;",
         "    unsigned short fileOffset, fileSize;     /* in the 32-bit file layout */",
         "    unsigned short nativeOffset, nativeSize; /* in the native layout */",
         "    unsigned char pointer;",
         "} PortRecordField;", "",
         "typedef struct PortRecordTable {",
         "    const char *name;",
         "    unsigned fileSize;      /* sizeof the record in the file layout */",
         "    unsigned nativeSize;    /* sizeof the record on this target */",
         "    unsigned fieldCount;",
         "    const PortRecordField *fields;",
         "    int identical;          /* same size and field offsets in both layouts, no pointers */",
         "} PortRecordTable;", ""]
    for r in records:
        h.append("extern const PortRecordTable PortRecord_%s; /* %s */" % (r["name"], r["role"]))
    h.append("")
    h.append("/* sizeof in the file layout, for counts such as length / sizeof(record). */")
    for r in records:
        h.append("#define PORT_FILE_SIZEOF_%s %d" % (r["name"], r["size32"]))
    h.append("")
    h.append("/* Every table, for the tests. */")
    h.append("extern const PortRecordTable *const PortRecordTables[];")
    h.append("extern const unsigned PortRecordTableCount;")
    h.append("")
    h.append("#endif")
    c = [HEADER % (FILE_TARGET, native),
         '#include "functions.h"']
    for extra in sorted({v for v in visible.values() if v}):
        c.append('#include "%s"' % extra)
    c += ['#include "port_widen.h"', "#include <stddef.h>", ""]
    c.append("/* The native column is checked against the compiler (on the 32-bit build against the file layout). */")
    c.append("#define PORT_SIZE(type, size) PORT_ASSERT(sizeof(type) == (size))")
    c.append("#define PORT_OFFSET(type, field, offset) PORT_ASSERT(offsetof(type, field) == (offset))")
    c.append("")
    for r in records:
        if visible[r["name"]] is None:
            continue
        c.append("PORT_SIZE(%s, %s);" % (r["name"], nat(r["sizen"], r["size32"])))
        for f in r["fields"]:
            if "/" not in f.name:
                c.append("PORT_OFFSET(%s, %s, %s);" % (r["name"], f.name, nat(f.offn, f.off32)))
    c.append("")
    for r in records:
        c.append("static const PortRecordField fields_%s[] = {" % r["name"])
        for f in r["fields"]:
            c.append("    { \"%s\", %d, %d, %s, %s, %d }," % (f.name, f.off32, f.size32, nat(f.offn, f.off32), nat(f.sizen, f.size32),
                                                           1 if f.pointer else 0))
        c.append("};")
        c.append("const PortRecordTable PortRecord_%s = { \"%s\", %d, %s, %d, fields_%s, %s };"
                 % (r["name"], r["name"], r["size32"], nat(r["sizen"], r["size32"]), len(r["fields"]), r["name"],
                    "1" if r["identical"] else "PORT_N(0, 1)"))
        c.append("")
    c.append("const PortRecordTable *const PortRecordTables[] = {")
    for r in records:
        c.append("    &PortRecord_%s," % r["name"])
    c.append("};")
    c.append("const unsigned PortRecordTableCount = %d;" % len(records))
    return "\n".join(h) + "\n", "\n".join(c) + "\n"


def audit_records(text):
    """{record name: (size, alignment, field lines)} for every record in a layout dump."""
    records = {}
    name = None
    fields = []
    for line in text.split("\n"):
        if line.startswith("*** Dumping"):
            name = None
            continue
        m = re.match(r"^\s+0 \| (struct|union) (.*)$", line)
        if m and name is None and not re.match(r"^\s+0 \| \s", line):
            name = (m.group(1) + " " + m.group(2)).replace(ROOT + "/", "")
            fields = []
            continue
        m = re.match(r"^\s+\| \[sizeof=(\d+), (?:dsize=\d+, )?align=(\d+)", line)
        if m and name:
            records[name] = (int(m.group(1)), int(m.group(2)), tuple(fields))
            name = None
            continue
        if name:
            fields.append(line.strip().replace(ROOT + "/", ""))
    return records


def audit(native):
    """The record types of the game sources whose size, alignment or field offsets differ between the two layouts.
    Returns {name: (size in the file layout, native size, first source, 'size' | 'fields')}."""
    found = {}
    # The sources CMakeLists.txt builds (the setup dialog and the MPEG player are replaced by the platform).
    skipped = re.compile(r"^(win|d3dsetup|mpeg|mpegvlc|mpegtab|mpegjit|mpegfile|mpegdbg|mpegsnd)\.c$")
    sources = sorted(f for f in os.listdir(os.path.join(ROOT, "game/src")) if f.endswith(".c") and not skipped.match(f))
    for source in sources:
        both = []
        for target in (FILE_TARGET, native):
            text = run_clang(target, os.path.join(ROOT, "game/src", source),
                             ["-Xclang", "-fdump-record-layouts-complete", "-fsyntax-only"], tolerant=True)
            both.append(audit_records(text))
        for name, (size32, align32, fields32) in both[0].items():
            if name not in both[1] or name.startswith("struct PortRecord") or name in found:
                continue
            sizen, alignn, fieldsn = both[1][name]
            if (size32, align32) != (sizen, alignn):
                found[name] = (size32, sizen, source, "size")
            elif fields32 != fieldsn:
                found[name] = (size32, sizen, source, "fields")
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--audit", action="store_true", help="list every record type whose size differs, and stop")
    parser.add_argument("--check", action="store_true", help="fail if the generated files are out of date")
    parser.add_argument("--cross", action="store_true", help="also compare with the x86_64 Linux layout")
    parser.add_argument("--native", default=None, help="native target triple (default: clang's host)")
    args = parser.parse_args()
    native = args.native or host_target()
    if args.audit:
        found = audit(native)
        print("%d record types differ in size between %s and %s" % (len(found), FILE_TARGET, native))
        for name in sorted(found):
            size32, sizen, source, what = found[name]
            print("| %s | %d | %d | %s | %s |" % (name, size32, sizen, source, what))
        return
    by_source = {}
    for source, t, role, visible in RECORDS:
        by_source.setdefault(source, []).append((t, role))
    records = []
    with tempfile.TemporaryDirectory() as workdir:
        for source, items in by_source.items():
            types = [t for t, _ in items]
            trees, values, _ = analyse(source, types, native, workdir)
            for t, role in items:
                rec = build_record(t, role, trees[t], values, native)
                check_fields(rec)
                records.append(rec)
            if args.cross:
                other = "x86_64-pc-linux-gnu"
                _, other_values, _ = analyse(source, types, other, workdir)
                for key, n in other_values[other].items():
                    if values[native][key] != n:
                        raise SystemExit("native layout differs between %s and %s at %s" % (native, other, key))
    order = {t: i for i, (_, t, _, _) in enumerate(RECORDS)}
    visible = {t: v for _, t, _, v in RECORDS}
    records.sort(key=lambda r: order[r["name"]])
    header, source_text = emit(records, native, visible)
    paths = (os.path.join(ROOT, "game/include/port_records.h"), os.path.join(ROOT, "game/src/port_records.c"))
    if args.check:
        for path, text in zip(paths, (header, source_text)):
            if not os.path.exists(path) or open(path).read() != text:
                raise SystemExit("%s is out of date" % path)
        print("port_records: up to date")
        return
    for path, text in zip(paths, (header, source_text)):
        open(path, "w").write(text)
    for r in records:
        print("%-14s file %4d  native %4d  fields %2d  %s" % (r["name"], r["size32"], r["sizen"], len(r["fields"]),
                                                              "identical" if r["identical"] else "DIFFERS"))


if __name__ == "__main__":
    main()
