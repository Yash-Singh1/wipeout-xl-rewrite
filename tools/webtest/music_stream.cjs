// Verify the production cache against deterministic PCM, without retaining a
// source disc. Reads include seeks, track changes, eviction, abort and retries.
const assert = require('node:assert/strict');
const fs = require('node:fs'), vm = require('node:vm');
const chunkSize = 1 << 20;
const byte = (track, at) => (track * 31 + at * 17 + (at >>> 8)) & 255;
let active = 0, maximum = 0, failure, calls = 0, time = 0;
const sandbox = {Module: {}, AbortController, Uint8Array, setTimeout, clearTimeout, Date: {now: () => time},
  fetch: async (url, options) => {
    const [, trackText, indexText] = url.split('/'), track = +trackText, index = +indexText;
    assert.equal(options.cache, 'force-cache');
    active++; calls++; maximum = Math.max(maximum, active);
    try {
      await new Promise((resolve, reject) => {
        const id = setTimeout(resolve, 2);
        options.signal.addEventListener('abort', () => {clearTimeout(id); reject(new Error('Aborted'));});
      });
      if (failure === 'http') return {ok: false, status: 503};
      if (failure === 'throw') throw new Error('Offline');
      const length = failure === 'short' ? 7 : index === 19 ? 2371 : chunkSize;
      const bytes = new Uint8Array(length);
      for (let i = 0; i < bytes.length; i++) bytes[i] = byte(track, index * chunkSize + i);
      return {ok: true, arrayBuffer: async () => bytes.buffer};
    } finally {active--;}
  }};
vm.createContext(sandbox); vm.runInContext(fs.readFileSync('platform/web/phone-loader.js', 'utf8'), sandbox);
const make = track => sandbox.phoneTrackChunks({name: track + '.bin', stream: true,
  size: chunkSize * 19 + 2371, chunks: Array.from({length: 20}, (_, i) =>
    ({url: '/' + track + '/' + i, size: i === 19 ? 2371 : chunkSize}))});
const pause = () => new Promise(resolve => setTimeout(resolve, 15));
async function check(track, id, offset, length = 2352) {
  assert(await track.stream.ensure(offset, length));
  const output = new Uint8Array(length + 8).fill(99);
  assert(track.stream.copy(offset, length, output));
  for (let i = 0; i < length; i++) assert.equal(output[i], byte(id, offset + i));
  assert(output.subarray(length).every(x => x === 99));
  const stats = sandbox.phoneMusicCache.stats();
  assert(stats.bytes <= 6 * chunkSize); assert(stats.entries <= 6); assert(stats.pending <= 2);
}
(async () => {
  const tracks = Array.from({length: 10}, (_, i) => make(i));
  for (let id = 0; id < tracks.length; id++) {
    const header = await tracks[id].stream.header();
    assert.equal(header.length, 65536);
    assert.equal(header[123], byte(id, 123));
  }
  assert.equal(calls, 10, 'startup reads just one header chunk per song');
  for (const offset of [0, chunkSize - 1176, chunkSize, 7 * chunkSize + 81,
                        3 * chunkSize - 1, 19 * chunkSize + 19])
    await check(tracks[0], 0, offset);
  await check(tracks[9], 9, 10 * chunkSize + 211);
  await check(tracks[0], 0, 14 * chunkSize + 57);
  const sentinel = new Uint8Array(2352).fill(77);
  assert.equal(tracks[0].stream.copy(-1, 2352, sentinel), false);
  assert.equal(await tracks[0].stream.ensure(chunkSize * 20, 2352), false);
  assert(sentinel.every(x => x === 77));
  await pause();
  // Failed reads discard partial bytes and back off before retrying.
  for (const kind of ['http', 'short', 'throw']) {
    const bad = make(50); failure = kind;
    await assert.rejects(bad.stream.ensure(0, 2352), /503|Incomplete|Offline/);
    const before = calls;
    assert.equal(await bad.stream.ensure(0, 2352), false);
    assert.equal(calls, before, 'failed music must not refetch every sector');
    time += 1001;
    failure = null; await check(bad, 50, 0); bad.stream.close(); await pause();
  }
  const closing = make(80), job = closing.stream.ensure(0, 2352);
  closing.stream.close();
  assert.equal(await job, false);
  for (const track of tracks) track.stream.close();
  await pause();
  assert.equal(sandbox.phoneMusicCache.stats().bytes, 0);
  assert.equal(sandbox.phoneMusicCache.stats().pending, 0);
  assert(maximum <= 2, 'at most two music fetches');
  console.log('Music streaming: exact PCM across chunks/seeks/tracks, 6 MiB shared bound, two fetches, failure/retry and close: OK');
})().catch(error => {console.error(error); process.exitCode = 1;});
