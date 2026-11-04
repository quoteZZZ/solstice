/* 1080 初次布局 vs 1440→1080 未重排 vs 重排后：几何 + 命中 + 截图 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'hunt-shots');
const read = () => {
  const hw = document.getElementById('handwrap');
  const hb = hw.getBoundingClientRect();
  const slots = [...document.querySelectorAll('.handslot')].map(e => { const b = e.getBoundingClientRect(); return { x: Math.round(b.x), right: Math.round(b.right), y: Math.round(b.y), bottom: Math.round(b.bottom) }; });
  const cards = [...document.querySelectorAll('.handslot .card')].map(e => { const b = e.getBoundingClientRect(); return { left: Math.round(b.left), right: Math.round(b.right) }; });
  const rp = document.getElementById('rightpanel').getBoundingClientRect();
  const ab = document.getElementById('actbar').getBoundingClientRect();
  const dk = document.getElementById('deck').getBoundingClientRect();
  const hit = (x, y) => { const e = document.elementFromPoint(x, y); return { x, y, el: e ? (e.id || (e.className && e.className.baseVal) || e.className) : null, inAct: !!(e && e.closest && e.closest('#actbar')), inCard: !!(e && e.closest && e.closest('.handslot')) }; };
  return {
    iw: innerWidth, scale: getComputedStyle(document.documentElement).getPropertyValue('--hand-scale').trim(),
    hw: { x: Math.round(hb.x), w: Math.round(hw.clientWidth), sw: hw.scrollWidth },
    n: slots.length, first: slots[0], last: slots[slots.length - 1],
    firstCard: cards[0], lastCard: cards[cards.length - 1],
    rpLeft: Math.round(rp.left), actbar: { x: Math.round(ab.x), right: Math.round(ab.right), y: Math.round(ab.y), bottom: Math.round(ab.bottom) },
    deck: { x: Math.round(dk.x), right: Math.round(dk.right), y: Math.round(dk.y), bottom: Math.round(dk.bottom) },
    hits: [hit(ab.x + 18, ab.y + 18), hit(ab.x + 18, ab.y + 60), hit(ab.x + 18, ab.bottom - 18), hit(dk.x + 20, dk.y + 20), hit(dk.x + 60, dk.y + 20), hit(ab.x + 30, ab.y + 90)]
  };
};
(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1080, height: 700 }, deviceScaleFactor: 1 });
  await page.goto(PAGE); await page.waitForTimeout(1500);
  await page.evaluate(() => { const { S, D, UI } = window.__dbg; UI.enterFromCreator(); UI.updateHud(); UI.renderHand(); const l = D.byId.get('terrain.earth.0.matter.land'); for (let x = -3; x <= 3; x++) for (let y = -3; y <= 3; y++) S.cmd.cast(l, x, y); S.W.essence = 999; UI.updateHud(); UI.renderRight(); UI.renderHand(); });
  await page.waitForTimeout(400);
  const fresh = await page.evaluate(read);
  await page.screenshot({ path: path.join(OUT, 'p3-11-fresh-1080.png') });

  await page.setViewportSize({ width: 1440, height: 900 });
  await page.waitForTimeout(500);
  await page.evaluate(() => window.__dbg.UI.renderHand());
  await page.waitForTimeout(300);
  const at1440 = await page.evaluate(read);
  await page.setViewportSize({ width: 1080, height: 700 });
  await page.waitForTimeout(700);
  const stale = await page.evaluate(read);
  await page.screenshot({ path: path.join(OUT, 'p3-12-resize-stale-1080.png') });
  await page.evaluate(() => window.__dbg.UI.renderHand());
  await page.waitForTimeout(300);
  const rerendered = await page.evaluate(read);
  await page.screenshot({ path: path.join(OUT, 'p3-13-resize-rerender-1080.png') });

  const out = { fresh, at1440, stale, rerendered };
  fs.writeFileSync(path.join(OUT, 'evidence-1080.json'), JSON.stringify(out, null, 1));
  console.log(JSON.stringify(out, null, 1));
  await browser.close();
})();
