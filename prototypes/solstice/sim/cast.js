/* Solstice · 施卡、手牌、经济、痕迹
   一张卡在世界里做什么，只由 card.sim.op 决定；没有按卡名 if/else。 */
(function (S, D) {
  const W = S.W;

  S.PACK_COST = 30;
  S.PACK_SIZE = 5;
  S.HAND_MAX = 8;

  const isGround = e => e.ground;

  /* ───────── 抽卡 ───────── */
  /* 卡池分两段（SETTING §5.1.3「成就解锁卡池」）：低的恒开，高的等图鉴。
     现在图鉴只有 6 条，按主题给每个组配一个扩展包等于凭空编一张 55 张卡的主题表，
     所以用一条不编内容的规则顶上：**每点亮一条图鉴，可抽的最高环 +1**。
     0 条时只能抽 ≤3 环（44 张），6 条全亮才放 tier 9 的大陆出来。 */
  S.POOL_BASE_TIER = 3;
  S.poolCap = () => Math.min(9, S.POOL_BASE_TIER + S.codexLit());
  S.poolTotal = () => D.cards.filter(c => !(c.evolve && c.evolve.from)).length;
  S.drawPool = () => D.cards.filter(c => !(c.evolve && c.evolve.from) && c.tier <= S.poolCap());

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
    /* 开局保底：三张地形基础，否则第一回合无从下手。
       原来三条 if 顺序往 0/1/2 槽里硬写，后一条会把前一条刚写进去的卡、
       连同原本就在那个槽里的同类卡一起盖掉：手牌里唯一的土地正好坐在 1 号槽时，
       「太阳」保底就把它吃了 —— 开局拿到一手没有土地、世界里一格也没有的牌，
       土地又是唯一能造格的卡，直接软锁。现在只替换「不承担任何保底职责」的槽。 */
    const NEED = [
      ['land', 'terrain.earth.0.matter.land'],
      ['sun', 'sky.light.0.matter.sun'],
      ['spring', 'terrain.mineral.0.matter.water']
    ];
    const keep = new Set();
    for (const [op] of NEED) {
      W.hand.forEach((c, i) => { if (c.sim.op === op) keep.add(i); });
    }
    for (const [op, id] of NEED) {
      if (W.hand.some(c => c.sim.op === op)) continue;
      const c = D.byId.get(id);
      if (!c) continue;
      let slot = -1;
      for (let i = 0; i < W.hand.length; i++) if (!keep.has(i)) { slot = i; break; }
      if (slot < 0) { W.hand.push(c); keep.add(W.hand.length - 1); }
      else { W.hand[slot] = c; keep.add(slot); }
    }
    W.sel = 0;
    return W.hand;
  };

  /* 撕包只负责把 5 张摊在桌上，不往手牌里塞。
     原来是一包 5 张全收：手牌上限 8，一包就把手牌灌满，剩下的只能硬着头皮打或者回收，
     「抽到新卡」从此不是一次决策而是一次倾倒（REVIEW-DEMO F-2）。
     Balatro 的 booster pack 与 StS 的三选一都是让玩家选，这里照这个来。 */
  S.drawPack = function () {
    if (W.essence < S.PACK_COST) return { ok: false, msg: '时素不够（要 ' + S.PACK_COST + '）' };
    W.essence -= S.PACK_COST;
    const ex = new Set(W.hand.map(c => c.id)), got = [];
    for (let i = 0; i < S.PACK_SIZE; i++) {
      const c = S.drawOne(ex);
      if (!c) break;
      ex.add(c.id); got.push(c);
    }
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

  /* 一张卡「此刻能不能打」——只判世界的最低条件，不判落点（落点由拖影上的提示说）。
     与 S.gateCheck 同源，只是把「先放哪一格」这层去掉，好让手牌一次问八张。
     开局世界是空的时候，除土地 / 太阳 / 律之外的卡全都是 false —— 手牌降饱和就是引导。 */
  S.playable = function (card, life) {
    const op = card && card.sim && card.sim.op;
    if (!op) return false;
    if (op === 'land' || op === 'sun' || op === 'law') return true;
    if (!W.cells.size) return false;
    life = life || S.lifeStats();
    if (op === 'human' && (W.sapient || !life.fauna)) return false;
    const gate = D.gateOf(card);
    if (gate === 'fauna' && !life.flora) return false;
    if (gate === 'sapient' && !life.fauna) return false;
    return true;
  };

  /* 施一张卡会动到哪些格：落点预览的第二层描边要用它（REVIEW-DEMO R-1 / #11）。
     判据与各 op 的实现同源：土地是 size×size 的方块（落在虚空时那些格还不存在，
     所以这里给坐标而不是格对象），field 的邻居光照与湿地标记会带到邻格，
     其余 op 只动落点那一格；太阳与律是全局的，不画范围。 */
  S.castRange = function (card, cell) {
    if (!card || !cell) return [];
    const op = card.sim && card.sim.op;
    if (!op || op === 'sun' || op === 'law') return [];
    if (op === 'land') {
      const size = card.sim.size == null ? 2 : card.sim.size;
      const out = [];
      for (let dx = -size; dx <= size; dx++) {
        for (let dy = -size; dy <= size; dy++) {
          const c = S.at(cell.x + dx, cell.y + dy);
          out.push({ x: cell.x + dx, y: cell.y + dy, h: c ? c.h : 0.30, fresh: !c });
        }
      }
      return out;
    }
    const out = [{ x: cell.x, y: cell.y, h: cell.h, fresh: false }];
    if (op === 'field') {
      const m = card.sim;
      if (m.neighbors || m.flag === 'wetland') {
        for (const n of S.nb(cell)) out.push({ x: n.x, y: n.y, h: n.h, fresh: false });
      }
    }
    return out;
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
      if (card.tier >= near.tier) { near.tier = card.tier; near.name = card.name.zh; near.idCard = card.id; }
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
    if (cell) S.classifyAround(cell);      // 地形由 sim 判，render 只读（REVIEW-DEMO X-6）
    /* 年表只记"世界变了形"的施卡：长出地块、改了全局律。
       原条件是 (size!==before || cell)，后半恒真 → 每张卡都进年表，一晚 71 条（REVIEW-DEMO C 段）。 */
    const shaped = W.cells.size !== before || op === 'law' || op === 'field' || op === 'sun';
    if (cell && !cell.flags.hidden && shaped)
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
