/* Solstice · 状态机审计探针 2（补洞：面板按键穿透 / 焦点 / 设置读数 / 拖拽重复施卡）
   不改源码；只在页面内挂计数器。 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const argv = process.argv.slice(2);
const arg = (k, d) => {
  const eq = argv.find(a => a.startsWith('--' + k + '='));
  if (eq) return eq.slice(k.length + 3);
  const i = argv.indexOf('--' + k);
  return i >= 0 && argv[i + 1] && !argv[i + 1].startsWith('--') ? argv[i + 1] : d;
};
const VW = +arg('w', 1680), VH = +arg('h', 1000);
const OUT = path.join(__dirname, 'state-shots');
const ONLY = (arg('only', '') || '').split(',').filter(Boolean);
fs.mkdirSync(OUT, { recursive: true });

const SNAP = () => {
  const { S, UI, D, R } = window.__dbg;
  const st = UI.state;
  const el = id => document.getElementById(id);
  return {
    panel: st.panel, event: !!st.event, packStage: UI.packStage, packCards: (UI.packCards || []).length,
    uiMode: st.uiMode, setGroup: st.setGroup,
    hand: S.W.hand.map(c => c.id), sel: S.W.sel, castN: window.__castN || 0,
    essence: S.W.essence, month: S.W.month, gear: S.gear,
    cells: S.W.cells.size, creator: el('creator').classList.contains('on'),
    overlay: el('overlay').classList.contains('show'), eventOn: el('eventwrap').classList.contains('on'),
    cursor: R.cam.hover ? [R.cam.hover.x, R.cam.hover.y] : null,
    ghost: document.querySelectorAll('#ghost').length,
    active: document.activeElement ? (document.activeElement.id || String(document.activeElement.className)) : null,
    waiting: [...document.querySelectorAll('[data-k]')].filter(b => /等待按键/.test(b.textContent)).map(b => b.dataset.k),
    pbodyScroll: (() => { const b = document.getElementById('pbody'); const s = b && b.querySelector('div[style*="overflow:auto"]'); return s ? s.scrollTop : null; })(),
    toasts: [...document.querySelectorAll('#toasts .toast, #phead .toast')].map(t => t.textContent.trim())
  };
};

const results = [];
(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  async function open(o) {
    const page = await browser.newPage({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
    const errs = [];
    page.on('pageerror', e => errs.push('[PAGEERROR] ' + e.message));
    await page.goto(PAGE);
    await page.waitForTimeout(1100);
    await page.evaluate(o2 => {
      const { S, D, UI } = window.__dbg;
      UI.enterFromCreator();
      if (o2.land) {
        const land = D.byId.get('terrain.earth.0.matter.land');
        for (let x = -2; x <= 2; x++) for (let y = -2; y <= 2; y++) S.cmd.cast(land, x, y);
      }
      S.W.essence = o2.essence == null ? 500 : o2.essence;
      S.setGear(o2.gear == null ? 2 : o2.gear);
      UI.syncGears(); UI.updateHud(); UI.renderHand(); UI.renderRight(); UI.renderOverlay();
      /* 施卡计数：包一层 UI.cast，不改源码 */
      if (!window.__castWrapped) {
        const orig = UI.cast;
        window.__castN = 0;
        UI.cast = function () { window.__castN++; return orig.apply(this, arguments); };
        window.__castWrapped = true;
      }
    }, o || {});
    await page.waitForTimeout(150);
    return { page, errs };
  }
  let n = 0;
  async function shot(page, tag) { n++; const f = 'p2-' + String(n).padStart(2, '0') + '-' + tag + '.png'; await page.waitForTimeout(250); await page.screenshot({ path: path.join(OUT, f) }); return f; }
  const snap = p => p.evaluate(SNAP);
  async function rec(id, title, o, run) {
    if (ONLY.length && !ONLY.includes(id)) return;
    let page, errs = [];
    try {
      ({ page, errs } = await open(o));
      const data = await run(page, shot);
      results.push({ id, title, data, errs });
      console.log('◉', id, title, '→', JSON.stringify(data));
    } catch (e) {
      results.push({ id, title, err: String(e && e.stack || e), errs });
      console.log('✗', id, title, e && e.message);
    } finally { if (page) await page.close(); }
  }

  await rec('F1', '面板开着按功能键：Space / X / R / J / E / P 是否穿透到世界', { land: true }, async (page, shot) => {
    await page.evaluate(() => window.__dbg.UI.openPanel('codex'));
    await page.waitForTimeout(120);
    const a = await snap(page);
    const rows = [];
    for (const k of [' ', 'x', 'r', 'j']) {
      const before = await snap(page);
      await page.keyboard.press(k === ' ' ? 'Space' : k);
      await page.waitForTimeout(140);
      const after = await snap(page);
      rows.push({ key: k, castDelta: after.castN - before.castN, gear: after.gear, handChanged: JSON.stringify(after.hand) !== JSON.stringify(before.hand), panel: after.panel, toasts: after.toasts });
    }
    await shot(page, 'panel-keythrough');
    const packStill = await snap(page);
    await page.keyboard.press('e');
    await page.waitForTimeout(200);
    const afterE = await snap(page);
    await shot(page, 'panel-e');
    return { rows, castTotal: packStill.castN, panelBeforeE: packStill.panel, afterE: { panel: afterE.panel, event: afterE.event } };
  });

  await rec('F2', '拖拽中按 Space：是否一次手势施两张', { land: true }, async (page, shot) => {
    const r = await page.evaluate(() => { const b = document.querySelectorAll('.handslot')[0].getBoundingClientRect(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; });
    const a = await snap(page);
    await page.mouse.move(r.x, r.y);
    await page.mouse.down();
    for (let i = 1; i <= 4; i++) { await page.mouse.move(r.x, r.y - i * 50); await page.waitForTimeout(25); }
    const dragStart = await snap(page);
    await page.keyboard.press('Space');
    await page.waitForTimeout(250);
    const afterSpace = await snap(page);
    await page.mouse.move(VW / 2, VH * 0.4);
    await page.waitForTimeout(80);
    await page.mouse.up();
    await page.waitForTimeout(300);
    const afterUp = await snap(page);
    await shot(page, 'drag-space-count');
    return { castBefore: a.castN, dragStart, castAfterSpace: afterSpace.castN, castAfterUp: afterUp.castN, handBefore: a.hand, handAfter: afterUp.hand, cells: [a.cells, afterSpace.cells, afterUp.cells] };
  });

  await rec('F3', '设置→界面→常驻栏：当前值是否高亮正确（UI.uiMode 读错地方）', { land: true }, async (page, shot) => {
    await page.keyboard.press('o');
    await page.waitForTimeout(150);
    await page.evaluate(() => document.querySelector('[data-sg="ui"]').click());
    await page.waitForTimeout(150);
    await page.evaluate(() => document.querySelector('[data-set="ui"][data-val="全隐"]').click());
    await page.waitForTimeout(250);
    const a = await snap(page);
    const on = await page.evaluate(() => [...document.querySelectorAll('[data-set="ui"]')].map(b => b.dataset.val + (b.classList.contains('on') ? '*' : '')));
    await shot(page, 'settings-uibar-on');
    /* 关掉再开 */
    await page.keyboard.press('Escape');
    await page.waitForTimeout(120);
    await page.keyboard.press('Escape');
    await page.waitForTimeout(200);
    const b = await snap(page);
    await page.keyboard.press('o');
    await page.waitForTimeout(150);
    const on2 = await page.evaluate(() => [...document.querySelectorAll('[data-set="ui"]')].map(b => b.dataset.val + (b.classList.contains('on') ? '*' : '')));
    await shot(page, 'settings-uibar-reopen');
    return { modeAfterSet: a.uiMode, highlighted: on, modeAfterReopen: b.uiMode, highlightedReopen: on2 };
  });

  await rec('F4', '设置→界面→常驻栏：全隐态下 P 键能否恢复 / Esc 顺序', { land: true }, async (page, shot) => {
    await page.evaluate(() => window.__dbg.UI.setUiMode('全隐'));
    await page.waitForTimeout(120);
    await page.keyboard.press('p');
    await page.waitForTimeout(300);
    const a = await snap(page);
    await page.keyboard.press('p');
    await page.waitForTimeout(300);
    const b = await snap(page);
    return { afterFirstP: a.uiMode, afterSecondP: b.uiMode };
  });

  await rec('F5', 'Tab：面板开着时按键多少次才进面板 / 背景按钮能否被 Enter 触发', { land: true }, async (page, shot) => {
    await page.keyboard.press('l');
    await page.waitForTimeout(200);
    const seq = [];
    for (let i = 0; i < 30; i++) {
      await page.keyboard.press('Tab');
      await page.waitForTimeout(30);
      seq.push(await page.evaluate(() => {
        const a = document.activeElement;
        const inP = !!(a && a.closest('#panel'));
        return (a ? (a.id || String(a.className) || a.tagName) : '-') + (inP ? '@P' : '@bg');
      }));
    }
    const firstPanel = seq.findIndex(s => s.endsWith('@P'));
    await shot(page, 'tab-count');
    return { firstPanelTab: firstPanel + 1, total: seq.length, seq: seq.slice(0, 20) };
  });

  await rec('F6', '事件开着：开面板 / 开摆件 / 开创世 的叠加与退出顺序', { land: true }, async (page, shot) => {
    const out = [];
    await page.keyboard.press('e'); await page.waitForTimeout(200);
    const e0 = await snap(page);
    await page.keyboard.press('p'); await page.waitForTimeout(350);
    const p1 = await snap(page); out.push({ step: 'E→P', event: p1.event, mode: p1.uiMode, creator: p1.creator });
    await page.keyboard.press('Escape'); await page.waitForTimeout(200);
    const esc1 = await snap(page); out.push({ step: 'Esc1', event: esc1.event, mode: esc1.uiMode });
    await page.keyboard.press('Escape'); await page.waitForTimeout(250);
    const esc2 = await snap(page); out.push({ step: 'Esc2', event: esc2.event, mode: esc2.uiMode });
    /* 事件 → 创世 */
    await page.keyboard.press('e'); await page.waitForTimeout(200);
    await page.keyboard.press('g'); await page.waitForTimeout(300);
    const g1 = await snap(page);
    await shot(page, 'event-creator');
    await page.keyboard.press('Escape'); await page.waitForTimeout(200);
    const g2 = await snap(page);
    await page.keyboard.press('Escape'); await page.waitForTimeout(200);
    const g3 = await snap(page);
    return { out, eventOpened: e0.event, afterG: { event: g1.event, creator: g1.creator }, esc1: { event: g2.event, creator: g2.creator }, esc2: { creator: g3.creator } };
  });

  await rec('F7', '改绑等待中切换分组 → 等待态是否还在（无提示）', { land: true }, async (page, shot) => {
    await page.keyboard.press('k'); await page.waitForTimeout(150);
    await page.evaluate(() => document.querySelector('[data-k="pack"]').click());
    await page.waitForTimeout(100);
    const w0 = await snap(page);
    await page.evaluate(() => document.querySelector('[data-sg="video"]').click());
    await page.waitForTimeout(150);
    const w1 = await snap(page);
    await shot(page, 'rebind-group-switch');
    await page.keyboard.press('b');   /* 意在「抽卡」，实际会被当成改键 */
    await page.waitForTimeout(200);
    const w2 = await snap(page);
    const keys = await page.evaluate(() => window.__dbg.UI.KEYS.filter(k => k.ed).map(k => k.act + '=' + k.key).join(' '));
    return { waiting0: w0.waiting, group: w1.setGroup, waitingAfterSwitch: w1.waiting, panelAfterB: w2.panel, toasts: w2.toasts, keys };
  });

  await rec('F8', '空手牌：抽卡 / 选卡 / 施卡 / 面板', { land: true }, async (page, shot) => {
    await page.evaluate(() => { window.__dbg.S.W.hand = []; window.__dbg.S.W.sel = -1; window.__dbg.UI.renderHand(); window.__dbg.UI.updateHud(); });
    const a = await snap(page);
    await page.evaluate(() => window.__dbg.UI.openPanel('pack'));
    const swap = await page.$eval('#pbody', b => (b.textContent.match(/收下的那张换掉\s*(\S+)/) || [])[1] || null);
    await page.click('[data-act="packNext"]'); await page.waitForTimeout(150);
    await page.click('.packpick'); await page.waitForTimeout(150);
    const after = await snap(page);
    await page.keyboard.press('Enter'); await page.waitForTimeout(150);
    await page.keyboard.press('1'); await page.waitForTimeout(100);
    const end = await snap(page);
    await shot(page, 'empty-hand-pack');
    return { hand0: a.hand.length, sel0: a.sel, swapLabel: swap, handAfterPick: after.hand.length, selAfterPick: after.sel, castAfterEnter: end.castN };
  });

  await rec('F9', '面板重绘是否重置滚动位置（按同键 / 切面板回来）', { land: true }, async (page, shot) => {
    await page.evaluate(() => { for (let i = 0; i < 40; i++) window.__dbg.S.cmd.tick(1); });
    await page.keyboard.press('c'); await page.waitForTimeout(250);
    const sel = await page.evaluate(() => {
      const box = [...document.querySelectorAll('#pbody div')].find(d => getComputedStyle(d).overflow === 'auto');
      if (!box) return null;
      box.scrollTop = 260;
      return { before: box.scrollTop, h: box.scrollHeight };
    });
    await page.keyboard.press('c');   /* 同键重开 */
    await page.waitForTimeout(250);
    const after = await page.evaluate(() => {
      const box = [...document.querySelectorAll('#pbody div')].find(d => getComputedStyle(d).overflow === 'auto');
      return box ? box.scrollTop : null;
    });
    await shot(page, 'chron-scroll-reset');
    return { scroll: sel, afterSameKey: after };
  });

  fs.writeFileSync(path.join(__dirname, 'state-probe2.json'), JSON.stringify(results, null, 1));
  console.log('---', results.length);
  await browser.close();
})();
