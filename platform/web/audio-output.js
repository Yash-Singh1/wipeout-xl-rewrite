// Schedule the game's PCM ahead of playback. ScriptProcessor callbacks share the
// main thread with rendering and miss their deadlines on slower phones.
function createGameAudioOutput() {
  var context, destination, rate, cursor = 0, filled = 0, paused = false, started = false;
  var frames = 2048, lead = 0.12, maximumLead = 0.35;
  var staging = [new Float32Array(frames), new Float32Array(frames)], scheduled = [];
  var counters;
  function release(record, stop) {
    if (!record.source) return;
    record.source.onended = null;
    if (stop) record.source.stop();
    record.source.disconnect(); record.source = null;
  }
  function prune() {
    if (paused || !context) return;
    scheduled = scheduled.filter(function (record) {
      if (record.start + (record.buffer.length - record.offset) / rate > context.currentTime) return true;
      release(record, false); return false;
    });
  }
  function play(record) {
    var source = context.createBufferSource();
    source.buffer = record.buffer; source.connect(destination); record.source = source;
    source.onended = function () {
      release(record, false);
      var index = scheduled.indexOf(record);
      if (index >= 0) scheduled.splice(index, 1);
    };
    source.start(record.start, record.offset / rate);
  }
  function submit() {
    prune();
    var now = context.currentTime;
    if (cursor < now || !started) {
      if (started) counters.underruns++;
      cursor = now + lead;
    }
    if (cursor + frames / rate > now + maximumLead) {
      counters.droppedFrames += frames;
      return; // bound both queued latency and live AudioBuffer allocations
    }
    var buffer = context.createBuffer(2, frames, rate);
    for (var channel = 0; channel < 2; channel++) buffer.getChannelData(channel).set(staging[channel]);
    var record = { buffer: buffer, offset: 0, start: cursor, source: null };
    scheduled.push(record); play(record);
    cursor += frames / rate; started = true; counters.buffers++;
  }
  function close() {
    scheduled.forEach(function (record) { release(record, true); });
    scheduled = []; context = null; destination = null; filled = 0; cursor = 0; paused = false; started = false;
  }
  return {
    open: function (audio, output, sampleRate) {
      close();
      if (!audio || audio.state === 'closed' || !output) return false;
      context = audio; destination = output; rate = sampleRate;
      counters = { frames: 0, buffers: 0, nonzeroBlocks: 0, peak: 0, underruns: 0, droppedFrames: 0 };
      return true;
    },
    queue: function (samples, offset, count) {
      if (!context || paused) return;
      var nonzero = false;
      for (var i = 0; i < count; i++) {
        for (var channel = 0; channel < 2; channel++) {
          var value = samples[offset + i * 2 + channel] / 32768;
          staging[channel][filled] = value;
          if (value !== 0) nonzero = true;
          counters.peak = Math.max(counters.peak, Math.abs(value));
        }
        if (++filled === frames) { submit(); filled = 0; }
      }
      counters.frames += count;
      if (nonzero) counters.nonzeroBlocks++;
    },
    pause: function (value) {
      if (!context || value === paused) return;
      var now = context.currentTime;
      if (value) {
        prune();
        scheduled.forEach(function (record) {
          record.offset += Math.max(0, Math.min(record.buffer.length - record.offset,
            Math.floor((now - record.start) * rate)));
          record.delay = Math.max(0, record.start - now);
          release(record, true);
        });
      } else {
        cursor = 0;
        scheduled.forEach(function (record) {
          record.start = now + record.delay; play(record);
          cursor = Math.max(cursor, record.start + (record.buffer.length - record.offset) / rate);
        });
      }
      paused = value;
    },
    close: close,
    snapshot: function () {
      prune();
      return context ? Object.assign({}, counters, { paused: paused, pendingBuffers: scheduled.length,
        pendingFrames: filled, queuedSeconds: paused ? 0 : Math.max(0, cursor - context.currentTime) }) : null;
    }
  };
}
Module.gameAudioOutput = createGameAudioOutput();
