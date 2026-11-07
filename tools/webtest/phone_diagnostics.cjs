const assert = require('node:assert/strict');
const fs = require('node:fs'), vm = require('node:vm');
let request, contexts = 0, now = 4000;
function FakeGL() {}
FakeGL.prototype.drawArrays = function (mode, first, count) {
  assert.equal(this, gl); assert.deepEqual([mode, first, count], [4, 0, 3]);
  now += 2;
  return 17;
};
const gl = new FakeGL();
const context = {sampleRate:48000,state:'running',currentTime:4,audio_playback:{}};
const sandbox = {Module:{preRun:[],output:[],HEAPU8:new Uint8Array(16),SDL3:{audioContext:context}},
  window:{WebGL2RenderingContext:FakeGL},WebGL2RenderingContext:FakeGL,
  ENV:{},canvas:{width:10,height:10,addEventListener(){},getContext(){
    contexts++;assert(sandbox.Module.pictureUnseen!==undefined);return {getContextAttributes:()=>({alpha:false})};}},
  phoneRevision:'hash',performance:{now:()=>now},navigator:{userAgent:'test'},document:{hidden:false},
  setInterval(){},fetch:async (url,options)=>{request={url,body:JSON.parse(options.body)};}};
vm.createContext(sandbox);vm.runInContext(fs.readFileSync('platform/web/phone-diagnostics.js','utf8'),sandbox);
sandbox.Module.preRun[0]();assert.equal(sandbox.ENV.WIPEOUT_TIMING_LOG,'/phone-timing.csv');
sandbox.phoneReport();assert.equal(contexts,0,'diagnostics must not create the WebGL context before SDL');
sandbox.Module.gameAudioOutput={snapshot:()=>({buffers:10,underruns:2,pendingBuffers:3})};
assert.equal(gl.drawArrays(4,0,3),17,'graphics profiling preserves the result and call arguments');
sandbox.Module.pictureUnseen=false;sandbox.phoneReport();
assert.equal(request.url,'phone-diagnostics');
assert.equal(request.body.data.audioOutput.buffers,10);
assert.equal(request.body.data.audioOutput.underruns,2);
assert.equal(contexts,1);
assert.deepEqual(request.body.data.graphicsCalls.drawArrays,{calls:1,ms:2,max:2});
sandbox.phoneReport();
assert.deepEqual(request.body.data.graphicsCalls,{},'graphics measurements reset between reports');
console.log('Phone diagnostics: report scheduled audio, preserve GL calls and avoid premature context creation: OK');
