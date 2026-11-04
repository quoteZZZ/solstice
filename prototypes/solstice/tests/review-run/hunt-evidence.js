/* 汇总关键读数到 hunt-shots/evidence.json（只读页面） */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'hunt-shots');
(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1080, height: 700 }, deviceScaleFactor: 1 });
  const errs = [];
  page.on('pageerror', e => errs.push(e.message));
  await page.goto(PAGE); await page.waitForTimeout(1500);
  const E = { viewport: [1080, 700], errors: errs };
  await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    UI.enterFromCreator(); UI.updateHud(); UI.renderHand();
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -3; x <= 3; x++) for (let y = -3; y <= 3; y++) S.cmd.cast(land, x, y);
    S.W.essence = 999; UI.updateHud(); UI.renderRight(); UI.renderHand();
    window.__calls = [];
    const o = UI.cast;
    UI.cast = function (...a) {
      window.__calls.push({ hand0: S.W.hand[0] && S.W.hand[0].name.zh, stack: new Error().stack.split('\n').slice(1, 5).join(' | ') });
      return o.apply(this, a);
    };
  });
  await page.waitForTimeout(400);

  /* 1. 手牌、动作栏、牌堆、右栏的几何 */
  E.geometry = await page.evaluate(() => {
    const q = s => { const e = document.querySelector(s); const b = e.getBoundingClientRect(); return { x: Math.round(b.x), y: Math.round(b.y), w: Math.round(b.width), h: Math.round(b.height), right: Math.round(b.right) }; };
    const ab = q('#actbar'), dk = q('#deck'), rp = q('#rightpanel'), hw = q('#handwrap');
    const slots = [...document.querySelectorAll('.handslot')].map(e => { const b = e.getBoundingClientRect(); return { x: Math.round(b.x), right: Math.round(b.right), y: Math.round(b.y), bottom: Math.round(b.bottom) }; });
    const cards = [...document.querySelectorAll('.handslot .card')].map(e => { const b = e.getBoundingClientRect(); return { left: Math.round(b.left), right: Math.round(b.right), top: Math.round(b.top), bottom: Math.round(b.bottom) }; });
    const hit = (x, y) => { const e = document.elementFromPoint(x, y); return { x, y, el: e ? (e.id || (e.className && e.className.baseVal) || e.className) : null, inAct: !!(e && e.closest && e.closest('#actbar')), inCard: !!(e && e.closest && e.closest('.handslot')) }; };
    return {
      handwrap: hw, actbar: ab, deck: dk, rightpanel: rp,
      slots, firstCard: cards[0], lastCard: cards[cards.length - 1],
      handCenter: Math.round((slots[0].x + slots[slots.length - 1].right) / 2), wrapCenter: Math.round(hw.x + hw.w / 2),
      sw: document.getElementById('handwrap').scrollWidth, cw: document.getElementById('handwrap').clientWidth,
      hits: [hit(ab.x + 18, ab.y + 18), hit(ab.x + 18, ab.y + 60), hit(ab.x + 18, ab.y + ab.h - 18), hit(dk.x + 20, dk.y + 20), hit(dk.x + 60, dk.y + 20), hit(rp.x + 8, 520), hit(rp.x + 16, 540), hit(hw.x + 2, 520)]
    };
  });

  /* 2. 面板开着时空格施卡 */
  E.spaceBehindPanel = await page.evaluate(async () => {
    const { S, D, UI } = window.__dbg;
    const grass = D.byId.get('flora.wood.1.matter.grass');
    S.W.hand[0] = grass; S.W.sel = 0; S.W.essence = 999; UI.renderHand();
    const ents = () => { let n = 0; for (const c of S.W.cells.values()) n += c.ents.length; return n; };
    const before = { hand: S.W.hand.length, ents: ents(), cells: S.W.cells.size };
    UI.openPanel('pack');
    return { before, panel: UI.state.panel };
  });
  await page.waitForTimeout(200);
  await page.keyboard.press(' ');
  await page.waitForTimeout(400);
  E.spaceAfter = await page.evaluate(() => {
    const { S, UI } = window.__dbg;
    let n = 0; for (const c of S.W.cells.values()) n += c.ents.length;
    return { hand: S.W.hand.length, ents: n, cells: S.W.cells.size, panel: UI.state.panel, head: [...document.querySelectorAll('#phead .toast.ih')].map(e => e.textContent.trim()), top: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()) };
  });
  await page.evaluate(() => window.__dbg.UI.closePanel());

  /* 3. 拖到已有格：两次 UI.cast */
  await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    S.W.hand[0] = D.byId.get('flora.wood.1.matter.grass'); S.W.sel = 0; UI.renderHand();
    const c = S.at(0, 0); c.W = 1.5; c.L = 1.5; window.__calls = [];
  });
  const pt = await page.evaluate(() => {
    const { R } = window.__dbg; const rc = document.getElementById('cv').getBoundingClientRect();
    for (let gy = .1; gy < .9; gy += .02) for (let gx = .1; gx < .9; gx += .02) {
      const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy;
      const c = R.cam.pickAt(x, y);
      if (c && c.x === 0 && c.y === 0) return { x: Math.round(x), y: Math.round(y) };
    }
    return null;
  });
  const c0 = await page.evaluate(() => { const b = document.querySelector('.handslot').getBoundingClientRect(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; });
  await page.mouse.move(c0.x, c0.y); await page.mouse.down();
  await page.mouse.move(pt.x, pt.y, { steps: 8 }); await page.waitForTimeout(150);
  await page.mouse.up(); await page.waitForTimeout(500);
  E.dragDouble = await page.evaluate(() => {
    const { S } = window.__dbg;
    let n = 0; for (const c of S.W.cells.values()) n += c.ents.length;
    return { cellPoint: null, hand: S.W.hand.length, names: S.W.hand.map(c => c.name.zh), ents: n, calls: window.__calls, toasts: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()) };
  });
  E.dragDouble.cellPoint = pt;

  /* 4. 手牌末张与右栏的命中 */
  await page.evaluate(() => window.__dbg.UI.renderHand());
  await page.waitForTimeout(300);
  E.overlap = await page.evaluate(() => {
    const rp = document.getElementById('rightpanel').getBoundingClientRect();
    const pts = [818, 824, 830, 835].map(x => {
      const e = document.elementFromPoint(x, 520);
      return { x, el: e ? (e.id || (e.className && e.className.baseVal) || e.className) : null, inCard: !!(e && e.closest && e.closest('.handslot')) };
    });
    return { rpLeft: Math.round(rp.left), pts };
  });

  fs.writeFileSync(path.join(OUT, 'evidence.json'), JSON.stringify(E, null, 1));
  console.log(JSON.stringify(E, null, 1).slice(0, 4000));
  await browser.close();
})();
