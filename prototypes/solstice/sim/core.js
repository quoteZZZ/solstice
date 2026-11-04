/* Solstice · 仿真内核：状态、格网、实体
   TECH.md §3.1：sim 不碰 DOM、不读 window、不 import three。
   这里只有数字和对象；表现层自己来读。 */
window.SOL = window.SOL || {};
SOL.sim = SOL.sim || {};
(function (S, D) {

  /* ───────────────── 状态 ───────────────── */
  const W = S.W = {
    cells: new Map(),          // "x,y" → cell
    month: 0,                  // 世界月（1 tick = 1 月，TIME.md §1）
    hour: 6,                   // 表现钟：90 秒 = 1 天，只喂雾 / 昼夜表现，不参与结算
    essence: 0, essAcc: 0,     // 时素
    pity: 0,                   // 连续没抽到高环的次数（旱情补偿，见 cast.js）
    sun: 0,                    // 太阳卡累计（光照叠加，最多 3）
    sunTemp: 0,                // 太阳带来的基准温度（只加一次，见 cast.js opSun）
    law: { lifeX: 1, propX: 1, evapX: 1, lightOff: 0 },
    speed: 1, paused: true,
    hand: [], sel: -1,
    rerollN: 0,                // 连续重抽次数（梯价用，施卡后归零）
    recycleLog: null,          // 最近一次回收的回执（撤销只认它，见 cast.js）
    sapient: 0,                // 智慧层（启智后果，永久）
    traces: [], traceSeq: 0,
    chron: [],
    seen: new Set(),           // 已见卡（账号级：换界不清空，TIME.md §6）
    world: { name: '—', form: '单岛界', climate: '温带海洋', seed: ['SOL','ISL','----','----'], traits: [] },
    cellsChanged: true,
    castSeq: 0
  };

  S.logLines = [];

  /* ───────────────── 钩子：多播注册表 ─────────────────
     旧版是 S.onLog / S.onChron 直接赋值——第二个订阅者会静默顶掉第一个，
     而 S.onTick 干脆没声明过（在 time.js 里被调用、在 ui 里被赋值）。
     改成注册表后，加一层订阅（成就、教程、录制）不用去改已有订阅者。 */
  const hooks = { log: [], chron: [], tick: [], codex: [] };
  S.on = function (name, fn) {
    (hooks[name] || (hooks[name] = [])).push(fn);
    return fn;
  };
  S.emit = function (name, a, b) {
    const list = hooks[name];
    if (!list) return;
    for (const fn of list) fn(a, b);
  };

  /* ───────────────── 随机：sim 的唯一随机源 ─────────────────
     TECH.md §4 确定性：sim 里不许直接调 Math.random()，一律走 S.rand()。
     种子由世界坐标（world.seed）给，所以「同一坐标 = 同一个世界」在结算层成立。 */
  let rngState = 0x9e3779b9;

  S.setSeed = function (str) {
    const t = String(str == null ? 'SOL' : str);
    let h = 2166136261;
    for (let i = 0; i < t.length; i++) { h ^= t.charCodeAt(i); h = Math.imul(h, 16777619); }
    rngState = (h >>> 0) || 1;          // 0 会让 mulberry32 卡死，兜底为 1
    return rngState;
  };

  /* mulberry32：小、快、状态只有 32 位，能进 digest */
  S.rand = function () {
    rngState = (rngState + 0x6D2B79F5) >>> 0;
    let t = rngState;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };

  S.randInt = n => (S.rand() * n) | 0;
  S.pick = a => a[S.randInt(a.length)];
  S.rngState = () => rngState;

  S.log = function (msg, hot) {
    S.logLines.push({ msg: msg, hot: !!hot });
    if (S.logLines.length > 60) S.logLines.shift();
    S.emit('log', msg, !!hot);
  };

  /* 年表：类型 → 图标/颜色在 ui 层；这里只存 {month, kind, text} */
  S.addChron = function (kind, text) {
    W.chron.unshift({ month: W.month, kind: kind, text: text });
    if (W.chron.length > 300) W.chron.pop();
    S.emit('chron');
  };

  /* ───────────────── 格网 ───────────────── */
  const K = S.K = (x, y) => x + ',' + y;
  S.at = (x, y) => W.cells.get(K(x, y));
  S.nb = c => [[c.x + 1, c.y], [c.x - 1, c.y], [c.x, c.y + 1], [c.x, c.y - 1]]
    .map(([x, y]) => S.at(x, y)).filter(Boolean);
  S.ring = (c, r) => {
    const out = []; const seen = new Set([K(c.x, c.y)]);
    let cur = [c];
    for (let i = 0; i < r; i++) {
      const next = [];
      for (const n of cur) for (const t of S.nb(n)) {
        const k = K(t.x, t.y);
        if (seen.has(k)) continue;
        seen.add(k); next.push(t); out.push(t);
      }
      cur = next;
    }
    return out;
  };

  S.addCell = function (x, y, opts) {
    if (W.cells.has(K(x, y))) return W.cells.get(K(x, y));
    const o = opts || {};
    const c = {
      x: x, y: y,
      T: 20, W: o.w == null ? 0.5 : o.w, L: 0,
      kind: 'land', h: o.h == null ? 0.30 : o.h,
      ents: [], mesh: null,
      mods: { l: 0, t: 0, evap: 1 },
      flags: {},
      born: W.month
    };
    W.cells.set(K(x, y), c);
    W.cellsChanged = true;
    return c;
  };

  /* 地形由实体反推，不由卡名写死（world-3d 的分类逻辑，改成读 tags） */
  S.classify = function (cell) {
    let best = 'land';
    for (const e of cell.ents) {
      if (e.tags.includes('watersource')) return 'water';
    }
    if (cell.ents.some(e => e.tags.includes('barrier'))) return 'rock';
    if (cell.ents.some(e => e.shape === 'tree')) return 'forest';
    if (cell.ents.some(e => e.shape === 'bush' || e.shape === 'grass')) return 'grass';
    if (cell.flags.wetland) return 'water';
    return best;
  };

  /* 施卡后立刻重判这一格与邻格的地形。写回 sim 而不是等 render 顺手算，
     否则 digest 会依赖"有没有渲染过一帧"（REVIEW-DEMO X-6）。 */
  S.classifyAround = function (cell) {
    if (!cell) return;
    cell.kind = S.classify(cell);
    for (const n of S.nb(cell)) n.kind = S.classify(n);
  };

  /* 地形表：h 是**表现高度**（render 读它决定方块多高），col 是底色。
     两者都只有表现层在消费，但放在 sim 里是因为 kind 由 sim 判（X-6）——
     分开两处写会分叉：曾经 render 自己抄了一份高度表，改了一处忘另一处。
     h 的差值就是玩家能看见的高差：水面最低、岩最高，草地/林地比裸地高一点。 */
  S.TERRAIN = {
    land:  { h: 0.30, col: 0x5c5340 },
    water: { h: 0.08, col: 0x1b4a60 },
    rock:  { h: 0.62, col: 0x535d69 },
    grass: { h: 0.36, col: 0x25452c },
    forest:{ h: 0.44, col: 0x1c3520 }
  };

  /* ───────────────── 实体 ───────────────── */
  S.spawn = function (card, cell, extra) {
    const sp = card.species || {};
    const e = {
      uid: ++W.castSeq,
      id: card.id,
      name: card.name.zh,
      card: card,
      tags: card.runtime.tags || [],
      ground: !!(extra && extra.ground),
      age: (extra && extra.age) || 0,
      hp: sp.hp || 999,
      lifespan: sp.lifespan || 999,
      grow: sp.grow || 0,
      edible: sp.edible || 0,
      propag: sp.propag || 0,
      minW: sp.minW || 1,          // 繁殖需要的最低水量（卡面按物种写的门槛，见 eco.js）
      size: sp.size || 1,
      diet: sp.diet || null,
      speed: sp.speed || 1,
      shape: sp.shape || 'blob',
      fed: 0
    };
    cell.ents.push(e);
    W.cellsChanged = true;
    return e;
  };

  S.entsOf = (cell, tag) => cell.ents.filter(e => !tag || e.tags.includes(tag));

  /* 脏标记由 sim 自己清：render 读完之后通知 sim，而不是伸手写 W。
     （旧版 render/grid.js 里直接写 W.cellsChanged = false，属于读的一方改状态） */
  S.clearCellDirty = function () { W.cellsChanged = false; };

  /* 生命三层：从实体现场统计，不用手工计数器（ui-flow 里计数器曾经和实体分叉） */
  S.lifeStats = function () {
    const flora = new Set(), fauna = new Set();
    for (const c of W.cells.values()) {
      for (const e of c.ents) {
        if (e.tags.includes('grower')) flora.add(e.id);
        else if (e.tags.includes('mobile')) fauna.add(e.id);
      }
    }
    return { flora: flora.size, fauna: fauna.size, sapient: W.sapient, floraSet: flora, faunaSet: fauna };
  };

  /* 世界摘要：确定性回归的种子（TECH.md §4）。
     随机状态也混进 hash —— 随机流分叉就等于世界分叉，不该只在格子对上时才算一致。 */
  S.digest = function () {
    const cells = [...W.cells.values()].sort((a, b) => (a.y - b.y) || (a.x - b.x));
    const body = cells.map(c => [
      c.x, c.y, c.kind,
      c.W.toFixed(2), c.T.toFixed(1), c.L.toFixed(1),
      c.ents.map(e => e.id + '@' + Math.floor(e.age)).sort().join('|')
    ].join(':')).join('\n');
    let h = 2166136261;
    for (let i = 0; i < body.length; i++) { h ^= body.charCodeAt(i); h = Math.imul(h, 16777619); }
    h ^= rngState; h = Math.imul(h, 16777619);
    return {
      hash: (h >>> 0).toString(16),
      rng: rngState >>> 0,
      cells: W.cells.size,
      month: W.month,
      essence: W.essence,
      sun: W.sun,
      law: Object.assign({}, W.law),
      traces: W.traces.length,
      sapient: W.sapient
    };
  };

  /* 重置：创世重掷时清空世界，但保留卡池与手牌逻辑。
     重掷 = 重新播种，所以同一个 world.seed 必然重置出同一个世界。 */
  S.resetWorld = function (world) {
    W.cells.clear();
    W.month = 0; W.hour = 6; W.essence = 0; W.essAcc = 0; W.sun = 0; W.sunTemp = 0; W.pity = 0;
    W.recycleLog = null; W.rerollN = 0;
    W.law = { lifeX: 1, propX: 1, evapX: 1, lightOff: 0 };
    W.traces = []; W.traceSeq = 0; W.chron = []; W.sapient = 0;
    W.castSeq = 0;
    W.world = world;
    S.setSeed((world && world.seed && world.seed.join('-')) || 'SOL');
    if (S.codexReset) S.codexReset();   // 图鉴跨世界共通，只清连续月计数
    W.cellsChanged = true;
    S.logLines = [];
  };

})(SOL.sim, SOL.data);
