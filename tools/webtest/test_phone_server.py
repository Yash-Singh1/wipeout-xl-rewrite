"""Check exact disc bytes, immutable chunk URLs and gzip/identity HTTP responses."""
import gzip
import importlib.util
from pathlib import Path
import tempfile
import threading
import unittest
import re
from urllib.request import Request, urlopen
from functools import partial
from urllib.parse import urljoin

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('serve_web', ROOT / 'tools/serve_web.py')
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


class PhoneServerTest(unittest.TestCase):
    def test_local_diagnostics_are_bounded(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'build' / 'web'
            root.mkdir(parents=True)
            http = server.ThreadingHTTPServer(('127.0.0.1', 0), server.handler_for(root, {}))
            thread = threading.Thread(target=http.serve_forever, daemon=True)
            thread.start()
            try:
                url = f'http://127.0.0.1:{http.server_port}/phone-diagnostics'
                payload = b'{"session":"xr-123","data":{"heap":123,"audio":{"state":"running"}}}'
                with urlopen(Request(url, data=payload, headers={'Content-Type': 'application/json'})) as response:
                    self.assertEqual(response.status, 204)
                import json
                saved = json.loads((root.parent / 'phone-diagnostics/xr-123.json').read_text())
                self.assertEqual(saved['audio']['state'], 'running')
                from urllib.error import HTTPError
                for body, status in ((b'{"session":"../bad","data":{}}', 400), (b'x' * 65537, 413)):
                    with self.assertRaises(HTTPError) as error:
                        urlopen(Request(url, data=body))
                    self.assertEqual(error.exception.code, status)
            finally:
                http.shutdown(); http.server_close(); thread.join()

    def test_music_preserves_tracks_and_audio(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data, music = b'data' * 2000, b'music' * 3000
            (root / 'data.bin').write_bytes(data)
            (root / 'music.bin').write_bytes(music)
            cue = root / 'music.cue'
            cue.write_text('FILE "data.bin" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n'
                           'FILE "music.bin" BINARY\n TRACK 02 AUDIO\n INDEX 00 00:00:00\n INDEX 01 00:02:00\n')
            (root / 'wipeoutxl.html').write_text('<script async src=wipeoutxl.js></script>')
            (root / 'wipeoutxl.js').write_bytes(b'runtime')
            (root / 'wipeoutxl.wasm').write_bytes(b'wasm')
            manifest, served = server.prepare(root, cue, root / 'cache', music=True)
            self.assertEqual(manifest['size'], len(data) + len(music))
            self.assertEqual(len(manifest['tracks']), 2)
            self.assertNotIn('stream', manifest['tracks'][0])
            self.assertTrue(manifest['tracks'][1]['stream'])
            for track, expected in zip(manifest['tracks'], (data, music)):
                self.assertEqual(b''.join(gzip.decompress(served[c['url']][0].read_bytes())
                                          for c in track['chunks']), expected)
            text = (root / 'phone-music-disc.cue').read_text()
            self.assertIn('TRACK 02 AUDIO', text)
            self.assertIn('INDEX 01 00:02:00', text)
            self.assertIn('phone-track-02.bin', text)
            page = (root / 'phone-music.html').read_text()
            self.assertIn('delete ENV.WIPEOUT_MUTE;', page)
            self.assertIn("return 'phone-music-disc.cue'", page)
            self.assertNotIn("ENV.WIPEOUT_MUTE = '1'", page)
            self.assertNotIn('phoneReport', page)
            self.assertIn('phoneReport', (root / 'phone-music-check.html').read_text())
            self.assertFalse((root / 'phone.html').exists())
            muted, muted_chunks = server.prepare(root, cue, root / 'cache')
            served.update(muted_chunks)
            exported = root / 'site' / 'nested' / 'game'
            server.export_static(root, served, exported, music=True)
            # Serve with a plain file handler, without the helper's gzip routes.
            handler = partial(server.SimpleHTTPRequestHandler, directory=str(root / 'site'))
            http = server.ThreadingHTTPServer(('127.0.0.1', 0), handler)
            thread = threading.Thread(target=http.serve_forever, daemon=True)
            thread.start()
            try:
                page_url = f'http://127.0.0.1:{http.server_port}/nested/game/index.html'
                with urlopen(page_url) as response:
                    page = response.read().decode()
                self.assertNotIn('/phone-runtime/', page)
                self.assertNotIn('/phone-chunks/', page)
                self.assertIn('delete ENV.WIPEOUT_MUTE;', page)
                self.assertNotIn('phoneReport', page)
                script = re.search(r'src="(phone-runtime/[^\"]+/wipeoutxl.js)"', page)[1]
                for asset, expected in ((script, b'runtime'),
                                        (script.replace('.js', '.wasm'), b'wasm'),
                                        (manifest['tracks'][1]['chunks'][0]['url'].lstrip('/'), music)):
                    with urlopen(urljoin(page_url, asset)) as response:
                        self.assertFalse(response.headers.get('Content-Encoding'))
                        self.assertEqual(response.read(), expected)
                with urlopen(urljoin(page_url, 'phone-music-disc.cue')) as response:
                    self.assertIn(b'TRACK 02 AUDIO', response.read())
            finally:
                http.shutdown(); http.server_close(); thread.join()

    def test_stream_and_http_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = bytes(range(256)) * 8192 + b'last short chunk'
            (root / 'data.bin').write_bytes(source)
            # Music must never be opened or copied into the phone build.
            cue = root / 'test.cue'
            cue.write_text('FILE "data.bin" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n'
                           'FILE "missing-music.bin" BINARY\n TRACK 02 AUDIO\n INDEX 01 00:00:00\n')
            (root / 'wipeoutxl.html').write_text('<script async src=wipeoutxl.js></script>')
            (root / 'wipeoutxl.wasm').write_bytes(b'wasm fixture')
            (root / 'wipeoutxl.js').write_bytes(b'original runtime')
            manifest, served = server.prepare(root, cue, root / 'cache')
            self.assertEqual(manifest['size'], len(source))
            self.assertEqual(len(manifest['chunks']), 3)
            self.assertNotIn('AUDIO', (root / 'phone-disc.cue').read_text())
            self.assertIn('phone-data.bin', (root / 'phone-disc.cue').read_text())
            runtime_url = re.search(r'src="(/phone-runtime/[^\"]+/wipeoutxl.js)"',
                                    (root / 'phone.html').read_text())[1]
            self.assertEqual(b''.join(gzip.decompress(path.read_bytes()) for path, _ in served.values()), source)
            http = server.ThreadingHTTPServer(('127.0.0.1', 0), server.handler_for(root, served))
            thread = threading.Thread(target=http.serve_forever, daemon=True)
            thread.start()
            try:
                base = f'http://127.0.0.1:{http.server_port}'
                for encoding in ('gzip', 'identity', 'gzip;q=0'):
                    result = bytearray()
                    for chunk in manifest['chunks']:
                        with urlopen(Request(base + chunk['url'], headers={'Accept-Encoding': encoding})) as response:
                            data = response.read()
                            self.assertEqual(len(data), int(response.headers['Content-Length']))
                            self.assertIn('immutable', response.headers['Cache-Control'])
                            self.assertEqual(response.headers['Vary'], 'Accept-Encoding')
                            result.extend(gzip.decompress(data) if encoding == 'gzip' else data)
                    self.assertEqual(result, source)
                with urlopen(base + '/phone.html') as response:
                    self.assertEqual(response.headers['Cache-Control'], 'no-cache')
                    self.assertIn(b'WIPEOUT_RENDER_SCALE', response.read())
            finally:
                http.shutdown(); http.server_close(); thread.join()
            first_url = manifest['chunks'][0]['url']
            (root / 'data.bin').write_bytes(source + b'changed')
            (root / 'wipeoutxl.js').write_bytes(b'new runtime')
            changed, _ = server.prepare(root, cue, root / 'cache')
            self.assertNotEqual(first_url, changed['chunks'][0]['url'])
            self.assertEqual((root / runtime_url.lstrip('/')).read_bytes(), b'original runtime')
            self.assertNotIn(runtime_url, (root / 'phone.html').read_text())


if __name__ == '__main__':
    unittest.main()
