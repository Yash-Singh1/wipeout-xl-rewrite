const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const scope = {}; vm.createContext(scope);
vm.runInContext(fs.readFileSync('platform/web/viewport.js', 'utf8'), scope);
function events() {
  const listeners = {};
  return {addEventListener(name, use) {(listeners[name] ??= []).push(use);},
    dispatchEvent(event) { for (const use of listeners[event.type] || []) use(event); }};
}
function fixture(visual) {
  const canvas = {style: {}}, frames = [], timers = new Map(); let timerId = 0, resizes = 0;
  const insets = {paddingLeft: '0px', paddingRight: '0px', paddingTop: '0px', paddingBottom: '0px'};
  const browser = Object.assign(events(), {innerWidth: 375, innerHeight: 600, devicePixelRatio: 2,
    document: Object.assign(events(), {createElement() {return {style: {}};}, body: {appendChild() {}}}),
    getComputedStyle() {return insets;}, Event: class {constructor(type) {this.type = type;}},
    requestAnimationFrame(use) {frames.push(use);},
    setTimeout(use) {timers.set(++timerId, use); return timerId;}, clearTimeout(id) {timers.delete(id);}});
  if (visual) browser.visualViewport = Object.assign(events(),
    {width: 375, height: 550, scale: 1, offsetTop: 0, offsetLeft: 0});
  // Model SDL's external_size callback, which reads CSS rather than stale innerWidth.
  browser.addEventListener('resize', () => {
    resizes++;
    canvas.width = Math.round(parseFloat(canvas.style.width) * browser.devicePixelRatio);
    canvas.height = Math.round(parseFloat(canvas.style.height) * browser.devicePixelRatio);
  });
  const emit = (target, type) => target.dispatchEvent(new browser.Event(type));
  const flush = () => {let limit = 10; while (frames.length) {assert(limit-- > 0, 'resize event loop'); frames.shift()();}};
  scope.installGameViewport(canvas, browser); flush();
  return {canvas, browser, timers, flush, emit, insets, resizes: () => resizes};
}
const f = fixture(true), {canvas, browser, flush, emit} = f;
assert.equal(canvas.width, 750); assert.equal(canvas.height, 1100);
emit(browser, 'orientationchange'); flush(); // Early event still reports portrait.
browser.visualViewport.width = 667; browser.visualViewport.height = 325;
for (const callback of [...f.timers.values()]) callback(); flush(); // Final size arrives without window resize.
assert.equal(canvas.style.width, '667px'); assert.equal(canvas.width, 1334);
assert.equal(canvas.height, 650); assert.equal(browser.innerWidth, 375);
browser.visualViewport.width = 375; browser.visualViewport.height = 550;
emit(browser.visualViewport, 'resize'); flush();
assert.equal(canvas.width, 750); assert.equal(canvas.height, 1100);
const count = f.resizes();
for (let i = 0; i < 20; i++) emit(browser.visualViewport, 'resize');
flush(); assert.equal(f.resizes(), count); // Unchanged values do not repeatedly reset the drawing buffer.
browser.devicePixelRatio = 3; emit(browser.visualViewport, 'resize'); flush();
assert.equal(canvas.width, 1125); assert.equal(canvas.height, 1650);
browser.visualViewport.height = 480; emit(browser.visualViewport, 'resize'); flush();
assert.equal(canvas.height, 1440); // Browser toolbar / keyboard.
browser.visualViewport.offsetTop = 10; emit(browser.visualViewport, 'scroll'); flush();
assert.equal(canvas.style.top, '10px');
browser.visualViewport.scale = 2; browser.innerWidth = 667; browser.innerHeight = 325;
emit(browser.visualViewport, 'resize'); flush(); assert.equal(canvas.width, 2001); // Do not double-apply pinch zoom.
browser.innerWidth = browser.innerHeight = 0;
browser.visualViewport.width = browser.visualViewport.height = 0;
emit(browser.visualViewport, 'resize'); flush(); assert.equal(canvas.width, 2001); // Hidden zero-sized viewports retain the buffer.
const fallback = fixture(false);
fallback.browser.innerWidth = 800; fallback.browser.innerHeight = 450;
fallback.emit(fallback.browser, 'resize'); fallback.flush();
assert.equal(fallback.canvas.width, 1600); assert.equal(fallback.canvas.height, 900);
// Notched landscape display, then portrait with a status area and home indicator.
Object.assign(fallback.insets, {paddingLeft: '44px', paddingRight: '44px', paddingBottom: '21px'});
fallback.emit(fallback.browser.document, 'fullscreenchange'); fallback.flush();
assert.equal(fallback.canvas.style.left, '44px'); assert.equal(fallback.canvas.width, 1424);
assert.equal(fallback.canvas.height, 858);
Object.assign(fallback.insets, {paddingLeft: '0px', paddingRight: '0px', paddingTop: '44px', paddingBottom: '34px'});
fallback.browser.innerWidth = 375; fallback.browser.innerHeight = 812;
fallback.emit(fallback.browser, 'orientationchange'); fallback.flush();
assert.equal(fallback.canvas.style.left, '0px'); assert.equal(fallback.canvas.style.top, '44px');
assert.equal(fallback.canvas.height, 1468);
console.log('Viewport: delayed orientation, both directions, visible-height changes, DPR, safe areas and fallback: OK');
