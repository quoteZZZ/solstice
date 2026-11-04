/* 键盘细项：手牌焦点与 Space/Enter、选中跟随、抽包可达、面板焦点、Esc */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const argv = process.argv.slice(2);
const arg = (k, d) => { const e = argv.find(a => a.startsWith('--' + k + '=')); if (e) return e.slice(k.length + 3); const i = argv.indexOf('--' + k); return i >= 0 && argv[i + 1] && !argv[i + 1].startsWith('--') ? argv[i + 1] : d; };
const VW = +arg('w', 1080), VH = +arg('h', 700);
const OUT = path.resolve(arg('out', path.join(__dirname, 'keyboard-probe-' + VW + 'x' + VH + '.json')));

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
  const log = { viewport: [VW, VH] };
  await page.goto(PAGE); await page.waitForTimeout(1400);
  await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    UI.enterFromCreator();
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -3; x <= 3; x++) for (let y = -3; y <= 3; y++) S.cmd.cast(land, x, y);
    S.W.essence = 999;
    UI.updateHud(); UI.renderHand(); UI.renderRight();
  });
  await page.waitForTimeout(400);

  // 焦点是否跟随 Tab 移动、sel 是否跟随
  log.tabSel = await page.evaluate(async () => {
    const slots = [...document.querySelectorAll('.handslot')];
    slots[0].focus();
    const out = [];
    for (let i = 0; i < 5; i++) {
      const a = document.activeElement;
      out.push({ active: a.dataset.i, sel: window.__dbg.S.W.sel, fv: a.matches(':focus-visible') });
      // 用真实 Tab 键前进
      window.__tabNext = true;
      break;
    }
    return out;
  });
  await page.keyboard.press('Tab');
  await page.keyboard.press('Tab');
  log.tabSel2 = await page.evaluate(() => ({
    active: document.activeElement.dataset.i, sel: window.__dbg.S.W.sel,
    focusCard: (document.activeElement.innerText || '').split('\n')[1],
    selCard: (document.querySelector('.handslot.sel') || {}).innerText
  }));

  // Space on focused card (5th) — 是否施的是焦点牌
  log.space = await page.evaluate(() => {
    const slots = [...document.querySelectorAll('.handslot')];
    const target = 4;
    slots[target].focus();
    window.__dbg.S.W.essence = 999;
    return {
      focused: target,
      focusedName: (slots[target].innerText || '').split('\n')[1],
      selBefore: window.__dbg.S.W.sel,
      selName: (slots[window.__dbg.S.W.sel].innerText || '').split('\n')[1],
      cellsBefore: window.__dbg.S.W.cells.size,
      handBefore: window.__dbg.S.W.hand.map(c => c.name.zh)
    };
  });
  await page.keyboard.press(' ');
  await page.waitForTimeout(500);
  log.space.after = await page.evaluate(() => ({
    sel: window.__dbg.S.W.sel, cells: window.__dbg.S.W.cells.size,
    hand: window.__dbg.S.W.hand.map(c => c.name.zh),
    toast: (document.querySelector('.toast') || {}).innerText,
    focused: document.activeElement.dataset && document.activeElement.dataset.i
  }));

  // Enter on focused card（游标存在时）
  await page.keyboard.press('w'); await page.waitForTimeout(250);
  const before = await page.evaluate(() => ({ cells: window.__dbg.S.W.cells.size, sel: window.__dbg.S.W.sel }));
  await page.keyboard.press('Enter'); await page.waitForTimeout(400);
  log.enterOnHand = { before, after: await page.evaluate(() => ({ cells: window.__dbg.S.W.cells.size, sel: window.__dbg.S.W.sel, toast: (document.querySelector('.toast') || {}).innerText })) };

  // 空格/Enter 在暂停按钮上是否照常
  // 抽包按钮可用时是否进入 Tab 序
  log.packTab = await page.evaluate(async () => {
    window.__dbg.S.W.essence = 999; window.__dbg.UI.updateHud();
    document.getElementById('bPack').focus();
    return { disabled: document.getElementById('bPack').disabled, text: document.getElementById('bPack').innerText, canFocus: document.activeElement.id };
  });

  // 面板焦点：从抽包按钮按 Enter 打开
  await page.keyboard.press('Enter');
  await page.waitForTimeout(400);
  log.panel = await page.evaluate(() => ({
    open: window.__dbg.UI.state.panel,
    active: document.activeElement.id || document.activeElement.className,
    inPanel: !!document.activeElement.closest('#panel')
  }));
  const reached = [];
  for (let i = 0; i < 12; i++) {
    await page.keyboard.press('Tab');
    await page.waitForTimeout(40);
    reached.push(await page.evaluate(() => {
      const a = document.activeElement;
      return { text: (a.innerText || '').replace(/\s+/g, ' ').slice(0, 14), inPanel: !!a.closest('#panel'), cls: a.className };
    }));
  }
  log.panel.tab12 = reached;
  log.panel.backgroundTabs = reached.filter(r => !r.inPanel).length;

  // 面板内 Esc
  await page.keyboard.press('Escape'); await page.waitForTimeout(250);
  log.panel.escClosed = await page.evaluate(() => !window.__dbg.UI.state.panel);

  // 手牌锁定时（焦点在键位改绑中）Esc 行为
  log.rebindEsc = await page.evaluate(async () => {
    window.__dbg.UI.openPanel('settings'); window.__dbg.UI.state.setGroup = 'keys'; window.__dbg.UI.renderOverlay();
    const b = document.querySelector('[data-k="random"]'); if (b) b.click();
    return { waiting: true, panel: window.__dbg.UI.state.panel };
  });
  await page.keyboard.press('Escape'); await page.waitForTimeout(150);
  log.rebindEsc.afterOneEsc = await page.evaluate(() => ({ panel: window.__dbg.UI.state.panel, waitingText: document.querySelector('[data-k="random"]') && document.querySelector('[data-k="random"]').innerText }));
  await page.keyboard.press('Escape'); await page.waitForTimeout(150);
  log.rebindEsc.afterTwoEsc = await page.evaluate(() => ({ panel: window.__dbg.UI.state.panel }));

  // 设置→键位 滚动与截断
  await page.evaluate(() => { window.__dbg.UI.openPanel('settings'); window.__dbg.UI.state.setGroup = 'keys'; window.__dbg.UI.renderOverlay(); });
  await page.waitForTimeout(250);
  log.keysPanel = await page.evaluate(() => {
    const pb = document.getElementById('pbody');
    const sc = [...pb.querySelectorAll('*')].filter(el => {
      const cs = getComputedStyle(el);
      return (cs.overflowY === 'auto' || cs.overflowY === 'scroll');
    }).map(el => ({ cls: el.className, h: Math.round(el.getBoundingClientRect().height), sh: el.scrollHeight, ch: el.clientHeight, over: el.scrollHeight - el.clientHeight }));
    const rows = [...document.querySelectorAll('.keyRow')].map(r => { const b = r.getBoundingClientRect(); return { text: (r.innerText || '').replace(/\s+/g, ' ').slice(0, 16), y: Math.round(b.y), b: Math.round(b.bottom), w: Math.round(b.width) }; });
    const nb = [...document.querySelectorAll('.keyRow .btn')].map(b => { const r = b.getBoundingClientRect(); return { w: Math.round(r.width), h: Math.round(r.height) }; });
    return { scrollers: sc, rows: rows.length, firstRow: rows[0], lastRow: rows[rows.length - 1], rowBtnSizes: nb.slice(0, 3), panelH: Math.round(document.getElementById('panel').getBoundingClientRect().height) };
  });

  fs.writeFileSync(OUT, JSON.stringify(log, null, 1));
  console.log(JSON.stringify(log, null, 1).slice(0, 4000));
  await browser.close();
})();
