/* Solstice · 状态机审计探针 3：拖拽手势与功能键的交叉 / 模式恢复 / 事件携带进新世界 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'state-shots');
const VW = 1680, VH = 1000;
const results = [];
const log = console.log;

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  async function open(o) {
    const page = await browser.newPage({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
    await page.goto(PAGE);
    await page.waitForTimeout(1100);
    await page.evaluate(o2 => {
      const { S, D, UI } = window.__dbg;
      UI.enterFromCreator();
      const land = D.byId.get('terrain.earth.0.matter.land');
      for (let x = -2; x <= 2; x++) for (let y = -2; y <= 2; y++) S.cmd.cast(land, x, y);
      S.W.essence = 500; S.setGear(2);
      UI.syncGears(); UI.updateHud(); UI.renderHand(); UI.renderRight();
      window.__casts = [];
      const orig = UI.cast;
      UI.cast = function (i, cell, pt) { window.__casts.push(W_card(i)); return orig.apply(this, arguments); };
      function W_card(i) { const c = S.W.hand[i]; return c ? c.id : '#' + i; }
    }, o || {});
    await page.waitForTimeout(150);
    return page;
  }
  const snap = p => p.evaluate(() => {
    const { S, UI, R } = window.__dbg;
    return { casts: window.__casts.slice(), hand: S.W.hand.map(c => c.id), sel: S.W.sel, cells: S.W.cells.size,
      panel: UI.state.panel, event: !!UI.state.event, uiMode: UI.state.uiMode, gear: S.gear,
      creator: document.getElementById('creator').classList.contains('on'),
      ghost: document.querySelectorAll('#ghost').length, month: S.W.month, essence: S.W.essence,
      cursor: R.cam.hover ? [R.cam.hover.x, R.cam.hover.y] : null };
  });
  let n = 0;
  const shot = async (p, tag) => { n++; const f = 'p3-' + String(n).padStart(2, '0') + '-' + tag + '.png'; await p.waitForTimeout(250); await p.screenshot({ path: path.join(OUT, f) }); return f; };

  async function drag(page, dx, dy) {
    const r = await page.evaluate(() => { const b = document.querySelectorAll('.handslot')[0].getBoundingClientRect(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; });
    await page.mouse.move(r.x, r.y);
    await page.mouse.down();
    for (let i = 1; i <= 4; i++) { await page.mouse.move(r.x + (dx || 0) * i / 4, r.y + (dy || -220) * i / 4); await page.waitForTimeout(25); }
    return r;
  }

  /* G1 拖放（不按键）：是否只施一张 */
  {
    const page = await open({});
    await drag(page, 0, -220);
    await page.mouse.move(VW / 2, VH * 0.4); await page.waitForTimeout(60);
    await page.mouse.up(); await page.waitForTimeout(300);
    const s = await snap(page);
    results.push({ id: 'G1', title: '拖放一次施几张', casts: s.casts, ghost: s.ghost });
    log('G1', JSON.stringify(s.casts));
    await page.close();
  }

  /* G2 拖拽中按 Space 再松手 */
  {
    const page = await open({});
    await drag(page, 0, -220);
    await page.keyboard.press('Space'); await page.waitForTimeout(220);
    const mid = await snap(page);
    await page.mouse.move(VW / 2, VH * 0.4); await page.waitForTimeout(60);
    await page.mouse.up(); await page.waitForTimeout(320);
    const s = await snap(page);
    await shot(page, 'drag-space');
    results.push({ id: 'G2', title: '拖拽中 Space → 松手', castsMid: mid.casts, castsEnd: s.casts, ghost: s.ghost, handSel: s.sel });
    log('G2', JSON.stringify({ mid: mid.casts, end: s.casts }));
    await page.close();
  }

  /* G3 拖拽中按 X（回收）再松手 */
  {
    const page = await open({});
    await drag(page, 0, -220);
    await page.keyboard.press('x'); await page.waitForTimeout(200);
    const mid = await snap(page);
    await page.mouse.move(VW / 2, VH * 0.4); await page.waitForTimeout(60);
    await page.mouse.up(); await page.waitForTimeout(320);
    const s = await snap(page);
    results.push({ id: 'G3', title: '拖拽中 X → 松手', casts: s.casts, handBefore: mid.hand, handAfter: s.hand, sel: s.sel });
    log('G3', JSON.stringify({ casts: s.casts, sel: s.sel }));
    await page.close();
  }

  /* G4 拖拽中按 B（面板）再松手 */
  {
    const page = await open({});
    await drag(page, 0, -220);
    await page.keyboard.press('b'); await page.waitForTimeout(220);
    const mid = await snap(page);
    await page.mouse.move(VW / 2, VH * 0.4); await page.waitForTimeout(60);
    await page.mouse.up(); await page.waitForTimeout(320);
    const s = await snap(page);
    results.push({ id: 'G4', title: '拖拽中 B → 松手（面板下施卡）', panel: mid.panel, casts: s.casts, ghost: s.ghost });
    log('G4', JSON.stringify({ panel: mid.panel, casts: s.casts }));
    await page.close();
  }

  /* G5 拖拽中 Esc（应取消，不施卡） */
  {
    const page = await open({});
    await drag(page, 0, -220);
    const before = await snap(page);
    await page.keyboard.press('Escape'); await page.waitForTimeout(220);
    await page.mouse.move(VW / 2, VH * 0.4); await page.mouse.up(); await page.waitForTimeout(300);
    const s = await snap(page);
    results.push({ id: 'G5', title: '拖拽中 Esc', castsBefore: before.casts, castsAfter: s.casts, ghost: s.ghost });
    log('G5', JSON.stringify(s.casts));
    await page.close();
  }

  /* G6 仅右栏 → P → P 能否回到仅右栏 */
  {
    const page = await open({});
    const seq = await page.evaluate(() => {
      const UI = window.__dbg.UI;
      UI.setUiMode('仅右栏'); const a = UI.state.uiMode;
      UI.toggleAmbient(); const b = UI.state.uiMode;
      UI.toggleAmbient(); const c = UI.state.uiMode;
      return [a, b, c];
    });
    results.push({ id: 'G6', title: '仅右栏 → P → P', seq });
    log('G6', JSON.stringify(seq));
    await page.close();
  }

  /* G7 事件开着 → G → 连按两次 Enter 进新区：事件是否跟过去 */
  {
    const page = await open({});
    await page.keyboard.press('e'); await page.waitForTimeout(220);
    const e0 = await snap(page);
    await page.keyboard.press('g'); await page.waitForTimeout(300);
    const g1 = await snap(page);
    await page.keyboard.press('Enter'); await page.waitForTimeout(250);
    const a1 = await snap(page);
    await page.keyboard.press('Enter'); await page.waitForTimeout(350);
    const w = await snap(page);
    await page.keyboard.press('Escape'); await page.waitForTimeout(250);
    const esc = await snap(page);
    await shot(page, 'event-new-world');
    results.push({ id: 'G7', title: '事件 → G → Enter×2', event0: e0.event, creator: g1.creator, armed: a1.creator, month: w.month, cells: w.cells, eventAfterNewWorld: w.event, creatorAfter: w.creator, eventAfterEsc: esc.event });
    log('G7', JSON.stringify({ month: w.month, cells: w.cells, event: w.event, creator: w.creator }));
    await page.close();
  }

  /* G8 撤销 toast 被开面板清空 */
  {
    const page = await open({});
    const before = await page.evaluate(() => {
      const { UI, S } = window.__dbg;
      S.W.sel = 1;
      UI.recycle();
      return { toasts: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.trim()), hand: S.W.hand.length };
    });
    await page.keyboard.press('l'); await page.waitForTimeout(200);
    const after = await page.evaluate(() => ({ toasts: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.trim()) }));
    await shot(page, 'undo-cleared');
    results.push({ id: 'G8', title: '回收撤销 toast → 开面板', before, after });
    log('G8', JSON.stringify({ before, after }));
    await page.close();
  }

  fs.writeFileSync(path.join(__dirname, 'state-probe3.json'), JSON.stringify(results, null, 1));
  await browser.close();
})();
