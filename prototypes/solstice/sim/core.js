/* 至日 · 仿真内核：状态、格网、实体
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
    sun: 0,                    // 太阳卡累计
    law: { lifeX: 1, propX: 1, evapX: 1, lightOff: 0 },
    speed: 1, paused: true,
    hand: [], sel: -1,
    sapient: 0,                // 智慧层（启智后果，永久）
    traces: [], traceSeq: 0,
    chron: [],
    seen: new Set(),           // 已见卡（账号级：换界不清空，TIME.md §6）
    world: { name: '—', form: '单岛界', climate: '温带海洋', seed: ['SOL','ISL','----','----'], traits: [] },
    cellsChanged: true,
    castSeq: 0
  };

  S.logLines = [];
  S.onLog = null;
  S.onChron = null;

  S.log = function (msg, hot) {
    S.logLines.push({ msg: msg, hot: !!hot });
    if (S.logLines.length > 60) S.logLines.shift();
    if (S.onLog) S.onLog(msg, !!hot);
  };

  /* 年表：类型 → 图标/颜色在 ui 层；这里只存 {month, kind, text} */
  S.addChron = function (kind, text) {
    W.chron.unshift({ month: W.month, kind: kind, text: text });
    if (W.chron.length > 300) W.chron.pop();
    if (S.onChron) S.onChron();
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

  S.TERRAIN = {
    land:  { h: 0.30, col: 0x4a4232 },
    water: { h: 0.11, col: 0x15394a },
    rock:  { h: 0.62, col: 0x434b55 },
    grass: { h: 0.34, col: 0x1b3120 },
    forest:{ h: 0.40, col: 0x142718 }
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

  /* 世界摘要：确定性回归的种子（TECH.md §4） */
  S.digest = function () {
    const cells = [...W.cells.values()].sort((a, b) => (a.y - b.y) || (a.x - b.x));
    const body = cells.map(c => [
      c.x, c.y, c.kind,
      c.W.toFixed(2), c.T.toFixed(1), c.L.toFixed(1),
      c.ents.map(e => e.id + '@' + Math.floor(e.age)).sort().join('|')
    ].join(':')).join('\n');
    let h = 2166136261;
    for (let i = 0; i < body.length; i++) { h ^= body.charCodeAt(i); h = Math.imul(h, 16777619); }
    return {
      hash: (h >>> 0).toString(16),
      cells: W.cells.size,
      month: W.month,
      essence: W.essence,
      sun: W.sun,
      law: Object.assign({}, W.law),
      traces: W.traces.length,
      sapient: W.sapient
    };
  };

  /* 重置：创世重掷时清空世界，但保留卡池与手牌逻辑 */
  S.resetWorld = function (world) {
    W.cells.clear();
    W.month = 0; W.hour = 6; W.essence = 0; W.essAcc = 0; W.sun = 0;
    W.law = { lifeX: 1, propX: 1, evapX: 1, lightOff: 0 };
    W.traces = []; W.traceSeq = 0; W.chron = []; W.sapient = 0;
    W.castSeq = 0;
    W.world = world;
    W.cellsChanged = true;
    S.logLines = [];
  };

})(SOL.sim, SOL.data);
