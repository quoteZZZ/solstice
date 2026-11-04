/* 探针 6：成功拖放到底几次 cast（带栈） */
const path = require('path');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const VW = 1680, VH = 1000;
(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  for (const mode of ['void', 'cell']) {
    const page = await browser.newPage({ viewport: { width: VW, height: VH } });
    await page.goto(PAGE); await page.waitForTimeout(1100);
    await page.evaluate(() => {
      const { S, D, UI } = window.__dbg;
      UI.enterFromCreator();
      S.W.cells.clear();
      S.W.hand[0] = D.byId.get('terrain.earth.0.matter.land');
      S.W.sel = 0; S.W.essence = 999; S.setGear(0);
      UI.syncGears(); UI.renderHand(); UI.updateHud();
      window.__log = [];
      const o = UI.cast;
      UI.cast = function (i, cell, pt) {
        window.__log.push({ call: 'UI.cast', i, cell: cell ? [cell.x, cell.y] : null, pt: pt ? [pt.x, pt.y] : null,
          stack: (new Error().stack || '').split('\n').slice(2, 4).map(s => s.trim().replace(/^at /, '')).join(' << ') });
        return o.apply(this, arguments);
      };
      document.addEventListener('pointerup', e => window.__log.push({ call: 'doc-pointerup', tgt: e.target.id || e.target.tagName }), true);
      document.addEventListener('pointerup', e => window.__log.push({ call: 'doc-pointerup-bubble', tgt: e.target.id || e.target.tagName }));
    });
    if (mode === 'cell') {
      await page.evaluate(() => { const { S, D } = window.__dbg; const land = D.byId.get('terrain.earth.0.matter.land'); for (let x = 0; x <= 4; x++) for (let y = 0; y <= 4; y++) S.cmd.cast(land, x, y); });
    }
    const r = await page.evaluate(() => { const b = document.querySelectorAll('.handslot')[0].getBoundingClientRect(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; });
    await page.mouse.move(r.x, r.y); await page.mouse.down();
    for (let i = 1; i <= 4; i++) { await page.mouse.move(r.x, r.y - i * 55); await page.waitForTimeout(25); }
    await page.mouse.move(VW / 2, VH * 0.33); await page.waitForTimeout(80);
    await page.mouse.up(); await page.waitForTimeout(500);
    const a = await page.evaluate(() => ({ log: window.__log, cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.map(c => c.id),
      toasts: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.trim()) }));
    console.log('===', mode, 'cells', a.cells);
    a.log.forEach((l, i) => console.log(i, JSON.stringify(l)));
    console.log('toasts', JSON.stringify(a.toasts));
    await page.close();
  }
  await browser.close();
})();
