/* 至日 · 卡表装配
   三类卡合成一张表；编号、稀有度、攻防、回收价全部在这里派生——
   数据文件里不写这些（写了就会分叉）。
   校验（CARD-SCHEMA §8）分两级：能自动判定的**硬错误直接抛**，装配不下去；
   渐进填卡期的软问题（引用先指后填）只记进 SOL.data.warn，由 STATUS 记存量。 */
(function () {
  const D = SOL.data;
  const E = D.elements;
  const CLS_ORDER = { matter: 0, rite: 1, law: 2 };
  const CLS_BONUS = { matter: 5, rite: 10, law: 15 };
  const DUR_BONUS = { instant: 0, sustain: 10, permanent: 25 };
  const RARITY = [
    ['常见', 0], ['少见', 2], ['稀有', 4], ['罕见', 6], ['至日', 9]
  ];

  const all = [].concat(D.matter || [], D.rite || [], D.law || []);

  /* ── 默认值与派生 ── */
  for (const c of all) {
    c.primary = c.primary || c.elements[0];
    c.duration = c.duration || (c.class === 'law' ? 'permanent' : c.class === 'rite' ? 'instant' : 'sustain');
    c.rarity_word = RARITY.filter(([, t]) => c.tier >= t).pop()[0];
    c.value = c.value == null ? 4 : c.value;
    c.reclaim = c.tier === 0 ? 0 : Math.round(c.value * 0.15);
    const radius = c.radius_w || 0.006;
    const dur = DUR_BONUS[c.duration] || 0;
    const mobility = (c.runtime.tags || []).some(t => t === 'fly' || t === 'swim') ? 5 : 0;
    const size = radius <= 0.03 ? 5 : radius >= 0.20 ? 20 : 10;
    c.atk = Math.round((c.tier * 15 + CLS_BONUS[c.class] + radius * 100 + mobility) / 5) * 5;
    c.def = Math.round((c.tier * 10 + dur + size) / 5) * 5;
  }

  /* ── 图鉴编号：元素 → 环 → 类 → 同格顺序（schema §1） ── */
  const sorted = all.slice().sort((a, b) =>
    (E.order.indexOf(a.primary) - E.order.indexOf(b.primary)) ||
    (a.tier - b.tier) ||
    (CLS_ORDER[a.class] - CLS_ORDER[b.class]) ||
    (all.indexOf(a) - all.indexOf(b))
  );
  const pad = n => String(n).padStart(4, '0');
  sorted.forEach((c, i) => { c.number = '№ ' + pad(i + 1) + ' / ' + pad(sorted.length); c._n = i + 1; });

  /* ── 索引 ── */
  D.cards = sorted;
  D.total = sorted.length;
  D.byId = new Map(sorted.map(c => [c.id, c]));
  D.byName = new Map(sorted.map(c => [c.name.zh, c]));
  D.bySlug = new Map(sorted.map(c => [c.slug, c]));
  D.byElement = el => sorted.filter(c => c.elements.includes(el));
  D.byTier = t => sorted.filter(c => c.tier === t);

  /* ── 元素工具 ── */
  D.el = el => E.defs[el] || E.defs['土'];
  D.elColor = el => (E.defs[el] || E.defs['土']).c;
  D.elRgb = el => (E.defs[el] || E.defs['土']).rgb;
  D.elCls = el => E.cls[el] || 'el-earth';
  D.sigil = el => E.sigil[el] || E.sigil['土'];
  D.clsName = { matter: '物', rite: '术', law: '律' };

  /* ── 校验（specs/CARD-SCHEMA.md §8）──
     硬错误：坏数据进内存比当场报错贵得多，直接抛，装配完才继续。
     软警告：卡池是渐进填的，允许「先指后填」，只记录不拦。 */
  const BINDS = ['clock.hour', 'fog.opacity'];   // §8 规则 7：表现读数白名单，新增读数要同次登记
  const hard = [], soft = [];
  const bad = (c, msg) => hard.push(c.id + ' · ' + msg);
  const len = (c, field, text, max) => {
    if (text.length > max) bad(c, field + ' 超长 ' + text.length + '/' + max);
  };

  const seenId = new Set();
  for (const c of all) {
    /* 0 身份：id 唯一且非空（旧版这条写成了恒真的自反判断，等于没查） */
    if (!c.id) bad(c, '缺少 id');
    if (seenId.has(c.id)) bad(c, '重复 id');
    seenId.add(c.id);

    /* 1 文案长度：超一个字都不行，卡面会被裁 */
    len(c, 'line.fx', c.line.fx, 40);
    (c.line.cond || []).forEach(t => len(c, 'line.cond', t, 24));
    (c.lore || []).forEach(l => len(c, 'lore.t', l.t, 100));
    len(c, 'usage', c.usage || '', 40);
    if (c.flavor) len(c, 'flavor', c.flavor.zh || '', 30);

    /* 2 高亮指得到：hl 是 fx 的子串，指不到就渲染不出高亮（卡面会静默丢标记） */
    (c.line.hl || []).forEach(h => {
      if (c.line.fx.indexOf(h) < 0) bad(c, 'line.hl「' + h + '」不在 line.fx 里');
    });

    /* 3 卡面小数字：最多 4 条（版式只有 4 格），每条要有 k 与 v */
    const nums = c.line.num || [];
    if (nums.length > 4) bad(c, 'line.num ' + nums.length + ' 条（上限 4）');
    nums.forEach(n => { if (!n.k || !n.v) bad(c, 'line.num 缺 k 或 v'); });

    /* 7 bind 只能是已登记的表现读数，防止拼错后静默读到 undefined */
    (c.stats || []).forEach(s => {
      if (s.bind && BINDS.indexOf(s.bind) < 0) bad(c, 'stats.bind 未登记：' + s.bind);
    });

    /* 4 引用解析（软） */
    (c.rel || []).forEach(r => { if (!D.byId.has(r.to)) soft.push(c.name.zh + ' · 关系指向不存在的卡：' + r.to); });
    if (c.evolve && c.evolve.to && !D.byId.has(c.evolve.to)) soft.push(c.name.zh + ' · 进化目标不存在：' + c.evolve.to);
    if (c.evolve && c.evolve.from && !D.byId.has(c.evolve.from)) soft.push(c.name.zh + ' · 进化来源不存在：' + c.evolve.from);

    /* 5 复合卡该更稀有（软）——旧版这条带「含风元素」的特判，实际只对夜沼鸟生效，已去掉 */
    if (c.elements.length >= 3 && c.tier <= 1) soft.push(c.name.zh + ' · ' + c.elements.length + ' 元素却只有 ' + c.tier + ' 环');
  }

  D.errors = hard;
  D.warn = soft;
  if (soft.length) console.warn('[卡表校验] ' + soft.length + ' 条警告：\n' + soft.join('\n'));
  if (hard.length) {
    const msg = '[卡表校验] ' + hard.length + ' 条硬错误，拒绝装配：\n' + hard.join('\n');
    console.error(msg);
    throw new Error(msg);
  }

  /* ── 抽卡权重：只看环级（CARD-INDEX §9） ── */
  D.weight = tier => [1, 1, 0.9, 0.75, 0.62, 0.44, 0.31, 0.16, 0.11, 0.075, 0.03][Math.min(10, Math.max(0, tier))];

  /* ── 生命层门控 ── */
  D.gateOf = c => (c.runtime && c.runtime.gate) || null;
})();
