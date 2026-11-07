// Fullscreen must be requested from a user gesture. iPhone Safari on iOS 16
// instead needs a Home Screen launch; a canvas cannot hide its browser chrome.
function installGameFullscreen(browser) {
  var doc = browser.document, root = doc.documentElement;
  var standaloneQuery = browser.matchMedia('(display-mode: standalone)');
  var fullscreenQuery = browser.matchMedia('(display-mode: fullscreen)');
  var button = doc.createElement('button');
  button.id = 'game-fullscreen';
  button.type = 'button';
  var help = doc.createElement('div');
  help.id = 'fullscreen-help';
  help.hidden = true;
  help.setAttribute('role', 'dialog');
  help.setAttribute('aria-modal', 'true');
  help.setAttribute('aria-labelledby', 'fullscreen-title');
  help.innerHTML = '<div><h2 id="fullscreen-title">Play full screen</h2><p></p>' +
    '<button type="button">Got it</button></div>';
  doc.body.appendChild(button);
  doc.body.appendChild(help);
  function active() { return doc.fullscreenElement || doc.webkitFullscreenElement; }
  function update() {
    button.hidden = !!browser.navigator.standalone || standaloneQuery.matches ||
                    (fullscreenQuery.matches && !active());
    var label = active() ? 'Exit full screen' : 'Full screen';
    var path = active() ? 'M3 8h5V3M16 3v5h5M21 16h-5v5M8 21v-5H3'
                        : 'M8 3H3v5M16 3h5v5M21 16v5h-5M8 21H3v-5';
    button.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" ' +
      'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false">' +
      '<path d="' + path + '"></path></svg>';
    button.setAttribute('aria-label', label);
    button.title = label;
  }
  function close() { help.hidden = true; button.focus(); }
  var dismiss = help.querySelector('button');
  dismiss.addEventListener('click', close);
  help.addEventListener('click', function (event) { if (event.target === help) close(); });
  help.addEventListener('keydown', function (event) {
    if (event.key === 'Escape') { event.preventDefault(); close(); }
    if (event.key === 'Tab') { event.preventDefault(); dismiss.focus(); }
    event.stopPropagation();
  });
  function explain(failed) {
    var ios = /iPad|iPhone|iPod/.test(browser.navigator.userAgent) ||
              (browser.navigator.platform === 'MacIntel' && browser.navigator.maxTouchPoints > 1);
    help.querySelector('p').textContent = ios
      ? 'On iPhone, open this page in Safari, tap Share, then Add to Home Screen. ' +
        'Launch Wipeout XL from that icon to play without the browser bars.'
      : failed ? 'The browser could not enter full screen. Try again or allow full screen in its settings.'
               : 'This browser does not support full screen for games.';
    help.hidden = false;
    dismiss.focus();
  }
  button.addEventListener('click', function () {
    var exit = doc.exitFullscreen || doc.webkitExitFullscreen;
    var enter = root.requestFullscreen || root.webkitRequestFullscreen;
    var action = active() ? exit : enter;
    if (!action || (!active() && doc.fullscreenEnabled === false && !doc.webkitFullscreenEnabled)) {
      explain(false);
      return;
    }
    try {
      var result = action.call(active() ? doc : root);
      if (result && result.catch) result.catch(function () { explain(true); });
    } catch (error) { explain(true); }
  });
  doc.addEventListener('fullscreenchange', update);
  doc.addEventListener('webkitfullscreenchange', update);
  [standaloneQuery, fullscreenQuery].forEach(function (query) {
    if (query.addEventListener) query.addEventListener('change', update);
    else if (query.addListener) query.addListener(update);
  });
  update();
}

if (typeof window !== 'undefined') installGameFullscreen(window);
