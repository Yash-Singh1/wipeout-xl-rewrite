var audioBuffers = [], audioDestination = {};
Module.gameAudio = { destination: function () { return audioDestination; } };
Module.SDL3 = { audioContext: {
  state: 'running', currentTime: 0,
  createBuffer: function (channels, length, rate) {
    var samples = [new Float32Array(length), new Float32Array(length)];
    var buffer = { length: length, sampleRate: rate, getChannelData: function (c) { return samples[c]; } };
    audioBuffers.push(buffer); return buffer;
  },
  createBufferSource: function () { return { connect: function () {}, disconnect: function () {},
    start: function () {}, stop: function () {} }; }
} };
