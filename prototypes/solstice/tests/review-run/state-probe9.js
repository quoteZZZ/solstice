/* 探针 9：复核工作区未提交的第三轮修复（S06/S25/S26）与 S09 残留 */
const path = require('path');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;

(async () => {
  const b = await chromium.launch({ channel: 'msedge', headless: true });
  async function open(land) {
    const p = await b.newPage({ viewport: { width: 1680, height: 1000 } });
    await p.goto(PAGE);
    await p.waitForTimeout(1100);
    await p.evaluate(l => {
      const { S, D, UI } = window.__dbg;
      UI.enterFromCreator();
      if (l) {
        const c = D.byId.get('terrain.earth.0.matter.land');
        for (let x = -2; x <= 2; x++) for (let y = -2; y <= 2; y++) S.cmd.cast(c, x, y);
      }
      S.W.essence = 500; S.setGear(2);
      UI.syncGears(); UI.updateHud(); UI.renderHand();
      window.__casts = [];
      const o = UI.cast;
      UI.cast = function (i, cell, pt) {
        window.__casts.push({ i: i, card: (S.W.hand[i] || {}).id });
        return o.apply(this, arguments);
      };
    }, !!land);
    await p.waitForTimeout(150);
    return p;
  }
  const snap = p => p.evaluate(() => {
    const { S, UI } = window.__dbg;
    return {
      panel: UI.state.panel, event: !!UI.state.event, setGroup: UI.state.setGroup, codexTab: UI.state.codexTab,
      packStage: UI.packStage, packCards: (UI.packCards || []).length, cells: S.W.cells.size, month: S.W.month,
      essence: S.W.essence, creator: document.getElementById('creator').classList.contains('on'),
      casts: window.__casts.length, ghost: document.querySelectorAll('#ghost').length,
      gear: S.gear
    };
  });
  async function drag(p) {
    const r = await p.evaluate(() => {
      const q = document.querySelectorAll('.handslot')[0].getBoundingClientRect();
      return { x: q.x + q.width / 2, y: q.y + q.height / 2 };
    });
    await p.mouse.move(r.x, r.y); await p.mouse.down();
    for (let i = 1; i <= 4; i++) { await p.mouse.move(r.x, r.y - 220 * i / 4); await p.waitForTimeout(30); }
  }

  /* S06 新区是否还继承旧包 */
  {
    const p = await open(true);
    await p.evaluate(() => window.__dbg.UI.openPanel('pack'));
    await p.click('[data-act="packNext"]'); await p.waitForTimeout(180);
    const before = await snap(p);
    await p.keyboard.press('Escape'); await p.waitForTimeout(120);
    await p.keyboard.press('g'); await p.waitForTimeout(300);
    await p.click('[data-c="enter"]'); await p.waitForTimeout(200);
    await p.click('[data-c="enter"]'); await p.waitForTimeout(350);
    const world2 = await snap(p);
    await p.keyboard.press('b'); await p.waitForTimeout(220);
    const after = await snap(p);
    console.log('S06', JSON.stringify({ before: { stage: before.packStage, cards: before.packCards }, world2: { month: world2.month, cells: world2.cells, ess: world2.essence }, afterB: { stage: after.packStage, cards: after.packCards, panel: after.panel } }));
    await p.close();
  }

  /* S25 面板内 Enter / Space（显式聚焦到非当前页签，Space 同时看是否误施卡） */
  {
    const p = await open(true);
    await p.keyboard.press('l'); await p.waitForTimeout(250);
    await p.evaluate(() => document.querySelector('[data-ctab="ach"]').focus());
    const f1 = await p.evaluate(() => ({ tag: document.activeElement.tagName, ctab: document.activeElement.dataset && document.activeElement.dataset.ctab }));
    const b1 = await snap(p);
    await p.keyboard.press('Enter'); await p.waitForTimeout(300);
    const a1 = await snap(p);
    await p.evaluate(() => document.querySelector('[data-ctab="cards"]').focus());
    const b2 = await snap(p);
    await p.keyboard.press('Space'); await p.waitForTimeout(350);
    const a2 = await snap(p);
    console.log('S25', JSON.stringify({ focus: f1, enter: [b1.codexTab, a1.codexTab], space: { tab: [b2.codexTab, a2.codexTab], castDelta: a2.casts - b2.casts, cells: [b2.cells, a2.cells] } }));
    await p.close();
  }

  /* S26 面板内 K / H */
  {
    const p = await open(true);
    await p.keyboard.press('c'); await p.waitForTimeout(250);
    const c0 = await snap(p);
    await p.keyboard.press('k'); await p.waitForTimeout(300);
    const k = await snap(p);
    await p.keyboard.press('h'); await p.waitForTimeout(300);
    const h = await snap(p);
    console.log('S26', JSON.stringify({ start: c0.panel, afterK: { panel: k.panel, group: k.setGroup }, afterH: { panel: h.panel, group: h.setGroup } }));
    await p.close();
  }

  /* S09 事件叠层是否仍在 */
  {
    const p = await open(true);
    await p.keyboard.press('e'); await p.waitForTimeout(250);
    await p.keyboard.press('l'); await p.waitForTimeout(300);
    const a = await snap(p);
    await p.keyboard.press('Escape'); await p.waitForTimeout(250);
    const b1 = await snap(p);
    await p.keyboard.press('g'); await p.waitForTimeout(350);
    const c = await snap(p);
    console.log('S09', JSON.stringify({ eventL: { event: a.event, panel: a.panel }, esc1: { event: b1.event, panel: b1.panel }, eventG: { event: c.event, creator: c.creator, panel: c.panel } }));
    await p.close();
  }

  /* S07/S08/S20 回归 sanity */
  {
    const p = await open(true);
    await p.keyboard.press('l'); await p.waitForTimeout(200);
    const a0 = await snap(p);
    await p.keyboard.press('Space'); await p.waitForTimeout(200);
    const a1 = await snap(p);
    await p.keyboard.press('Escape'); await p.waitForTimeout(150);
    await drag(p);
    await p.keyboard.press('Space'); await p.waitForTimeout(200);
    const m = await snap(p);
    await p.mouse.move(840, 430); await p.mouse.up(); await p.waitForTimeout(350);
    const e = await snap(p);
    console.log('S07S08', JSON.stringify({ spaceInPanel: a1.casts - a0.casts, midCasts: m.casts, endCasts: e.casts, ghostMid: m.ghost }));
    await p.close();
  }
  {
    const p = await open(true);
    await drag(p);
    await p.keyboard.press('b'); await p.waitForTimeout(300);
    const m = await snap(p);
    await p.mouse.move(840, 430); await p.mouse.up(); await p.waitForTimeout(350);
    const e = await snap(p);
    console.log('S20', JSON.stringify({ panelMid: m.panel, ghostMid: m.ghost, endCasts: e.casts, cells: e.cells }));
    await p.close();
  }

  await b.close();
})();
