/* 至日 · 卡表装配
   三类卡合成一张表；编号、稀有度、攻防、回收价全部在这里派生——
   数据文件里不写这些（写了就会分叉）。校验失败只报警不拦，报警可在 SOL.data.warn 里看。 */
(function () {
  const D = SOL.data;
  const E = D.elements;
  const CLS_ORDER = { matter: 0, rite: 1, law: 2 };
  const CLS_BONUS = { matter: 5, rite: 10, law: 15 };
  const DUR_BONUS = { instant: 0, sustain: 10, permanent: 25 };
  const RARITY = [
    ['常见', 0], ['少见', 2], ['稀有', 4], ['罕见', 6], ['至日', 9]
  ];

  const warn = [];
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

  /* ── 校验（specs/CARD-SCHEMA.md §8） ── */
  const chk = (c, field, text, max) => {
    if (text.length > max) warn.push(c.name.zh + ' · ' + field + ' 超长 ' + text.length + '/' + max);
  };
  for (const c of all) {
    chk(c, 'line.fx', c.line.fx, 40);
    (c.line.cond || []).forEach(t => chk(c, 'line.cond', t, 24));
    (c.lore || []).forEach(l => chk(c, 'lore', l.t, 100));
    chk(c, 'usage', c.usage || '', 40);
    if (c.flavor) chk(c, 'flavor', c.flavor.zh || '', 30);
    if (!D.byId.has(c.id)) warn.push('重复 id: ' + c.id);
    if (c.elements.includes('风') && c.elements[0] !== '风' && c.tier <= 1 && c.elements.length === 3)
      warn.push(c.name.zh + ' · 三元素却只有 ' + c.tier + ' 环');
    (c.rel || []).forEach(r => { if (!D.byId.has(r.to)) warn.push(c.name.zh + ' · 关系指向不存在的卡：' + r.to); });
    if (c.evolve && c.evolve.to && !D.byId.has(c.evolve.to)) warn.push(c.name.zh + ' · 进化目标不存在：' + c.evolve.to);
    if (c.evolve && c.evolve.from && !D.byId.has(c.evolve.from)) warn.push(c.name.zh + ' · 进化来源不存在：' + c.evolve.from);
  }
  D.warn = warn;
  if (warn.length) console.warn('[卡表校验] ' + warn.length + ' 条：\n' + warn.join('\n'));

  /* ── 抽卡权重：只看环级（CARD-INDEX §9） ── */
  D.weight = tier => [1, 1, 0.9, 0.75, 0.62, 0.44, 0.31, 0.16, 0.11, 0.075, 0.03][Math.min(10, Math.max(0, tier))];

  /* ── 生命层门控 ── */
  D.gateOf = c => (c.runtime && c.runtime.gate) || null;
})();
