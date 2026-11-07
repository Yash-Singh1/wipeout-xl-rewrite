const assert = require('node:assert/strict'), fs = require('node:fs'), vm = require('node:vm');
const scope = {Module:{}};vm.createContext(scope);
vm.runInContext(fs.readFileSync('platform/web/audio-output.js','utf8'),scope);
function fixture() {
  const sources=[],buffers=[],destination={};
  const context={state:'running',currentTime:0,
    createBuffer(channels,length,rate){
      const data=Array.from({length:channels},()=>new Float32Array(length));
      const buffer={length,sampleRate:rate,getChannelData:i=>data[i]};buffers.push(buffer);return buffer;
    },createBufferSource(){
      const source={connect(d){assert.equal(d,destination);},disconnect(){this.disconnected=true;},
        start(time,offset){this.time=time;this.offset=offset;},stop(){this.stopped=true;}};
      sources.push(source);return source;
    }};
  const output=scope.createGameAudioOutput();assert(output.open(context,destination,44100));
  return {output,context,sources,buffers};
}
const f=fixture(), samples=new Int16Array(4096);
for(let i=0;i<samples.length;i++)samples[i]=(i*31&65535)-32768;
// Queue blocks exactly as psx_io.c does, including an offset into the live heap.
for(let i=0;i<8;i++)f.output.queue(samples,i*512,256);
assert.equal(f.sources.length,1);assert.equal(f.sources[0].time,0.12);
for(let c=0;c<2;c++)for(let i=0;i<2048;i++)
  assert.equal(f.buffers[0].getChannelData(c)[i],samples[i*2+c]/32768);
assert.equal(f.buffers[0].sampleRate,44100,'native resampling retains the original source rate');
f.context.currentTime=0.04;f.output.queue(samples,0,2048);
assert.equal(f.sources[1].time,f.sources[0].time+2048/44100,'blocks must be contiguous');
// Playback was submitted ahead, so a missed main-thread callback needs no action.
f.context.currentTime=0.12;assert.equal(f.sources[0].stopped,undefined);
f.context.currentTime=0.5;f.output.queue(samples,0,2048);
assert.equal(f.output.snapshot().underruns,1);assert.equal(f.output.snapshot().pendingBuffers,1);
assert(f.sources[0].disconnected);assert(f.sources[1].disconnected);
// A fast-forward or stalled audio clock must not allocate an unbounded node queue.
for(let i=0;i<200;i++)f.output.queue(samples,0,2048);
assert(f.output.snapshot().pendingBuffers<=8);assert(f.output.snapshot().droppedFrames>0);
const before=f.sources.length;f.output.pause(true);
assert(f.output.snapshot().paused);assert(f.sources.at(-1).stopped);
f.context.currentTime=2;f.output.queue(samples,0,2048);assert.equal(f.sources.length,before);
f.output.pause(false);assert(!f.output.snapshot().paused);assert(f.sources.length>before);
f.output.close();assert.equal(f.output.snapshot(),null);assert(f.sources.at(-1).stopped);
// Pause in the middle of a buffer resumes from the unplayed source sample.
const p=fixture();p.output.queue(samples,0,2048);p.context.currentTime=0.12+1000/44100;
p.output.pause(true);p.context.currentTime=3;p.output.pause(false);
assert(Math.abs(p.sources[1].offset-1000/44100)<1/44100);
assert.equal(p.sources[1].time,3);
console.log('Scheduled web audio: exact PCM, contiguous source timing, bounded allocation, underrun recovery and pause/resume: OK');
