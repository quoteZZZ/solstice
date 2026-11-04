/* 取证 3：手牌与右栏的像素级重叠、面板后的空格施卡、拖到已有格上的双施。 */
const path = require('path');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'hunt-shots');
(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1080, height: 700 }, deviceScaleFactor: 1 });
  page.on('pageerror', e => console.log('PAGEERROR', e.message));
  await page.goto(PAGE); await page.waitForTimeout(1500);
  await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    UI.enterFromCreator(); UI.updateHud(); UI.renderHand();
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -3; x <= 3; x++) for (let y = -3; y <= 3; y++) S.cmd.cast(land, x, y);
    S.W.essence = 999; UI.updateHud(); UI.renderRight(); UI.renderHand();
    window.__calls = [];
    const orig = UI.cast;
    UI.cast = function (...a) {
      window.__calls.push({ hand0: S.W.hand[0] && S.W.hand[0].name.zh, stack: new Error().stack.split('\n').slice(1, 5).join(' | ') });
      return orig.apply(this, a);
    };
  });
  await page.waitForTimeout(350);

  /* 1. 手牌最后一格与右栏：谁在上面 */
  const geo = await page.evaluate(() => {
    const slots = [...document.querySelectorAll('.handslot')];
    const last = slots[slots.length - 1].getBoundingClientRect();
    const rp = document.getElementById('rightpanel').getBoundingClientRect();
    const pts = [[810, 500], [815, 500], [820, 520], [828, 540], [805, 490], [50, 500]].map(([x, y]) => {
      const el = document.elementFromPoint(x, y);
      return { x, y, el: el ? (el.id || (el.className && el.className.baseVal) || el.tagName) : null, card: !!(el && el.closest && el.closest('.handslot')) };
    });
    return { lastSlot: { x: Math.round(last.x), right: Math.round(last.right) }, rpLeft: Math.round(rp.left), pts };
  });
  console.log('geo', JSON.stringify(geo));
  await page.screenshot({ path: path.join(OUT, 'p3-01-hand-right-edge.png'), clip: { x: 740, y: 450, width: 140, height: 260 } });
  await page.screenshot({ path: path.join(OUT, 'p3-02-hand-left-edge.png'), clip: { x: 30, y: 450, width: 120, height: 260 } });

  /* 2. 面板开着时空格施卡（有地的世界） */
  const space = await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    const grass = D.byId.get('flora.wood.1.matter.grass');
    S.W.hand[0] = grass; S.W.sel = 0; S.W.essence = 999; UI.renderHand();
    const ents = () => { let n = 0; for (const c of S.W.cells.values()) n += c.ents.length; return n; };
    UI.openPanel('pack');
    return { before: { hand: S.W.hand.length, ents: ents(), cells: S.W.cells.size } };
  });
  await page.waitForTimeout(250);
  await page.keyboard.press(' ');
  await page.waitForTimeout(350);
  const spaceAfter = await page.evaluate(() => {
    const { S, UI } = window.__dbg;
    let n = 0; for (const c of S.W.cells.values()) n += c.ents.length;
    return { hand: S.W.hand.length, ents: n, cells: S.W.cells.size, panel: UI.state.panel, toastHead: [...document.querySelectorAll('#phead .toast.ih')].map(e => e.textContent.trim()), toastTop: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()) };
  });
  console.log('space', JSON.stringify({ space, spaceAfter }));
  await page.screenshot({ path: path.join(OUT, 'p3-03-space-behind-panel.png') });
  await page.evaluate(() => window.__dbg.UI.closePanel());

  /* 3. 拖到已有地块上：两次 cast 是否都成功 */
  const r = await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    S.W.hand[0] = D.byId.get('flora.wood.1.matter.grass');
    S.W.sel = 0; UI.renderHand();
    const c = S.at(0, 0);
    c.W = Math.max(c.W, 1.5); c.L = Math.max(c.L, 1.5);
    window.__calls = [];
    return { cell: { x: c.x, y: c.y, W: c.W, L: c.L }, hand: S.W.hand.length };
  });
  /* 找 (0,0) 格的屏幕点 */
  const pt = await page.evaluate(() => {
    const { R, S } = window.__dbg;
    const rc = document.getElementById('cv').getBoundingClientRect();
    for (let gy = .1; gy < .9; gy += .02) for (let gx = .1; gx < .9; gx += .02) {
      const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy;
      const c = R.cam.pickAt(x, y);
      if (c && c.x === 0 && c.y === 0) return { x: Math.round(x), y: Math.round(y) };
    }
    return null;
  });
  console.log('pt0', JSON.stringify(pt), 'r', JSON.stringify(r));
  if (pt) {
    const c = await page.evaluate(() => { const b = document.querySelector('.handslot').getBoundingClientRect(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; });
    await page.mouse.move(c.x, c.y); await page.mouse.down();
    await page.mouse.move(pt.x, pt.y, { steps: 8 }); await page.waitForTimeout(150);
    await page.mouse.up(); await page.waitForTimeout(500);
    const after = await page.evaluate(() => {
      const { S, UI } = window.__dbg;
      let n = 0; for (const c of S.W.cells.values()) n += c.ents.length;
      return { hand: S.W.hand.length, names: S.W.hand.map(c => c.name.zh), ents: n, toasts: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()), head: [...document.querySelectorAll('#phead .toast.ih')].map(e => e.textContent.trim()), calls: window.__calls };
    });
    console.log('drag-cell', JSON.stringify(after, null, 1));
    await page.screenshot({ path: path.join(OUT, 'p3-04-drag-cell.png') });
  }

  await browser.close();
})();
