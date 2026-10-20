#!/usr/bin/env python3
"""Compiler diagnostics of one game source, one line each with the source text.  usage: tools/diag.py game/src/FILE.c"""
#!/usr/bin/env python3
import subprocess,sys,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
f=sys.argv[1]
extra=sys.argv[2:]
r=subprocess.run([str(ROOT/'tools/cc.sh'),'-ferror-limit=0','-fno-color-diagnostics','-fno-caret-diagnostics','-fno-diagnostics-show-note-include-stack']+extra+[f],capture_output=True,text=True)
src=open(ROOT/f,encoding='latin-1').read().split('\n')
seen=set()
for l in r.stderr.split('\n'):
    m=re.match(r'^(game/[^:]+):(\d+):(\d+): (warning|error|note): (.*)$',l)
    if not m: continue
    file,ln,col,kind,msg=m.groups()
    if kind=='note': continue
    key=(file,ln,msg[:60])
    if key in seen: continue
    seen.add(key)
    msg=re.sub(r' \[-W([a-z0-9-]+)\]$',r' [\1]',msg)
    print('%s:%s %s: %s'%(file.split('/')[-1],ln,kind,msg[:200]))
    if file==f: print('     | '+src[int(ln)-1].strip()[:240])
