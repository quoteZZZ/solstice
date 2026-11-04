/* 键盘细项 v2：原生按钮 Enter/Space、面板焦点、手牌焦点牌 vs 选中牌 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const argv = process.argv.slice(2);
const arg = (k, d) => { const e = argv.find(a => a.startsWith('--' + k + '=')); if (e) return e.slice(k.length + 3); const i = argv.indexOf('--' + k); return i >= 0 && argv[i + 1] && !argv[i + 1].startsWith('--') ? argv[i + 1] : d; };
const VW = +arg('w', 1080), VH = +arg('h', 700);
const OUT = path.resolve(arg('out', path.join(__dirname, 'keyboard-probe3-' + VW + 'x' + VH + '.json')));

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

  // 原生档位按钮：Enter / Space 是否真的换档
  log.gearButtons = {};
  for (const [txt, key] of [['0.5×', 'Enter'], ['2×', ' ']]) {
    await page.evaluate(t => { const b = [...document.querySelectorAll('#gears .btn')].find(x => x.innerText === t); b.focus(); }, txt);
    const before = await page.evaluate(() => window.__dbg.S.gear);
    await page.keyboard.press(key === ' ' ? 'Space' : 'Enter');
    await page.waitForTimeout(300);
    log.gearButtons[txt] = { key, before, after: await page.evaluate(() => window.__dbg.S.gear), active: await page.evaluate(() => document.activeElement.innerText) };
  }

  // 用 B 键打开面板（唯一可靠入口），看焦点
  await page.evaluate(() => document.activeElement.blur());
  await page.keyboard.press('b');
  await page.waitForTimeout(400);
  log.panel = await page.evaluate(() => ({
    open: window.__dbg.UI.state.panel,
    active: document.activeElement.tagName + '.' + document.activeElement.className,
    inPanel: !!document.activeElement.closest('#panel')
  }));
  const reached = [];
  for (let i = 0; i < 14; i++) {
    await page.keyboard.press('Tab'); await page.waitForTimeout(45);
    reached.push(await page.evaluate(() => {
      const a = document.activeElement;
      return { text: (a.innerText || '').replace(/\s+/g, ' ').slice(0, 12), inPanel: !!a.closest('#panel'), cls: a.className };
    }));
  }
  log.panel.tab14 = reached;
  log.panel.firstInPanelIndex = reached.findIndex(r => r.inPanel);
  log.panel.backgroundTabsBeforePanel = reached.slice(0, Math.max(0, reached.findIndex(r => r.inPanel))).length;
  // 面板开着时，背景的档位按钮按 Enter 会不会生效
  await page.keyboard.press('Escape'); await page.waitForTimeout(200);
  log.panel.escClosed = await page.evaluate(() => !window.__dbg.UI.state.panel);

  // 面板开着时点击/键控背景（用 Tab 走到背景档位，再按 Enter）
  await page.keyboard.press('b'); await page.waitForTimeout(300);
  await page.evaluate(() => document.activeElement.blur());
  for (let i = 0; i < 2; i++) await page.keyboard.press('Tab');
  const bgBefore = await page.evaluate(() => ({ gear: window.__dbg.S.gear, active: document.activeElement.innerText }));
  await page.keyboard.press('Enter'); await page.waitForTimeout(250);
  log.backgroundActionWhilePanelOpen = { before: bgBefore, afterGear: await page.evaluate(() => window.__dbg.S.gear), panel: await page.evaluate(() => window.__dbg.UI.state.panel) };
  await page.keyboard.press('Escape');

  // 手牌：选中太阳（1），焦点第 5 张（火），空格/Enter 打的是哪张
  await page.evaluate(() => { window.__dbg.S.W.essence = 999; window.__dbg.UI.select(1); });
  await page.waitForTimeout(150);
  await page.evaluate(() => { document.querySelectorAll('.handslot')[4].focus(); });
  log.focusedVsSelected = await page.evaluate(() => ({
    sel: window.__dbg.S.W.sel, selName: window.__dbg.S.W.hand[window.__dbg.S.W.sel].name.zh,
    focused: 4, focusedName: window.__dbg.S.W.hand[4].name.zh,
    hand: window.__dbg.S.W.hand.map(c => c.name.zh)
  }));
  await page.keyboard.press('Space'); await page.waitForTimeout(500);
  log.focusedVsSelected.afterSpace = await page.evaluate(() => ({
    sel: window.__dbg.S.W.sel, selName: window.__dbg.S.W.hand[window.__dbg.S.W.sel].name.zh,
    hand: window.__dbg.S.W.hand.map(c => c.name.zh),
    toast: [...document.querySelectorAll('.toast .t')].map(t => t.innerText),
    sun: window.__dbg.S.W.sun
  }));

  // 右栏「年表 ▸」是否可达：tab 到 .rp.grow 后 Enter
  await page.evaluate(() => { document.activeElement.blur(); document.querySelector('.rp.grow').scrollTop = 0; });
  await page.keyboard.press('Tab'); await page.keyboard.press('Tab'); await page.keyboard.press('Tab');
  await page.keyboard.press('Tab'); await page.keyboard.press('Tab'); await page.keyboard.press('Tab');
  log.rightPanelFocus = await page.evaluate(() => ({ active: document.activeElement.className, text: (document.activeElement.innerText || '').slice(0, 20), panel: window.__dbg.UI.state.panel }));
  await page.keyboard.press('Enter'); await page.waitForTimeout(300);
  log.rightPanelFocus.afterEnter = await page.evaluate(() => ({ panel: window.__dbg.UI.state.panel }));

  fs.writeFileSync(OUT, JSON.stringify(log, null, 1));
  console.log(JSON.stringify(log, null, 1).slice(0, 5000));
  await browser.close();
})();
