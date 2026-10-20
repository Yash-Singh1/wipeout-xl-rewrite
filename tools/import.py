#!/usr/bin/env python3
"""Import the game sources from the PS1 decompilation (SCUS-94351) into game/.

Takes `src/` and `include/` of a decomp-pc commit, keeps the all-C D3D build
the decomp's gate verifies (EQUIV and NON_MATCHING defined, NETPLAY and
POWERVR not), makes the 32-bit integer model explicit (`portable_types`),
and writes them to game/ with the commit recorded in
game/IMPORT. Commit the result on the `decomp` branch, then merge that branch
into the port branch: port changes to game/ are ordinary edits on top.
"""
import argparse
import io
import re
from pathlib import Path
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DECOMP = ROOT.parent / 'wipeout-xl' / 'decomp'
SKIP_DIRS = {'leak'}
DEFINES = ['-DEQUIV', '-DNON_MATCHING']


def unifdef(text):
    result = subprocess.run(['unifdef', '-B', *DEFINES], input=text, capture_output=True,
                            text=True, encoding='latin-1')
    if result.returncode > 1:
        raise RuntimeError(result.stderr)
    return result.stdout


TOKEN_RE = re.compile(r"""
    (?P<comment>/\*.*?\*/|//[^\n]*)
  | (?P<string>"(?:\\.|[^"\\\n])*")
  | (?P<char>'(?:\\.|[^'\\\n])*')
  | (?P<number>\b(?:0[xX][0-9a-fA-F]+|\d+)[uUlL]*\b)
  | (?P<word>\b[A-Za-z_]\w*\b)
""", re.S | re.X)
FORMAT_LONG_RE = re.compile(r'(%[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?)l([diouxX])')


def portable_types(text):
    """Make the 32-bit Windows integer model explicit for any target.

    `long` was 32 bits under MSVC; it becomes `int` (`unsigned long` becomes
    `unsigned int`, `long int` becomes `int`). Integer literal `L` suffixes and
    `%l` printf length modifiers go with it, and `__int64` becomes
    `long long`. Comments are left alone.
    """
    out = []
    position = 0
    after_long = False
    for match in TOKEN_RE.finditer(text):
        out.append(text[position:match.start()])
        token = match.group()
        kind = match.lastgroup
        if kind == 'word' and token == 'long' and after_long:
            after_long = False  # second half of `long long`: stays 64 bits
        elif kind == 'word' and token == 'long':
            following = text[match.end():match.end() + 16].lstrip()
            if re.match(r'long\b', following):
                after_long = True  # `long long` stays 64 bits
            elif re.match(r'(int|double)\b', following):
                token = '' if following.startswith('int') else token
            else:
                token = 'int'
        elif kind == 'word' and token == '__int64':
            token = 'long long'
        elif kind == 'number':
            token = re.sub(r'[lL]', '', token)
        elif kind == 'string':
            token = FORMAT_LONG_RE.sub(r'\1\2', token)
        out.append(token)
        position = match.end()
    out.append(text[position:])
    return ''.join(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--decomp', type=Path, default=DEFAULT_DECOMP)
    parser.add_argument('--commit', default='main')
    args = parser.parse_args()

    commit = subprocess.run(['git', '-C', str(args.decomp), 'rev-parse', args.commit],
                            check=True, capture_output=True, text=True).stdout.strip()
    archive = subprocess.run(['git', '-C', str(args.decomp), 'archive', commit, 'src', 'include'],
                             check=True, capture_output=True).stdout
    out = ROOT / 'game'
    for sub in ('src', 'include'):
        shutil.rmtree(out / sub, ignore_errors=True)
    count = 0
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        for member in tar.getmembers():
            parts = Path(member.name).parts
            if not member.isfile() or (len(parts) > 2 and parts[1] in SKIP_DIRS):
                continue
            if not member.name.endswith(('.c', '.h')):
                continue
            text = tar.extractfile(member).read().decode('latin-1')
            target = out / member.name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(portable_types(unifdef(text)), encoding='latin-1')
            count += 1
    (out / 'IMPORT').write_text(f'decomp {commit}\ndefines {" ".join(DEFINES)}\n')
    print(f'{count} files from {commit[:10]}')


if __name__ == '__main__':
    main()
