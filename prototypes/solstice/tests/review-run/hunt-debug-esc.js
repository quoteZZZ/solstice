/* Esc 取消拖拽到底做了什么 */
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
      window.__calls.push({ args: a.slice(0, 3), hand0: S.W.hand[0] && S.W.hand[0].name.zh, stack: new Error().stack.split('\n').slice(1, 5).join(' | ') });
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
  await page.mouse.move(c.x, c.y); await page.mouse.down();
  await page.mouse.move(pt.x, pt.y, { steps: 8 }); await page.waitForTimeout(150);
  console.log('mid', await page.evaluate(() => ({ calls: window.__calls.length, cells: window.__dbg.S.W.cells.size, ghosts: document.querySelectorAll('#ghost').length })));
  await page.keyboard.press('Escape');
  await page.waitForTimeout(200);
  console.log('afterEsc', await page.evaluate(() => ({
    calls: window.__calls, cells: window.__dbg.S.W.cells.size, ghosts: document.querySelectorAll('#ghost').length,
    toasts: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()),
    hand: window.__dbg.S.W.hand.length
  })));
  await page.screenshot({ path: path.join(__dirname, 'hunt-shots', 'debug-esc1.png') });
  await page.mouse.up();
  await page.waitForTimeout(300);
  console.log('afterUp', await page.evaluate(() => ({
    calls: window.__calls, cells: window.__dbg.S.W.cells.size, ghosts: document.querySelectorAll('#ghost').length,
    toasts: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()),
    hand: window.__dbg.S.W.hand.length
  })));
  await browser.close();
})();
