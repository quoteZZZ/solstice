/* 探针 8：在 89216f8（第二轮修复提交）上复核受按键守卫影响的条目 */
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
    const lbl = () => {
      const m = (document.getElementById('pbody') || {}).textContent;
      const r = m && m.match(/收下的那张换掉\s*(\S+)/);
      return r ? r[1] : null;
    };
    return {
      casts: window.__casts, sel: S.W.sel, hand: S.W.hand.map(c => c.id),
      cells: S.W.cells.size, essence: S.W.essence, gear: S.gear,
      panel: UI.state.panel, event: !!UI.state.event, overlay: document.getElementById('overlay').classList.contains('show'),
      creator: document.getElementById('creator').classList.contains('on'),
      ghost: document.querySelectorAll('#ghost').length, label: lbl(),
      active: document.activeElement ? (document.activeElement.id || String(document.activeElement.className)) : null,
      cursor: R.cam.hover ? [R.cam.hover.x, R.cam.hover.y] : null,
      toasts: [...document.querySelectorAll('#toasts .toast, #phead .toast')].map(t => t.textContent.trim()),
      waiting: [...document.querySelectorAll('[data-k]')].filter(x => /等待按键/.test(x.textContent)).map(x => x.dataset.k),
      keys: UI.KEYS.filter(k => k.ed).map(k => k.act + '=' + k.key).join(' '),
      scroll: (() => { const q = document.querySelector('#pbody div[style*="overflow:auto"]'); return q ? q.scrollTop : null; })()
    };
  });
  async function drag(p, dy) {
    const r = await p.evaluate(() => {
      const q = document.querySelectorAll('.handslot')[0].getBoundingClientRect();
      return { x: q.x + q.width / 2, y: q.y + q.height / 2 };
    });
    await p.mouse.move(r.x, r.y); await p.mouse.down();
    for (let i = 1; i <= 4; i++) { await p.mouse.move(r.x, r.y + (dy || -220) * i / 4); await p.waitForTimeout(30); }
  }

  /* 1. S07 面板开着按功能键 */
  {
    const p = await open(true);
    await p.keyboard.press('l'); await p.waitForTimeout(200);
    const a = await snap(p);
    const rows = [];
    for (const k of ['Space', 'x', 'r', 'j']) {
      await p.keyboard.press(k); await p.waitForTimeout(150);
      const s = await snap(p);
      rows.push({ k, castN: s.casts.length, gear: s.gear, sel: s.sel, panel: s.panel, ess: s.essence });
    }
    console.log('S07', JSON.stringify({ rows, gearBefore: a.gear, essBefore: a.essence }));
    await p.close();
  }

  /* 2. S05 抽卡面板里按 5 还能不能改替换目标 */
  {
    const p = await open(true);
    await p.evaluate(() => {
      const { S, UI } = window.__dbg;
      S.W.hand = S.drawPool().slice(0, 8); S.W.sel = 2; UI.renderHand(); UI.openPanel('pack');
    });
    await p.click('[data-act="packNext"]'); await p.waitForTimeout(200);
    const l1 = (await snap(p)).label;
    const sel1 = (await snap(p)).sel;
    await p.keyboard.press('5'); await p.waitForTimeout(180);
    const after5 = await snap(p);
    await p.click('.packpick'); await p.waitForTimeout(200);
    const afterPick = await snap(p);
    console.log('S05', JSON.stringify({ label1: l1, sel1, labelAfterKey5: after5.label, selAfterKey5: after5.sel, replacedSlot: after5.sel, handHas: afterPick.hand.length }));
    await p.close();
  }

  /* 3. S08 拖拽中 Space / X / 1 */
  {
    const p = await open(true);
    for (const k of ['Space', 'x', '1']) {
      await p.mouse.move(700, 700); await p.waitForTimeout(60);
      await drag(p);
      const pre = await snap(p);
      await p.keyboard.press(k); await p.waitForTimeout(200);
      const mid = await snap(p);
      await p.mouse.move(840, 430); await p.waitForTimeout(60); await p.mouse.up(); await p.waitForTimeout(350);
      const end = await snap(p);
      console.log('S08-' + k, JSON.stringify({ preCasts: pre.casts.length, midCasts: mid.casts.length, midGhost: mid.ghost, endCasts: end.casts.length, sel: end.sel }));
    }
    await p.close();
  }

  /* 4. S20 拖拽中按 B */
  {
    const p = await open(true);
    await drag(p);
    await p.keyboard.press('b'); await p.waitForTimeout(300);
    const mid = await snap(p);
    await p.mouse.move(840, 430); await p.mouse.up(); await p.waitForTimeout(350);
    const end = await snap(p);
    console.log('S20', JSON.stringify({ panelMid: mid.panel, overlayMid: mid.overlay, ghostMid: mid.ghost, casts: end.casts.length, cells: end.cells }));
    await p.close();
  }

  /* 5. S09 事件开着开面板 / 开创世 */
  {
    const p = await open(true);
    await p.keyboard.press('e'); await p.waitForTimeout(220);
    await p.keyboard.press('l'); await p.waitForTimeout(250);
    const a = await snap(p);
    await p.keyboard.press('Escape'); await p.waitForTimeout(200);
    const b = await snap(p);
    await p.keyboard.press('g'); await p.waitForTimeout(300);
    const c = await snap(p);
    console.log('S09', JSON.stringify({ event1: a.event, panel1: a.panel, afterEscEvent: b.event, panelStill: b.panel, eventAfterG: c.event, creator: c.creator }));
    await p.close();
  }

  /* 6. S15 同键切换 + K/H 在面板里是否失效 */
  {
    const p = await open(true);
    await p.keyboard.press('c'); await p.waitForTimeout(200);
    const a = await snap(p);
    await p.keyboard.press('c'); await p.waitForTimeout(250);
    const b = await snap(p);
    await p.keyboard.press('k'); await p.waitForTimeout(250);
    const k = await snap(p);
    await p.keyboard.press('h'); await p.waitForTimeout(250);
    const h = await snap(p);
    await p.keyboard.press('o'); await p.waitForTimeout(250);
    const o = await snap(p);
    await p.keyboard.press('b'); await p.waitForTimeout(250);
    const bb = await snap(p);
    console.log('S15', JSON.stringify({ c1: a.panel, c2: b.panel, afterK: k.panel, afterH: h.panel, afterO: o.panel, afterB: bb.panel }));
    await p.close();
  }

  /* 7. S16 K 之后 O 的落点（K 必须先关面板） */
  {
    const p = await open(true);
    await p.keyboard.press('o'); await p.waitForTimeout(200);
    const o1 = await snap(p);
    await p.evaluate(() => document.querySelector('[data-sg="ui"]').click()); await p.waitForTimeout(150);
    await p.keyboard.press('Escape'); await p.waitForTimeout(150);
    await p.keyboard.press('k'); await p.waitForTimeout(200);
    const k1 = await p.evaluate(() => window.__dbg.UI.state.setGroup + '|' + window.__dbg.UI.state.panel);
    await p.keyboard.press('Escape'); await p.waitForTimeout(150);
    await p.keyboard.press('o'); await p.waitForTimeout(200);
    const o2 = await p.evaluate(() => window.__dbg.UI.state.setGroup + '|' + window.__dbg.UI.state.panel);
    console.log('S16', JSON.stringify({ firstGroup: o1.panel, afterK: k1, reopenO: o2 }));
    await p.close();
  }

  /* 8. S12 Tab 次数 + 背景 Enter */
  {
    const p = await open(true);
    await p.keyboard.press('l'); await p.waitForTimeout(200);
    const seq = [];
    for (let i = 0; i < 30; i++) {
      await p.keyboard.press('Tab'); await p.waitForTimeout(30);
      seq.push(await p.evaluate(() => {
        const a = document.activeElement;
        const inP = !!(a && a.closest('#panel'));
        return (a ? (a.id || String(a.className) || a.tagName) : '-') + (inP ? '@P' : '@bg');
      }));
    }
    console.log('S12', JSON.stringify({ firstPanel: seq.findIndex(s => s.endsWith('@P')) + 1 }));
    await p.close();
  }

  await b.close();
})();
