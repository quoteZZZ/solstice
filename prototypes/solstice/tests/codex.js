/* Solstice · 图鉴回归（SETTING.md §5.1 / STATUS.md §2）
   验的不是「有没有渲染」，是「6 条判定能不能被真的玩出来」：
   每条都给一个可复现的剧本，能点亮 = PASS。

   顺带验两件容易退化的：
   · 换界后已经点亮的页不丢、连续月计数归零
   · 只有雨 / 只有泉，都不该点亮「旱涝同界」（否则这条判定就是假的）

   node tests/codex.js      （或 npm run codex）
   退出码 0 = PASS，1 = FAIL。 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.resolve(__dirname, '..');
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
  const quiet = { log() {}, warn() {}, error: console.error };
  const sandbox = { console: quiet };
  sandbox.window = sandbox;
  vm.createContext(sandbox);
  for (const f of FILES) vm.runInContext(fs.readFileSync(path.join(ROOT, f), 'utf8'), sandbox, { filename: f });
  const S = sandbox.SOL.sim, D = sandbox.SOL.data;
  S.startWorld({ name: 'T', form: '单岛界', climate: '温带海洋',
    seed: ['SOL', 'ISL', 'A1B2', 'C3D4'], traits: [] });
  return { S, D, SOL: sandbox.SOL };
}

const CAST = (S, D, id, x, y) => {
  const r = S.cast(D.byId.get(id), x || 0, y || 0);
  if (!r.ok) throw new Error('施卡失败：' + id + ' / ' + r.msg);
};

const rows = [];
const bad = [];
function ok(name, cond, note) {
  rows.push((cond ? '  ok   ' : '  FAIL ') + name + (note ? '（' + note + '）' : ''));
  if (!cond) bad.push(name);
}

/* ── 1 三阳：连打三张太阳 ── */
{
  const { S, D } = boot();
  CAST(S, D, 'terrain.earth.0.matter.land');
  CAST(S, D, 'sky.light.0.matter.sun');
  CAST(S, D, 'sky.light.0.matter.sun');
  ok('两阳还不算三阳', !S.codex.on.has('sky.three'));
  CAST(S, D, 'sky.light.0.matter.sun');
  ok('三阳', S.codex.on.has('sky.three'), '太阳 ' + S.W.sun);
}

/* ── 2 长夜：不升太阳，种一株，走满一年 ── */
{
  const { S, D } = boot();
  CAST(S, D, 'terrain.earth.0.matter.land');
  CAST(S, D, 'flora.wood.0.matter.moss');
  for (let i = 0; i < 11; i++) S.tick();
  ok('第 11 月还没到 12 月', !S.codex.on.has('sky.dark'));
  S.tick();
  ok('长夜', S.codex.on.has('sky.dark'), '第 ' + S.codex.at['sky.dark'].month + ' 月');
}

/* ── 3 长存：一眼泉，等它不干 ── */
{
  const { S, D } = boot();
  CAST(S, D, 'terrain.earth.0.matter.land');
  CAST(S, D, 'terrain.mineral.0.matter.water');
  for (let i = 0; i < 26 && !S.codex.on.has('aqua.hold'); i++) S.tick();
  ok('长存', S.codex.on.has('aqua.hold'), '第 ' + S.codex.at['aqua.hold'].month + ' 月');
  ok('只有泉不算旱涝同界', !S.codex.on.has('aqua.split'));
}

/* ── 4 旱涝同界：泉在一头，另一头烧到见底 ── */
{
  const { S, D } = boot();
  CAST(S, D, 'terrain.earth.0.matter.land');
  CAST(S, D, 'terrain.mineral.0.matter.water');
  for (let i = 0; i < 6; i++) S.tick();
  ok('全湿还不算同界', !S.codex.on.has('aqua.split'));
  /* 烧到见底为止，不写死次数：基础降水是会调的（0.12 → 0.24 就让「两把火烧干」失效了），
     剧本应该按「达成条件」写，而不是按「当时的调节参数」写。 */
  for (let i = 0; i < 5 && S.at(2, 2).W > 0.05; i++) CAST(S, D, 'rite.fire.1.rite.fire', 2, 2);
  ok('旱涝同界', S.codex.on.has('aqua.split'),
    'W ' + S.at(2, 2).W.toFixed(2) + ' ↔ ' + S.at(0, 0).W.toFixed(2));
}

/* ── 5 成林：土地 + 太阳 + 一株苔，让它自己长满 ── */
{
  const { S, D } = boot();
  CAST(S, D, 'terrain.earth.0.matter.land');
  CAST(S, D, 'sky.light.0.matter.sun');
  CAST(S, D, 'flora.wood.0.matter.moss');
  for (let i = 0; i < 220 && !S.codex.on.has('eco.forest'); i++) S.tick();
  ok('成林', S.codex.on.has('eco.forest'), '第 ' + S.codex.at['eco.forest'].month + ' 月');
}

/* ── 6 三级链：草 + 猴 + 狼 ── */
{
  const { S, D } = boot();
  CAST(S, D, 'terrain.earth.0.matter.land');
  CAST(S, D, 'sky.light.0.matter.sun');
  CAST(S, D, 'flora.wood.1.matter.grass');
  CAST(S, D, 'flora.wood.1.matter.grass', 1, 0);
  CAST(S, D, 'flora.wood.1.matter.grass', 0, 1);
  S.tick(); S.tick();
  CAST(S, D, 'fauna.wood.1.matter.monkey');
  ok('有草有猴还不算三级链', !S.codex.on.has('eco.chain'));
  CAST(S, D, 'fauna.dark.2.matter.wolf');
  for (let i = 0; i < 40 && !S.codex.on.has('eco.chain'); i++) S.tick();
  ok('三级链', S.codex.on.has('eco.chain'), '第 ' + S.codex.at['eco.chain'].month + ' 月');
}

/* ── 7 换界：图鉴跨世界共通，计数归零 ── */
{
  const { S, D } = boot();
  CAST(S, D, 'terrain.earth.0.matter.land');
  CAST(S, D, 'sky.light.0.matter.sun');
  CAST(S, D, 'sky.light.0.matter.sun');
  CAST(S, D, 'sky.light.0.matter.sun');
  CAST(S, D, 'terrain.mineral.0.matter.water');
  for (let i = 0; i < 5; i++) S.tick();
  const before = S.codexCount();
  S.startWorld({ name: 'T2', form: '单岛界', climate: '温带海洋',
    seed: ['SOL', 'ARC', 'Z9Y8', 'X7W6'], traits: [] });
  ok('换界不丢已点亮的页', S.codexCount() === before, before + ' 条');
  ok('换界把连续月计数归零', (S.codex.st['aqua.hold'] || 0) === 0);
  const again = S.checkCodex();
  ok('新界不再算三阳', !again || !again.some(a => a.id === 'sky.three'));
}

/* ── 8 卡包：四档环区间、图鉴挡最高档 ── */
{
  const { S, D } = boot();
  const base = S.drawPool().length;
  const hiOpen0 = S.packOpen(S.PACKS.find(p => p.id === 'high'));
  ok('开局高阶包未解锁', hiOpen0 === false);
  ok('开局池不含高阶环', S.drawPool().every(c => S.tierOpen(c.tier)), base + ' 张');
  ok('高阶卡确实被挡在池外', base < S.poolTotal(), base + ' < ' + S.poolTotal());
  ok('基础 / 进阶包一直买得到',
    S.packOpen(S.packOf('basic')) && S.packOpen(S.packOf('mid')));
  CAST(S, D, 'terrain.earth.0.matter.land');
  CAST(S, D, 'sky.light.0.matter.sun');
  CAST(S, D, 'sky.light.0.matter.sun');
  CAST(S, D, 'sky.light.0.matter.sun');
  S.checkCodex();
  ok('点亮一条后高阶包解锁', S.packOpen(S.packOf('high')), '图鉴 ' + S.codexLit());
  ok('点亮后卡池变大', S.drawPool().length > base, base + ' → ' + S.drawPool().length);
  ok('包里有 6 张', (() => {
    S.W.essence = 99;
    const r = S.drawPack('mid');
    return r.ok && r.got.length === S.PACK_SIZE;
  })(), 'PACK_SIZE = ' + S.PACK_SIZE);
}

/* ── 汇总 ── */
console.log('图鉴回归 · ' + boot().S.CODEX.length + ' 条判定，' + FILES.length + ' 个 sim/data 文件');
rows.forEach(r => console.log(r));
console.log(bad.length ? '\nFAIL · ' + bad.length + ' 项：' + bad.join('、') : '\nPASS · ' + rows.length + ' 项');
process.exit(bad.length ? 1 : 0);
