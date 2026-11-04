/* Solstice · 施卡、手牌、经济、痕迹
   一张卡在世界里做什么，只由 card.sim.op 决定；没有按卡名 if/else。 */
(function (S, D) {
  const W = S.W;

  S.PACK_SIZE = 6;
  S.FRESH_PER_PACK = 2;      // 一包里至少两张是没见过的（重复保护的配额，见 drawPack）
  S.HAND_MAX = 8;

  const isGround = e => e.ground;

  /* ───────── 抽卡 ───────── */
  /* 卡包按环级分档（SETTING §3.2.1）。价格只跟环区间走，不看世界发展到哪一步——
     「没有纪元、没有世界规模，高环卡能不能拿到，只看你愿不愿意花时素」。
     唯一的例外是 §5.1.3 的「成就是解锁卡池的途径」：高阶包要 1 条图鉴、超环包要 3 条。
     两节文档在这里本来是打架的（§3.2.1 说全靠时素，§5.1.3 说要成就），
     折中办法是让成就只挡最高的那一两档，基础 / 进阶永远买得到。 */
  S.PACKS = [
    { id: 'basic', name: '基础包', cost: 12, lo: 0, hi: 3, need: 0 },
    { id: 'mid', name: '进阶包', cost: 24, lo: 1, hi: 6, need: 0 },
    { id: 'high', name: '高阶包', cost: 48, lo: 3, hi: 9, need: 1 },
    { id: 'ultra', name: '超环包', cost: 96, lo: 10, hi: 10, need: 3 }
  ];

  S.poolTotal = () => D.cards.filter(c => !(c.evolve && c.evolve.from)).length;
  S.poolIn = p => D.cards.filter(c => !(c.evolve && c.evolve.from) && c.tier >= p.lo && c.tier <= p.hi);
  /* 池里一张卡都没有的档不出现在界面上：超环包现在就是空的，
     等 tier 10 的卡填进来它会自己出现，不用再改这里。 */
  S.packList = () => S.PACKS.filter(p => S.poolIn(p).length > 0);
  S.packOf = id => S.packList().find(p => p.id === id) || S.packList()[0];
  S.packOpen = p => S.codexLit() >= p.need;
  /* 某一环此刻能不能抽到：得有一档「买得到」的包覆盖它。图鉴进度就是这么反映到卡库上的。 */
  S.tierOpen = t => S.PACKS.some(p => S.packOpen(p) && t >= p.lo && t <= p.hi);
  S.drawPool = () => D.cards.filter(c => !(c.evolve && c.evolve.from) && S.tierOpen(c.tier));

  /* ── 旱情补偿（borrowed：Slay the Spire 的 card rarity offset）──
     静态权重表下，高阶包单抽出现 7 环以上的概率只有 4.6%（一包 6 张里至少一张 24.7%），
     平均要买四包、192 时素、32 分钟才见一张高环卡；而运气好时又可能连着来。
     StS 的做法是给稀有率一个偏移量：每出一张普通卡 +1%、出稀有归零。
     这里照同一个意思，但用乘法而不是百分点——因为我们的权重表是按环递减的，
     加法会把 tier 9（0.03）抬成和 tier 3（0.75）一个量级。 */
  S.PITY_TIER = 7;      // 从这一环起算「高环」
  S.PITY_STEP = 0.08;   // 每次没抽到，高环权重 +8%
  S.PITY_MAX = 3.2;     // 软上限：最多放大到 3.2 倍
  S.PITY_HARD = 30;     // 硬保底：连续 30 次没出，下一包必然给一张

  S.drawOne = function (exclude, pack, fresh) {
    const src = pack ? S.poolIn(pack) : S.drawPool();
    let pool = src.filter(c => !exclude.has(c.id));
    if (!pool.length) return null;
    /* 重复保护（borrowed：炉石 2020 的 duplicate protection）：池里还有没抽到过的，就只从里面抽。
       我们池只有 53 张、一包摊 6 张，靠运气去撞，最后几张会拖很久；
       而「卡库里暗着的格子」本来就已经在说「这些你还没见过」，抽包照着它发就是了。 */
    if (fresh) {
      const unseen = pool.filter(c => !W.seen.has(c.id));
      if (unseen.length) pool = unseen;
    }
    const hi = pool.filter(c => c.tier >= S.PITY_TIER);
    /* 硬保底只在高环池非空时生效：没抽到过高环又连着 30 次没出，下一张直接给 */
    if (hi.length && W.pity >= S.PITY_HARD) {
      const got = hi[S.randInt(hi.length)];
      W.seen.add(got.id); W.pity = 0;
      return got;
    }
    const k = Math.min(S.PITY_MAX, 1 + W.pity * S.PITY_STEP);
    const w = pool.map(c => D.weight(c.tier) * (c.tier >= S.PITY_TIER ? k : 1));
    let r = S.rand() * w.reduce((a, b) => a + b, 0);
    let got = pool[pool.length - 1];
    for (let i = 0; i < pool.length; i++) { r -= w[i]; if (r <= 0) { got = pool[i]; break; } }
    W.seen.add(got.id);
    W.pity = got.tier >= S.PITY_TIER ? 0 : W.pity + 1;
    return got;
  };

  S.dealHand = function () {
    const ex = new Set();
    W.hand = [];
    W.recycleLog = null;
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

  /* 撕一包只把 6 张摊在桌上，不往手牌里塞。
     原来是一包全收：手牌上限 8，一包就把手牌灌满，剩下的只能硬着头皮打或者回收，
     「抽到新卡」从此不是一次决策而是一次倾倒（REVIEW-DEMO F-2）。
     Balatro 的 booster pack 与 StS 的三选一都是让玩家选，这里照这个来。 */
  S.drawPack = function (packId) {
    const p = S.packOf(packId);
    if (!S.packOpen(p)) return { ok: false, msg: '「' + p.name + '」还没解锁（要 ' + p.need + ' 条图鉴）' };
    if (W.essence < p.cost) return { ok: false, msg: '时素不够（要 ' + p.cost + '）' };
    W.essence -= p.cost;
    const ex = new Set(W.hand.map(c => c.id)), got = [];
    for (let i = 0; i < S.PACK_SIZE; i++) {
      /* 前 FRESH_PER_PACK 张保证是没见过的，其余照旧从全池抽。
         6 张全保护实测过头了：高阶池只有 15 张，三包就见完，tier≥7 平均不到两包就出一次
         （原本四包），稀有度直接失效；抽穿基础池也从「约 18 包」缩到 7 包。
         配额制同时保住两件事：一包里总有新面孔，稀有卡也还是稀有卡。 */
      const c = S.drawOne(ex, p, i < S.FRESH_PER_PACK);
      if (!c) break;
      ex.add(c.id); got.push(c);
    }
    got.sort((a, b) => a.tier - b.tier);
    S.log('撕开「' + p.name + '」 · −' + p.cost + ' 时素', true);
    return { ok: true, pack: p, got: got };
  };

  S.recycle = function (i) {
    const c = W.hand[i];
    if (!c) return { ok: false, msg: '没选卡' };
    if (c.tier === 0) return { ok: false, msg: '0 环卡不返还时素' };
    W.hand.splice(i, 1);
    W.essence += c.reclaim;
    S.log('回收「' + c.name.zh + '」+' + c.reclaim);
    W.sel = Math.min(W.sel, Math.max(0, W.hand.length - 1));
    /* 撤销窗口只留最近这一次，并且只认这一张 id。
       原来 unrecycle 只校验时素，不校验「这张卡是不是刚回收的」也不校验手牌上限：
       连按四次就能把同一张卡插进手牌四份，而 0 环卡 reclaim = 0，等于零成本印卡（实测 8 → 12 张）。 */
    W.recycleLog = { id: c.id, at: i, gain: c.reclaim };
    return { ok: true, gain: c.reclaim, at: i, id: c.id };
  };

  /* 撤销回收：一张牌点错就没了，界面上没有任何确认步骤——
     点错一次的代价和主动回收一样大，不合理。撤销窗口只给几秒（toast 的生命），
     并且只对「刚回收的那一张、刚拿回的那一笔」有效；时素已经花掉就撤销不了。 */
  S.unrecycle = function (i, id) {
    const rc = W.recycleLog;
    if (!rc) return { ok: false, msg: '没有可撤销的回收' };
    if (id != null && rc.id !== id) return { ok: false, msg: '只能撤销刚回收的那一张' };
    if (W.hand.length >= S.HAND_MAX) return { ok: false, msg: '手牌满了' };
    const c = D.byId.get(rc.id);
    if (!c) return { ok: false, msg: '这张卡不在了' };
    if (W.essence < rc.gain) return { ok: false, msg: '时素已经花掉' };
    W.essence -= rc.gain;
    const at = Math.max(0, Math.min(W.hand.length, i == null ? rc.at : i));
    W.hand.splice(at, 0, c);
    W.sel = Math.max(0, Math.min(W.hand.length - 1, at));
    W.recycleLog = null;      // 撤销是一次性的，不能连按
    S.log('收回「' + c.name.zh + '」−' + rc.gain);
    return { ok: true };
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
    /* 施卡成功是「一次事件」，不只是「状态变了一下」：表现层要能对同一个瞬间做反馈
       （落点扩散环）。挂在注册表上，render 订阅，不必让 ui 记得去通知画面。 */
    S.emit('cast', { x: x, y: y, card: card, op: op, cell: cell });
    /* 年表只记"世界变了形"的施卡：长出地块、改了全局律。
       原条件是 (size!==before || cell)，后半恒真 → 每张卡都进年表，一晚 71 条（REVIEW-DEMO C 段）。 */
    const shaped = W.cells.size !== before || op === 'law' || op === 'field' || op === 'sun';
    if (cell && !cell.flags.hidden && shaped)
      S.addChron('干涉', '「' + card.name.zh + '」落在 (' + x + ', ' + y + ')');
    W.cellsChanged = true;
    W.rerollN = 0;       // 真的用掉一手牌了，重抽梯价归零
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
        /* 边缘要能看见：卡面写「边缘的雾不是装饰」，可原来这一圈除了数值之外
           和里面一模一样。给个持久标记，render 把它往背景色压，外圈自然收进雾里。 */
        c.flags.edge = true;
      }
      made++;
    }
    S.log('土地铺开 ' + made + ' 格');
    return { ok: true, made: made };
  }

  function opSun(card) {
    W.sun = Math.min(3, W.sun + 1);
    /* 卡面写「温度 +0.5（只加一次）」，而这里原来只加光照——于是「第二次升太阳只会让
       阴影更硬，温度不再涨」这句设定在代码里找不到任何依据。现在两次都真加。 */
    if (!W.sunTemp) W.sunTemp = 0.5;
    S.log('太阳升起 · 光照 ' + W.sun);
    return { ok: true };
  }

  function opSpring(card, cell) {
    if (cell.ents.some(e => e.tags.includes('watersource'))) return { ok: false, msg: '这一格已经有水源' };
    /* 「水为 0 的地块无法形成瀑布」——卡面写死的限制，之前没有任何地方判它 */
    if (card.sim.needW && cell.W <= 0) return { ok: false, msg: '干地成不了水——先给水或下一场雨' };
    const e = S.spawn(card, cell, { ground: true });
    e.tags = e.tags.slice();
    cell.W = Math.min(3, cell.W + (card.sim.strong || 1));
    /* 瀑布与溅水的四邻供水。原来只有本格那一下，卡面承诺的「四邻 +0.8」是空的。 */
    if (card.sim.spill) for (const n of S.nb(cell)) n.W = Math.min(3, n.W + card.sim.spill);
    /* 「本格抬高」：写 cell.h 而不是 mods —— 抬高是地形，会改变方块高度与界缘线 */
    if (card.sim.rise) cell.h = Math.min(0.9, cell.h + card.sim.rise);
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
    /* 邻格两个通道分开写。原来 neighbors 只有「邻格光照」一条路，而引流卡写的是
       `neighbors:0.4` 想加水量——于是它一直在改邻格的光照，水一格没多。 */
    if (m.neighbors) for (const n of S.nb(cell)) n.mods.l += m.neighbors;
    if (m.nbW) for (const n of S.nb(cell)) n.W = Math.min(3, n.W + m.nbW);
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
    W.recycleLog = null;       // 手牌一动，回执的位置就失效了
    const ex = new Set(W.hand.map(c => c.id));
    const c = S.drawOne(ex);
    if (c) W.hand.splice(Math.min(i, W.hand.length), 0, c);
    W.sel = Math.min(W.sel, Math.max(0, W.hand.length - 1));
  };

})(SOL.sim, SOL.data);
