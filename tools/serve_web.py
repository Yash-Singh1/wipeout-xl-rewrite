#!/usr/bin/env python3
"""Serve phone tests with compressed, cacheable disc chunks and optional music."""
import argparse
import gzip
import hashlib
import json
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import re
import shutil
from urllib.parse import unquote, urlsplit

CHUNK_SIZE = 1 << 20
ROOT = Path(__file__).resolve().parent.parent


def cache_track(source, name, cache):
    digest = hashlib.sha256()
    with source.open('rb') as f:
        while piece := f.read(CHUNK_SIZE):
            digest.update(piece)
    key = digest.hexdigest()
    directory = cache / key
    directory.mkdir(parents=True, exist_ok=True)
    manifest = dict(name=name, size=source.stat().st_size, chunks=[])
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
    return manifest, served


def prepare(build, cue, cache, music=False):
    blocks = [b for b in re.split(r'(?=^\s*FILE\s+")', cue.read_text(), flags=re.M | re.I)
              if re.match(r'\s*FILE\s+"', b, re.I)]
    if not blocks or not re.search(r'TRACK\s+\d+\s+MODE[12]/', blocks[0], re.I):
        raise ValueError('The first CUE file must be a binary data track')
    if not music and re.search(r'TRACK\s+\d+\s+AUDIO', blocks[0], re.I):
        raise ValueError('The muted phone server requires separate data and music files')
    tracks, cue_parts, served = [], [], {}
    for index, block in enumerate(blocks if music else blocks[:1]):
        match = re.match(r'\s*FILE\s+"([^"]+)"\s+BINARY', block, re.I)
        if not match:
            raise ValueError('Phone tests require binary CUE files')
        name = f'phone-track-{index + 1:02}.bin' if music else 'phone-data.bin'
        source = cue.parent / match[1].replace('\\', '/').split('/')[-1]
        track, chunks = cache_track(source, name, cache)
        if music and re.search(r'TRACK\s+\d+\s+AUDIO', block, re.I) and not re.search(r'MODE[12]/', block, re.I):
            track['stream'] = True
        tracks.append(track)
        served.update(chunks)
        cue_parts.append(block[:match.start(1)] + name + block[match.end(1):])
    stem = 'phone-music' if music else 'phone'
    (build / f'{stem}-disc.cue').write_text(''.join(cue_parts))
    manifest = tracks[0] if not music else dict(tracks=tracks, size=sum(t['size'] for t in tracks))
    page = (build / 'wipeoutxl.html').read_text()
    loader = (ROOT / 'platform/web/phone-loader.js').read_text()
    # Keep the published JS/WASM pair stable while another build links new files.
    runtime = {name: (build / name).read_bytes() for name in ('wipeoutxl.js', 'wipeoutxl.wasm')}
    revision = hashlib.sha256(b''.join(runtime.values())).hexdigest()
    runtime_path = build / 'phone-runtime' / revision
    runtime_path.mkdir(parents=True, exist_ok=True)
    for name, data in runtime.items():
        destination = runtime_path / name
        if not destination.exists():
            destination.write_bytes(data)
    hook = '''<script>
LOADER
var phoneManifest = MANIFEST;
hostedDisc = async function () { return 'CUE_NAME'; };
var fullUrlTrack = urlTrack;
urlTrack = function (url) {
  var name = decodeURIComponent(new URL(url).pathname.split('/').pop());
  var track = (phoneManifest.tracks || [phoneManifest]).find(function (track) { return track.name === name; });
  return track ? phoneTrackChunks(track) : fullUrlTrack(url);
};
Module.locateFile = function (name) { return '/phone-runtime/REVISION/' + name; };
Module.preRun = [function () {
  MUTE_SETTING
  Object.assign(ENV, {WIPEOUT_SKIP_INTRO:'1', WIPEOUT_SKIP_LOGOS:'1',
    WIPEOUT_TOUCH:'1', WIPEOUT_RENDER_SCALE:'3', WIPEOUT_WIDESCREEN:'1', WIPEOUT_FRAME_RATE:'uncapped'});
  var main = Module.callMain;
  Module.callMain = function (args) {
    FS.mkdirTree('/libsdl/Psygnosis/WipeoutXL');
    FS.writeFile('/libsdl/Psygnosis/WipeoutXL/wipeoutxl.ini',
      'show_fps=1\\nrecord_frames=0\\npause_unfocused=1\\n');
    return main(args);
  };
}];
</script>'''.replace('LOADER', loader).replace('MANIFEST', json.dumps(manifest)).replace('REVISION', revision).replace('CUE_NAME', f'{stem}-disc.cue').replace(
        'MUTE_SETTING', 'delete ENV.WIPEOUT_MUTE;' if music else "ENV.WIPEOUT_MUTE = '1';")
    pattern = r'<script\s+async\s+src=(?:"wipeoutxl\.js"|wipeoutxl\.js)></script>'
    replacement = hook + f'<script async src="/phone-runtime/{revision}/wipeoutxl.js"></script>'
    page, count = re.subn(pattern, lambda _: replacement, page)
    if count != 1:
        raise ValueError('Could not find the generated game script tag')
    (build / f'{stem}.html').write_text(page)
    if music:
        diagnostic_hook = (ROOT / 'platform/web/phone-diagnostics.js').read_text()
        tag = f'<script async src="/phone-runtime/{revision}/wipeoutxl.js"></script>'
        diagnostic_page = page.replace(tag, '<script>var phoneRevision = ' + json.dumps(revision) + ';\n' +
                                       diagnostic_hook + '</script>' + tag)
        (build / 'phone-music-check.html').write_text(diagnostic_page)
    return manifest, served


def handler_for(build, served):
    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            self.chunk = False
            super().__init__(*args, directory=str(build), **kwargs)

        def end_headers(self):
            self.send_header('Cache-Control', 'public, max-age=31536000, immutable' if self.chunk else 'no-cache')
            super().end_headers()

        def do_POST(self):
            if urlsplit(self.path).path != '/phone-diagnostics':
                self.send_error(404)
                return
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 0 < length <= 65536:
                    self.send_error(413)
                    return
                message = json.loads(self.rfile.read(length))
                session = message['session']
                if not re.fullmatch(r'[a-z0-9-]{1,40}', session) or not isinstance(message['data'], dict):
                    raise ValueError('Invalid diagnostic session')
                directory = build.parent / 'phone-diagnostics'
                directory.mkdir(exist_ok=True)
                (directory / f'{session}.json').write_text(json.dumps(message['data']))
            except (ValueError, KeyError, TypeError):
                self.send_error(400)
                return
            self.send_response(204)
            self.end_headers()

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


def export_static(build, served, destination, music=False):
    """Write ordinary files, including raw chunks, with no custom HTTP headers required."""
    destination.mkdir(parents=True, exist_ok=True)
    for stem in ('phone', 'phone-music') if music else ('phone',):
        page = (build / f'{stem}.html').read_text()
        revision = re.search(r'/phone-runtime/([0-9a-f]{64})/wipeoutxl.js', page)[1]
        runtime = Path('phone-runtime') / revision
        (destination / runtime).mkdir(parents=True, exist_ok=True)
        for name in ('wipeoutxl.js', 'wipeoutxl.wasm'):
            shutil.copyfile(build / runtime / name, destination / runtime / name)
        # Relative URLs allow the export to live in a subdirectory of a site.
        page = page.replace('/phone-runtime/', 'phone-runtime/').replace('/phone-chunks/', 'phone-chunks/')
        (destination / f'{stem}.html').write_text(page)
        shutil.copyfile(build / f'{stem}-disc.cue', destination / f'{stem}-disc.cue')
        if stem == ('phone-music' if music else 'phone'):
            (destination / 'index.html').write_text(page)
    for url, (compressed, _) in served.items():
        output = destination / url.lstrip('/')
        output.parent.mkdir(parents=True, exist_ok=True)
        with gzip.open(compressed, 'rb') as source, output.open('wb') as target:
            shutil.copyfileobj(source, target, CHUNK_SIZE)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/web')
    parser.add_argument('--disc', type=Path)
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=8768)
    parser.add_argument('--music', action='store_true', help='Also publish phone-music.html with all CD tracks and audio enabled')
    parser.add_argument('--export', type=Path, help='Write a standalone static site instead of starting a server')
    args = parser.parse_args()
    build = args.build.resolve()
    cue = args.disc or build / unquote(json.loads((build / 'wipeoutxl-data.json').read_text())['disc'])
    manifest, served = prepare(build, cue, build.parent / 'phone-cache')
    wire_bytes = sum(path.stat().st_size for path, _ in served.values())
    print(f'Muted phone build: {manifest["size"]:,} data bytes; {wire_bytes:,} compressed bytes; '
          f'{len(served)} cacheable chunks', flush=True)
    if args.music:
        music_manifest, music_chunks = prepare(build, cue, build.parent / 'phone-cache', music=True)
        served.update(music_chunks)
        print(f'Music page: {music_manifest["size"]:,} disc bytes; open /phone-music.html', flush=True)
    if args.export:
        export_static(build, served, args.export.resolve(), args.music)
        print(f'Static site exported to {args.export.resolve()}', flush=True)
        return
    print(f'Open http://{args.host}:{args.port}/phone.html', flush=True)
    ThreadingHTTPServer((args.host, args.port), handler_for(build, served)).serve_forever()


if __name__ == '__main__':
    main()
