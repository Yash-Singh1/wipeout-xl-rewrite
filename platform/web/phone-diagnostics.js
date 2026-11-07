// Local opt-in phone test only. Static exports do not include this hook.
var phoneSession = Date.now().toString(36) + '-' + Math.random().toString(36).slice(2, 9);
Module.preRun.push(function () { ENV.WIPEOUT_TIMING_LOG = '/phone-timing.csv'; });
var phoneContextLosses = 0;
canvas.addEventListener('webglcontextlost', function () { phoneContextLosses++; });
var phoneGraphics = {};
if (window.WebGL2RenderingContext) {
  ['drawArrays', 'drawElements', 'bufferData', 'texImage2D', 'texImage3D', 'texSubImage2D',
   'texSubImage3D', 'blitFramebuffer', 'clear', 'readPixels', 'getError'].forEach(function (name) {
    var original = WebGL2RenderingContext.prototype[name];
    if (!original) return;
    WebGL2RenderingContext.prototype[name] = function () {
      var started = performance.now();
      try { return original.apply(this, arguments); }
      finally {
        var elapsed = performance.now() - started;
        var stats = phoneGraphics[name] || (phoneGraphics[name] = { calls: 0, ms: 0, max: 0 });
        stats.calls++; stats.ms += elapsed; stats.max = Math.max(stats.max, elapsed);
      }
    };
  });
}
function phoneReport() {
  var audio = Module.SDL3 && Module.SDL3.audioContext;
  var gl = Module.pictureUnseen !== undefined ? canvas.getContext('webgl2') : null;
  var timing = typeof FS !== 'undefined' && FS.analyzePath('/phone-timing.csv').exists
    ? FS.readFile('/phone-timing.csv', { encoding: 'utf8' }).split('\n').slice(-15).join('\n') : '';
  var data = {
    revision: phoneRevision, seconds: performance.now() / 1000, browser: navigator.userAgent,
    status: typeof statusLine !== 'undefined' ? statusLine.textContent : null,
    saves: Module.gameSaves ? Module.gameSaves.snapshot() : null,
    hidden: document.hidden, pixels: [canvas.width, canvas.height],
    heap: Module.HEAPU8 ? Module.HEAPU8.length : 0,
    music: typeof phoneMusicCache !== 'undefined' && phoneMusicCache ? phoneMusicCache.stats() : null,
    activation: Module.gameAudio ? Module.gameAudio.snapshot() : null,
    audio: audio ? { state: audio.state, seconds: audio.currentTime, sampleRate: audio.sampleRate } : null,
    audioOutput: Module.gameAudioOutput ? Module.gameAudioOutput.snapshot() : null,
    gpuQueue: Module.gamePresentation ? Module.gamePresentation.snapshot() : null,
    contextLost: phoneContextLosses, graphics: gl ? gl.getContextAttributes() : null,
    graphicsCalls: phoneGraphics,
    timing: timing, output: Module.output.slice(-15)
  };
  fetch('phone-diagnostics', { method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ session: phoneSession, data: data }) }).catch(function () {});
  phoneGraphics = {};
}
setInterval(phoneReport, 1000);
