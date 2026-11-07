// CSS owns the visible size; SDL's resize callback updates its logical window
// and high-DPI drawing buffer together. iOS can update visualViewport after the
// window resize/orientation events, including while the browser toolbar moves.
function installGameViewport(canvas, browser) {
  var pending = false, previous = {}, settle = [];
  function update() {
    pending = false;
    var viewport = browser.visualViewport;
    var visible = viewport && Math.abs((viewport.scale || 1) - 1) < 0.01 &&
                  viewport.width > 0 && viewport.height > 0;
    var width = Math.round(visible ? viewport.width : browser.innerWidth);
    var height = Math.round(visible ? viewport.height : browser.innerHeight);
    if (width <= 0 || height <= 0) return;
    var left = visible ? Math.round(viewport.offsetLeft || 0) : 0;
    var top = visible ? Math.round(viewport.offsetTop || 0) : 0;
    var ratio = browser.devicePixelRatio || 1;
    var resized = width !== previous.width || height !== previous.height || ratio !== previous.ratio;
    if (!resized && left === previous.left && top === previous.top) return;
    previous = { width: width, height: height, ratio: ratio, left: left, top: top };
    canvas.style.width = width + 'px';
    canvas.style.height = height + 'px';
    canvas.style.left = left + 'px';
    canvas.style.top = top + 'px';
    if (resized) browser.dispatchEvent(new browser.Event('resize'));
  }
  function schedule() {
    if (!pending) {
      pending = true;
      browser.requestAnimationFrame(update);
    }
  }
  function rotated() {
    settle.forEach(function (id) { browser.clearTimeout(id); });
    schedule();
    // Some iOS versions publish the final dimensions after the rotation animation.
    settle = [100, 300, 700, 1500].map(function (delay) { return browser.setTimeout(schedule, delay); });
  }
  browser.addEventListener('resize', schedule);
  browser.addEventListener('orientationchange', rotated);
  if (browser.visualViewport) {
    browser.visualViewport.addEventListener('resize', schedule);
    browser.visualViewport.addEventListener('scroll', schedule);
  }
  browser.document.addEventListener('visibilitychange', function () {
    if (!browser.document.hidden) rotated();
  });
  update(); // Before SDL creates the window, so it detects CSS-controlled sizing.
}

if (typeof window !== 'undefined') installGameViewport(Module.canvas, window);
