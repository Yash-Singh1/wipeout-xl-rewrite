"""Embed compiled Metal IR so both standalone and bundled executables can load it."""
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
data = source.read_bytes()
if data[:4] != b"MTLB":
    raise SystemExit("Not a Metal library: " + str(source))
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text("/* Generated from wipeout.metallib. */\nstatic const unsigned char metalLibraryBytes[] = {\n" +
                  "".join("    " + ",".join(str(b) for b in data[i:i+32]) + ",\n"
                          for i in range(0, len(data), 32)) + "};\n")
