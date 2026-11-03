/* 至日 · 评审取图
   跑法（Node 18+）：node tests/shots.js
   把所有界面状态各截一张，写给评审用（tests/review/*.png，已 gitignore）。
   不是回归：只看图，不断言。回归在 smoke / cards / det / codex。 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', 'index.html')).href;
const OUT = path.join(__dirname, 'review');

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({
    viewport: { width: 1680, height: 1000 },
    deviceScaleFactor: 2
  });
  const errors = [];
  page.on('console', m => { if (m.type() === 'error') errors.push('[console] ' + m.text()); });
  page.on('pageerror', e => errors.push('[PAGEERROR] ' + e.message));

  const shot = async (name) => {
    await page.waitForTimeout(350);
    await page.screenshot({ path: path.join(OUT, name + '.png') });
    console.log('shot', name);
  };

  await page.goto(PAGE);
  await page.waitForTimeout(2000);

  /* 1. 创世（首屏） */
  await shot('01-creator');

  /* 2. 进界：空世界 + 手牌 + 新手提示 */
  await page.evaluate(() => {
    const { UI, S } = window.__dbg;
    UI.enterWorld();
    S.cmd.gear(0);
    UI.updateHud();
    UI.renderRight();
  });
  await shot('02-empty');

  /* 3. 世界发育：造陆、点太阳、引水、种草种树、放兔子和狼，跑 30 个月 */
  await page.evaluate(() => {
    const { S, UI, D } = window.__dbg;
    const g = id => D.byId.get(id);
    const put = (id, cells) => cells.forEach(([x, y]) => S.cmd.cast(g(id), x, y));
    const land = g('terrain.earth.0.matter.land');
    for (let x = -3; x <= 3; x++) for (let y = -3; y <= 3; y++) S.cmd.cast(land, x, y);
    S.cmd.cast(g('sky.light.0.matter.sun'), 0, 0);
    put('terrain.mineral.0.matter.water', [[0, -1], [1, -1], [2, -1]]);
    put('flora.wood.1.matter.grass', [[1, 0], [2, 0], [3, 0], [-1, 1], [-2, 1], [0, 2]]);
    put('flora.wood.2.matter.tree', [[-1, 0], [-2, 0], [3, 1]]);
    S.cmd.cast(g('fauna.earth.1.matter.rabbit'), 1, 1);
    S.cmd.cast(g('fauna.earth.1.matter.rabbit'), 2, 2);
    S.cmd.cast(g('fauna.dark.2.matter.wolf'), -1, 2);
    S.cmd.cast(g('rite.water.2.rite.rain'), 0, 0);
    for (let i = 0; i < 30; i++) S.cmd.tick(1);
    UI.updateHud();
    UI.renderRight();
    UI.renderHand();
  });
  await shot('03-world');

  /* 4. 手牌特写 */
  const hand = await page.$('#handwrap');
  if (hand) {
    await page.waitForTimeout(200);
    await hand.screenshot({ path: path.join(OUT, '04-hand.png') });
    console.log('shot 04-hand');
  }

  /* 5. 顶栏 + 左右栏单独来一张 */
  await page.evaluate(() => document.querySelector('#leftrail').scrollIntoView());

  /* 6~11. 六个面板 */
  const panels = [
    ['pack', '05-pack'],
    ['codex', '07-codex-cards'],
    ['chron', '08-chron'],
    ['settings', '09-settings'],
    ['keys', '10-keys'],
    ['help', '11-help']
  ];
  for (const [name, file] of panels) {
    await page.evaluate(p => { window.__dbg.UI.openPanel(p); }, name);
    await shot(file);
  }

  /* 7. 卡库第二面：图鉴墙 */
  await page.evaluate(() => {
    const { UI } = window.__dbg;
    UI.openPanel('codex');
    UI.state.codexTab = 'ach';
    UI.renderOverlay();
  });
  await shot('06-codex-ach');

  /* 8. 单卡详情 */
  await page.evaluate(() => {
    const { UI } = window.__dbg;
    UI.closePanel();
    UI.openCard('fauna.water.1.matter.nightmarsh_bird');
  });
  await shot('12-card-detail');

  /* 9. 世界事件 */
  await page.evaluate(() => {
    const { UI } = window.__dbg;
    UI.closePanel();
    UI.fireEvent('灾变');
  });
  await shot('13-event');

  /* 10. 全隐模式：只看世界 */
  await page.evaluate(() => {
    const { UI } = window.__dbg;
    UI.closeEvent();
    UI.setUiMode('全隐');
  });
  await shot('14-ui-hidden');

  console.log('errors', JSON.stringify(errors));
  await browser.close();
  if (errors.length) process.exitCode = 1;
})();
