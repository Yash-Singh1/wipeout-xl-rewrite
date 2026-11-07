const fs = require('node:fs'), assert = require('node:assert/strict'), vm = require('node:vm');
const code = fs.readFileSync('platform/video.c','utf8');
const start = code.indexOf('EM_ASYNC_JS(void, ShowPicture, (void), {');
const body = code.slice(code.indexOf('{', start) + 1, code.indexOf('\n});', start));
const run = new (Object.getPrototypeOf(async function(){}).constructor)('Module','document','requestAnimationFrame','setTimeout', body);
const flush = () => new Promise(resolve => setImmediate(resolve));
(async () => {
  const module = {gamePresentation:{wait:async()=>{}}}, frames = [], timers = [];let finished = false;
  const job = run(module,{hidden:false},fn=>frames.push(fn),(fn,ms)=>timers.push({fn,ms})).then(()=>finished=true);
  assert.equal(module.pictureUnseen,true);assert.equal(frames.length,1);assert.equal(timers.length,0);
  await flush();assert.equal(finished,false,'a timer alone must not release a visible presentation');
  frames.shift()();await flush();assert.equal(finished,false,'allow compositor to consume the frame before returning');
  assert.equal(timers[0].ms,0);timers.shift().fn();await job;
  assert.equal(finished,true);assert.equal(module.pictureUnseen,false);
  const hidden = run(module,{hidden:true},()=>{throw new Error('hidden pages must not wait for rAF');},
    (fn,ms)=>timers.push({fn,ms}));
  await flush();assert.equal(timers[0].ms,16);timers.shift().fn();await hidden;
  const scope = {};vm.createContext(scope);
  vm.runInContext(fs.readFileSync('platform/web/presentation.js','utf8'),scope);
  let now=0, serial=0, completed=false, lost=false;const deleted=[];
  const gl={SYNC_GPU_COMMANDS_COMPLETE:1,TIMEOUT_EXPIRED:2,WAIT_FAILED:3,
    fenceSync:()=>++serial,flush(){},isContextLost:()=>lost,
    clientWaitSync:()=>completed?4:2,deleteSync:sync=>deleted.push(sync)};
  const queue=scope.createGamePresentation({document:{hidden:false},performance:{now:()=>now},
    requestAnimationFrame:fn=>frames.push(fn)});
  queue.submitted(gl);await queue.wait();assert.equal(frames.length,0);
  queue.submitted(gl);const waiting=queue.wait();assert.equal(frames.length,1);
  now=34;completed=true;frames.shift()();await waiting;
  assert.deepEqual(deleted,[1]);assert.equal(queue.snapshot().pending,1);
  assert.equal(queue.snapshot().waitMs,34);
  queue.submitted(gl);lost=true;await queue.wait();assert.equal(queue.snapshot().pending,0);
  assert.deepEqual(deleted,[1,2,3]);
  console.log('Web presentation: wait for refresh and compositor, then release; hidden pages retain a timer fallback: OK');
})().catch(error=>{console.error(error);process.exitCode=1;});
