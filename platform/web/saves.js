// IndexedDB can remain blocked by another page. Stage restoration until the
// entire read completes so a timeout cannot later overwrite a running game.
function createGameSaves(fs, storage, browser) {
  var phase = 'idle', error = '', operations = [], originals = {}, guards = {};
  var writable = false, saving = false, again = false, writes = 0, watching = false;
  function owned(path) { return path === '/libsdl' || path.indexOf('/libsdl/') === 0; }
  function message(reason) { return reason && reason.message || String(reason); }
  function unavailable(reason) {
    phase = 'unavailable'; error = message(reason); writable = false; operations = [];
  }
  function guard(name) {
    originals[name] = storage[name];
    guards[name] = function () {
      var args = Array.prototype.slice.call(arguments), callback = args[args.length - 1];
      if (owned(args[0])) {
        if (phase === 'restoring') {
          operations.push({ name: name, args: args.slice(0, -1) }); callback(null); return;
        }
        if (phase === 'unavailable') { callback(new Error('Saved-data restoration was cancelled')); return; }
      }
      return originals[name].apply(storage, args);
    };
    storage[name] = guards[name];
  }
  function save() {
    if (!writable) return;
    if (saving) { again = true; return; }
    saving = true; again = false;
    var finished = false;
    var timer = browser.setTimeout(function () {
      if (finished) return;
      finished = true; saving = false;
      unavailable(new Error('Saving timed out'));
    }, 10000);
    function complete(reason) {
      if (finished) return;
      finished = true; browser.clearTimeout(timer); saving = false;
      if (reason) { unavailable(reason); return; }
      writes++;
      if (again) save();
    }
    try { fs.syncfs(false, complete); } catch (reason) { complete(reason); }
  }
  return {
    restore: function () {
      phase = 'restoring';
      guard('storeLocalEntry'); guard('removeLocalEntry');
      return new Promise(function (resolve) {
        var finished = false;
        function complete(reason) {
          if (finished) return;
          finished = true; browser.clearTimeout(timer);
          if (!reason) {
            phase = 'applying';
            operations.forEach(function (operation) {
              if (reason) return;
              originals[operation.name].apply(storage, operation.args.concat(function (failure) { reason = failure; }));
            });
          }
          operations = [];
          if (reason) {
            // Keep the guards installed: a late IndexedDB callback may still
            // try to apply a restored file after gameplay has started.
            unavailable(reason); resolve(false); return;
          }
          Object.keys(originals).forEach(function (name) {
            if (storage[name] === guards[name]) storage[name] = originals[name];
          });
          phase = 'ready'; writable = true; resolve(true);
        }
        var timer = browser.setTimeout(function () { complete(new Error('Loading saved data timed out')); }, 5000);
        try { fs.syncfs(true, complete); } catch (reason) { complete(reason); }
      });
    },
    watch: function () {
      if (watching || !writable) return;
      watching = true;
      browser.setInterval(function () { if (!browser.document.hidden) save(); }, 3000);
      browser.addEventListener('pagehide', save);
      browser.document.addEventListener('visibilitychange', function () { if (browser.document.hidden) save(); });
    },
    save: save,
    snapshot: function () { return { phase: phase, error: error, writable: writable, saving: saving, writes: writes }; }
  };
}
Module.createGameSaves = createGameSaves;
