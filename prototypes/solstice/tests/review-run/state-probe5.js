/* 探针 5：拖放的一次手势两次施卡（成功路径）/ 事件跨世界 / 撤销 toast 被清 */
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
  async function open(fn) {
    const page = await browser.newPage({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
    const errs = []; page.on('pageerror', e => errs.push(e.message));
    await page.goto(PAGE); await page.waitForTimeout(1100);
    await page.evaluate(fn);
    await page.waitForTimeout(150);
    return { page, errs };
  }
  let n = 0;
  const shot = async (p, tag) => { n++; const f = 'p5-' + n + '-' + tag + '.png'; await p.waitForTimeout(300); await p.screenshot({ path: path.join(OUT, f) }); return f; };

  /* I1 空世界上拖一张土地到虚空：一次拖放打了几次 */
  {
    const { page, errs } = await open(() => {
      const { S, D, UI } = window.__dbg;
      UI.enterFromCreator();
      S.W.cells.clear(); S.W.cellSeq = 0;
      const land = D.byId.get('terrain.earth.0.matter.land');
      S.W.hand[0] = land; S.W.sel = 0;
      S.W.essence = 500; S.setGear(0);
      UI.syncGears(); UI.renderHand(); UI.updateHud(); UI.renderRight();
      window.__slots = []; window.__toasts = [];
      const o = S.cmd.playSlot;
      S.cmd.playSlot = function (i, x, y) { const r = o.apply(this, arguments); window.__slots.push({ i, at: [x, y], ok: r.ok, msg: r.msg || '', card: (S.W.hand[i] || {}).id }); return r; };
    });
    const r = await page.evaluate(() => { const b = document.querySelectorAll('.handslot')[0].getBoundingClientRect(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; });
    await page.mouse.move(r.x, r.y); await page.mouse.down();
    for (let i = 1; i <= 4; i++) { await page.mouse.move(r.x, r.y - i * 55); await page.waitForTimeout(25); }
    await page.mouse.move(VW / 2, VH * 0.33); await page.waitForTimeout(80);
    await page.mouse.up(); await page.waitForTimeout(500);
    const a = await page.evaluate(() => ({ slots: window.__slots, cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.map(c => c.id), toasts: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.trim()) }));
    out.push({ id: 'I1', a, errs });
    console.log('I1', JSON.stringify(a.slots), 'cells', a.cells, 'toasts', JSON.stringify(a.toasts));
    await shot(page, 'drag-void');
    await page.close();
  }

  /* I2 事件 → 创世 → 进新区 → 承受：事件后果是否写进新世界 */
  {
    const { page, errs } = await open(() => {
      const { S, D, UI } = window.__dbg;
      UI.enterFromCreator();
      const land = D.byId.get('terrain.earth.0.matter.land');
      for (let x = -1; x <= 1; x++) for (let y = -1; y <= 1; y++) S.cmd.cast(land, x, y);
      S.W.essence = 500; S.setGear(2);
      UI.syncGears(); UI.updateHud(); UI.renderHand();
    });
    await page.keyboard.press('e'); await page.waitForTimeout(220);
    const e0 = await page.evaluate(() => window.__dbg.UI.state.event && window.__dbg.UI.state.event.t);
    await page.keyboard.press('g'); await page.waitForTimeout(300);
    await page.keyboard.press('Enter'); await page.waitForTimeout(250);
    await page.keyboard.press('Enter'); await page.waitForTimeout(350);
    const w = await page.evaluate(() => ({ month: window.__dbg.S.W.month, cells: window.__dbg.S.W.cells.size, chron: window.__dbg.S.W.chron.length, event: !!window.__dbg.UI.state.event }));
    await page.evaluate(() => { const b = document.querySelector('[data-ev="bear"]'); if (b) b.click(); });
    await page.waitForTimeout(300);
    const after = await page.evaluate(() => ({ month: window.__dbg.S.W.month, cells: window.__dbg.S.W.cells.size, chron: window.__dbg.S.W.chron.map(c => c.month + '|' + c.kind + '|' + c.text), event: !!window.__dbg.UI.state.event }));
    out.push({ id: 'I2', e0, w, after, errs });
    console.log('I2', JSON.stringify({ e0, w, after }));
    await shot(page, 'event-new-world-bear');
    await page.close();
  }

  /* I3 回收后可撤销 → 开面板：撤销入口是否消失 */
  {
    const { page, errs } = await open(() => {
      const { S, UI } = window.__dbg;
      UI.enterFromCreator();
      S.W.essence = 0;
      const i = S.W.hand.findIndex(c => c.tier > 0);
      S.W.sel = i >= 0 ? i : 0;
      UI.renderHand();
    });
    const before = await page.evaluate(() => {
      const { UI, S } = window.__dbg;
      UI.recycle();
      return { essence: S.W.essence, toast: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.trim()), hasUndo: !!document.querySelector('.toact') };
    });
    await page.keyboard.press('l'); await page.waitForTimeout(250);
    const after = await page.evaluate(() => ({ toast: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.trim()), canUndo: !!document.querySelector('.toact') }));
    out.push({ id: 'I3', before, after, errs });
    console.log('I3', JSON.stringify({ before, after }));
    await shot(page, 'undo-cleared');
    await page.close();
  }

  /* I4 4× 连施 + 月份推进 */
  {
    const { page, errs } = await open(() => {
      const { S, D, UI } = window.__dbg;
      UI.enterFromCreator();
      const land = D.byId.get('terrain.earth.0.matter.land');
      for (let x = -1; x <= 1; x++) for (let y = -1; y <= 1; y++) S.cmd.cast(land, x, y);
      S.W.essence = 500; S.setGear(4);
      UI.syncGears(); UI.updateHud(); UI.renderHand();
    });
    const a = await page.evaluate(() => ({ month: window.__dbg.S.W.month, gear: window.__dbg.S.gear }));
    await page.evaluate(() => { for (let k = 0; k < 8; k++) { const S = window.__dbg.S; const i = S.W.hand.length ? k % S.W.hand.length : -1; if (i >= 0) S.cmd.playSlot(i, 4 + (k % 3), 4); } });
    await page.waitForTimeout(1400);
    const b = await page.evaluate(() => ({ month: window.__dbg.S.W.month, gear: window.__dbg.S.gear, cells: window.__dbg.S.W.cells.size }));
    out.push({ id: 'I4', a, b, errs });
    console.log('I4', JSON.stringify({ a, b, errs }));
    await page.close();
  }

  fs.writeFileSync(path.join(__dirname, 'state-probe5.json'), JSON.stringify(out, null, 1));
  await browser.close();
})();
