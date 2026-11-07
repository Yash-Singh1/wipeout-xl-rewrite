const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const code = fs.readFileSync('platform/web/phone-loader.js', 'utf8');

async function test(failure) {
  let active = 0, maximum = 0, requests = 0, aborted = false;
  const pieces = Array.from({length: 13}, (_, i) => new Uint8Array(i + 37).fill(i));
  const sandbox = {AbortController, Uint8Array, fetch: async (url, options) => {
    assert.equal(options.cache, 'force-cache');
    options.signal.addEventListener('abort', () => { aborted = true; });
    active++; maximum = Math.max(maximum, active); requests++;
    const index = Number(url);
    await new Promise(resolve => setTimeout(resolve, 1 + (12 - index) % 4));
    active--;
    return {ok: !(failure === 'http' && index === 2), status: 503,
      arrayBuffer: async () => failure === 'short' && index === 2
        ? new Uint8Array(3).buffer : pieces[index].buffer};
  }};
  vm.createContext(sandbox); vm.runInContext(code, sandbox);
  const size = pieces.reduce((n, x) => n + x.length, 0);
  const track = sandbox.phoneTrackChunks({name: 'data.bin', size,
    chunks: pieces.map((p, i) => ({url: String(i), size: p.length}))});
  assert.equal(await track.size(), size);
  assert.equal(track.url, undefined); // No whole-track Cache API copy after streaming.
  const read = async () => { let n = 0;
    for await (const piece of track.pieces()) assert.deepEqual(piece, pieces[n++]);
    assert.equal(n, pieces.length);
  };
  if (failure) await assert.rejects(read, failure === 'http' ? /503/ : /Incomplete/);
  else { await read(); assert.equal(requests, pieces.length); }
  assert(maximum <= 4); assert(aborted);
  await new Promise(resolve => setTimeout(resolve, 10));
}
(async () => {
  await test(); await test('http'); await test('short');
  console.log('Phone chunk loader: ordered output, four-request bound, cache policy and failed-chunk cleanup: OK');
})().catch(error => { console.error(error); process.exitCode = 1; });
