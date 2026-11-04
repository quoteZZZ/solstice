/* 回归验证探针 2（补充取证；只读源码）
   node tests/review-run/regress-probe2.js
   产出：tests/review-run/regress-probe2/*.png + probes.json */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const ROOT = path.join(__dirname, '..', '..');
const PAGE = pathToFileURL(path.join(ROOT, 'index.html')).href;
const OUT = path.join(__dirname, 'regress-probe2');
fs.mkdirSync(OUT, { recursive: true });
const out = { probes: {}, shots: [] };
let shotN = 0;

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1366, height: 800 }, deviceScaleFactor: 1 });
  const errs = [];
  page.on('pageerror', e => errs.push(String(e.message)));
  async function shot(name) {
    shotN++;
    const f = String(shotN).padStart(2, '0') + '-' + name + '.png';
    await page.waitForTimeout(240);
    await page.screenshot({ path: path.join(OUT, f) });
    out.shots.push(f);
    return f;
  }
  const P = (k, v) => { out.probes[k] = v; };

  await page.goto(PAGE);
  await page.waitForTimeout(1500);
  /* 创世页：量图例（U17）+ 第 0 年的顶栏写法（U21b） */
  await shot('creator-legend');
  P('U17b', await page.evaluate(() => {
    const i = document.querySelector('#creator .legend i');
    const b = i.getBoundingClientRect();
    const list = [...document.querySelectorAll('#creator .legend span')].map(s => {
      const sw = s.querySelector('i').getBoundingClientRect();
      return [s.textContent.trim(), +sw.width.toFixed(1), getComputedStyle(s.querySelector('i')).backgroundColor];
    });
    return { w: +b.width.toFixed(2), h: +b.height.toFixed(2), items: list };
  }));

  await page.evaluate(() => { window.__dbg.UI.enterFromCreator(); window.__dbg.UI.updateHud(); window.__dbg.UI.renderHand(); });
  await page.waitForTimeout(400);
  P('U21b_clock', await page.evaluate(() => {
    const c = document.getElementById('clock');
    return { text: c.textContent.replace(/\s+/g, ' ').trim(), year: document.getElementById('cYear').textContent,
      month: document.getElementById('cMonth').textContent, wMonth: window.__dbg.S.W.month };
  }));
  await shot('clock-month0');

  /* ── F14 细节 ── */
  P('F14b', await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -2; x <= 2; x++) for (let y = -2; y <= 2; y++) S.cmd.cast(land, x, y);
    document.getElementById('toasts').innerHTML = '';
    let i = S.W.hand.findIndex(c => c.sim.op === 'land');
    if (i < 0) { S.W.hand[0] = land; i = 0; }
    UI.select(i);
    const h0 = S.W.hand.length, c0 = S.W.cells.size, id0 = S.W.hand[i].id;
    const res = S.cmd.playSlot(i, 0, 0);
    return {
      res, handBefore: h0, handAfter: S.W.hand.length, cellsBefore: c0, cellsAfter: S.W.cells.size,
      sameCardStillThere: S.W.hand.some(c => c.id === id0),
      toasts: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.replace(/\s+/g, ' ').trim())
    };
  }));
  await shot('f14b-reject');

  /* ── H01b：拖到「已有地块」上看落点预览；顺便数一次拖拽触发了几次 cast ── */
  P('H01b', await page.evaluate(() => {
    const { S, D, UI, R } = window.__dbg;
    /* 计数：包一层 UI.cast，看不改源码 */
    window.__castN = 0;
    const orig = UI.cast;
    UI.cast = function () { window.__castN++; return orig.apply(this, arguments); };
    /* 手牌 0 号位放土地卡并选中 */
    const land = D.byId.get('terrain.earth.0.matter.land');
    S.W.hand[0] = land; UI.select(0);
    /* 找一个「有格子」的屏幕点 */
    const rc = R.canvas.getBoundingClientRect();
    const hand = document.getElementById('handwrap').getBoundingClientRect();
    let found = null;
    for (let gy = 0.2; gy < 0.7 && !found; gy += 0.03)
      for (let gx = 0.3; gx < 0.8; gx += 0.03) {
        const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy;
        if (y > hand.top - 30) continue;
        const c = R.cam.pickAt(x, y);
        if (c) { found = { x: Math.round(x), y: Math.round(y), cell: { x: c.x, y: c.y } }; break; }
      }
    return { pt: found, cells: S.W.cells.size };
  }));

  if (out.probes.H01b.pt) {
    const s = await page.evaluate(() => {
      const b = document.querySelector('.handslot').getBoundingClientRect();
      return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
    });
    const t = out.probes.H01b.pt;
    await page.mouse.move(s.x, s.y);
    await page.mouse.down();
    for (let i = 1; i <= 8; i++) {
      await page.mouse.move(s.x + (t.x - s.x) * i / 8, s.y + (t.y - s.y) * i / 8);
      await page.waitForTimeout(25);
    }
    await page.waitForTimeout(150);
    P('H01b.mid', await page.evaluate(() => {
      const g = document.getElementById('ghost');
      return { ghost: !!g, ghosts: document.querySelectorAll('#ghost').length,
        tip: g && g.querySelector('.tip') ? g.querySelector('.tip').textContent : null,
        marks: window.__dbg.R.marks.count(), hover: !!window.__dbg.R.cam.hover,
        castN: window.__castN, cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length };
    }));
    await shot('h01b-drag-over-cell-preview');
    await page.mouse.up();
    await page.waitForTimeout(400);
    P('H01b.after', await page.evaluate(() => ({
      ghost: document.querySelectorAll('#ghost').length, castN: window.__castN,
      cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length,
      toasts: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.replace(/\s+/g, ' ').trim())
    })));
    await shot('h01b-after-drop');
  }

  /* ── H04b：真的连按两次 pointerdown（合成事件，不松手） ── */
  P('H04b', await page.evaluate(() => {
    const slots = document.querySelectorAll('.handslot');
    const a = slots[0], b = slots[Math.min(1, slots.length - 1)];
    const mk = el => {
      const r = el.getBoundingClientRect();
      return new PointerEvent('pointerdown', { bubbles: true, cancelable: true,
        clientX: r.x + r.width / 2, clientY: r.y + r.height / 2, pointerId: 1, pointerType: 'touch', isPrimary: true });
    };
    a.dispatchEvent(mk(a));
    const g1 = document.querySelectorAll('#ghost').length;
    b.dispatchEvent(mk(b));
    const g2 = document.querySelectorAll('#ghost').length;
    const first = document.getElementById('ghost');
    return { afterFirst: g1, afterSecond: g2,
      left: first ? getComputedStyle(first).left : null, top: first ? getComputedStyle(first).top : null };
  }));
  await shot('h04b-double-pointerdown');
  await page.evaluate(() => document.dispatchEvent(new Event('pointerup', { bubbles: true })));
  await page.waitForTimeout(200);
  P('H04b.afterUp', await page.evaluate(() => ({ ghosts: document.querySelectorAll('#ghost').length,
    cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length })));

  /* ── H06b：成功回执（落点环 + 飘字） ── */
  P('H06b', await page.evaluate(() => {
    const { S, D, UI, R } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    S.W.hand[0] = land; UI.select(0);
    let spot = null;
    for (let r = 8; r < 40 && !spot; r++)
      for (let x = -r; x <= r && !spot; x++)
        for (let y = -r; y <= r; y++) if (!S.at(x, y)) { spot = { x, y }; break; }
    const before = S.W.cells.size;
    const res = S.cmd.playSlot(0, spot.x, spot.y);
    /* UI.cast 里才发飘字；这里走一遍 UI 的路径才有回执 */
    return { spot, before, after: S.W.cells.size, resOk: res.ok, made: res.made };
  }));
  /* 用 UI.cast 走一遍，量飘字与环 */
  P('H06b.ui', await page.evaluate(() => {
    const { S, D, UI, R } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    S.W.hand[0] = land; UI.select(0);
    let spot = null;
    for (let r = 8; r < 60 && !spot; r++)
      for (let x = -r; x <= r && !spot; x++)
        for (let y = -r; y <= r; y++) if (!S.at(x, y)) { spot = { x, y }; break; }
    const before = S.W.cells.size;
    UI.cast(0, null, spot);
    const fly = document.getElementById('gainfly');
    /* 落点环：marks 的 pulse 池在 scene 里，找 renderOrder 5 的 mesh 是否 visible */
    let pulses = 0, vis = 0;
    R.scene.traverse(o => { if (o.isMesh && o.renderOrder === 5) { pulses++; if (o.visible) vis++; } });
    return { before, after: S.W.cells.size, fly: fly ? fly.textContent : null,
      flyOp: fly ? getComputedStyle(fly).opacity : null, pulses, visiblePulses: vis };
  }));
  await shot('h06b-receipt');

  /* ── U22b：详情列底部渐隐（面板开着量） ── */
  P('U22b', await page.evaluate(() => {
    const { UI } = window.__dbg;
    UI.openPanel('codex');
    const cd = document.querySelector('.cdscroll');
    const a = cd ? getComputedStyle(cd, '::after') : null;
    const pb = document.getElementById('pbody');
    window.__dbg.UI.renderRight(true);
    const grow = document.querySelector('#rightpanel .rp.grow');
    const g = grow ? getComputedStyle(grow, '::after') : null;
    return {
      cd: a ? { content: a.content, h: a.height, bg: a.backgroundImage.slice(0, 46), pos: a.position, sticky: a.bottom } : null,
      cdScrollable: cd ? cd.scrollHeight - cd.clientHeight : null,
      pbodyAfter: pb ? getComputedStyle(pb, '::after').height : null,
      grow: g ? { content: g.content, h: g.height, bg: g.backgroundImage.slice(0, 46) } : null,
      growScrollable: grow ? grow.scrollHeight - grow.clientHeight : null
    };
  }));
  await shot('u22b-scroll-hint');

  /* ── P20b：找 '—' 的上下文 ── */
  P('P20b', await page.evaluate(() => {
    const { D, UI } = window.__dbg;
    const hits = [];
    for (const c of D.cards.slice(0, 12)) {
      UI.openCard(c.id);
      const d = document.querySelector('.cdscroll');
      const t = d ? d.textContent : '';
      if (/未定|—/.test(t)) {
        const idx = t.indexOf('—');
        hits.push({ card: c.name.zh, hasWeiDing: /未定/.test(t), ctx: idx >= 0 ? t.slice(Math.max(0, idx - 14), idx + 14) : null });
      }
    }
    const d2 = document.querySelector('.cdscroll');
    return { hits: hits.slice(0, 5), firstCardText: d2 ? d2.textContent.replace(/\s+/g, ' ').slice(0, 260) : null };
  }));
  await shot('p20b-detail');

  /* ── U32b：宽屏下 log 是否可读 ── */
  P('U32b', await page.evaluate(() => {
    const { S, UI } = window.__dbg;
    UI.closePanel();
    for (let i = 0; i < 3; i++) UI.log('土地铺开 ' + (12 + i) + ' 格', i === 0);
    const el = document.getElementById('log');
    const cs = getComputedStyle(el);
    const b = el.getBoundingClientRect();
    return { display: cs.display, bg: cs.backgroundImage, color: cs.color, fs: cs.fontSize,
      lines: el.children.length, w: Math.round(b.width), h: Math.round(b.height),
      textShadow: cs.textShadow.slice(0, 30) };
  }));
  await shot('u32b-log');

  P('errs', errs);
  fs.writeFileSync(path.join(OUT, 'probes.json'), JSON.stringify(out, null, 1));
  console.log(JSON.stringify(out.probes, null, 1));
  await browser.close();
})().catch(e => { console.error('FAIL', e); process.exit(1); });
