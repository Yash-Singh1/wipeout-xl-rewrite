"""Embed this project's shader source without a runtime file dependency."""
import json
import sys
from pathlib import Path
source, output = map(Path, sys.argv[1:])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('/* Generated from the native shader source. */\nstatic const char metalShaderSource[] =\n' +
                  ''.join('    ' + json.dumps(line + '\n') + '\n' for line in source.read_text().splitlines()) + ';\n')
