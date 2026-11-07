// Create and resume Web Audio in the Play button's real user gesture. Starting
// SDL after an automatic disc download leaves iOS audio suspended otherwise.
function installGameAudio(module, browser) {
  var doc = browser.document, context, media, mediaSink, launch, started = false, starting = false;
  var muted = false, timeout;
  var play = doc.getElementById('play'), without = doc.getElementById('play-muted');
  var status = doc.getElementById('audio-status');
  var enable = doc.createElement('button');
  enable.id = 'enable-audio'; enable.type = 'button'; enable.textContent = 'Enable sound'; enable.hidden = true;
  doc.body.appendChild(enable);
  function update() {
    enable.hidden = muted || !started || (context && context.state === 'running' && (!media || !media.paused));
  }
  function unlock() {
    if (!context) {
      var Audio = browser.AudioContext || browser.webkitAudioContext;
      if (!Audio) return Promise.reject(new Error('Web Audio is unavailable'));
      context = new Audio();
      module.SDL3 = module.SDL3 || {};
      module.SDL3.audioContext = context; // SDL reuses this context when opening its device.
      context.addEventListener('statechange', update);
      var navigator = browser.navigator || {};
      var ios = /iPhone|iPad|iPod/.test(navigator.userAgent || '') ||
                (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
      if (ios && context.createMediaStreamDestination) {
        // Use media playback for the actual mixed samples on iOS, rather than
        // an ambient-only Web Audio destination that the Silent switch mutes.
        mediaSink = context.createMediaStreamDestination();
        media = doc.createElement('audio');
        media.playsInline = true; media.hidden = true; media.srcObject = mediaSink.stream;
        media.addEventListener('playing', update); media.addEventListener('pause', update);
        doc.body.appendChild(media);
      }
    }
    // Call resume and start inside the gesture, before any promise continuation.
    var result = context.resume();
    if (media && media.paused) {
      var playing = media.play();
      if (playing && playing.catch) playing.catch(update);
    }
    var source = context.createBufferSource();
    source.buffer = context.createBuffer(1, 1, context.sampleRate);
    source.connect(context.destination); source.start(0);
    return Promise.resolve(result);
  }
  function start() {
    if (started || starting) return;
    starting = true; play.disabled = true; status.textContent = '';
    var resumed;
    try { resumed = unlock(); } catch (error) { resumed = Promise.reject(error); }
    var waiting = new Promise(function (resolve) { timeout = browser.setTimeout(resolve, 1500); });
    Promise.race([resumed, waiting]).then(function () {
      if (!context || context.state !== 'running') throw new Error('Audio is still suspended');
      started = true; starting = false; browser.clearTimeout(timeout); update(); launch();
    }).catch(function () {
      starting = false; play.disabled = false; browser.clearTimeout(timeout);
      status.textContent = 'Sound could not start. Tap Play to try again, or play without sound.';
      without.hidden = false;
    });
  }
  play.addEventListener('click', start);
  without.addEventListener('click', function () {
    if (started || starting) return;
    muted = true; started = true;
    // The shell's game startup reads this before main parses its environment.
    if (typeof ENV !== 'undefined') ENV.WIPEOUT_MUTE = '1';
    if (context) context.close();
    if (media) media.pause();
    update(); launch();
  });
  function resume() {
    if (!started || muted || !context || (context.state === 'running' && (!media || !media.paused))) return;
    try { unlock().then(update).catch(update); } catch (error) { update(); }
  }
  enable.addEventListener('click', resume);
  ['pointerdown', 'touchend', 'keydown'].forEach(function (name) {
    doc.addEventListener(name, resume, { capture: true, passive: true });
  });
  return {
    destination: function () { return mediaSink || (context && context.destination); },
    ready: function (callback, silent) {
      launch = callback; muted = silent;
      if (muted) { started = true; launch(); return; }
      play.hidden = false;
    },
    snapshot: function () {
      return { started: started, muted: muted, state: context ? context.state : 'not started',
        media: media ? { paused: media.paused, ready: media.readyState, seconds: media.currentTime,
          error: media.error ? media.error.code : null } : null };
    }
  };
}
if (typeof window !== 'undefined') Module.gameAudio = installGameAudio(Module, window);
