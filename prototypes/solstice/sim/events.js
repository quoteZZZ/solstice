/* Solstice · 世界事件：承受或改写
   事件不是失败判定，是「要不要接住它」：承受 = 让它发生；改写 = 花时素换一种后果。
   两种结果都真的改世界，所以效果逻辑属于 sim——放在 ui 里就是 UI 在改世界状态。
   取格子用 S.randInt（TECH.md §4：sim 里不许出现 Math.random()），
   否则「同一事件在不同帧点开」会得到不同后果。 */
(function (S, D) {
  const W = S.W;
  const cmd = S.cmd = S.cmd || {};   // 与 sim/cmd.js 同一个面：事件的「承受 / 改写」也是命令

  function anyCell() {
    const cells = [...W.cells.values()];
    return cells.length ? cells[S.randInt(cells.length)] : null;
  }
  function someCells(n) {
    const cells = [...W.cells.values()];
    const out = [];
    for (let i = 0; i < n && cells.length; i++) out.push(cells[S.randInt(cells.length)]);
    return out;
  }
  const gain = n => { W.essence += n; };

  /* 文案与图标（a）只给 ui 读；apply / rewrite 只在这里执行。 */
  const EVENTS = S.EVENTS = [
    { k: '禁物', t: '锈蚀之核', a: 'rust',
      b: '界缘裂开一线，一枚不属于这里的核落进来。它把周围的金属慢慢吃成锈，但锈土里会长出别处没有的赤色苔。',
      tags: ['只启不养', '不可回收', '同时只许一件'],
      apply: () => { const c = anyCell(); if (c) { c.flags.metal = true; c.W = Math.max(0, c.W - 0.6); } },
      rewrite: () => { gain(4); } },
    { k: '禁物', t: '无光之钉', a: 'nail',
      b: '一枚钉子插进夜里，钉子上方三尺没有星。它不是黑暗，是把光让开。',
      tags: ['只启不养', '不可回收', '夜间更暗'],
      apply: () => { const c = anyCell(); if (c) c.mods.l -= 1; },
      rewrite: () => { const c = anyCell(); if (c) c.mods.l -= 0.4; } },
    { k: '禁物', t: '逆流之钟', a: 'bell',
      b: '钟自己响了。此后这一带的水先往山上走，再落下来——落下来的水更清。',
      tags: ['只启不养', '不可回收', '水往高处'],
      apply: () => { for (const c of someCells(5)) c.W = Math.min(3, c.W + 0.6); },
      rewrite: () => { gain(6); } },
    { k: '灾变', t: '山火过境', a: 'fire',
      b: '火从西坡往上走，走到雪线自己停了。焦土是黑的，但三年后这里会比别处更肥。',
      tags: ['留下焦土', '灰是肥料', '兽群迁移'],
      apply: () => { for (const c of someCells(3)) { c.ents = c.ents.filter(e => e.ground); c.W = Math.max(0, c.W - 0.5); c.fertile = 0.3; } },
      rewrite: () => { const c = anyCell(); if (c) { c.ents = c.ents.filter(e => e.ground); c.fertile = 0.3; } } },
    { k: '灾变', t: '陨石', a: 'meteor',
      b: '一道白线，然后是一个坑。坑底的石头化了又凝，像一块没人磨过的玻璃。',
      tags: ['留下陨坑', '玻璃质', '扬尘 10 日'],
      apply: () => { const c = anyCell(); if (c) { c.ents = c.ents.filter(e => e.ground); c.W = 0; c.mods.l -= 0.3; } },
      rewrite: () => { gain(5); } },
    { k: '灾变', t: '冰川期', a: 'ice',
      b: '雪线压到海面，整界安静下来。冰退的时候，会把山磨成峡湾。',
      tags: ['雪线下降', '数十日', '留下冰碛'],
      apply: () => { for (const c of W.cells.values()) c.mods.t -= 1; },
      rewrite: () => { for (const c of W.cells.values()) c.mods.t -= 0.3; } },
    { k: '文明', t: '第一座聚落', a: 'hut',
      b: '三间屋子围着一口井。它们不声不响地出现了，像本来就该在那里。',
      tags: ['需已有植物', 'Z3 可见', '可被卡改写'],
      apply: () => { W.sapient = Math.max(1, W.sapient); },
      rewrite: () => { W.sapient = 0; } },
    { k: '文明', t: '第一道城墙', a: 'wall',
      b: '有人把石头垒成一条线。线内是「我们」，线外还是世界。',
      tags: ['疆域', '不可越过', '可被攻破'],
      apply: () => { const c = anyCell(); if (c) S.spawn(D.byId.get('terrain.mineral.0.matter.rock'), c, { ground: true }); },
      rewrite: () => { gain(3); } },
    { k: '文明', t: '第一部文字', a: 'script',
      b: '有人在石头上刻了一道。从这一刻起，年表开始自己说话。',
      tags: ['年表自动记录', '记忆可继承'],
      apply: () => { W.sapient = Math.max(1, W.sapient); S.addChron('文明', '第一道文字被刻下'); },
      rewrite: () => { S.addChron('文明', '有人把第一道文字磨掉了'); } }
  ];

  /* 改写的价在 sim：UI 只负责显示与提示，不负责扣钱。
     原来 60 = 600 秒（1×）的纯等，单局根本攒不到 —— 于是事件永远只有一个按钮，
     「承受 / 改写」是假选择。压到一包的钱（12），它才真的成为一个取舍。 */
  S.REWRITE_COST = 12;

  cmd.pickEvent = function (kind) {
    const pool = kind ? EVENTS.filter(e => e.k === kind) : EVENTS;
    return pool.length ? pool[S.randInt(pool.length)] : null;
  };

  /* 承受：让它发生，并记进年表 */
  cmd.bearEvent = function (e) {
    if (!e) return { ok: false, msg: '没有事件' };
    e.apply();
    S.addChron(e.k === '文明' ? '文明' : '灾变', e.t + '：被承受');
    return { ok: true };
  };

  /* 改写：花钱换后果。钱不够就什么都不动。 */
  cmd.rewriteEvent = function (e) {
    if (!e) return { ok: false, msg: '没有事件' };
    if (W.essence < S.REWRITE_COST) return { ok: false, msg: '时素不够' };
    W.essence -= S.REWRITE_COST;
    e.rewrite();
    S.addChron('干涉', '改写了「' + e.t + '」');
    return { ok: true, cost: S.REWRITE_COST };
  };

})(SOL.sim, SOL.data);
