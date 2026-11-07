// Bound outstanding GPU work as well as browser refreshes. A refresh callback
// does not mean the GPU finished a frame, particularly on older mobile GPUs.
function createGamePresentation(browser) {
  var gl, fences = [], waits = 0, waitMs = 0, failures = 0;
  function discard() {
    fences.forEach(function (sync) { gl.deleteSync(sync); });
    fences = [];
  }
  return {
    submitted: function (context) {
      if (gl && gl !== context) discard();
      gl = context;
      if (gl.isContextLost()) { discard(); return; }
      var sync = gl.fenceSync(gl.SYNC_GPU_COMMANDS_COMPLETE, 0);
      if (sync) fences.push(sync); else failures++;
      gl.flush();
    },
    wait: async function () {
      var started = browser.performance.now(), waited = false;
      while (fences.length >= 2) {
        if (gl.isContextLost()) { discard(); break; }
        var status = gl.clientWaitSync(fences[0], 0, 0);
        if (status === gl.TIMEOUT_EXPIRED) {
          waited = true;
          await new Promise(function (resolve) {
            if (browser.document.hidden) browser.setTimeout(resolve, 16);
            else browser.requestAnimationFrame(resolve);
          });
        } else {
          if (status === gl.WAIT_FAILED) failures++;
          gl.deleteSync(fences.shift());
        }
      }
      if (waited) { waits++; waitMs += browser.performance.now() - started; }
    },
    snapshot: function () { return { pending: fences.length, waits: waits, waitMs: waitMs, failures: failures }; }
  };
}
if (typeof window !== 'undefined') Module.gamePresentation = createGamePresentation(window);
