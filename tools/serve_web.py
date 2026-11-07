#!/usr/bin/env python3
"""Serve a muted phone test with compressed, cacheable game-data chunks."""
import argparse
import gzip
import hashlib
import json
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit

CHUNK_SIZE = 1 << 20
ROOT = Path(__file__).resolve().parent.parent


def prepare(build, cue, cache):
    blocks = re.split(r'(?=^\s*FILE\s+")', cue.read_text(), flags=re.M | re.I)
    block = next((b for b in blocks if re.match(r'\s*FILE\s+"', b, re.I)), '')
    match = re.match(r'\s*FILE\s+"([^"]+)"\s+BINARY', block, re.I)
    if not match or not re.search(r'TRACK\s+\d+\s+MODE[12]/', block, re.I):
        raise ValueError('The first CUE file must be a binary data track')
    if re.search(r'TRACK\s+\d+\s+AUDIO', block, re.I):
        raise ValueError('The muted phone server requires separate data and music files')
    source = cue.parent / match[1].replace('\\', '/').split('/')[-1]
    digest = hashlib.sha256()
    with source.open('rb') as f:
        while piece := f.read(CHUNK_SIZE):
            digest.update(piece)
    key = digest.hexdigest()
    directory = cache / key
    directory.mkdir(parents=True, exist_ok=True)
    manifest = dict(name='phone-data.bin', size=source.stat().st_size, chunks=[])
    served = {}
    with source.open('rb') as f:
        for index in range((manifest['size'] + CHUNK_SIZE - 1) // CHUNK_SIZE):
            piece = f.read(CHUNK_SIZE)
            path = directory / f'{index:04}.gz'
            if not path.exists():
                temporary = path.with_suffix('.tmp')
                temporary.write_bytes(gzip.compress(piece, compresslevel=4, mtime=0))
                temporary.replace(path)
            url = f'/phone-chunks/{key}/{index:04}.bin'
            manifest['chunks'].append(dict(url=url, size=len(piece)))
            served[url] = (path, len(piece))
    cue_text = block[:match.start(1)] + manifest['name'] + block[match.end(1):]
    (build / 'phone-disc.cue').write_text(cue_text)
    page = (build / 'wipeoutxl.html').read_text()
    loader = (ROOT / 'platform/web/phone-loader.js').read_text()
    revision = str((build / 'wipeoutxl.wasm').stat().st_mtime_ns)
    hook = '''<script>
LOADER
var phoneManifest = MANIFEST;
hostedDisc = async function () { return 'phone-disc.cue'; };
var fullUrlTrack = urlTrack;
urlTrack = function (url) {
  return decodeURIComponent(new URL(url).pathname.split('/').pop()) === phoneManifest.name
    ? phoneTrackChunks(phoneManifest) : fullUrlTrack(url);
};
Module.locateFile = function (name) { return name + '?rev=REVISION'; };
Module.preRun = [function () {
  Object.assign(ENV, {WIPEOUT_MUTE:'1', WIPEOUT_SKIP_INTRO:'1', WIPEOUT_SKIP_LOGOS:'1',
    WIPEOUT_TOUCH:'1', WIPEOUT_RENDER_SCALE:'3', WIPEOUT_WIDESCREEN:'1', WIPEOUT_FRAME_RATE:'uncapped'});
  var main = Module.callMain;
  Module.callMain = function (args) {
    FS.mkdirTree('/libsdl/Psygnosis/WipeoutXL');
    FS.writeFile('/libsdl/Psygnosis/WipeoutXL/wipeoutxl.ini',
      'show_fps=1\\nrecord_frames=0\\npause_unfocused=1\\n');
    return main(args);
  };
}];
</script>'''.replace('LOADER', loader).replace('MANIFEST', json.dumps(manifest)).replace('REVISION', revision)
    pattern = r'<script\s+async\s+src=(?:"wipeoutxl\.js"|wipeoutxl\.js)></script>'
    replacement = hook + f'<script async src="wipeoutxl.js?rev={revision}"></script>'
    page, count = re.subn(pattern, lambda _: replacement, page)
    if count != 1:
        raise ValueError('Could not find the generated game script tag')
    (build / 'phone.html').write_text(page)
    return manifest, served


def handler_for(build, served):
    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            self.chunk = False
            super().__init__(*args, directory=str(build), **kwargs)

        def end_headers(self):
            self.send_header('Cache-Control', 'public, max-age=31536000, immutable' if self.chunk else 'no-cache')
            super().end_headers()

        def send_head(self):
            item = served.get(unquote(urlsplit(self.path).path))
            if not item:
                return super().send_head()
            path, raw_size = item
            self.chunk = True
            encodings = self.headers.get('Accept-Encoding', '').lower().split(',')
            compressed = any(value.split(';', 1)[0].strip() == 'gzip' and
                             not re.search(r';\s*q=0(?:\.0*)?\s*(?:;|$)', value)
                             for value in encodings)
            self.send_response(200)
            self.send_header('Content-Type', 'application/octet-stream')
            self.send_header('Content-Length', str(path.stat().st_size if compressed else raw_size))
            self.send_header('Vary', 'Accept-Encoding')
            if compressed:
                self.send_header('Content-Encoding', 'gzip')
            self.end_headers()
            return path.open('rb') if compressed else gzip.open(path, 'rb')

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/web')
    parser.add_argument('--disc', type=Path)
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=8768)
    args = parser.parse_args()
    build = args.build.resolve()
    cue = args.disc or build / unquote(json.loads((build / 'wipeoutxl-data.json').read_text())['disc'])
    manifest, served = prepare(build, cue, build.parent / 'phone-cache')
    wire_bytes = sum(path.stat().st_size for path, _ in served.values())
    print(f'Muted phone build: {manifest["size"]:,} data bytes; {wire_bytes:,} compressed bytes; '
          f'{len(served)} cacheable chunks', flush=True)
    print(f'Open http://{args.host}:{args.port}/phone.html', flush=True)
    ThreadingHTTPServer((args.host, args.port), handler_for(build, served)).serve_forever()


if __name__ == '__main__':
    main()
