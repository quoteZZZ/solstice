/* 追一次拖拽里 UI.cast 被调了几次、从哪来的 */
const path = require('path');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
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
      window.__calls.push({ args: a.slice(0, 3), stack: new Error().stack.split('\n').slice(1, 6).join(' | '), hand0: S.W.hand[0] && S.W.hand[0].name.zh });
      return orig.apply(this, a);
    };
  });
  await page.waitForTimeout(300);
  await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    S.W.hand[0] = D.byId.get('terrain.earth.0.matter.land'); S.W.sel = 0; UI.renderHand();
  });
  const pt = await page.evaluate(() => {
    const { R } = window.__dbg; const rc = document.getElementById('cv').getBoundingClientRect();
    for (let gy = .12; gy < .5; gy += .03) for (let gx = .1; gx < .9; gx += .03) {
      const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy;
      if (!R.cam.pickAt(x, y)) return { x: Math.round(x), y: Math.round(y) };
    }
    return null;
  });
  const c = await page.evaluate(() => { const b = document.querySelector('.handslot').getBoundingClientRect(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; });
  console.log('pt', pt, 'card', c);
  await page.mouse.move(c.x, c.y); await page.mouse.down();
  await page.mouse.move(pt.x, pt.y, { steps: 8 }); await page.waitForTimeout(150);
  console.log('before up', await page.evaluate(() => ({ calls: window.__calls.length, hand: window.__dbg.S.W.hand.length, cells: window.__dbg.S.W.cells.size })));
  await page.mouse.up(); await page.waitForTimeout(500);
  console.log(await page.evaluate(() => ({
    calls: window.__calls, hand: window.__dbg.S.W.hand.length, cells: window.__dbg.S.W.cells.size,
    toasts: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()),
    ghosts: document.querySelectorAll('#ghost').length,
    handNames: window.__dbg.S.W.hand.map(c => c.name.zh)
  })));
  await page.screenshot({ path: path.join(__dirname, 'hunt-shots', 'debug-double.png') });
  await browser.close();
})();
