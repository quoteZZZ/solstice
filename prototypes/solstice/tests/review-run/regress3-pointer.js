/* 第二轮回归 · 新快照：指针/拖拽类复测（1080×700）+ 修正 U22/U31/切包可买判定
   node tests/review-run/regress3-pointer.js */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const ROOT = path.join(__dirname, '..', '..');
const PAGE = pathToFileURL(path.join(ROOT, 'index.html')).href;
const OUT = path.join(__dirname, 'regress3-pointer');
fs.mkdirSync(OUT, { recursive: true });
const P = {};
let n = 0;

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1080, height: 700 }, deviceScaleFactor: 1 });
  const errs = [];
  page.on('pageerror', e => errs.push(String(e.message)));
  const shot = async name => { n++; const f = String(n).padStart(2, '0') + '-' + name + '.png'; await page.waitForTimeout(240); await page.screenshot({ path: path.join(OUT, f) }); return f; };
  const ev = fn => page.evaluate(fn);

  await page.goto(PAGE);
  await page.waitForTimeout(1600);
  P.E16 = await ev(() => getComputedStyle(document.documentElement).getPropertyValue('--hand-scale').trim());

  await ev(() => { window.__dbg.UI.enterFromCreator(); window.__dbg.UI.updateHud(); window.__dbg.UI.renderHand(); });
  await page.waitForTimeout(500);

  /* F14（走 UI 路径要 toast） */
  P.F14 = await ev(() => {
    const { S, D, UI } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -2; x <= 2; x++) for (let y = -2; y <= 2; y++) S.cmd.cast(land, x, y);
    document.getElementById('toasts').innerHTML = '';
    S.W.hand[0] = land; UI.select(0); UI.renderHand();
    const h0 = S.W.hand.length, c0 = S.W.cells.size;
    UI.cast(0, { x: 0, y: 0 }, null);
    return { hand: [h0, S.W.hand.length], cells: [c0, S.W.cells.size],
      toasts: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.replace(/\s+/g, ' ').trim()) };
  });
  await shot('f14');

  /* U22（面板开着时读，别先 close） */
  P.U22 = await ev(() => {
    const { UI } = window.__dbg;
    UI.openPanel('codex');
    const cd = document.querySelector('.cdscroll');
    const read = el => { const c = getComputedStyle(el, '::after'); return { h: c.height, content: c.content, bg: c.backgroundImage.slice(0, 34), pos: c.position }; };
    const a = read(cd), b = read(document.getElementById('pbody'));
    const scrollable = cd.scrollHeight - cd.clientHeight;
    UI.closePanel();
    UI.renderRight(true);
    const g = read(document.querySelector('#rightpanel .rp.grow'));
    const gs = (() => { const e = document.querySelector('#rightpanel .rp.grow'); return e.scrollHeight - e.clientHeight; })();
    return { cd: a, pbody: b, grow: g, scrollable, growScrollable: gs };
  });

  /* U31（先制造痕迹） */
  P.U31 = await ev(() => {
    const { S, D, R } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -5; x <= 5; x++) for (let y = -5; y <= 5; y++) S.cmd.cast(land, x, y);
    const rings = R.traceGroup.children.filter(m => m.geometry.type === 'RingGeometry');
    return { traces: S.W.traces.length, rings: rings.length, opacity: rings[0] && rings[0].material.opacity,
      r: rings[0] && +rings[0].geometry.parameters.outerRadius.toFixed(3),
      coords: S.W.traces.slice(0, 3).map(t => [t.x, t.y]) };
  });

  /* 切包可买判定（先把 packStage 归 0） */
  P.packAfford = await ev(() => {
    const { UI, S } = window.__dbg;
    UI.packStage = 0; UI.packCards = [];
    S.W.essence = 15; UI.updateHud(); UI.openPanel('pack');
    const rd = () => { const b = document.querySelector('#pfoot [data-act="packNext"]'); return { t: b.textContent, d: b.disabled }; };
    document.querySelector('.packrow').click(); const basic = rd();
    [...document.querySelectorAll('.packrow')].find(r => /进阶/.test(r.textContent)).click(); const mid = rd();
    UI.closePanel();
    return { basic, mid, top: document.getElementById('bPack').textContent };
  });

  /* ── 拖拽 ── */
  await ev(() => {
    const { S, D, R, UI } = window.__dbg;
    S.W.essence = 500; R.cam.setZoom(2);
    window.__casts = [];
    S.on('cast', e => window.__casts.push(e.card.name.zh + '@' + e.x + ',' + e.y));
  });
  await page.waitForTimeout(1200);
  await ev(() => { const C = window.__dbg.R.cam; C.snapGoal(); C.place(); });

  const geom = await ev(() => {
    const { R } = window.__dbg;
    const rc = R.canvas.getBoundingClientRect();
    const hand = document.getElementById('handwrap').getBoundingClientRect();
    const rp = document.getElementById('rightpanel').getBoundingClientRect();
    let cell = null, vd = null, hits = 0;
    for (let y = rc.top + 60; y < hand.top - 40; y += 12) {
      for (let x = rc.left + 60; x < rp.left - 20; x += 12) {
        const c = R.cam.pickAt(x, y);
        if (c) { hits++; if (!cell) cell = { x: Math.round(x), y: Math.round(y) }; }
        else if (!vd) vd = { x: Math.round(x), y: Math.round(y) };
      }
    }
    const b = document.querySelector('.handslot').getBoundingClientRect();
    return { cell, vd, hits, slot: { x: b.x + b.width / 2, y: b.y + b.height / 2 },
      handTop: Math.round(hand.top), rpLeft: Math.round(rp.left), cells: window.__dbg.S.W.cells.size };
  });
  P.geom = geom;

  async function drag(tx, ty, key) {
    await ev(() => { window.__casts = []; document.getElementById('toasts').innerHTML = ''; });
    await page.mouse.move(geom.slot.x, geom.slot.y);
    await page.mouse.down();
    for (let i = 1; i <= 8; i++) {
      await page.mouse.move(geom.slot.x + (tx - geom.slot.x) * i / 8, geom.slot.y + (ty - geom.slot.y) * i / 8);
      await page.waitForTimeout(25);
    }
    const mid = await ev(() => ({
      tip: document.querySelector('#ghost .tip') ? document.querySelector('#ghost .tip').textContent : null,
      marks: window.__dbg.R.marks.count(), ghosts: document.querySelectorAll('#ghost').length, casts: window.__casts.slice()
    }));
    if (key) { await page.keyboard.press(key); await page.waitForTimeout(260); }
    const afterKey = await ev(() => ({ casts: window.__casts.slice(), ghosts: document.querySelectorAll('#ghost').length }));
    await page.mouse.up();
    await page.waitForTimeout(420);
    const afterUp = await ev(() => ({ casts: window.__casts.slice(), cells: window.__dbg.S.W.cells.size,
      hand: window.__dbg.S.W.hand.length, ghosts: document.querySelectorAll('#ghost').length,
      fly: document.getElementById('gainfly') ? document.getElementById('gainfly').textContent : null,
      toasts: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.replace(/\s+/g, ' ').trim()) }));
    return { mid, afterKey, afterUp };
  }

  /* 7 H01：拖到已有格 */
  await ev(() => { const { S, D, UI } = window.__dbg; S.W.hand[0] = D.byId.get('terrain.mineral.0.matter.water'); UI.select(0); UI.renderHand(); });
  P.H01 = await drag(geom.cell.x, geom.cell.y);
  await shot('h01-drop');

  /* 7b 拖到虚空（土地卡） */
  await ev(() => { const { S, D, UI } = window.__dbg; S.W.hand[0] = D.byId.get('terrain.earth.0.matter.land'); UI.select(0); UI.renderHand(); });
  P.H01void = await drag(geom.vd.x, geom.vd.y);
  await shot('h01-void');

  /* 8 H02 向下 62 */
  await ev(() => { const { S, D, UI } = window.__dbg; S.W.hand[0] = D.byId.get('terrain.mineral.0.matter.water'); UI.select(0); UI.renderHand(); });
  P.H02 = await drag(geom.slot.x + 6, geom.slot.y + 62);
  await shot('h02');

  /* 9 U38 Esc */
  await ev(() => { const { S, D, UI } = window.__dbg; S.W.hand[0] = D.byId.get('terrain.mineral.0.matter.water'); UI.select(0); UI.renderHand(); });
  P.U38 = await drag(geom.cell.x, geom.cell.y, 'Escape');
  await shot('u38');

  /* S08 Space / X 在拖拽中 */
  await ev(() => { const { S, D, UI } = window.__dbg; S.W.hand[0] = D.byId.get('terrain.mineral.0.matter.water'); UI.select(0); UI.renderHand(); });
  P.S08space = await drag(geom.cell.x, geom.cell.y, 'Space');
  await ev(() => { const { S, D, UI } = window.__dbg; S.W.hand[0] = D.byId.get('terrain.mineral.0.matter.water'); UI.select(0); UI.renderHand(); });
  P.S08x = await drag(geom.cell.x, geom.cell.y, 'x');

  /* 10 H04 合成两次 pointerdown */
  P.H04 = await ev(() => {
    const slots = document.querySelectorAll('.handslot');
    const mk = el => { const r = el.getBoundingClientRect();
      return new PointerEvent('pointerdown', { bubbles: true, cancelable: true, clientX: r.x + r.width / 2, clientY: r.y + r.height / 2, pointerId: 1, pointerType: 'touch', isPrimary: true }); };
    slots[0].dispatchEvent(mk(slots[0]));
    const a = document.querySelectorAll('#ghost').length;
    slots[Math.min(1, slots.length - 1)].dispatchEvent(mk(slots[1] || slots[0]));
    const b = document.querySelectorAll('#ghost').length;
    const g = document.getElementById('ghost');
    const left = g ? getComputedStyle(g).left : null;
    document.dispatchEvent(new Event('pointerup', { bubbles: true }));
    return { afterFirst: a, afterSecond: b, left, afterUp: document.querySelectorAll('#ghost').length };
  });
  await shot('h04');

  /* 11 H06 成功回执 */
  P.H06 = await ev(() => {
    const { S, D, UI, R } = window.__dbg;
    S.W.hand[0] = D.byId.get('terrain.earth.0.matter.land'); UI.select(0); UI.renderHand();
    let spot = null;
    for (let r = 7; r < 60 && !spot; r++) for (let x = -r; x <= r && !spot; x++)
      for (let y = -r; y <= r; y++) if (!S.at(x, y)) { spot = { x, y }; break; }
    const before = S.W.cells.size;
    UI.cast(0, null, spot);
    let pulses = 0, vis = 0;
    R.scene.traverse(o => { if (o.isMesh && o.renderOrder === 5) { pulses++; if (o.visible) vis++; } });
    const fly = document.getElementById('gainfly');
    return { before, after: S.W.cells.size, fly: fly && fly.textContent, flyOp: fly && getComputedStyle(fly).opacity, pulses, vis };
  });
  await shot('h06');

  /* 点世界施卡 / Enter 施卡 仍可用 */
  P.clickPath = await ev(async () => {
    const { S, D, UI } = window.__dbg;
    S.W.hand[0] = D.byId.get('terrain.mineral.0.matter.water'); UI.select(0); UI.renderHand();
    window.__casts = [];
    return { card: S.W.hand[0].name.zh };
  });
  await page.mouse.move(geom.cell.x, geom.cell.y); await page.waitForTimeout(150);
  await page.mouse.click(geom.cell.x, geom.cell.y); await page.waitForTimeout(350);
  P.clickPath.casts = await ev(() => window.__casts.slice());

  P.errs = errs;
  fs.writeFileSync(path.join(OUT, 'probes.json'), JSON.stringify(P, null, 1));
  console.log(JSON.stringify(P, null, 1));
  await browser.close();
})().catch(e => { console.error('FAIL', e); process.exit(1); });
