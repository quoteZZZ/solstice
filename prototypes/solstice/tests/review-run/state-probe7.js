/* 探针 7：在工作树（含 S01–S04 修复）上复核 */
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
        window.__casts.push({ i: i, cell: cell ? [cell.x, cell.y] : null, card: (S.W.hand[i] || {}).id });
        return o.apply(this, arguments);
      };
    }, !!land);
    await p.waitForTimeout(150);
    return p;
  }
  const snap = p => p.evaluate(() => {
    const { S, UI, R } = window.__dbg;
    return {
      casts: window.__casts, hand: S.W.hand.map(c => c.id), cells: S.W.cells.size,
      panel: UI.state.panel, creator: document.getElementById('creator').classList.contains('on'),
      fromWorld: UI.creator.fromWorld, armed: UI.creator.armed,
      hasBack: !!document.querySelector('[data-c="back"]'),
      ghost: document.querySelectorAll('#ghost').length, uiMode: UI.state.uiMode,
      waiting: [...document.querySelectorAll('[data-k]')].filter(x => /等待按键/.test(x.textContent)).map(x => x.dataset.k),
      keys: UI.KEYS.filter(k => k.ed).map(k => k.act + '=' + k.key).join(' '),
      cursor: R.cam.hover ? [R.cam.hover.x, R.cam.hover.y] : null,
      toasts: [...document.querySelectorAll('#toasts .toast, #phead .toast')].map(t => t.textContent.trim())
    };
  });
  async function drag(p, dy) {
    const r = await p.evaluate(() => {
      const q = document.querySelectorAll('.handslot')[0].getBoundingClientRect();
      return { x: q.x + q.width / 2, y: q.y + q.height / 2 };
    });
    await p.mouse.move(r.x, r.y); await p.mouse.down();
    for (let i = 1; i <= 4; i++) { await p.mouse.move(r.x, r.y + (dy || -220) * i / 4); await p.waitForTimeout(30); }
    return r;
  }

  /* S01 空世界创世 */
  {
    const p = await open(false);
    await p.keyboard.press('g'); await p.waitForTimeout(300);
    const g = await snap(p);
    await p.keyboard.press('Escape'); await p.waitForTimeout(250);
    const e = await snap(p);
    console.log('S01', JSON.stringify({ fromWorld: g.fromWorld, hasBack: g.hasBack, creatorAfterEsc: e.creator }));
    await p.close();
  }

  /* S02 Esc 取消：选一张落在已有地块上必成功的卡 */
  {
    const p = await open(true);
    await p.evaluate(() => {
      const { S, D, UI } = window.__dbg;
      const c = D.byId.get('terrain.mineral.0.matter.water');
      const i = S.W.hand.findIndex(x => x.sim.op === 'spring');
      S.W.hand[i >= 0 ? i : 0] = c; S.W.sel = i >= 0 ? i : 0; UI.renderHand();
    });
    await drag(p);
    await p.keyboard.press('Escape'); await p.waitForTimeout(250);
    const mid = await snap(p);
    await p.mouse.move(840, 420); await p.mouse.up(); await p.waitForTimeout(250);
    const a = await snap(p);
    console.log('S02', JSON.stringify({ midCasts: mid.casts.length, midGhost: mid.ghost, endCasts: a.casts.length, endGhost: a.ghost, cards: a.casts.map(c => c.card) }));
    await p.close();
  }

  /* S03 单次拖放（水卡，落在已有格） */
  {
    const p = await open(true);
    await p.evaluate(() => {
      const { S, D, UI } = window.__dbg;
      const c = D.byId.get('terrain.mineral.0.matter.water');
      const i = S.W.hand.findIndex(x => x.sim.op === 'spring');
      S.W.hand[i >= 0 ? i : 0] = c; S.W.sel = i >= 0 ? i : 0; UI.renderHand();
    });
    await drag(p); await p.mouse.move(840, 430); await p.waitForTimeout(80); await p.mouse.up(); await p.waitForTimeout(400);
    const a = await snap(p);
    console.log('S03', JSON.stringify({ casts: a.casts, ghost: a.ghost, toasts: a.toasts }));
    await p.close();
  }

  /* S03b 虚空土地：不应再有多余 toast */
  {
    const p = await open(false);
    await p.evaluate(() => {
      const { S, D, UI } = window.__dbg;
      S.W.hand[0] = D.byId.get('terrain.earth.0.matter.land'); S.W.sel = 0; UI.renderHand();
    });
    await drag(p); await p.mouse.move(840, 360); await p.waitForTimeout(80); await p.mouse.up(); await p.waitForTimeout(400);
    const a = await snap(p);
    console.log('S03b', JSON.stringify({ casts: a.casts, cells: a.cells, toasts: a.toasts }));
    await p.close();
  }

  /* S04 等待态 */
  {
    const p = await open(true);
    await p.keyboard.press('k'); await p.waitForTimeout(150);
    await p.evaluate(() => document.querySelector('[data-k="recycle"]').click()); await p.waitForTimeout(100);
    const w = await snap(p);
    await p.evaluate(() => document.getElementById('ovbg').click()); await p.waitForTimeout(150);
    await p.keyboard.press('w'); await p.waitForTimeout(200);
    const a = await snap(p);
    await p.keyboard.press('k'); await p.waitForTimeout(150);
    await p.evaluate(() => document.querySelector('[data-k="pack"]').click()); await p.waitForTimeout(100);
    await p.evaluate(() => document.querySelector('[data-sg="video"]').click()); await p.waitForTimeout(120);
    await p.keyboard.press('b'); await p.waitForTimeout(250);
    const s = await snap(p);
    console.log('S04', JSON.stringify({ waiting: w.waiting, keysAfterW: a.keys, wAfterCursor: a.cursor, wAfterPanel: a.panel, groupSwitchPanel: s.panel, toasts: s.toasts }));
    await p.close();
  }

  /* S08 拖拽 + Space */
  {
    const p = await open(true);
    await drag(p);
    await p.keyboard.press('Space'); await p.waitForTimeout(250);
    const mid = await snap(p);
    await p.mouse.move(840, 430); await p.waitForTimeout(80); await p.mouse.up(); await p.waitForTimeout(350);
    const a = await snap(p);
    console.log('S08', JSON.stringify({ midCasts: mid.casts.length, endCasts: a.casts.length, cards: a.casts.map(c => c.card) }));
    await p.close();
  }

  /* S20 拖拽 + 开面板 */
  {
    const p = await open(true);
    await drag(p);
    await p.keyboard.press('b'); await p.waitForTimeout(300);
    const mid = await snap(p);
    await p.mouse.move(840, 430); await p.mouse.up(); await p.waitForTimeout(300);
    const a = await snap(p);
    console.log('S20', JSON.stringify({ ghost: mid.ghost, cells: a.cells, casts: a.casts.length, panel: mid.panel }));
    await p.close();
  }

  await b.close();
})();
