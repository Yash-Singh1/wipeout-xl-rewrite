const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const scope = {}; vm.createContext(scope);
vm.runInContext(fs.readFileSync('platform/web/fullscreen.js', 'utf8'), scope);
function element() {
  const handlers = {}, children = {};
  return {hidden: false, attrs: {}, setAttribute(name, value) {this.attrs[name] = value;},
    addEventListener(name, use) {handlers[name] = use;},
    emit(name, event = {}) {handlers[name]?.(event);}, focus() {this.focused = true;},
    querySelector(name) {return children[name] ??= element();}};
}
function fixture(ios = false) {
  const added = [], queries = {}, root = {}, doc = element();
  Object.assign(doc, {documentElement: root, createElement: element, body: {appendChild(e) {added.push(e);}}});
  const browser = {document: doc, navigator: {userAgent: ios ? 'iPhone' : 'Android'},
    matchMedia(name) {return queries[name] = element();}};
  scope.installGameFullscreen(browser);
  return {doc, root, browser, queries, button: added[0], help: added[1]};
}
(async () => {
  const ios = fixture(true);
  ios.button.emit('click');
  assert.equal(ios.help.hidden, false);
  assert.match(ios.help.querySelector('p').textContent, /Safari.*Share.*Add to Home Screen/);
  assert.equal(ios.help.querySelector('button').focused, true);
  ios.help.querySelector('button').emit('click');
  assert.equal(ios.help.hidden, true); assert.equal(ios.button.focused, true);
  const modern = fixture(); let entered = 0, exited = 0;
  modern.doc.fullscreenEnabled = true;
  modern.root.requestFullscreen = function () {assert.equal(this, modern.root); entered++;};
  modern.doc.exitFullscreen = function () {assert.equal(this, modern.doc); exited++;};
  modern.button.emit('click'); assert.equal(entered, 1);
  modern.doc.fullscreenElement = modern.root; modern.doc.emit('fullscreenchange');
  assert.equal(modern.button.attrs['aria-label'], 'Exit full screen');
  modern.queries['(display-mode: fullscreen)'].matches = true; modern.doc.emit('fullscreenchange');
  assert.equal(modern.button.hidden, false); // Keep the exit control during requested fullscreen.
  modern.button.emit('click'); assert.equal(exited, 1);
  modern.doc.fullscreenElement = null; modern.doc.emit('fullscreenchange');
  assert.equal(modern.button.hidden, true); // Home Screen fullscreen launch needs no button.
  const legacy = fixture(); let prefixed = 0;
  legacy.root.webkitRequestFullscreen = () => {prefixed++;};
  legacy.doc.webkitFullscreenEnabled = true;
  legacy.button.emit('click'); assert.equal(prefixed, 1);
  const failed = fixture();
  failed.root.requestFullscreen = () => Promise.reject(new Error('Denied'));
  failed.button.emit('click'); await Promise.resolve();
  assert.equal(failed.help.hidden, false);
  assert.match(failed.help.querySelector('p').textContent, /could not enter/);
  const denied = fixture(); denied.doc.fullscreenEnabled = false;
  denied.root.requestFullscreen = () => {throw new Error('must not request disabled fullscreen');};
  denied.button.emit('click'); assert.equal(denied.help.hidden, false);
  ios.browser.navigator.standalone = true; ios.doc.emit('fullscreenchange');
  assert.equal(ios.button.hidden, true);
  assert.match(fs.readFileSync('platform/web/shell.html', 'utf8'), /name="apple-mobile-web-app-capable" content="yes"/);
  console.log('Fullscreen: direct/prefixed entry, exit, iOS instructions, denied requests and standalone launch: OK');
})().catch(error => {console.error(error); process.exitCode = 1;});
