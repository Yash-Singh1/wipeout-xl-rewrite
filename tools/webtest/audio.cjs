const assert = require('node:assert/strict');
const fs = require('node:fs'), vm = require('node:vm');
const scope = {ENV: {}}; vm.createContext(scope);
vm.runInContext(fs.readFileSync('platform/web/audio.js', 'utf8'), scope);
function node() {
  const events = {};
  return {hidden: true, disabled: false, textContent: '', addEventListener(name, fn) {events[name] = fn;},
    emit(name) {events[name]?.();}};
}
function fixture(ios = false) {
  const nodes = Object.fromEntries(['play','play-muted','audio-status'].map(id => [id,node()]));
  const doc = node(), timers = new Map(); let id = 0, created = 0, gesture = false, launches = 0;
  doc.getElementById = id => nodes[id]; doc.createElement = kind => {
    const n = node();
    if (kind === 'audio') Object.assign(n, {paused:true,readyState:4,currentTime:0,
      play() {assert(gesture, 'media playback must begin in the gesture');this.paused=false;this.emit('playing');return Promise.resolve();},
      pause() {this.paused=true;this.emit('pause');}});
    return n;
  };
  doc.body = {appendChild(n) {nodes[n.id] = n;}};
  function FakeAudio() {
    assert(gesture, 'audio context must be created in the gesture'); created++;
    Object.assign(this, node(), {state: 'suspended', sampleRate: 48000, destination: {},
      resume() {
        assert(gesture, 'resume must be called before any await');
        if (this.denied) return Promise.reject(new Error('blocked'));
        if (this.pending) return new Promise(() => {});
        this.state = 'running'; this.emit('statechange'); return Promise.resolve();
      }, createBuffer() {return {};}, createBufferSource() {
        return {connect() {},start() {assert(gesture);}};
      }, createMediaStreamDestination() {return this.sink={stream:{}};},
      createScriptProcessor() {return {connect(destination) {this.connected=destination;}};},
      close() {this.state = 'closed';}});
  }
  const browser = {document: doc, AudioContext: FakeAudio,navigator:{userAgent:ios?'iPhone':'desktop'},
    setTimeout(fn) {timers.set(++id, fn);return id;}, clearTimeout(id) {timers.delete(id);}};
  const module = {}, audio = scope.installGameAudio(module, browser);
  function emit(n, event) {gesture = true;n.emit(event);gesture = false;}
  return {nodes, module, audio, doc, timers, browser, emit, created: () => created,
    ready(muted = false) {audio.ready(() => launches++, muted);}, launches: () => launches};
}
const flush = () => new Promise(resolve => setImmediate(resolve));
(async () => {
  const f = fixture();f.ready();
  assert.equal(f.launches(),0);assert.equal(f.created(),0);assert.equal(f.nodes.play.hidden,false);
  f.emit(f.nodes.play,'click');f.emit(f.nodes.play,'click');await flush();
  assert.equal(f.created(),1);assert.equal(f.launches(),1);assert.equal(f.module.SDL3.audioContext.state,'running');
  assert.equal(f.nodes['enable-audio'].hidden,true);
  f.module.SDL3.audioContext.state = 'interrupted';f.module.SDL3.audioContext.emit('statechange');
  assert.equal(f.nodes['enable-audio'].hidden,false);
  f.emit(f.doc,'touchend');await flush();assert.equal(f.nodes['enable-audio'].hidden,true);
  assert.equal(f.launches(),1);
  const muted = fixture();muted.ready(true);assert.equal(muted.launches(),1);assert.equal(muted.created(),0);
  const denied = fixture();denied.ready();
  denied.browser.AudioContext = function () {throw new Error('Unavailable');};
  denied.emit(denied.nodes.play,'click');await flush();
  assert.equal(denied.launches(),0);assert.match(denied.nodes['audio-status'].textContent,/could not start/);
  assert.equal(denied.nodes['play-muted'].hidden,false);
  denied.emit(denied.nodes['play-muted'],'click');assert.equal(denied.launches(),1);
  assert.equal(scope.ENV.WIPEOUT_MUTE,'1');delete scope.ENV.WIPEOUT_MUTE;
  const slow = fixture();slow.ready();
  const Original = slow.browser.AudioContext;
  slow.browser.AudioContext = function () {const c = new Original();c.pending = true;return c;};
  slow.emit(slow.nodes.play,'click');for (const fn of slow.timers.values()) fn();await flush();
  assert.equal(slow.launches(),0);assert.equal(slow.nodes.play.disabled,false);
  slow.module.SDL3.audioContext.pending = false;slow.emit(slow.nodes.play,'click');await flush();
  assert.equal(slow.launches(),1);assert.equal(slow.created(),1);
  const iphone = fixture(true);iphone.ready();iphone.emit(iphone.nodes.play,'click');await flush();
  const c = iphone.module.SDL3.audioContext;
  assert.equal(iphone.audio.destination(),c.sink);
  assert.equal(iphone.audio.snapshot().media.paused,false);
  assert.equal(iphone.launches(),1);
  console.log('Audio activation: trusted-gesture creation/resume, single start, interruption recovery, failure/timeout retry and mute: OK');
})().catch(error => {console.error(error);process.exitCode = 1;});
