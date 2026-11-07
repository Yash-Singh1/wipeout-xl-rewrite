// Bounded streaming for the muted LAN test page. Fingerprinted chunk URLs use
// the browser's HTTP cache, which also works on an HTTP LAN origin without Cache API.
function phoneTrackChunks(manifest) {
  return {
    name: manifest.name,
    size: async function () { return manifest.size; },
    pieces: async function* () {
      var controller = new AbortController();
      var parallel = Math.min(4, manifest.chunks.length), jobs = [];
      function read(index) {
        var chunk = manifest.chunks[index];
        return fetch(chunk.url, { cache: 'force-cache', signal: controller.signal })
          .then(async function (response) {
            if (!response.ok) throw new Error(chunk.url + ': ' + response.status);
            var bytes = new Uint8Array(await response.arrayBuffer());
            if (bytes.length !== chunk.size) throw new Error('Incomplete disc chunk ' + index);
            return { bytes: bytes };
          }).catch(function (error) { return { error: error }; });
      }
      try {
        for (var i = 0; i < parallel; i++) jobs[i] = read(i);
        for (i = 0; i < manifest.chunks.length; i++) {
          var result = await jobs[i % parallel];
          if (result.error) throw result.error;
          jobs[i % parallel] = i + parallel < manifest.chunks.length ? read(i + parallel) : null;
          yield result.bytes;
        }
      } finally {
        controller.abort();
      }
    }
  };
}
