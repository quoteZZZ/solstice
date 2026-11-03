/* 至日 · 施卡、手牌、经济、痕迹
   一张卡在世界里做什么，只由 card.sim.op 决定；没有按卡名 if/else。 */
(function (S, D) {
  const W = S.W;

  S.PACK_COST = 30;
  S.PACK_SIZE = 5;
  S.HAND_MAX = 8;

  const isGround = e => e.ground;

  /* ───────── 抽卡 ───────── */
  S.drawPool = () => D.cards.filter(c => !(c.evolve && c.evolve.from));

  S.drawOne = function (exclude) {
    const pool = S.drawPool().filter(c => !exclude.has(c.id));
    if (!pool.length) return null;
    const w = pool.map(c => D.weight(c.tier));
    let r = S.rand() * w.reduce((a, b) => a + b, 0);
    for (let i = 0; i < pool.length; i++) { r -= w[i]; if (r <= 0) { W.seen.add(pool[i].id); return pool[i]; } }
    const last = pool[pool.length - 1];
    W.seen.add(last.id);
    return last;
  };

  S.dealHand = function () {
    const ex = new Set();
    W.hand = [];
    for (let i = 0; i < S.HAND_MAX; i++) {
      const c = S.drawOne(ex);
      if (!c) break;
      ex.add(c.id); W.hand.push(c);
    }
    /* 开局保底：三张地形基础，否则第一回合无从下手 */
    if (!W.hand.some(c => c.sim.op === 'land')) W.hand[0] = D.byId.get('terrain.earth.0.matter.land');
    if (!W.hand.some(c => c.sim.op === 'sun')) W.hand[1] = D.byId.get('sky.light.0.matter.sun');
    if (!W.hand.some(c => c.sim.op === 'spring')) W.hand[2] = D.byId.get('terrain.mineral.0.matter.water');
    W.sel = 0;
    return W.hand;
  };

  S.drawPack = function () {
    if (W.essence < S.PACK_COST) return { ok: false, msg: '时素不够（要 ' + S.PACK_COST + '）' };
    W.essence -= S.PACK_COST;
    const ex = new Set(W.hand.map(c => c.id)), got = [];
    for (let i = 0; i < S.PACK_SIZE; i++) {
      const c = S.drawOne(ex);
      if (!c) break;
      ex.add(c.id); got.push(c);
    }
    for (const c of got) if (W.hand.length < S.HAND_MAX) W.hand.push(c);
    got.sort((a, b) => a.tier - b.tier);
    S.log('撕开一包 · −' + S.PACK_COST + ' 时素', true);
    return { ok: true, got: got };
  };

  S.recycle = function (i) {
    const c = W.hand[i];
    if (!c) return { ok: false, msg: '没选卡' };
    if (c.tier === 0) return { ok: false, msg: '0 环卡不返还时素' };
    W.hand.splice(i, 1);
    W.essence += c.reclaim;
    S.log('回收「' + c.name.zh + '」+' + c.reclaim);
    W.sel = Math.min(W.sel, Math.max(0, W.hand.length - 1));
    return { ok: true, gain: c.reclaim };
  };

  /* ───────── 门控 ───────── */
  S.gateCheck = function (c, cell) {
    const gate = D.gateOf(c);
    const life = S.lifeStats();
    if (gate === 'fauna' && !life.flora) return '这里还没有植物——先种下去';
    if (gate === 'sapient' && !life.fauna) return '这里还没有兽——启智无从谈起';
    if (c.sim.op === 'human') {
      let near = false;
      if (cell) {
        for (const cc of [cell].concat(S.nb(cell)))
          if (cc.ents.some(e => e.tags.includes('mobile'))) near = true;
      }
      if (!near) return '附近没有兽——智慧不会凭空出现';
    }
    return null;
  };

  /* ───────── 痕迹 ───────── */
  const TRACE_LIFE = [1, 2, 3, 6, 12, 18, 24, 36, 48, 999, 999];
  S.traceLife = c => TRACE_LIFE[Math.min(10, c.tier)];

  S.addTrace = function (card, cell) {
    if (!cell || cell.flags.hidden) return null;
    const el = card.primary;
    const near = W.traces.find(t => t.x === cell.x && t.y === cell.y && t.el === el);
    if (near) {
      near.count++; near.born = W.month;
      near.life = Math.max(near.life, S.traceLife(card));
      if (card.tier >= near.tier) { near.tier = card.tier; near.name = card.name.zh; near.id = card.id; }
      return near;
    }
    const t = {
      id: ++W.traceSeq, x: cell.x, y: cell.y, el: el, tier: card.tier,
      idCard: card.id, name: card.name.zh,
      born: W.month, life: S.traceLife(card), count: 1
    };
    W.traces.push(t);
    if (W.traces.length > 160) W.traces.shift();
    return t;
  };

  S.expireTraces = function () {
    const before = W.traces.length;
    W.traces = W.traces.filter(t => t.life >= 999 || W.month - t.born <= t.life);
    return before !== W.traces.length;
  };

  /* ───────── 施卡 ───────── */
  S.cast = function (card, x, y) {
    if (!card) return { ok: false, msg: '没选卡' };
    const op = card.sim && card.sim.op;
    if (!op) return { ok: false, msg: '这张卡没有实装' };

    let cell = null;
    if (op !== 'land' && op !== 'sun' && op !== 'law') {
      cell = S.at(x, y);
      if (!cell) return { ok: false, msg: '要放在已有地块上' };
    } else if (op === 'land' || op === 'sun') {
      cell = S.at(x, y);
    } else {
      cell = S.at(x, y);   // 律：可以指一格，也可以不指
    }

    const gate = S.gateCheck(card, cell);
    if (gate) return { ok: false, msg: gate };

    const before = W.cells.size;
    let res;
    switch (op) {
      case 'land': res = opLand(card, x, y); break;
      case 'sun': res = opSun(card); break;
      case 'spring': res = opSpring(card, cell); break;
      case 'barrier': res = opBarrier(card, cell); break;
      case 'spawn': res = opSpawn(card, cell); break;
      case 'rain': res = opRain(card, cell); break;
      case 'burn': res = opBurn(card, cell); break;
      case 'ignite': res = opIgnite(card, cell); break;
      case 'fertile': res = opFertile(card, cell); break;
      case 'grow': res = opGrow(card, cell); break;
      case 'wound': res = opWound(card, cell); break;
      case 'field': res = opField(card, cell); break;
      case 'law': res = opLaw(card, cell); break;
      case 'human': res = opHuman(card, cell); break;
      case 'bell': res = opBell(card, cell); break;
      default: res = { ok: false, msg: '未实装：' + op };
    }
    if (!res.ok) return res;

    if (cell) S.addTrace(card, cell);
    if (cell && !cell.flags.hidden && (W.cells.size !== before || cell))
      S.addChron('干涉', '「' + card.name.zh + '」落在 (' + x + ', ' + y + ')');
    W.cellsChanged = true;
    S.checkCodex();      // 「三阳」这类当月成立的当刻就亮，不必等下一月
    return res;
  };

  /* ── 各 op 的实现 ── */
  function opLand(card, x, y) {
    const size = card.sim.size == null ? 2 : card.sim.size;
    const rise = card.sim.rise || 0;
    const cooling = card.sim.cooling || 0;
    const dry = card.sim.dry || 0;
    let made = 0;
    for (let dx = -size; dx <= size; dx++) for (let dy = -size; dy <= size; dy++) {
      const c = S.addCell(x + dx, y + dy, { h: 0.30 });
      /* 金属富集：新地里有矿（含矿格敲钟 +2，见 opBell）——特征真的能读到 */
      if (W.effects && W.effects.metal > 0 && S.rand() < 0.35) c.flags.metal = true;
      if (rise) { c.h = Math.min(0.9, c.h + rise); c.mods.t -= cooling; if (dry) c.W = Math.max(0, c.W - dry); }
      if (card.sim.edgeFog && (Math.abs(dx) === size || Math.abs(dy) === size)) {
        c.mods.l -= 0.2; c.W = Math.min(3, c.W + 0.2);
      }
      made++;
    }
    S.log('土地铺开 ' + made + ' 格');
    return { ok: true, made: made };
  }

  function opSun(card) {
    W.sun = Math.min(3, W.sun + 1);
    S.log('太阳升起 · 光照 ' + W.sun);
    return { ok: true };
  }

  function opSpring(card, cell) {
    if (cell.ents.some(e => e.tags.includes('watersource'))) return { ok: false, msg: '这一格已经有水源' };
    const e = S.spawn(card, cell, { ground: true });
    e.tags = e.tags.slice();
    cell.W = Math.min(3, cell.W + (card.sim.strong || 1));
    return { ok: true };
  }

  function opBarrier(card, cell) {
    if (cell.ents.some(e => e.tags.includes('barrier'))) return { ok: false, msg: '这一格已经是岩体' };
    S.spawn(card, cell, { ground: true });
    return { ok: true };
  }

  function opSpawn(card, cell) {
    const n = card.sim.count || 1;
    const limit = 3;
    const targets = [cell].concat(card.sim.spread ? S.nb(cell) : []);
    let placed = 0;
    for (let i = 0; i < n; i++) {
      const t = targets[i % targets.length];
      if (t.ents.filter(e => !isGround(e)).length >= limit) continue;
      S.spawn(card, t, { age: 0 });
      placed++;
    }
    if (!placed) return { ok: false, msg: '这一格满了（上限 ' + limit + ' 个活物）' };
    return { ok: true, placed: placed };
  }

  function opRain(card, cell) {
    for (const c of [cell].concat(S.nb(cell))) {
      c.W = Math.min(3, c.W + card.sim.w);
      c.mods.l += card.sim.l || 0;
    }
    return { ok: true };
  }

  function opBurn(card, cell) {
    const n = cell.ents.filter(e => !isGround(e)).length;
    cell.ents = cell.ents.filter(isGround);
    cell.W = Math.max(0, cell.W + (card.sim.w || -0.6));
    if (card.sim.fertile) cell.fertile = Math.min(0.6, (cell.fertile || 0) + card.sim.fertile);
    S.log('烧掉 ' + n + ' 个活物');
    return { ok: true, killed: n };
  }

  function opIgnite(card, cell) {
    const i = cell.ents.findIndex(e => !isGround(e) && e.tags.includes('edible'));
    if (i < 0) return { ok: false, msg: '这一格没有可燃的植物' };
    cell.ents.splice(i, 1);
    cell.W = Math.max(0, cell.W - 0.2);
    cell.fertile = Math.min(0.6, (cell.fertile || 0) + 0.1);
    return { ok: true };
  }

  function opFertile(card, cell) {
    const sp = D.byId.get(card.sim.species);
    const n = card.sim.n || 2;
    let placed = 0;
    for (let i = 0; i < n; i++) {
      if (cell.ents.filter(e => !isGround(e)).length >= 3) break;
      S.spawn(sp, cell, { age: 0 }); placed++;
    }
    cell.fertile = Math.min(0.6, (cell.fertile || 0) + 0.3);
    if (!placed) return { ok: false, msg: '这一格满了' };
    return { ok: true, placed: placed };
  }

  function opGrow(card, cell) {
    const grow = cell.ents.filter(e => !isGround(e) && e.grow);
    if (!grow.length) return { ok: false, msg: '这一格没有植物' };
    const bonus = cell.W >= 1 ? 1 : 0.5;
    for (const e of grow) e.age += (card.sim.n || 6) * bonus;
    if (card.sim.w) cell.W = Math.max(0, cell.W + card.sim.w);
    if (card.sim.propagate) {
      const t = S.nb(cell).find(n => n.ents.filter(e => !isGround(e)).length < 2);
      if (t) S.spawn(grow[0].card, t, { age: 0 });
    }
    return { ok: true };
  }

  function opWound(card, cell) {
    const list = cell.ents.filter(e => !isGround(e));
    if (!list.length) return { ok: false, msg: '这一格没有活物' };
    for (const e of list) { e.hp += card.sim.hp || -1; e.stun = 1; }
    cell.ents = cell.ents.filter(e => isGround(e) || e.hp > 0);
    return { ok: true };
  }

  function opField(card, cell) {
    const m = card.sim;
    if (m.w) cell.W = Math.min(3, Math.max(0, cell.W + m.w));
    if (m.t) cell.mods.t += m.t;
    if (m.l) cell.mods.l += m.l;
    if (m.evap) cell.mods.evap *= m.evap;
    if (m.flag) cell.flags[m.flag] = true;
    if (m.neighbors) for (const n of S.nb(cell)) n.mods.l += m.neighbors;
    if (m.flag === 'wetland') for (const n of S.nb(cell)) n.W = Math.min(3, n.W + 0.3);
    return { ok: true };
  }

  function opLaw(card, cell) {
    const m = card.sim;
    if (m.light) W.law.lightOff = Math.min(2, W.law.lightOff - m.light);
    if (m.evap) W.law.evapX *= m.evap;
    if (m.set) for (const k in m.set) W.law[k] = Math.max(W.law[k], m.set[k]);
    S.addChron('气候', '世界律改变：' + card.name.zh);
    S.log('律 · ' + card.name.zh, true);
    return { ok: true };
  }

  function opHuman(card, cell) {
    if (W.sapient) return { ok: false, msg: '这一界已经有过智慧' };
    W.sapient = 1;
    S.addChron('文明', '有人在石头上刻了第一道');
    S.log('智慧层点亮', true);
    return { ok: true };
  }

  function opBell(card, cell) {
    const gain = cell.flags.metal ? 4 : 2;
    W.essence += gain;
    S.log('钟声 · +' + gain + ' 时素');
    if (W.sapient) S.addChron('文明', '钟声穿过聚落');
    return { ok: true, gain: gain };
  }

  /* ───────── 用掉一张手牌并补一张 ───────── */
  S.consume = function (i) {
    W.hand.splice(i, 1);
    const ex = new Set(W.hand.map(c => c.id));
    const c = S.drawOne(ex);
    if (c) W.hand.splice(Math.min(i, W.hand.length), 0, c);
    W.sel = Math.min(W.sel, Math.max(0, W.hand.length - 1));
  };

})(SOL.sim, SOL.data);
