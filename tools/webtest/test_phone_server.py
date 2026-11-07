"""Check exact disc bytes, immutable chunk URLs and gzip/identity HTTP responses."""
import gzip
import importlib.util
from pathlib import Path
import tempfile
import threading
import unittest
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('serve_web', ROOT / 'tools/serve_web.py')
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


class PhoneServerTest(unittest.TestCase):
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
            manifest, served = server.prepare(root, cue, root / 'cache')
            self.assertEqual(manifest['size'], len(source))
            self.assertEqual(len(manifest['chunks']), 3)
            self.assertNotIn('AUDIO', (root / 'phone-disc.cue').read_text())
            self.assertIn('phone-data.bin', (root / 'phone-disc.cue').read_text())
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
            changed, _ = server.prepare(root, cue, root / 'cache')
            self.assertNotEqual(first_url, changed['chunks'][0]['url'])


if __name__ == '__main__':
    unittest.main()
