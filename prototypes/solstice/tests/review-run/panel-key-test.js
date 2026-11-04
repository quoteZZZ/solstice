/* 面板内 Enter/Space 激活（只读，当前工作树） */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

(async () => {
  const OUT = path.resolve(path.join(__dirname, 'narrow-shots', 'kb-1080', 'panel-keys.json'));
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1080, height: 700 }, deviceScaleFactor: 1 });
  await page.goto(pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href);
  await page.waitForTimeout(1400);
  await page.evaluate(() => {
    const { S, UI } = window.__dbg;
    UI.enterFromCreator(); S.W.essence = 999; UI.updateHud();
  });
  await page.waitForTimeout(300);
  await page.keyboard.press('l');
  await page.waitForTimeout(400);

  const state = () => page.evaluate(() => ({
    panel: window.__dbg.UI.state.panel,
    tab: window.__dbg.UI.state.codexTab,
    sel: window.__dbg.S.W.sel,
    hand: window.__dbg.S.W.hand.map(c => c.name.zh),
    sun: window.__dbg.S.W.sun,
    cells: window.__dbg.S.W.cells.size,
    active: (document.activeElement.innerText || '').replace(/\s+/g, ' ').slice(0, 24),
    inPanel: !!document.activeElement.closest('#panel'),
    focusVisible: document.activeElement.matches(':focus-visible')
  }));
  const focus = txt => page.evaluate(t => {
    const b = [...document.querySelectorAll('#panel button')].find(x => x.innerText.startsWith(t));
    if (b) b.focus();
    return !!b;
  }, txt);

  const log = { viewport: [1080, 700], base: await state(), tests: [] };

  await focus('图鉴'); await page.waitForTimeout(100);
  await page.keyboard.press('Enter'); await page.waitForTimeout(500);
  log.tests.push({ key: 'Enter on 图鉴', after: await state() });

  await page.evaluate(() => { window.__dbg.UI.state.codexTab = 'cards'; window.__dbg.UI.renderOverlay(); });
  await page.waitForTimeout(200);
  await focus('图鉴'); await page.waitForTimeout(100);
  // 选中一张全局卡（太阳/律令），保证“Space 穿到世界”时有可观察的后果
  await page.evaluate(() => {
    const { S, UI } = window.__dbg;
    const i = S.W.hand.findIndex(c => c.sim.op === 'sun' || c.sim.op === 'law');
    if (i >= 0) UI.select(i);
  });
  await page.waitForTimeout(150);
  const beforeSpace = await state();
  await page.keyboard.press('Space'); await page.waitForTimeout(500);
  log.tests.push({ key: 'Space on 图鉴', before: beforeSpace, after: await state() });
  await page.screenshot({ path: path.resolve(path.join(__dirname, 'narrow-shots', 'panel-space-cast.png')) });

  await page.evaluate(() => { window.__dbg.UI.state.codexTab = 'cards'; window.__dbg.UI.renderOverlay(); });
  await page.waitForTimeout(200);
  await focus('关闭'); await page.waitForTimeout(100);
  await page.keyboard.press('Space'); await page.waitForTimeout(400);
  log.tests.push({ key: 'Space on 关闭', after: await state() });

  // 面板内 K/H
  await page.keyboard.press('k'); await page.waitForTimeout(300);
  log.tests.push({ key: 'K in panel', after: await page.evaluate(() => ({ panel: window.__dbg.UI.state.panel, group: window.__dbg.UI.state.setGroup })) });
  await page.keyboard.press('h'); await page.waitForTimeout(300);
  log.tests.push({ key: 'H in panel', after: await page.evaluate(() => ({ panel: window.__dbg.UI.state.panel, group: window.__dbg.UI.state.setGroup })) });

  fs.writeFileSync(OUT, JSON.stringify(log, null, 1));
  console.log(JSON.stringify(log, null, 1));
  await browser.close();
})();
