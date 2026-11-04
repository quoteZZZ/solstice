/* 探针 4：一次拖放到底调用了几次 UI.cast（含调用栈与结果） */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'state-shots');
const VW = 1680, VH = 1000;
const out = [];

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  async function open() {
    const page = await browser.newPage({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
    await page.goto(PAGE); await page.waitForTimeout(1100);
    await page.evaluate(() => {
      const { S, D, UI } = window.__dbg;
      UI.enterFromCreator();
      const land = D.byId.get('terrain.earth.0.matter.land');
      for (let x = -2; x <= 2; x++) for (let y = -2; y <= 2; y++) S.cmd.cast(land, x, y);
      S.W.essence = 500; S.setGear(2);
      UI.syncGears(); UI.updateHud(); UI.renderHand();
      window.__casts = []; window.__slotRes = [];
      const origCast = UI.cast;
      UI.cast = function (i, cell, pt) {
        window.__casts.push({ i: i, cell: cell ? [cell.x, cell.y] : null,
          stack: (new Error().stack || '').split('\n').slice(2, 5).map(s => s.trim().replace(/^at /, '')).join(' << ') });
        return origCast.apply(this, arguments);
      };
      const origSlot = S.cmd.playSlot;
      S.cmd.playSlot = function (i, x, y) {
        const r = origSlot.apply(this, arguments);
        window.__slotRes.push({ i: i, at: [x, y], ok: !!r.ok, msg: r.msg || '', card: (S.W.hand[i] || {}).id });
        return r;
      };
    });
    await page.waitForTimeout(150);
    return page;
  }
  const read = p => p.evaluate(() => {
    const { S } = window.__dbg;
    return { casts: window.__casts, slots: window.__slotRes, hand: S.W.hand.map(c => c.id), sel: S.W.sel,
      cells: S.W.cells.size, essence: S.W.essence, ghost: document.querySelectorAll('#ghost').length, month: S.W.month };
  });
  async function drag(page) {
    const r = await page.evaluate(() => { const b = document.querySelectorAll('.handslot')[0].getBoundingClientRect(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; });
    await page.mouse.move(r.x, r.y); await page.mouse.down();
    for (let i = 1; i <= 4; i++) { await page.mouse.move(r.x, r.y - i * 55); await page.waitForTimeout(25); }
  }
  let n = 0;
  const shot = async (p, tag) => { n++; const f = 'p4-' + n + '-' + tag + '.png'; await p.waitForTimeout(250); await p.screenshot({ path: path.join(OUT, f) }); return f; };

  /* H1 普通拖放 */
  {
    const page = await open();
    const before = await read(page);
    await drag(page); await page.mouse.move(VW / 2, VH * 0.4); await page.waitForTimeout(60);
    await page.mouse.up(); await page.waitForTimeout(350);
    const a = await read(page);
    out.push({ id: 'H1', before, after: a });
    console.log('H1 casts', a.casts.length, 'slots', JSON.stringify(a.slots));
    a.casts.forEach((c, i) => console.log('   #' + i, c.i, JSON.stringify(c.cell), '|', c.stack));
    await shot(page, 'drag-one');
    await page.close();
  }

  /* H2 Esc 取消后松手 */
  {
    const page = await open();
    await drag(page); await page.keyboard.press('Escape'); await page.waitForTimeout(150);
    const mid = await read(page);
    await page.mouse.move(VW / 2, VH * 0.4); await page.mouse.up(); await page.waitForTimeout(350);
    const a = await read(page);
    out.push({ id: 'H2', mid, after: a });
    console.log('H2 midGhost', mid.ghost, 'casts', a.casts.length, JSON.stringify(a.slots));
    a.casts.forEach((c, i) => console.log('   #' + i, c.i, JSON.stringify(c.cell), '|', c.stack));
    await shot(page, 'drag-esc');
    await page.close();
  }

  /* H3 拖拽中开面板再松手 */
  {
    const page = await open();
    await drag(page);
    const mid0 = await read(page);
    await page.keyboard.press('b'); await page.waitForTimeout(220);
    const mid = await read(page);
    await page.mouse.move(VW / 2, VH * 0.4); await page.waitForTimeout(60);
    await page.mouse.up(); await page.waitForTimeout(350);
    const a = await read(page);
    out.push({ id: 'H3', mid0Ghost: mid0.ghost, midGhost: mid.ghost, after: a });
    console.log('H3 ghost', mid0.ghost, '→', mid.ghost, '→', a.ghost, 'casts', a.casts.length, JSON.stringify(a.slots));
    a.casts.forEach((c, i) => console.log('   #' + i, c.i, JSON.stringify(c.cell), '|', c.stack));
    await shot(page, 'drag-panel');
    await page.close();
  }

  /* H4 世界单击一次 */
  {
    const page = await open();
    await page.mouse.move(VW / 2, VH * 0.4); await page.waitForTimeout(80);
    await page.mouse.down(); await page.waitForTimeout(40); await page.mouse.up(); await page.waitForTimeout(350);
    const a = await read(page);
    out.push({ id: 'H4', after: a });
    console.log('H4 clicks casts', a.casts.length, JSON.stringify(a.slots));
    a.casts.forEach((c, i) => console.log('   #' + i, c.i, JSON.stringify(c.cell), '|', c.stack));
    await page.close();
  }

  fs.writeFileSync(path.join(__dirname, 'state-probe4.json'), JSON.stringify(out, null, 1));
  await browser.close();
})();
