const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const scope={Module:{}};vm.createContext(scope);vm.runInContext(fs.readFileSync('platform/web/saves.js','utf8'),scope);
function fixture() {
  const files=new Map(),timers=new Map(),requests=[],intervals=[];let serial=0;
  const storage={storeLocalEntry(path,entry,callback){files.set(path,entry);callback(null);},
    removeLocalEntry(path,callback){files.delete(path);callback(null);}};
  const browser={setTimeout(fn,ms){timers.set(++serial,{fn,ms});return serial;},clearTimeout(id){timers.delete(id);},
    setInterval(fn){intervals.push(fn);},addEventListener(){},document:{addEventListener(){}}};
  const vfs={syncfs(populate,callback){requests.push({populate,callback});}};
  const saves=scope.createGameSaves(vfs,storage,browser);
  return {files,storage,browser,timers,requests,intervals,saves,expire(){const [id,timer]=timers.entries().next().value;
    timers.delete(id);timer.fn();}};
}
(async()=>{
  const f=fixture(), original=f.storage.storeLocalEntry;
  const restored=f.saves.restore();
  f.storage.storeLocalEntry('/libsdl/card', {contents:'saved'}, ()=>{});
  assert(!f.files.has('/libsdl/card'),'restoration must remain staged before the transaction completes');
  f.requests.shift().callback(null);assert.equal(await restored,true);
  assert.equal(f.files.get('/libsdl/card').contents,'saved');assert.equal(f.storage.storeLocalEntry,original);
  f.saves.save();f.saves.save();f.saves.save();
  assert.equal(f.requests.length,1,'never overlap save transactions');
  assert.equal(f.requests[0].populate,false);f.requests.shift().callback(null);
  assert.equal(f.requests.length,1,'coalesce busy requests into a later save');
  f.requests.shift().callback(null);assert.equal(f.saves.snapshot().writes,2);
  f.saves.watch();f.browser.document.hidden=true;f.intervals[0]();
  assert.equal(f.requests.length,0,'hidden tabs must not keep periodically saving stale state');
  f.browser.document.hidden=false;f.intervals[0]();assert.equal(f.requests.length,1);
  f.requests.shift().callback(null);
  const blocked=fixture(), waiting=blocked.saves.restore();
  blocked.storage.storeLocalEntry('/libsdl/card',{contents:'staged'},()=>{});
  blocked.expire();assert.equal(await waiting,false);assert.equal(blocked.files.size,0);
  blocked.files.set('/libsdl/card',{contents:'live game'});
  let rejected;
  blocked.storage.storeLocalEntry('/libsdl/card',{contents:'late old save'},reason=>rejected=reason);
  assert(rejected);assert.equal(blocked.files.get('/libsdl/card').contents,'live game');
  blocked.storage.removeLocalEntry('/libsdl/card',reason=>assert(reason));
  assert(blocked.files.has('/libsdl/card'));
  // Another mount is unaffected by the cancelled /libsdl restoration.
  blocked.storage.storeLocalEntry('/other/file',{contents:'other'},reason=>assert(!reason));
  assert(blocked.files.has('/other/file'));
  blocked.requests.shift().callback(null);blocked.saves.save();
  assert.equal(blocked.requests.length,0,'a timed-out load must never save defaults over existing browser data');
  assert.equal(blocked.saves.snapshot().phase,'unavailable');
  const failed=fixture(), failure=failed.saves.restore();
  failed.requests.shift().callback(new Error('database unavailable'));assert.equal(await failure,false);
  failed.saves.save();assert.equal(failed.requests.length,0);
  const slow=fixture(), loaded=slow.saves.restore();slow.requests.shift().callback(null);await loaded;
  slow.saves.save();slow.saves.save();slow.expire();
  assert.equal(slow.saves.snapshot().phase,'unavailable');
  slow.requests.shift().callback(null);assert.equal(slow.requests.length,0);
  console.log('Saved data: staged restoration, blocked/failed/late reads, preserved existing data, serialized writes and timeouts: OK');
})().catch(reason=>{console.error(reason);process.exitCode=1;});
