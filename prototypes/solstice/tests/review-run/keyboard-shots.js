/* 键盘路径取证：焦点环 / 游标 / 手牌键操作 / 面板焦点（只读） */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const argv = process.argv.slice(2);
const arg = (k, d) => { const e = argv.find(a => a.startsWith('--' + k + '=')); if (e) return e.slice(k.length + 3); const i = argv.indexOf('--' + k); return i >= 0 && argv[i + 1] && !argv[i + 1].startsWith('--') ? argv[i + 1] : d; };
const VW = +arg('w', 1080), VH = +arg('h', 700);
const OUT = path.resolve(arg('out', path.join(__dirname, 'keyboard-shots-' + VW + 'x' + VH)));
fs.mkdirSync(OUT, { recursive: true });

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
  const log = { viewport: [VW, VH], steps: [], errors: [] };
  page.on('pageerror', e => log.errors.push(e.message));
  await page.goto(PAGE);
  await page.waitForTimeout(1500);
  await page.evaluate(() => {
    window.__dbg.UI.enterFromCreator();
    window.__dbg.UI.updateHud(); window.__dbg.UI.renderHand(); window.__dbg.UI.renderRight();
  });
  await page.waitForTimeout(400);
  const snap = async (name, note) => {
    await page.waitForTimeout(250);
    const st = await page.evaluate(() => {
      const a = document.activeElement;
      const ar = a && a !== document.body ? a.getBoundingClientRect() : null;
      const hi = document.getElementById('hoverinfo');
      const cur = window.__dbg.W.sel;
      return {
        active: a ? { tag: a.tagName, cls: a.className, text: (a.innerText || '').replace(/\s+/g, ' ').slice(0, 20), rect: ar && [Math.round(ar.x), Math.round(ar.y), Math.round(ar.width), Math.round(ar.height)] } : null,
        focusVisible: a && a.matches && a.matches(':focus-visible'),
        sel: cur,
        hover: hi ? { display: getComputedStyle(hi).display, text: hi.innerText, rect: (() => { const r = hi.getBoundingClientRect(); return [Math.round(r.x), Math.round(r.y), Math.round(r.width), Math.round(r.height)]; })() } : null,
        selSlot: (() => { const s = document.querySelector('.handslot.sel'); const r = s && s.getBoundingClientRect(); return r && [Math.round(r.x), Math.round(r.y)]; })(),
        panel: window.__dbg.UI.state.panel,
        cells: window.__dbg.S.W.cells.size,
        essence: window.__dbg.S.W.essence
      };
    });
    await page.screenshot({ path: path.join(OUT, name + '.png') });
    log.steps.push({ name, note, state: st });
    console.log('shot', name, JSON.stringify(st.active && st.active.text));
  };

  // 1. 空世界 → Tab 到第 4 张手牌（12 次：5 档位 + 5 左栏 + 手牌）
  for (let i = 0; i < 14; i++) await page.keyboard.press('Tab');
  await snap('01-tab-hand', 'Tab 到手牌后的焦点环');

  // 2. 空世界 WASD 游标
  await page.evaluate(() => document.activeElement.blur());
  for (let i = 0; i < 4; i++) { await page.keyboard.press('d'); await page.waitForTimeout(90); }
  await snap('02-cursor-empty', '空世界上按 4 次 D 后的游标');

  // 3. 造一片地，再按 WASD，看读数
  await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -3; x <= 3; x++) for (let y = -3; y <= 3; y++) S.cmd.cast(land, x, y);
    UI.updateHud(); UI.renderRight(); UI.renderHand();
  });
  await page.waitForTimeout(300);
  for (let i = 0; i < 3; i++) { await page.keyboard.press('w'); await page.waitForTimeout(120); }
  await snap('03-cursor-land', '有地块后 WASD：游标与读数');

  // 4. 手牌焦点 + 空格（看施的是焦点牌还是选中牌）
  await page.evaluate(() => document.activeElement.blur());
  for (let i = 0; i < 14; i++) await page.keyboard.press('Tab');
  const before = await page.evaluate(() => ({ sel: window.__dbg.S.W.sel, activeName: (document.activeElement.innerText || '').split('\n')[1], hand: window.__dbg.S.W.hand.map(c => c.name.zh) }));
  await page.keyboard.press(' ');
  await page.waitForTimeout(400);
  const after = await page.evaluate(() => ({ sel: window.__dbg.S.W.sel, cells: window.__dbg.S.W.cells.size, toast: (document.querySelector('.toast') || {}).innerText, hand: window.__dbg.S.W.hand.map(c => c.name.zh) }));
  log.steps.push({ name: '04-space-on-focused-card', note: '焦点在第 5 张时按空格', before, after });
  await snap('04-space-cast', '手牌焦点牌按空格');

  // 5. 打开抽卡面板（B 键），焦点在哪
  await page.keyboard.press('b');
  await page.waitForTimeout(400);
  await snap('05-panel-open-focus', 'B 打开抽卡面板后的焦点');
  await page.keyboard.press('Tab');
  await page.keyboard.press('Tab');
  await snap('06-panel-tab2', '面板打开后 Tab 两次：焦点是否进面板');

  // 6. Esc 关面板
  await page.keyboard.press('Escape');
  await page.waitForTimeout(250);
  await snap('07-panel-esc', 'Esc 后');

  // 7. 设置→键位（面板内滚动）截图
  await page.evaluate(() => { window.__dbg.UI.openPanel('settings'); window.__dbg.UI.state.setGroup = 'keys'; window.__dbg.UI.renderOverlay(); });
  await page.waitForTimeout(300);
  await snap('08-keys-panel', '键位表底部是否被切');

  fs.writeFileSync(path.join(OUT, 'log.json'), JSON.stringify(log, null, 1));
  await browser.close();
  console.log('done', OUT);
})();
