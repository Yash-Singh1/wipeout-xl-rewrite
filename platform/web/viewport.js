// CSS owns the visible size; SDL's resize callback updates its logical window
// and high-DPI drawing buffer together. iOS can update visualViewport after the
// window resize/orientation events, including while the browser toolbar moves.
function installGameViewport(canvas, browser) {
  var pending = false, previous = {}, settle = [];
  // Read CSS env() values after each resize. Keep HUD and touch controls inside
  // the safe rectangle, including when a Home Screen app fills the display.
  var safe = browser.document.createElement('div');
  safe.style.cssText = 'position:fixed;visibility:hidden;pointer-events:none;' +
    'padding:env(safe-area-inset-top) env(safe-area-inset-right) ' +
    'env(safe-area-inset-bottom) env(safe-area-inset-left);';
  browser.document.body.appendChild(safe);
  function update() {
    pending = false;
    var viewport = browser.visualViewport;
    var visible = viewport && Math.abs((viewport.scale || 1) - 1) < 0.01 &&
                  viewport.width > 0 && viewport.height > 0;
    var width = Math.round(visible ? viewport.width : browser.innerWidth);
    var height = Math.round(visible ? viewport.height : browser.innerHeight);
    if (width <= 0 || height <= 0) return;
    var inset = browser.getComputedStyle(safe);
    var insetLeft = Math.max(0, Math.round(parseFloat(inset.paddingLeft) || 0));
    var insetRight = Math.max(0, Math.round(parseFloat(inset.paddingRight) || 0));
    var insetTop = Math.max(0, Math.round(parseFloat(inset.paddingTop) || 0));
    var insetBottom = Math.max(0, Math.round(parseFloat(inset.paddingBottom) || 0));
    width -= insetLeft + insetRight;
    height -= insetTop + insetBottom;
    if (width <= 0 || height <= 0) return;
    var left = (visible ? Math.round(viewport.offsetLeft || 0) : 0) + insetLeft;
    var top = (visible ? Math.round(viewport.offsetTop || 0) : 0) + insetTop;
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
  browser.document.addEventListener('fullscreenchange', rotated);
  browser.document.addEventListener('webkitfullscreenchange', rotated);
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
