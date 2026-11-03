/* 至日 · 确定性回归（TECH.md §4）
   同 seed 两跑 → digest 必须一致；异 seed → 必须不一致。
   node 直跑，不需要浏览器、不需要 Playwright —— 这正是 sim/ 零渲染依赖的意义。

   node tests/determinism.js      （或 npm run det）
   退出码 0 = PASS，1 = FAIL。
*/
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.resolve(__dirname, '..');

/* 只装 sim 与 data：render / ui 一个都不装，跑得起来就说明 sim 干净 */
const FILES = [
  'data/world.js',
  'data/cards.matter.js',
  'data/cards.rite.js',
  'data/cards.law.js',
  'data/index.js',
  'sim/core.js',
  'sim/eco.js',
  'sim/cast.js',
  'sim/time.js',
  'sim/codex.js'
];

function boot() {
  const quiet = { log() {}, warn() {}, error: console.error };   // 卡表校验的告警不用重复三遍
  const sandbox = { console: quiet };
  sandbox.window = sandbox;              // 浏览器里 window 就是全局，这里照抄
  vm.createContext(sandbox);
  for (const f of FILES) {
    const code = fs.readFileSync(path.join(ROOT, f), 'utf8');
    vm.runInContext(code, sandbox, { filename: f });
  }
  return sandbox.SOL;
}

const LAND = 'terrain.earth.0.matter.land';
const SUN = 'sky.light.0.matter.sun';
const WATER = 'terrain.mineral.0.matter.water';

const hasTag = (c, t) => !!(c.runtime && c.runtime.tags && c.runtime.tags.indexOf(t) >= 0);

/* 一段固定的施卡剧本 + N 月 tick：不依赖 UI、不依赖帧率 */
function run(seed) {
  const SOL = boot();
  const S = SOL.sim, D = SOL.data;

  S.startWorld({
    name: 'T', form: '单岛界', climate: '温带海洋',
    seed: seed.split('-'), traits: []
  });
  S.dealHand();

  const play = id => {
    const c = D.byId.get(id);
    if (!c) throw new Error('卡不存在：' + id);
    const r = S.cast(c, 0, 0);
    if (!r.ok) throw new Error('施卡失败：' + id + ' / ' + r.msg);
  };

  play(LAND);
  play(SUN);
  play(WATER);

  /* 让生态自己跑：草与兽落进 0,0，之后全靠 eco.js 的繁殖/捕食/死亡 */
  const spawnable = D.cards.filter(c => c.sim && c.sim.op === 'spawn');
  const grass = spawnable.find(c => hasTag(c, 'grower') && hasTag(c, 'propagator'));
  const beast = spawnable.find(c => hasTag(c, 'mobile'));
  if (grass) { S.cast(grass, 0, 0); S.cast(grass, 1, 0); }
  if (beast) S.cast(beast, 0, 0);

  for (let i = 0; i < 120; i++) S.tick();   // 10 个世界年
  return { d: S.digest(), life: S.lifeStats(), grass: grass && grass.id, beast: beast && beast.id };
}

const SSEED = 'SOL-ISL-A1B2-C3D4';
const alt = 'SOL-ARC-Z9Y8-X7W6';

let fail = 0;
const say = (ok, msg) => { console.log((ok ? '  ok   ' : '  FAIL ') + msg); if (!ok) fail++; };

console.log('确定性回归 · ' + FILES.length + ' 个 sim/data 文件，无渲染依赖\n');

const A = run(SSEED);
const B = run(SSEED);
const C = run(alt);

console.log('  剧本  草=' + A.grass + '  兽=' + A.beast);
console.log('  seed A  ' + SSEED + ' → hash ' + A.d.hash + ' | 格 ' + A.d.cells + ' | 种 ' + A.life.flora + '/' + A.life.fauna + ' | rng ' + A.d.rng);
console.log('  seed A′ 同 seed 重跑          → hash ' + B.d.hash + ' | 格 ' + B.d.cells + ' | 种 ' + B.life.flora + '/' + B.life.fauna + ' | rng ' + B.d.rng);
console.log('  seed C  ' + alt + ' → hash ' + C.d.hash + ' | 格 ' + C.d.cells + ' | 种 ' + C.life.flora + '/' + C.life.fauna + ' | rng ' + C.d.rng + '\n');

say(A.d.hash === B.d.hash, '同 seed 两跑 digest 一致');
say(A.d.rng === B.d.rng, '同 seed 两跑随机状态一致');
say(A.d.cells === B.d.cells && A.d.cells === C.d.cells, '格数一致（' + A.d.cells + '）');
say(A.d.hash !== C.d.hash, '异 seed 结果不同');

/* 生态真的跑了（否则"一致"只是两个空世界一致） */
say(A.life.flora > 0, '生态真的长起来了（植物种 ' + A.life.flora + '，动物种 ' + A.life.fauna + '）');
say(A.d.month === 120, 'tick 推进到 120 月（实际 ' + A.d.month + '）');

console.log('\n' + (fail ? 'FAIL · ' + fail + ' 项' : 'PASS · 6 项'));
process.exit(fail ? 1 : 0);

console.log('\n' + (fail ? 'FAIL · ' + fail + ' 项' : 'PASS · 6 项'));
process.exit(fail ? 1 : 0);
