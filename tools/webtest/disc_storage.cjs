// Exercise the page's real CUE classification and chunk copying without a browser.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const html = fs.readFileSync('platform/web/shell.html', 'utf8');
const code = html.slice(html.indexOf('function audioOnlyFiles'), html.indexOf('// A track: its size'));
const sandbox = {Module: {}}; vm.createContext(sandbox); vm.runInContext(code, sandbox);
const classify = sandbox.audioOnlyFiles;
let files = classify('FILE "data.bin" BINARY\n TRACK 01 MODE2/2352\nFILE "song.bin" BINARY\n TRACK 02 AUDIO');
assert.equal(files['data.bin'], false); assert.equal(files['song.bin'], true);
files = classify('FILE "shared.bin" BINARY\nTRACK 01 MODE2/2352\nTRACK 02 AUDIO\nFILE "shared.bin" BINARY\nTRACK 03 AUDIO');
assert.equal(files['shared.bin'], false);
files = classify('file "music\\song.bin" BINARY\ntrack 02 audio');
assert.equal(files['song.bin'], true);
const size = 3 * 1048576 + 73, source = new Uint8Array(size);
for (let i = 0; i < size; i++) source[i] = (i * 17 + (i >>> 8)) & 255;
const track = {size, chunks: []};
for (let i = 0; i < size; i += 1048576) track.chunks.push(source.slice(i, i + 1048576));
for (const offset of [0, 1, 1048576 - 100, 1048576, size - 2352, size - 1]) {
 const length = Math.min(2352, size - offset), output = new Uint8Array(length + 8).fill(99);
 assert.equal(sandbox.copyDiscBytes(track, offset, length, output), true);
 assert.deepEqual(output.slice(0, length), source.slice(offset, offset + length));
 assert(output.slice(length).every(x => x === 99));
}
assert.equal(sandbox.copyDiscBytes(track, size - 4, 8, new Uint8Array(8)), false);
assert.equal(sandbox.copyDiscBytes(track, -1, 8, new Uint8Array(8)), false);
console.log('CUE data/audio classification and exact cross-chunk sector copies: OK');

// Run the real asynchronous loader with fragmented streams, allocation failures
// and truncated input. Failed loads must release every native allocation before retry.
async function loaderTests() {
 let message;
 const context = {Module: {HEAPU8: new Uint8Array(8 << 20), discFiles: {}, gameAudio:{ready(){}}},
  document: {getElementById: () => ({style: {}, remove() {}})},
  runtimeReady: true, started: false, cueFiles: text => [...text.matchAll(/FILE "([^"]+)"/g)].map(x => x[1]),
  say(value) { message = value; }, openDiscCache: async () => null,
  FS: {mkdir() {}, writeFile() {}, mount() {}, syncfs(populate, callback) {callback(null);}},
  IDBFS: {storeLocalEntry(){},removeLocalEntry(){}},
  window: {setTimeout,clearTimeout,setInterval(){},addEventListener(){},document:{addEventListener(){}}}};
 const live = new Set(); let next = 8, allocationsLeft = Infinity;
 context.Module._malloc = size => { if (!allocationsLeft--) return 0; const p = next; next += size; live.add(p); return p; };
 context.Module._free = p => { assert(live.delete(p), 'free only live allocations'); };
 vm.createContext(context);
 vm.runInContext(fs.readFileSync('platform/web/saves.js','utf8') + code +
   html.slice(html.indexOf('async function start('), html.indexOf('// The player\'s files')), context);
 const data = source.slice(0, 128 << 10), music = source.slice(0, (1 << 20) + 73);
 const cue = 'FILE "data.bin" BINARY\nTRACK 01 MODE1/2048\nFILE "song.bin" BINARY\nTRACK 02 AUDIO';
 const track = (name, bytes, declared = bytes.length) => ({name, size: async () => declared,
  pieces: async function* () { for (let i = 0; i < bytes.length; i += 17329) yield bytes.subarray(i, i + 17329); }});
 let tracks = {'data.bin': track('data.bin', data), 'song.bin': track('song.bin', music, music.length + 1)};
 await context.start('test.cue', cue, name => tracks[name]);
 assert.equal(context.started, false); assert.equal(live.size, 0);
 assert.match(message, /shorter/); assert.equal(Object.keys(context.Module.discFiles).length, 0);
 tracks['song.bin'] = track('song.bin', music);
 allocationsLeft = 1;
 await context.start('test.cue', cue, name => tracks[name]);
 assert.equal(context.started, false); assert.equal(live.size, 0);
 allocationsLeft = Infinity;
 await context.start('test.cue', cue, name => tracks[name]);
 assert.equal(context.started, true); assert.equal(live.size, 2);
 const mapped = context.Module.discFiles['data.bin'], external = context.Module.discFiles['song.bin'];
 assert.deepEqual(context.Module.HEAPU8.slice(mapped.data, mapped.data + data.length), data);
 assert(external.external > 0); assert.equal(external.chunks.length, 2);
 const copy = new Uint8Array(music.length);
 assert(context.copyDiscBytes(external, 0, music.length, copy)); assert.deepEqual(copy, music);
 assert.deepEqual(context.Module.HEAPU8.slice(external.data, external.data + 65536), music.slice(0, 65536));
 // WAV-in-BIN must keep its full mapping for the native RIFF parser.
 for (const file of Object.values(context.Module.discFiles)) context.Module._free(file.data);
 context.started = false; context.Module.discFiles = {}; context.Module.discExternal = [null];
 const wave = music.slice(); wave.set([82, 73, 70, 70]);
 tracks['song.bin'] = track('song.bin', wave);
 await context.start('test.cue', cue, name => tracks[name]);
 const mappedWave = context.Module.discFiles['song.bin'];
 assert(!mappedWave.chunks && !mappedWave.external);
 assert.deepEqual(context.Module.HEAPU8.slice(mappedWave.data, mappedWave.data + wave.length), wave);
 // Streamed raw music maps only its prefix and never iterates the full file.
 for (const file of Object.values(context.Module.discFiles)) context.Module._free(file.data);
 context.started = false; context.Module.discFiles = {}; context.Module.discExternal = [null];
 let closed = 0;
 const streaming = {header: async () => music.slice(0, 65536), close() {closed++;},
  copy(offset, count, destination) {destination.set(music.subarray(offset, offset + count)); return true;}};
 tracks['song.bin'] = {name: 'song.bin', size: async () => 500 << 20, stream: streaming,
  pieces: async function* () {throw new Error('must not preload music');}};
 await context.start('test.cue', cue, name => tracks[name]);
 assert.equal(context.started, true); assert.equal(live.size, 2);
 const streamed = context.Module.discFiles['song.bin'];
 assert.equal(streamed.size, 500 << 20); assert.equal(streamed.stream, streaming);
 assert(!streamed.chunks && streamed.external > 0);
 assert.deepEqual(context.Module.HEAPU8.slice(streamed.data, streamed.data + 65536), music.slice(0, 65536));
 for (const file of Object.values(context.Module.discFiles)) context.Module._free(file.data);
 context.started = false; context.Module.discFiles = {}; context.Module.discExternal = [null];
 streaming.header = async () => {throw new Error('header failed');};
 await context.start('test.cue', cue, name => tracks[name]);
 assert.equal(context.started, false); assert.equal(live.size, 0); assert.equal(closed, 1);
 console.log('Disc loader: fragments, cleanup/retry, RIFF fallback and prefix-only streamed music: OK');
}
loaderTests().catch(error => { console.error(error); process.exitCode = 1; });
