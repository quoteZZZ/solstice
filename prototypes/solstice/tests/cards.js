/* Solstice · 卡表契约回归（CARD-SCHEMA §8 校验 / §9 完整数据）
   node tests/cards.js        （npm run cards）
   只装 data/ + 校验器，不碰渲染、不需要浏览器——所以能进 CI。
   硬错误必须 0 条（data/index.js 会直接抛）；软警告打印出来供 STATUS 记存量。 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.resolve(__dirname, '..');
const FILES = [
  'data/world.js',
  'data/cards.matter.js',
  'data/cards.rite.js',
  'data/cards.law.js',
  'data/index.js'
];
const BINDS = ['clock.hour', 'fog.opacity'];

let fail = 0;
const say = (ok, label, extra) => {
  console.log((ok ? '  ok   ' : '  FAIL ') + label + (extra ? '（' + extra + '）' : ''));
  if (!ok) fail++;
};

function boot() {
  const said = [];
  const sandbox = {
    console: {
      log() {},
      warn: (...a) => said.push(a.join(' ')),
      error: (...a) => said.push(a.join(' '))
    }
  };
  sandbox.window = sandbox;                 // 浏览器里 window 就是全局，这里照抄
  vm.createContext(sandbox);
  for (const f of FILES) {
    vm.runInContext(fs.readFileSync(path.join(ROOT, f), 'utf8'), sandbox, { filename: f });
  }
  return { SOL: sandbox.SOL, said };
}

let booted;
try {
  booted = boot();
} catch (e) {
  console.log('  FAIL 卡表拒绝装配（data/index.js 抛错）：');
  console.log(String(e.message).split('\n').map(l => '       ' + l).join('\n'));
  console.log('\nFAIL · 1 项');
  process.exit(1);
}

const D = booted.SOL.data;
const cards = D.cards || [];

say(!D.errors || D.errors.length === 0, '硬错误 0 条',
  D.errors && D.errors.length ? D.errors.join(' / ') : '');
say(cards.length > 0 && D.total === cards.length, '卡表装配完整（' + D.total + ' 张）');
say(cards.every(c => /^№ \d{4} \/ \d{4}$/.test(c.number || '')), '每张卡都有图鉴编号');
say(new Set(cards.map(c => c.id)).size === cards.length, 'id 无重复');
say(cards.every(c => c.line && typeof c.line.fx === 'string' && c.line.fx.length > 0), '每张卡都有卡面 fx');
say(cards.every(c => (c.line.hl || []).every(h => c.line.fx.indexOf(h) >= 0)), '高亮片段都能在 fx 里指到');
say(cards.every(c => (c.line.num || []).length <= 4), '卡面小数字不超过 4 条');
say(cards.every(c => (c.stats || []).every(s => !s.bind || BINDS.indexOf(s.bind) >= 0)), 'stats.bind 都是已登记读数');
say(cards.every(c => c.reclaim === (c.tier === 0 ? 0 : D.RECLAIM[Math.min(10, c.tier)])), 'reclaim 按环级查表（0 环为 0）');
say(Math.max(...cards.map(c => c.reclaim)) < 12, '单张回收价不到一包基础包（退回<一包，无套利）');

console.log('\n软警告 ' + (D.warn || []).length + ' 条（不拦，记进 STATUS.md §3）：');
(D.warn || []).forEach(w => console.log('  · ' + w));

console.log('\n' + (fail ? 'FAIL · ' + fail + ' 项' : 'PASS · 10 项'));
process.exit(fail ? 1 : 0);
