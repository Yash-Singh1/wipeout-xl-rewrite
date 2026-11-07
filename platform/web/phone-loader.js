// Bounded streaming for the LAN test pages. Fingerprinted chunk URLs use
// the browser's HTTP cache, which also works on an HTTP LAN origin without Cache API.
// Share one cache across all music tracks. Fetch at most two chunks at a time
// and retain at most six 1 MiB chunks, including on a seek or track change.
function createPhoneMusicCache() {
  var entries = new Map(), pending = new Set(), limit = 6, parallel = 2;
  var fetches = 0, failures = 0, lastFetchMs = 0;
  function touch(key, entry) { entries.delete(key); entries.set(key, entry); }
  function remove(key, entry) {
    entries.delete(key);
    if (entry.controller) entry.controller.abort();
  }
  async function load(chunk, protectedKeys, speculative) {
    var key = chunk.url, existing = entries.get(key);
    if (existing && existing.error && Date.now() >= existing.retryAfter) {
      remove(key, existing); existing = null;
    }
    if (existing) { touch(key, existing); return existing.promise; }
    while (pending.size >= parallel) {
      if (speculative) return null;
      await Promise.race(Array.from(pending));
      existing = entries.get(key);
      if (existing) { touch(key, existing); return existing.promise; }
    }
    if (entries.size >= limit) {
      var oldest = Array.from(entries).find(function (pair) { return !protectedKeys.has(pair[0]); });
      if (!oldest) return { error: new Error('Music read exceeds the buffer limit') };
      remove(oldest[0], oldest[1]);
    }
    var entry = { controller: new AbortController() };
    var started = Date.now(); fetches++;
    entries.set(key, entry);
    var timeout = setTimeout(function () { entry.controller.abort(); }, 15000);
    entry.promise = Promise.resolve().then(function () {
      return fetch(key, { cache: 'force-cache', signal: entry.controller.signal });
    })
      .then(async function (response) {
        if (!response.ok) throw new Error(key + ': ' + response.status);
        var bytes = new Uint8Array(await response.arrayBuffer());
        if (bytes.length !== chunk.size) throw new Error('Incomplete music chunk');
        if (entries.get(key) === entry) entry.bytes = bytes;
        return { bytes: bytes };
      }).catch(function (error) {
        failures++;
        if (entries.get(key) === entry) {
          entry.error = error; entry.retryAfter = Date.now() + 1000;
        }
        return { error: error };
      }).finally(function () {
        lastFetchMs = Date.now() - started;
        clearTimeout(timeout); pending.delete(entry.promise);
      });
    pending.add(entry.promise);
    return entry.promise;
  }
  return {
    load: load,
    peek: function (chunk) {
      var entry = entries.get(chunk.url);
      if (!entry || !entry.bytes) return null;
      touch(chunk.url, entry);
      return entry.bytes;
    },
    release: function (chunks) {
      chunks.forEach(function (chunk) {
        var entry = entries.get(chunk.url);
        if (entry) remove(chunk.url, entry);
      });
    },
    stats: function () {
      var bytes = 0;
      entries.forEach(function (entry) { if (entry.bytes) bytes += entry.bytes.length; });
      return { bytes: bytes, entries: entries.size, pending: pending.size, limit: limit, parallel: parallel,
        fetches: fetches, failures: failures, lastFetchMs: lastFetchMs };
    }
  };
}
var phoneMusicCache;
function phoneMusicStream(manifest) {
  var cache = phoneMusicCache || (phoneMusicCache = createPhoneMusicCache());
  var chunkSize = 1 << 20, closed = false, reportedError;
  function range(offset, count) {
    if (closed || offset < 0 || count <= 0 || offset + count > manifest.size) return null;
    var first = Math.floor(offset / chunkSize), last = Math.floor((offset + count - 1) / chunkSize);
    if (last - first >= 6) return null;
    return { first: first, last: last, keys: new Set(manifest.chunks.slice(first, last + 1).map(function (c) { return c.url; })) };
  }
  async function ensure(offset, count) {
    var part = range(offset, count);
    if (!part) return false;
    for (var i = part.first; i <= part.last; i++) {
      var result = await cache.load(manifest.chunks[i], part.keys, false);
      if (closed) return false;
      if (result.error) {
        if (reportedError === result.error) return false;
        reportedError = result.error;
        throw result.error;
      }
    }
    return true;
  }
  function copy(offset, count, destination, ahead) {
    var part = range(offset, count);
    if (!part) return false;
    var chunks = [];
    for (var i = part.first; i <= part.last; i++) {
      var bytes = cache.peek(manifest.chunks[i]);
      if (!bytes) return false;
      chunks.push(bytes);
    }
    for (var done = 0; done < count;) {
      var at = offset + done, index = Math.floor(at / chunkSize), begin = at % chunkSize;
      var bytes = chunks[index - part.first], take = Math.min(count - done, bytes.length - begin);
      destination.set(bytes.subarray(begin, begin + take), done);
      done += take;
    }
    if (ahead !== false) {
      // Almost twelve seconds of original PCM ahead of the mixer. Playback
      // waits at a cold sector without advancing past its original samples.
      for (i = part.last + 1; i <= part.last + 2 && i < manifest.chunks.length; i++)
        cache.load(manifest.chunks[i], part.keys, true);
    }
    return true;
  }
  return {
    ensure: ensure, copy: copy,
    header: async function () {
      var header = new Uint8Array(Math.min(65536, manifest.size));
      if (!await ensure(0, header.length) || !copy(0, header.length, header, false))
        throw new Error('Could not read music header');
      return header;
    },
    close: function () { closed = true; cache.release(manifest.chunks); }
  };
}
function phoneTrackChunks(manifest) {
  return {
    name: manifest.name,
    stream: manifest.stream ? phoneMusicStream(manifest) : null,
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
