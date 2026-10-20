#!/usr/bin/env python3
"""Builds tools/recordtest/loadtest: the game's own loaders linked with a recording allocator.

Every game/src/*.c is compiled unchanged (with loadtest_prelude.h, which routes DAlloc, DFree and
InitDynamicMem through the test's recorder), except iso9660.c (the test reads the files of the disc
directory instead of the CD drive) and malloc.c, which is the allocator under test (compiled
without the prelude and without the sanitizers, so that the test can poison the heap headers).
Three functions are cut where their loading is done, by one inserted line each (PATCHES): PsxMain
after the start-up sequence, MenuControl before its menu loop, race before its main loop. A patch
whose anchor text is missing or ambiguous stops the build.

The platform's pure computation (platform/psx/gte*.c, libgte.c) and libgpu.c are linked as they
are. Everything else the game mentions (the PsyQ sound, CD, pad and memory card libraries, the
movie player, the assembly drawing routines) is either defined by loadtest.c or generated here:
functions become stubs that stop the program if they are ever called, other symbols become zeroed
storage.

Global data (the strings, tables and constants the executable initialised) comes from game/data
when the tree has it; otherwise from the directory in $LOADTEST_DATA, otherwise from the `data`
branch of this repository (git show data:game/data/...).

usage: build_loadtest.py OUTPUT
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "game/src")

# Game sources that are not linked: the CD file system and Error, which draws a message screen and
# exits (both replaced by loadtest_standins.c).
EXCLUDED = {"iso9660", "error"}

# (file, anchor, replacement): the anchor must occur exactly once.
PATCHES = {
    "main": [
        ("    heapStart = portHeapArea.bytes;\n", "    heapStart = LoadtestMainHeap();\n"),
        ("    reserveHeap = (HeapBlock *)(heapStart + heapBytes);\n", "    reserveHeap = (HeapBlock *)LoadtestReserveHeap();\n"),
        ("    enabled = 1;\n    while (1) {\n", "    enabled = 1;\n    if (LoadtestHook(0)) return 0; while (1) {\n"),
    ],
    "menus": [
        ("    while (!negconCentre) {\n", "    if (LoadtestHook(1)) return 0; while (!negconCentre) {\n"),
    ],
    "racectrl": [
        ("sceneTextureCount=TextureTableCount;closewadfile(wad);", "sceneTextureCount=TextureTableCount;closewadfile(wad);LoadtestHook(3);"),
        ("restart:\n    Restart(&locals.resetData);", "restart:\n    if (LoadtestHook(2)) return; Restart(&locals.resetData);"),
    ],
}

# Names that the console's symbol table gives to words inside textureTable (textureTable is at 0x800CE468;
# menuTexturePtrsD 0x800CE470, deleteTextures 0x800CE474, endTextures 0x800CE478, menuTexturePtrsC 0x800CE47C,
# menuTexturePtrs 0x800CE480, menuTexturePtrsB 0x800CE484). The sources declare them as arrays of their own
# (the data generator will replace them by the table itself); until then the declaration lines become macros.
TEXTURE_TABLE_PARTS = {"menuTexturePtrsD": 2, "deleteTextures": 3, "endTextures": 4, "menuTexturePtrsC": 5,
                       "menuTexturePtrs": 6, "menuTexturePtrsB": 7}

PLATFORM = ["gte", "gte_tables", "libgte", "libgpu"]

SAN = ["-fsanitize=address,undefined", "-fsanitize-recover=address"]
FLAGS = ["-std=gnu11", "-fsigned-char", "-fno-strict-aliasing", "-fwrapv", "-g", "-O1", "-fcommon",
         "-I" + os.path.join(ROOT, "game/include"), "-I" + HERE,
         "-Wno-incompatible-pointer-types", "-Wno-int-conversion", "-Wno-pointer-sign",
         "-Wno-implicit-function-declaration", "-w"]
PORT = ["-include", os.path.join(ROOT, "platform/include/port.h")]
PRELUDE = ["-include", os.path.join(HERE, "loadtest_prelude.h")]


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def function_names():
    """Names the game's headers and sources declare as functions."""
    names = set()
    for sub in ("game/include", "game/src", "platform/psx"):
        for fn in os.listdir(os.path.join(ROOT, sub)):
            if not fn.endswith((".h", ".c")):
                continue
            text = open(os.path.join(ROOT, sub, fn), errors="replace").read()
            for m in re.finditer(r"^[A-Za-z_][\w\s\*]*?\b(\w+)\s*\([^;{}=]*\)\s*;", text, re.M):
                names.add(m.group(1))
    return names


def patched(name, work):
    text = open(os.path.join(work, "snapshot", name + ".c")).read()
    text = texture_table_parts(text)
    for anchor, replacement in PATCHES.get(name, []):
        if text.count(anchor) != 1:
            sys.exit("build_loadtest.py: %s.c: the text to patch occurs %d times (expected once):\n%s" % (name, text.count(anchor), anchor))
        if anchor.count("\n") != replacement.count("\n"):
            sys.exit("build_loadtest.py: the patch of %s.c must not change the line count (the test finds the heap calls by line)" % name)
        text = text.replace(anchor, replacement)
    path = os.path.join(work, "snapshot", name + ".c")
    open(path, "w").write(text)
    return path


def texture_table_parts(text):
    """The declaration lines of the names in TEXTURE_TABLE_PARTS become macros (same line count)."""
    for part, index in TEXTURE_TABLE_PARTS.items():
        text = re.sub(r"^extern [A-Za-z_ ]+\*\s*%s\[\];[ \t]*$" % part, "#define %s (textureTable + %d)" % (part, index), text, flags=re.M)
    return text


def data_sources(work):
    """The generated global data: game/data, $LOADTEST_DATA, or the data branch."""
    for candidate in (os.path.join(ROOT, "game/data"), os.environ.get("LOADTEST_DATA")):
        if candidate and os.path.isdir(candidate):
            return sorted(os.path.join(candidate, f) for f in os.listdir(candidate) if f.endswith(".c"))
    out = os.path.join(work, "data")
    os.mkdir(out)
    listing = subprocess.run(["git", "-C", ROOT, "ls-tree", "--name-only", "data", "game/data/"], capture_output=True, text=True)
    if listing.returncode:
        sys.exit("build_loadtest.py: no game/data, no $LOADTEST_DATA and no data branch: the strings and tables of the executable are missing")
    paths = []
    for entry in listing.stdout.split():
        if entry.endswith(".c"):
            body = subprocess.run(["git", "-C", ROOT, "show", "data:" + entry], capture_output=True, check=True).stdout
            path = os.path.join(out, os.path.basename(entry))
            open(path, "wb").write(body)
            paths.append(path)
    return sorted(paths)


def main():
    out = sys.argv[1]
    work = tempfile.mkdtemp(prefix="loadtest_")
    objects = []
    # Everything is compiled from a snapshot of game/src (other people edit it while this runs), and the test
    # finds the lines of the heap calls in the snapshot that is next to the executable.
    snapshot = os.path.join(work, "snapshot")
    os.mkdir(snapshot)
    sources = sorted(f[:-2] for f in os.listdir(SRC) if f.endswith(".c"))
    for name in sources:
        shutil.copyfile(os.path.join(SRC, name + ".c"), os.path.join(snapshot, name + ".c"))
    for name in sources:
        if name in EXCLUDED or name == "malloc":
            continue
        path = patched(name, work)
        obj = os.path.join(work, name + ".o")
        run(["clang"] + FLAGS + SAN + PORT + PRELUDE + ["-c", path, "-o", obj])
        objects.append(obj)
    obj = os.path.join(work, "malloc.o")
    run(["clang"] + FLAGS + PORT + ["-DDAlloc=RealDAlloc", "-DDFree=RealDFree", "-DInitDynamicMem=RealInitDynamicMem", "-c", os.path.join(snapshot, "malloc.c"), "-o", obj])
    objects.append(obj)
    for name in PLATFORM:
        obj = os.path.join(work, "platform_" + name + ".o")
        run(["clang", "-std=gnu11", "-g", "-O1", "-fsigned-char", "-fno-strict-aliasing", "-fwrapv", "-w",
             "-I" + os.path.join(ROOT, "platform"), "-I" + os.path.join(ROOT, "platform/psx"), "-I" + os.path.join(ROOT, "game/include"),
             "-c", os.path.join(ROOT, "platform/psx", name + ".c"), "-o", obj])
        objects.append(obj)
    for path in data_sources(work):
        obj = os.path.join(work, "data_" + os.path.basename(path)[:-2] + ".o")
        run(["clang"] + FLAGS + PORT + ["-c", path, "-o", obj])
        objects.append(obj)
    for name in ("loadtest", "loadtest_standins", "loadtest_noops"):
        obj = os.path.join(work, name + ".o")
        run(["clang"] + FLAGS + SAN + PORT + ["-c", os.path.join(HERE, name + ".c"), "-o", obj])
        objects.append(obj)

    link = ["clang"] + SAN + ["-lm"]
    probe = subprocess.run(link[:1] + SAN + objects + ["-lm", "-o", os.path.join(work, "probe")], capture_output=True, text=True)
    undefined = sorted(set(re.findall(r'^\s+"_(\w+)", referenced from', probe.stderr, re.M)))
    duplicates = sorted(set(re.findall(r'^duplicate symbol \'_(\w+)\'', probe.stderr, re.M)))
    if duplicates:
        sys.exit("build_loadtest.py: symbols defined twice: %s\n%s" % (" ".join(duplicates), probe.stderr[:3000]))
    functions = function_names()
    stubs = ["#include <stdio.h>", "#include <stdlib.h>"]
    for n in undefined:
        if n in functions:
            stubs.append('void %s(void) { fprintf(stderr, "stub %s called\\n"); exit(4); }' % (n, n))
        else:
            stubs.append("char %s[1 << 18] __attribute__((aligned(16)));" % n)
    stub_c = os.path.join(work, "stubs.c")
    open(stub_c, "w").write("\n".join(stubs) + "\n")
    run(["clang", "-c", stub_c, "-o", os.path.join(work, "stubs.o")])
    objects.append(os.path.join(work, "stubs.o"))
    result = subprocess.run(["clang"] + SAN + objects + ["-lm", "-o", out], capture_output=True, text=True)
    if result.returncode:
        sys.stderr.write(result.stderr)
        sys.exit(1)
    open(os.path.join(os.path.dirname(os.path.abspath(out)), "loadtest.stubs"), "w").write("\n".join(undefined) + "\n")
    shutil.rmtree(out + ".src", ignore_errors=True)
    shutil.copytree(snapshot, out + ".src")
    print("undefined symbols given stand-ins: %d" % len(undefined), file=sys.stderr)
    if not os.environ.get("LOADTEST_KEEP"):
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
