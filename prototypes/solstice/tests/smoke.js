/* Solstice · 合并原型自检
   跑法（Node 18+）：node tests/smoke.js
   用本机 Edge（playwright-core 不下载浏览器）：file:// 直接开，检查控制台报错、施卡链路、
   生态 tick、面板开关与卡表校验。 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', 'index.html')).href;

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1680, height: 1000 } });
  const errors = [];
  page.on('console', m => { if (m.type() === 'error') errors.push('[console] ' + m.text()); });
  page.on('pageerror', e => errors.push('[PAGEERROR] ' + e.message));

  await page.goto(PAGE);
  await page.waitForTimeout(1800);

  /* 1. 卡表装配与校验 */
  const table = await page.evaluate(() => {
    const D = window.SOL.data;
    const cls = { matter: 0, rite: 0, law: 0 };
    D.cards.forEach(c => cls[c.class]++);
    return {
      total: D.total, cls,
      warn: D.warn,
      ids: new Set(D.cards.map(c => c.id)).size,
      numbered: D.cards.every(c => /^№ \d{4} \/ \d{4}$/.test(c.number)),
      reclaimOk: D.cards.every(c => c.tier === 0 ? c.reclaim === 0 : c.reclaim === D.RECLAIM[Math.min(10, c.tier)])
    };
  });

  /* 2. 进界 + 造世界 */
  const world = await page.evaluate(() => {
    const { UI, S, W, D } = window.__dbg;
    UI.enterWorld();
    S.setGear(3);                     // 2×
    const land = D.cards.find(c => c.sim.op === 'land' && c.tier === 0);
    const sun = D.byId.get('sky.light.0.matter.sun');
    const water = D.byId.get('terrain.mineral.0.matter.water');
    const grass = D.byId.get('flora.wood.1.matter.grass');
    const rabbit = D.byId.get('fauna.earth.1.matter.rabbit');
    const wolf = D.byId.get('fauna.dark.2.matter.wolf');
    const tree = D.byId.get('flora.wood.2.matter.tree');

    const r1 = S.cast(land, 0, 0);
    const r2 = S.cast(sun, 0, 0);
    const r3 = S.cast(water, 0, -1);
    const r4 = S.cast(grass, 1, 0);
    const r5 = S.cast(tree, -1, 0);
    const gate = S.cast(rabbit, 1, 1);            // 有草 → 应成功
    const gated = S.cast(wolf, 2, 2);              // 无猎物也能落（食肉只判生命层）
    const rain = S.cast(D.byId.get('rite.water.2.rite.rain'), 0, 0);
    const beast = S.cast(D.byId.get('rite.light.8.rite.awaken'), 1, 1);
    return {
      cells: W.cells.size, sun: W.sun,
      results: [r1.ok, r2.ok, r3.ok, r4.ok, r5.ok, gate.ok, gated.ok, rain.ok, beast.ok],
      beastMsg: beast.msg,
      W00: +W.cells.get('0,0').W.toFixed(2),
      ents: W.cells.get('1,0').ents.map(e => e.name),
      traces: W.traces.length
    };
  });

  await page.waitForTimeout(400);
  await page.screenshot({ path: path.join(__dirname, 'shot-world.png') });

  /* 3. 跑 24 个月 + 确定性摘要 */
  const sim = await page.evaluate(() => {
    const { S, W } = window.__dbg;
    for (let i = 0; i < 24; i++) S.tick();
    const d1 = S.digest();
    for (let i = 0; i < 6; i++) S.tick();
    return { tick: d1, month: W.month, ess: W.essence, life: S.lifeStats() };
  });

  /* 4. 面板全开一遍 */
  const panels = await page.evaluate(() => {
    const { UI } = window.__dbg;
    const out = {};
    for (const p of ['pack', 'codex', 'chron', 'settings', 'keys', 'help']) {
      UI.openPanel(p);
      out[p] = document.querySelector('#panel #pbody').children.length > 0;
    }
    UI.closePanel();
    UI.openCard('fauna.water.1.matter.nightmarsh_bird');
    out.codexCard = !!document.querySelector('#panel .card');
    UI.closePanel();
    /* 卡库第二面：图鉴墙（6 条判定） */
    UI.openPanel('codex');
    UI.state.codexTab = 'ach';
    UI.renderOverlay();
    out.dexCells = document.querySelectorAll('#panel .axc').length;
    out.dexHead = document.getElementById('phead').textContent.indexOf('图鉴') >= 0;
    UI.state.codexTab = 'cards';
    UI.closePanel();
    UI.fireEvent('灾变');
    out.event = document.getElementById('eventwrap').classList.contains('on');
    /* 事件弹窗必须整块落在视口内。#eventwrap 是 inset:0 的整屏遮罩，一旦它自己被
       某个带 translate 的 keyframes 推走（panelIn 里就有 translate(-50%,-50%)），
       弹窗只剩一角露在屏幕左上——评审时真的发生过，而且三份取证坐标一模一样。
       两条判据分别挡两种成因：遮罩宽高 > 0 挡「容器塌成 0×0」，四条边在视口内挡「被 transform 推走」。 */
    const eb = document.getElementById('eventcard').getBoundingClientRect();
    const wb = document.getElementById('eventwrap').getBoundingClientRect();
    out.eventBox = {
      card: [Math.round(eb.x), Math.round(eb.y), Math.round(eb.width), Math.round(eb.height)],
      wrap: [Math.round(wb.width), Math.round(wb.height)],
      inside: eb.left >= 0 && eb.top >= 0 && eb.right <= innerWidth && eb.bottom <= innerHeight &&
        wb.width > 0 && wb.height > 0
    };
    UI.closeEvent();
    return out;
  });

  await page.waitForTimeout(300);
  await page.screenshot({ path: path.join(__dirname, 'shot-panel.png'), fullPage: false });

  /* 5. 手牌版式：正文字号必须是设计字号（曾经是 302 卡 × .5 = 6.5px），且 8 张不溢出容器 */
  const cards = await page.evaluate(() => {
    const { W, UI } = window.__dbg;
    const ids = ['terrain.earth.0.matter.land', 'flora.wood.1.matter.grass', 'rite.water.2.rite.rain'];
    W.hand = ids.map(id => window.SOL.data.byId.get(id));
    W.sel = 0;
    UI.renderHand();
    const el = document.querySelector('.handslot .card');
    const r = el.getBoundingClientRect();
    /* 字号要在重排前取：下面的 renderHand 会重建 innerHTML，旧节点变游离态（computed = ""） */
    const sumFont = parseFloat(getComputedStyle(el.querySelector('.hsum span')).fontSize);
    const sumLen = el.querySelector('.hsum span').textContent.length;
    const ringOn = el.querySelectorAll('.hringbar i.on').length;
    const cant = document.querySelectorAll('.card.h.cant').length;

    /* 塞满 8 张，量左右边界是否越过 #handwrap */
    while (W.hand.length < window.SOL.sim.HAND_MAX) {
      const c = window.SOL.sim.drawOne(new Set(W.hand.map(x => x.id)));
      if (!c) break;
      W.hand.push(c);
    }
    UI.renderHand();
    const wrap = document.getElementById('handwrap').getBoundingClientRect();
    const rects = [...document.querySelectorAll('.handslot')].map(s => s.getBoundingClientRect());
    const outL = rects.length ? Math.max(0, Math.round(wrap.left - Math.min(...rects.map(r => r.left)))) : 0;
    const outR = rects.length ? Math.max(0, Math.round(Math.max(...rects.map(r => r.right)) - wrap.right)) : 0;

    /* 55 张卡的手牌正文都装得下：3 行 clamp 时「树」「鹰」被切掉 18px，且没有省略号 */
    const probe = document.createElement('div');
    probe.style.cssText = 'position:fixed;left:-9999px;top:0;width:136px';
    document.body.appendChild(probe);
    const clipped = [];
    for (const c of window.SOL.data.cards) {
      probe.innerHTML = UI.cardFace(c, 'probe', 'h');
      const el = probe.querySelector('.hsum span');
      if (el && el.scrollHeight > el.clientHeight + 1) clipped.push(c.name.zh);
    }
    probe.remove();

    return {
      slots: rects.length,
      cardBox: [Math.round(r.width), Math.round(r.height)],
      ringOn: ringOn,
      sumFont: sumFont,
      sumLen: sumLen,
      cant: cant,
      overflow: outL + outR,
      clipped: clipped
    };
  });
  await page.evaluate(() => document.getElementById('handwrap').scrollIntoView());
  const hand = await page.$('#handwrap');
  if (hand) await hand.screenshot({ path: path.join(__dirname, 'shot-hand.png') });

  /* 6. 键盘全路径：WASD 走游标，Enter 在游标处落卡。全程不碰鼠标（REVIEW-DEMO F-3）。 */
  await page.evaluate(() => {
    const { W, UI } = window.__dbg;
    W.hand = [window.SOL.data.byId.get('terrain.earth.0.matter.land')];
    W.sel = 0; UI.renderHand(); UI.updateHud();
  });
  const before = await page.evaluate(() => window.__dbg.S.W.cells.size);
  await page.keyboard.press('d');
  await page.keyboard.press('d');
  await page.waitForTimeout(150);
  const cursor = await page.evaluate(() => {
    const { R } = window.__dbg;
    return {
      box: R.cam.hoverBox.visible,
      keep: !!R.cam.keepHover,
      at: R.cam.hover ? [R.cam.hover.x, R.cam.hover.y] : null
    };
  });
  await page.keyboard.press('Enter');
  await page.waitForTimeout(300);
  const after = await page.evaluate(() => window.__dbg.S.W.cells.size);
  const keyboard = { before, after, landed: after - before, ...cursor };

  /* 7. 时素只按真实时间涨：暂停也涨、4× 不会多涨（SETTING §3.1「不随加速变化」） */
  const ess = await page.evaluate(() => {
    const { S, W } = window.__dbg;
    const out = {};
    S.cmd.gear(0);                       // 暂停
    const e0 = W.essence;
    S.cmd.essenceTick(10000);            // 正好 10 秒
    out.paused = W.essence - e0;
    S.cmd.gear(4);                       // 4× 世界
    const e1 = W.essence;
    S.cmd.essenceTick(10000);
    out.fast = W.essence - e1;
    /* 世界档位只影响月数，不影响时素。一个月是 S.MONTH_MS 毫秒，4× 走同样长的一段真实时间应该推进 4 个月 */
    const m0 = W.month;
    S.cmd.advance(S.MONTH_MS);
    out.monthsAt4x = W.month - m0;
    S.cmd.gear(1);
    return out;
  });

  /* 8. 回收可撤销：点错一张不该不可挽回 */
  const undo = await page.evaluate(() => {
    const { S, W, UI } = window.__dbg;
    const D = window.SOL.data;
    W.hand = [D.byId.get('flora.wood.2.matter.tree'), D.byId.get('terrain.earth.0.matter.land')];
    W.sel = 0; W.essence = 0;
    UI.renderHand();
    const n0 = W.hand.length;
    UI.recycle();
    const mid = { n: W.hand.length, ess: W.essence };
    const btn = document.querySelector('#toasts .toast .toact');
    if (btn) btn.click();
    return { before: n0, mid: mid, after: W.hand.length, essBack: W.essence, hadButton: !!btn };
  });

  /* 9. 撤销只认刚回收的那一张、只一次；不能再凭空插卡（回收价改表后这条更值钱） */
  const undoGuard = await page.evaluate(() => {
    const { S, W } = window.__dbg;
    const D = window.SOL.data;
    const tree = D.byId.get('flora.wood.2.matter.tree');
    W.hand = [tree, D.byId.get('terrain.earth.0.matter.land')];
    W.sel = 0; W.essence = 100; W.recycleLog = null;
    const n0 = W.hand.length;
    const r1 = S.cmd.unrecycle(0, tree.id);          // 没有回执 → 必须拒绝
    const guard = !r1.ok && W.hand.length === n0;
    S.cmd.recycle(0);
    const r2 = S.cmd.unrecycle(0, tree.id);          // 刚回收 → 允许
    const r3 = S.cmd.unrecycle(0, tree.id);          // 第二次 → 拒绝
    return { guard, ok2: r2.ok, ok3: r3.ok, n: W.hand.length, cap: S.HAND_MAX };
  });

  /* 10. 抽卡：第二包不重发第一包发过的（重复保护）+ 硬保底一定给高环 */
  const draw = await page.evaluate(() => {
    const { S, W } = window.__dbg;
    W.seen = new Set(); W.pity = 0; W.essence = 9999; W.hand = [];
    const a = S.drawPack('basic').got.map(c => c.id);
    const b = S.drawPack('basic').got.map(c => c.id);
    const overlap = b.filter(id => a.indexOf(id) >= 0).length;
    const freshMin = S.PACK_SIZE - S.FRESH_PER_PACK;
    W.pity = S.PITY_HARD;
    const forced = S.drawOne(new Set(), S.packOf('high'));
    return { overlap, freshMin, forcedTier: forced && forced.tier, pityTier: S.PITY_TIER };
  });

  /* 10.5 重抽梯价：连抽翻倍、施卡归零（定额 5 时素时重抽比抽卡还划算） */
  const rr = await page.evaluate(() => {
    const { S, W } = window.__dbg;
    S.cmd.gear(0);
    W.essence = 300; W.rerollN = 0;
    const seq = [];
    for (let i = 0; i < 5; i++) { const r = S.cmd.reroll(); seq.push(r.cost); }
    const afterFish = W.rerollN;
    /* 施一张**真能生效**的卡。原来这里往 (0,0) 打手牌第一张，而 (0,0) 早就有地——
       纯铺地卡现在会被拒绝（F14 的修复），这一步就根本没施成、梯价自然没归零。
       改到空旷处，并用卡表里的地卡，不依赖手牌抽到什么。 */
    const land = window.SOL.data.byId.get('terrain.earth.0.matter.land');
    const castRes = S.cmd.cast(land, 60, 60);
    const reset = S.cmd.rerollCost();
    return { seq, afterFish, reset, castOk: !!castRes.ok };
  });

  /* 10.4 拖拽主路径：从手牌拖进世界必须真的施卡。
     这条是 H01 / U25 的守卫——手牌贴着屏幕底边，抬手去世界必然向上，
     旧逻辑里「向上 = 随机落点」会抢在「指定落点」前面，主路径等于不存在。 */
  const dragCast = await (async () => {
    try {
      await page.evaluate(() => {
        const { S, UI } = window.__dbg;
        S.W.hand[0] = window.SOL.data.byId.get('terrain.earth.0.matter.land');
        UI.renderHand(); UI.updateHud();
      });
      const pt = await page.evaluate(() => {
        const { R } = window.__dbg;
        const rc = document.getElementById('cv').getBoundingClientRect();
        const hand = document.getElementById('handwrap').getBoundingClientRect();
        for (let gy = 0.2; gy < 0.7; gy += 0.05) {
          for (let gx = 0.25; gx < 0.8; gx += 0.05) {
            const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy;
            if (y > hand.top - 20) continue;
            if (!R.cam.pickAt(x, y)) return { x, y };
          }
        }
        return null;
      });
      if (!pt) return { skip: true };
      const slot = await page.evaluate(() => {
        const b = document.querySelector('.handslot').getBoundingClientRect();
        return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
      });
      const before = await page.evaluate(() => window.__dbg.S.W.cells.size);
      await page.mouse.move(slot.x, slot.y);
      await page.mouse.down();
      for (let i = 1; i <= 8; i++) {
        await page.mouse.move(slot.x + (pt.x - slot.x) * i / 8, slot.y + (pt.y - slot.y) * i / 8);
        await page.waitForTimeout(20);
      }
      const tip = await page.evaluate(() => {
        const g = document.getElementById('ghost');
        const t = g && g.querySelector('.tip');
        return t ? t.textContent : null;
      });
      await page.mouse.up();
      await page.waitForTimeout(220);
      const after = await page.evaluate(() => window.__dbg.S.W.cells.size);
      const ghosts = await page.evaluate(() => document.querySelectorAll('#ghost').length);

      /* 反例：拖到右栏上松手，不许施卡。画布是整屏铺的，按矩形判「在世界里」
         会把面板背后的格子也算进来（实测 81→94）。 */
      const before2 = await page.evaluate(() => window.__dbg.S.W.cells.size);
      const pn = await page.evaluate(() => {
        const r = document.getElementById('rightpanel').getBoundingClientRect();
        return { x: Math.round(r.x + r.width / 2), y: Math.round(r.y + r.height * 0.4) };
      });
      const s2 = await page.evaluate(() => {
        const b = document.querySelector('.handslot').getBoundingClientRect();
        return { x: Math.round(b.x + b.width / 2), y: Math.round(b.y + b.height / 2) };
      });
      await page.mouse.move(s2.x, s2.y);
      await page.mouse.down();
      for (let i = 1; i <= 8; i++) {
        await page.mouse.move(s2.x + (pn.x - s2.x) * i / 8, s2.y + (pn.y - s2.y) * i / 8);
        await page.waitForTimeout(18);
      }
      await page.mouse.up();
      await page.waitForTimeout(180);
      const after2 = await page.evaluate(() => window.__dbg.S.W.cells.size);

      /* 一次拖放只能施一张（第二轮 S03：画布 pointerup 与 endDrag 各施一次）。
         不能用痕迹数当计数——土地打在虚空上不产生痕迹，得直接数 UI.cast 调用。 */
      await page.evaluate(() => {
        const { UI } = window.__dbg;
        window.__casts = 0;
        if (!UI.__castWrapped) {
          const orig = UI.cast;
          UI.cast = function () { window.__casts++; return orig.apply(this, arguments); };
          UI.__castWrapped = true;
        }
      });
      const st = await page.evaluate(() => {
        const b = document.querySelector('.handslot').getBoundingClientRect();
        return { x: Math.round(b.x + b.width / 2), y: Math.round(b.y + b.height / 2) };
      });
      await page.mouse.move(st.x, st.y);
      await page.mouse.down();
      for (let i = 1; i <= 8; i++) {
        await page.mouse.move(st.x + (pt.x - st.x) * i / 8, st.y + (pt.y - st.y) * i / 8);
        await page.waitForTimeout(16);
      }
      await page.mouse.up();
      await page.waitForTimeout(200);
      const t1 = await page.evaluate(() => { const n = window.__casts; window.__casts = 0; return n; });

      /* 拖拽中按 Esc 必须是取消，不能施卡（第二轮 S02） */
      const e0 = await page.evaluate(() => window.__dbg.S.W.cells.size);
      await page.mouse.move(st.x, st.y);
      await page.mouse.down();
      for (let i = 1; i <= 8; i++) {
        await page.mouse.move(st.x + (pt.x - st.x) * i / 8, st.y + (pt.y - st.y) * i / 8);
        await page.waitForTimeout(16);
      }
      await page.keyboard.press('Escape');
      await page.mouse.up();
      await page.waitForTimeout(200);
      const e1 = await page.evaluate(() => window.__dbg.S.W.cells.size);

      return { before, after, tip, ghosts, grew: after > before,
        panelBefore: before2, panelAfter: after2, tracesPerDrag: t1,
        escBefore: e0, escAfter: e1 };
    } catch (e) { return { err: String(e && e.message) }; }
  })();

  /* 10.5 本批修复的回归：价格来源 / 换包保护 / 空施不吃牌 / 跳过按钮 / 抽包文案 */
  const fixes = await page.evaluate(() => {
    const { S, W, D, UI } = window.__dbg;
    const out = {};
    const land = D.byId.get('terrain.earth.0.matter.land');

    /* F14：在一整片已有地块上再打一张纯铺地卡，必须被拒绝且不扣牌 */
    W.hand = [land, land];
    S.cmd.cast(land, 0, 0);
    const before = W.hand.length;
    const r = S.cmd.playSlot(0, 0, 0);
    out.landRefused = !r.ok;
    out.landMsg = r.msg || '';
    out.landNoConsume = W.hand.length === before;

    /* P01：面板标题与底部按钮的价格都要跟随选中的包 */
    UI.packSel = 'high';
    UI.packStage = 0; UI.packCards = [];
    UI.openPanel('pack'); UI.renderOverlay();
    const cost = String(S.packOf('high').cost);
    out.priceInTitle = document.getElementById('phead').textContent.indexOf(cost) >= 0;
    out.priceInButton = document.querySelector('[data-act="packNext"]').textContent.indexOf(cost) >= 0;
    out.packBtnAt = document.querySelector('[data-act="packNext"]').textContent.trim();

    /* P02：有未取走的牌时，点别的包种不能把牌吞掉。
       注意阶段号：撕开现在直接进「挑选」（stage 2），中间那站空白翻牌已删除。 */
    UI.packStage = 2; UI.packCards = [land];
    const selBefore = UI.packSel;
    const row = document.querySelector('[data-pack="basic"]');
    if (row) row.click();
    out.switchBlocked = UI.packSel === selBefore && (UI.packCards || []).length === 1;

    /* P03：第三阶段的主按钮不能是「不必挑」 */
    UI.packStage = 2; UI.packCards = [land]; UI.renderOverlay();
    out.skipLabel = (document.querySelector('[data-act="packNext"]') || {}).textContent || '';
    UI.packStage = 0; UI.packCards = []; UI.closePanel();

    /* U01：顶栏抽包按钮的文案必须由最便宜的可开包算出，不能是写死的 */
    UI.updateHud();
    out.packBtn = document.getElementById('bPack').textContent.trim();
    const cheap = S.packList().filter(p => S.packOpen(p))
      .reduce((a, p) => (!a || p.cost < a.cost ? p : a), null);
    out.cheapCost = cheap ? cheap.cost : null;

    /* U19：世界已经有格时，痕迹空态不能再写「世界还没被碰过」 */
    W.traces = [];
    UI.renderRight();
    out.traceEmpty = (document.querySelector('.rp.grow') || {}).textContent || '';
    return out;
  });

  /* 10.6 第三批：面板内的提示不能压面板；log 要有底板 */
  const ux3 = await page.evaluate(() => {
    const { UI } = window.__dbg;
    const out = {};
    UI.openPanel('pack'); UI.renderOverlay();
    const stack = document.getElementById('toasts');
    stack.innerHTML = '';
    UI.toast('测试提示', '副行');
    out.inHead = document.getElementById('phead').querySelectorAll('.toast.ih').length;
    out.inStack = stack.children.length;
    UI.closePanel();
    stack.innerHTML = '';
    UI.toast('世界里的提示', '');
    out.outside = stack.children.length;
    const log = getComputedStyle(document.getElementById('log'));
    out.logBg = log.backgroundImage.slice(0, 24);
    out.logColor = log.color;
    return out;
  });

  /* 10.7 图鉴详情：关系链要有中文标签、不许漏内部键名；没有值的指标不再占位 */
  const ux4 = await page.evaluate(() => {
    const { UI, D } = window.__dbg;
    const out = {};
    /* 挑一张带 rel 的卡：水族里第一个有 rel 的 */
    const withRel = Array.from(D.byId.values()).find(c => (c.rel || []).length);
    UI.openCard(withRel.id);
    const body = document.getElementById('pbody').textContent;
    out.card = withRel.name.zh;
    out.relRaw = /(^|[^a-z])(sym|foe|req|where|food)([^a-z]|$)/.test(body);
    out.relLabel = /共生|相冲|食物|需要|落点/.test(body);
    out.noteShown = withRel.rel.some(r => r.note && body.indexOf(r.note) >= 0);
    out.placeholder = body.indexOf('未定') >= 0;
    return out;
  });

  /* 10.8 月份写法：全站一个来源，且不能出现小数年 */
  const mon = await page.evaluate(() => {
    const { UI, S, W } = window.__dbg;
    const out = {};
    out.fmt = [0, 5, 11, 12, 23, 24, 30].map(m => UI.mon(m));
    /* 推到下一年，逼出一条「纪年」，再看它有没有印小数年 / 有没有重复年份 */
    W.month = 47; W.cells.size = 25;
    for (let i = 0; i < 3; i++) S.tick();
    const years = W.chron.filter(c => c.kind === '纪年');
    out.yearCount = years.length;
    out.yearText = years.length ? years[years.length - 1].text : '';
    out.decimalAnywhere = W.chron.some(c => /\d+\.\d+\s*年/.test(c.text));
    return out;
  });

  /* 10.9 窄屏两处硬伤：末卡压右栏、动作栏被卡盖（第二轮 N02/N07） */
  const narrowFit = await (async () => {
    try {
      /* 前面的用例把面板打开着，先关掉，否则遮罩会吃掉 hit-test */
      await page.evaluate(() => window.__dbg.UI.closePanel());
      await page.waitForTimeout(80);
      /* 先放大再缩回：手牌位置是烘死的像素，不重排会两边同时溢出（第二轮 X13） */
      await page.setViewportSize({ width: 1440, height: 900 });
      await page.waitForTimeout(200);
      await page.evaluate(() => { window.__dbg.UI.renderHand(); });
      await page.setViewportSize({ width: 1080, height: 700 });
      await page.waitForTimeout(620);
      await page.evaluate(() => { const { UI } = window.__dbg; UI.renderHand(); UI.updateHud(); });
      await page.waitForTimeout(120);
      const r = await page.evaluate(() => {
        const rp = document.getElementById('rightpanel').getBoundingClientRect();
        const slots = [...document.querySelectorAll('.handslot')];
        const last = slots[slots.length - 1].getBoundingClientRect();
        const ab = document.querySelectorAll('#actbar .ab')[3];
        const abr = ab.getBoundingClientRect();
        const hit = document.elementFromPoint(abr.x + abr.width / 2, abr.y + abr.height / 2);
        return {
          firstLeft: Math.round(last.left - last.left + slots[0].getBoundingClientRect().x),
          lastOverlap: Math.round(last.right - rp.x),
          actbarOk: !!(hit && hit.closest('#actbar'))
        };
      });
      await page.setViewportSize({ width: 1680, height: 1000 });
      await page.waitForTimeout(220);
      return r;
    } catch (e) { return { err: String(e && e.message) }; }
  })();

  /* 10.10 面板里的空格：应激活按钮，不许穿透到世界（第二轮 S25 补） */
  const spaceInPanel = await (async () => {
    const setup = await page.evaluate(() => {
      const { UI, S } = window.__dbg;
      UI.openPanel('codex'); UI.renderOverlay();
      const t = document.querySelector('#panel [data-ctab="ach"]');
      if (t) t.focus();
      return { tab: UI.state.codexTab, sun: S.W.sun };
    });
    await page.keyboard.press(' '); await page.waitForTimeout(220);
    const after = await page.evaluate(() => ({ tab: window.__dbg.UI.state.codexTab, sun: window.__dbg.S.W.sun }));
    await page.evaluate(() => { document.activeElement.blur(); });
    const sunBefore = await page.evaluate(() => window.__dbg.S.W.sun);
    await page.keyboard.press(' '); await page.waitForTimeout(220);
    const sunAfter = await page.evaluate(() => window.__dbg.S.W.sun);
    await page.evaluate(() => window.__dbg.UI.closePanel());
    return { setup, after, blurred: { sunBefore, sunAfter } };
  })();

  /* 11. 断言：上面几段是读数，这里决定退不退 0 */
  if (!table.reclaimOk) errors.push('[assert] reclaim 与环级表不一致');
  if (!(undoGuard.guard && undoGuard.ok2 && !undoGuard.ok3)) errors.push('[assert] 撤销守卫失效（应拒绝无回执/重复撤销）');
  if (undoGuard.n !== 2) errors.push('[assert] 撤销后手牌数 ' + undoGuard.n + '（应为 2）');
  if (draw.overlap > draw.freshMin) errors.push('[assert] 重复保护配额超限：' + draw.overlap + ' > ' + draw.freshMin);
  if (!(draw.forcedTier >= draw.pityTier)) errors.push('[assert] 硬保底没生效');
  if (rr.seq.join(',') !== '5,10,20,40,40') errors.push('[assert] 重抽梯价不对：' + rr.seq.join(','));
  if (rr.reset !== 5) errors.push('[assert] 施卡后重抽价没有归零：' + rr.reset);
  if (!rr.castOk) errors.push('[assert] 归零测试里那张卡根本没施成');
  if (dragCast.skip) errors.push('[assert] 找不到虚空点，拖拽用例没跑');
  if (dragCast.err) errors.push('[assert] 拖拽用例抛错：' + dragCast.err);
  if (!dragCast.grew) errors.push('[assert] 拖到世界没有施卡（cells ' + dragCast.before + '→' + dragCast.after + '）：' + dragCast.tip);
  if (dragCast.tip && /随机/.test(dragCast.tip)) errors.push('[assert] 拖到世界仍被判成随机落点：' + dragCast.tip);
  if (dragCast.ghosts) errors.push('[assert] 松手后还留着 ' + dragCast.ghosts + ' 张拖影');
  if (spaceInPanel.after.tab === spaceInPanel.setup.tab) {
    errors.push('[assert] 面板里按空格没有激活按钮（页签没切）');
  }
  if (spaceInPanel.after.sun !== spaceInPanel.setup.sun) {
    errors.push('[assert] 面板里按空格穿透到世界施了卡：sun ' + spaceInPanel.setup.sun + '→' + spaceInPanel.after.sun);
  }
  if (spaceInPanel.blurred.sunAfter !== spaceInPanel.blurred.sunBefore) {
    errors.push('[assert] 面板开着、焦点在背景时按空格仍然施卡');
  }
  if (narrowFit.err) errors.push('[assert] 窄屏用例抛错：' + narrowFit.err);
  if (narrowFit.firstLeft < 0) errors.push('[assert] 缩窗后首张手牌跑出左边界：x=' + narrowFit.firstLeft);
  if (narrowFit.lastOverlap > 0) errors.push('[assert] 1080 下末张手牌压进右栏 ' + narrowFit.lastOverlap + 'px');
  if (narrowFit.actbarOk === false) errors.push('[assert] 1080 下动作栏被手牌盖住');
  if (dragCast.tracesPerDrag !== 1) errors.push('[assert] 一次拖放调用了 ' + dragCast.tracesPerDrag + ' 次 UI.cast（应为 1）');
  if (dragCast.escAfter !== dragCast.escBefore) {
    errors.push('[assert] 拖拽中按 Esc 仍然施了卡：cells ' + dragCast.escBefore + '→' + dragCast.escAfter);
  }
  if (dragCast.panelAfter !== dragCast.panelBefore) {
    errors.push('[assert] 拖到右栏上竟然施了卡：cells ' + dragCast.panelBefore + '→' + dragCast.panelAfter);
  }
  if (mon.fmt.join('|') !== '1 月|6 月|12 月|1 年 1 月|1 年 12 月|2 年 1 月|2 年 7 月') {
    errors.push('[assert] 月份格式化不对：' + mon.fmt.join('|'));
  }
  if (mon.decimalAnywhere) errors.push('[assert] 年表出现小数年');
  if (mon.yearCount && mon.yearText.indexOf('年') >= 0) {
    errors.push('[assert] 纪年正文还在重复年份：' + mon.yearText);
  }
  if (ux4.relRaw) errors.push('[assert] 图鉴详情把内部键名印出来了（' + ux4.card + '）');
  if (!ux4.relLabel) errors.push('[assert] 图鉴详情的关系链没有中文标签（' + ux4.card + '）');
  if (!ux4.noteShown) errors.push('[assert] 关系链的 note 没有渲染（' + ux4.card + '）');
  if (ux4.placeholder) errors.push('[assert] 图鉴详情仍有「未定」占位：' + ux4.card);
  if (ux3.inHead !== 1) errors.push('[assert] 面板打开时提示没有进 #phead：' + ux3.inHead);
  if (ux3.inStack !== 0) errors.push('[assert] 面板打开时提示仍发到全屏顶部（会压面板）');
  if (ux3.outside !== 1) errors.push('[assert] 没有面板时提示没回到顶部栈');
  if (!/gradient/.test(ux3.logBg)) errors.push('[assert] log 没有底板，压在浅色地面上会读不出：' + ux3.logBg);
  if (!fixes.landRefused) errors.push('[assert] 空施没有被拒绝：' + fixes.landMsg);
  if (!fixes.landNoConsume) errors.push('[assert] 空施把牌吃掉了');
  if (!fixes.priceInTitle) errors.push('[assert] 抽卡标题价格没跟随选中包：' + fixes.priceInTitle);
  if (!fixes.priceInButton) errors.push('[assert] 抽卡按钮价格没跟随选中包：' + fixes.packBtnAt);
  if (!fixes.switchBlocked) errors.push('[assert] 有未取走的牌时仍能切包');
  if (!/跳过/.test(fixes.skipLabel)) errors.push('[assert] 第三阶段主按钮文案没改：' + fixes.skipLabel);
  if (fixes.cheapCost && fixes.packBtn.indexOf(String(fixes.cheapCost)) < 0) {
    errors.push('[assert] 抽包按钮文案与最便宜包价不符：' + fixes.packBtn + ' vs ' + fixes.cheapCost);
  }
  if (/世界还没被碰过/.test(fixes.traceEmpty)) errors.push('[assert] 痕迹空态仍在说「世界还没被碰过」');
  if (!panels.eventBox || !panels.eventBox.inside) {
    errors.push('[assert] 事件弹窗不在视口内：' + JSON.stringify(panels.eventBox));
  }

  const report = { table, world, sim, panels, cards, keyboard, ess, undo, undoGuard, draw, rr, fixes, dragCast, ux3, ux4, mon, narrowFit, spaceInPanel, errors };
  fs.writeFileSync(path.join(__dirname, 'report.json'), JSON.stringify(report, null, 2));
  console.log(JSON.stringify(report, null, 1));

  await browser.close();
  if (errors.length) process.exitCode = 1;
})();
