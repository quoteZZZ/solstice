/* Solstice · 状态机审计探针（临时脚本，不改源码）
   跑法：node tests/review-run/state-probe.js [--only=A,B] [--w=1680 --h=1000]
   每个场景独立开页，走真实输入（键/鼠标），只读取 __dbg 与 DOM。 */
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
fs.mkdirSync(OUT, { recursive: true });
const ONLY = (arg('only', '') || '').split(',').filter(Boolean);

const SNAP = () => {
  const { S, UI, D, R } = window.__dbg;
  const st = UI.state;
  const el = id => document.getElementById(id);
  return {
    panel: st.panel, event: !!st.event,
    packStage: UI.packStage, packCards: (UI.packCards || []).length, packSel: UI.packSel,
    setGroup: st.setGroup, codexTab: st.codexTab, codexEl: st.codexEl, codexSel: st.codexSel,
    chronFilter: st.chronFilter, dexGrp: st.dexGrp,
    hand: S.W.hand.map(c => c.id), sel: S.W.sel, handN: S.W.hand.length,
    essence: S.W.essence, month: S.W.month, gear: S.gear, paused: S.W.paused, speed: S.W.speed,
    uiMode: UI.uiMode,
    creator: el('creator').classList.contains('on'), fromWorld: UI.creator.fromWorld, armed: UI.creator.armed,
    overlay: el('overlay').classList.contains('show'), eventOn: el('eventwrap').classList.contains('on'),
    cells: S.W.cells.size, cursor: R.cam.hover ? [R.cam.hover.x, R.cam.hover.y] : null,
    ghost: document.querySelectorAll('#ghost').length,
    active: document.activeElement ? (document.activeElement.id || document.activeElement.className || document.activeElement.tagName) : null,
    waiting: [...document.querySelectorAll('[data-k]')].filter(b => /等待按键/.test(b.textContent)).map(b => b.dataset.k),
    toasts: [...document.querySelectorAll('#toasts .toast, #phead .toast')].map(t => t.textContent.trim()),
    editableKeys: UI.KEYS.filter(k => k.ed).map(k => k.act + '=' + k.key).join(' '),
    digest: S.digest()
  };
};

const results = [];
const log = (...a) => console.log(...a);

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });

  async function open(opts) {
    const page = await browser.newPage({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
    const errs = [];
    page.on('pageerror', e => errs.push('[PAGEERROR] ' + e.message));
    page.on('console', m => { if (m.type() === 'error') errs.push('[console] ' + m.text()); });
    await page.goto(PAGE);
    await page.waitForTimeout(1100);
    await page.evaluate(o => {
      const { S, D, UI } = window.__dbg;
      UI.enterFromCreator();               // 跳过创世：直接进界
      if (o.land) {
        const land = D.byId.get('terrain.earth.0.matter.land');
        for (let x = -2; x <= 2; x++) for (let y = -2; y <= 2; y++) S.cmd.cast(land, x, y);
      }
      S.W.essence = o.essence == null ? 500 : o.essence;
      S.setGear(o.gear == null ? 2 : o.gear);
      UI.syncGears(); UI.updateHud(); UI.renderHand(); UI.renderRight(); UI.renderOverlay();
    }, opts || {});
    await page.waitForTimeout(160);
    return { page, errs };
  }

  let n = 0;
  async function shot(page, tag) {
    n++;
    const f = String(n).padStart(2, '0') + '-' + tag + '.png';
    await page.waitForTimeout(280);
    await page.screenshot({ path: path.join(OUT, f) });
    return 'state-shots/' + f;
  }
  const snap = p => p.evaluate(SNAP);
  async function rec(id, title, steps) {
    if (ONLY.length && !ONLY.includes(id)) return;
    let page, errs = [];
    try {
      ({ page, errs } = await open(steps.opts || {}));
      const data = await steps.run(page, shot);
      results.push({ id, title, ok: !(data && data.bad), data, errs });
      log('◉', id, title, '→', JSON.stringify(data && data.verdict ? data.verdict : data && data.bad ? data.bad : 'ok'));
    } catch (e) {
      results.push({ id, title, ok: false, err: String(e && e.stack || e), errs });
      log('✗', id, title, e && e.message);
    } finally {
      if (page) await page.close();
    }
  }
  const press = (p, k) => p.keyboard.press(k);
  const evalq = (p, fn, a) => p.evaluate(fn, a);

  /* ═════════ 抽卡 ═════════ */

  await rec('A1', '0→撕开→挑选→收下一张', {
    opts: { land: true },
    async run(page, shot) {
      await page.evaluate(() => { window.__dbg.UI.openPanel('pack'); });
      await page.waitForTimeout(120);
      const seal = await snap(page);
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(150);
      const opened = await snap(page);
      await shot(page, 'pack-opened');
      await page.click('.packpick');
      await page.waitForTimeout(150);
      const picked = await snap(page);
      await shot(page, 'pack-picked');
      return { verdict: { sealEss: seal.essence, openedEss: opened.essence, stage: opened.packStage, cards: opened.packCards, afterPanel: picked.panel, afterStage: picked.packStage, handDelta: picked.handN - seal.handN, sel: picked.sel } };
    }
  });

  await rec('A2', '0→撕开→挑选→跳过', {
    opts: { land: true },
    async run(page, shot) {
      await page.evaluate(() => { window.__dbg.UI.openPanel('pack'); });
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(120);
      const opened = await snap(page);
      const label = await page.textContent('[data-act="packNext"]');
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(120);
      const after = await snap(page);
      await shot(page, 'pack-skipped');
      return { verdict: { label, stage: opened.packStage, cards: opened.packCards, afterPanel: after.panel, afterStage: after.packStage, afterCards: after.packCards, handDelta: after.handN - 8, essence: after.essence } };
    }
  });

  await rec('A3', '0→撕开→关面板→再开（stage/cards 是否保留）', {
    opts: { land: true },
    async run(page, shot) {
      await page.evaluate(() => window.__dbg.UI.openPanel('pack'));
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(150);
      const a = await snap(page);
      await press(page, 'Escape');
      await page.waitForTimeout(100);
      const closed = await snap(page);
      await press(page, 'b');
      await page.waitForTimeout(150);
      const re = await snap(page);
      await shot(page, 'pack-reopen');
      return { verdict: { beforeStage: a.packStage, beforeCards: a.packCards, closed: closed.panel, reopened: re.panel, stage: re.packStage, cards: re.packCards } };
    }
  });

  await rec('A4', '0→撕开→切包（应被拦）', {
    opts: { land: true },
    async run(page, shot) {
      await page.evaluate(() => window.__dbg.UI.openPanel('pack'));
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(120);
      const a = await snap(page);
      const rows = await page.$$('[data-pack]');
      const target = await rows[1].getAttribute('data-pack');
      await rows[1].click();
      await page.waitForTimeout(150);
      const b = await snap(page);
      await shot(page, 'pack-switch-blocked');
      return { verdict: { selBefore: a.packSel, clickRow: target, selAfter: b.packSel, stage: b.packStage, cards: b.packCards, toasts: b.toasts } };
    }
  });

  await rec('A5', '0→撕开→时素归零→再点主按钮', {
    opts: { land: true },
    async run(page, shot) {
      await page.evaluate(() => window.__dbg.UI.openPanel('pack'));
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(120);
      await page.evaluate(() => { window.__dbg.S.W.essence = 0; });
      const label = await page.textContent('[data-act="packNext"]');
      const disabled = await page.$eval('[data-act="packNext"]', b => b.disabled);
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(120);
      const after = await snap(page);
      await shot(page, 'pack-zero-essence');
      return { verdict: { label, disabled, afterPanel: after.panel, afterStage: after.packStage, cards: after.packCards } };
    }
  });

  await rec('A6', '手牌满时收下（换哪张 / 面板标签是否跟着 sel 变）', {
    opts: { land: true },
    async run(page, shot) {
      const setup = await page.evaluate(() => {
        const { S, UI, D } = window.__dbg;
        const pool = S.drawPool().slice(0, 8);
        S.W.hand = pool.slice();
        S.W.sel = 2;
        UI.renderHand(); UI.updateHud();
        UI.openPanel('pack');
        return { ids: pool.map(c => c.id), sel: S.W.sel };
      });
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(150);
      const label1 = await page.$eval('#pbody', b => {
        const m = b.textContent.match(/收下的那张换掉\s*(\S+)/);
        return m ? m[1] : null;
      });
      await press(page, '5');
      await page.waitForTimeout(120);
      const st5 = await snap(page);
      const label2 = await page.$eval('#pbody', b => {
        const m = b.textContent.match(/收下的那张换掉\s*(\S+)/);
        return m ? m[1] : null;
      });
      await shot(page, 'pack-full-hand');
      const beforeIds = st5.hand;
      const targetNameBefore = await page.evaluate(() => window.__dbg.S.W.hand[window.__dbg.S.W.sel].name.zh);
      await page.click('.packpick');
      await page.waitForTimeout(150);
      const after = await snap(page);
      return { verdict: { sel0: setup.sel, label1, labelOnOpen: label1, labelAfterKey5: label2, selAfterKey5: st5.sel, targetNameBefore, replacedSlot: st5.sel, handN: after.handN, toasts: after.toasts, beforeIds } };
    }
  });

  await rec('A7', '连抽两次（同一包种 / 换包种）', {
    opts: { land: true },
    async run(page, shot) {
      const out = [];
      for (let k = 0; k < 2; k++) {
        await page.evaluate(() => window.__dbg.UI.openPanel('pack'));
        await page.click('[data-act="packNext"]');
        await page.waitForTimeout(120);
        const a = await snap(page);
        await page.click('.packpick');
        await page.waitForTimeout(120);
        const b = await snap(page);
        out.push({ round: k, essenceAfterDraw: a.essence, stage: a.packStage, handAfter: b.handN, sel: b.sel, panel: b.panel });
      }
      await shot(page, 'pack-twice');
      return { verdict: out };
    }
  });

  await rec('A8', '撕开后按 B 再开面板', {
    opts: { land: true },
    async run(page, shot) {
      await page.evaluate(() => window.__dbg.UI.openPanel('pack'));
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(120);
      const a = await snap(page);
      await press(page, 'b');
      await page.waitForTimeout(120);
      const b = await snap(page);
      return { verdict: { stageBefore: a.packStage, cardsBefore: a.packCards, panelAfterB: b.panel, stageAfterB: b.packStage, cardsAfterB: b.packCards } };
    }
  });

  /* ═════════ 面板 ═════════ */

  await rec('B1', '四面板互切 + 同键再按是否关闭', {
    opts: { land: true },
    async run(page, shot) {
      const seq = [];
      for (const k of ['b', 'l', 'c', 'o']) {
        await press(page, k);
        await page.waitForTimeout(120);
        const s = await snap(page);
        seq.push({ key: k, panel: s.panel, setGroup: s.setGroup });
      }
      const before = await snap(page);
      await press(page, 'o');
      await page.waitForTimeout(120);
      const again = await snap(page);
      await page.keyboard.press('Escape');
      await page.waitForTimeout(100);
      const esc = await snap(page);
      await shot(page, 'panels-cycle');
      return { verdict: { seq, panelBeforeRepeat: before.panel, panelAfterSameKey: again.panel, overlayAfterSameKey: again.overlay, panelAfterEsc: esc.panel } };
    }
  });

  await rec('B2', '开面板 → 点遮罩关闭', {
    opts: { land: true },
    async run(page, shot) {
      await press(page, 'c');
      await page.waitForTimeout(120);
      const a = await snap(page);
      await page.evaluate(() => document.getElementById('ovbg').click());
      await page.waitForTimeout(100);
      const b = await snap(page);
      return { verdict: { open: a.panel, afterOverlayClick: b.panel, overlay: b.overlay } };
    }
  });

  await rec('B3', '卡库→点卡进详情→Esc', {
    opts: { land: true },
    async run(page, shot) {
      await press(page, 'l');
      await page.waitForTimeout(150);
      const before = await snap(page);
      const cell = await page.$('.cgt[data-card]');
      await cell.click();
      await page.waitForTimeout(150);
      const detail = await snap(page);
      await shot(page, 'codex-detail');
      await press(page, 'Escape');
      await page.waitForTimeout(120);
      const after = await snap(page);
      await press(page, 'l');
      await page.waitForTimeout(150);
      const reopen = await snap(page);
      return { verdict: { codexSelOnOpen: before.codexSel, codexSelAfterClick: detail.codexSel, panelAfterEsc: after.panel, reopenSel: reopen.codexSel, reopenEl: reopen.codexEl } };
    }
  });

  await rec('B4', '设置分组跨开合保留 / O 与 K 的落点', {
    opts: { land: true },
    async run(page, shot) {
      await press(page, 'o');
      await page.waitForTimeout(120);
      const open0 = await snap(page);
      await page.evaluate(() => document.querySelector('[data-sg="ui"]').click());
      await page.waitForTimeout(100);
      const g1 = await snap(page);
      await press(page, 'Escape');
      await page.waitForTimeout(100);
      await press(page, 'o');
      await page.waitForTimeout(120);
      const open1 = await snap(page);
      await press(page, 'k');
      await page.waitForTimeout(120);
      const keys = await snap(page);
      await press(page, 'Escape');
      await page.waitForTimeout(100);
      await press(page, 'o');
      await page.waitForTimeout(120);
      const open2 = await snap(page);
      await shot(page, 'settings-group');
      return { verdict: { firstGroup: open0.setGroup, clicked: g1.setGroup, reopenO: open1.setGroup, afterK: keys.setGroup, reopenOafterK: open2.setGroup } };
    }
  });

  await rec('B5', '键位改绑→Esc 取消 / 等待中关面板再按键', {
    opts: { land: true },
    async run(page, shot) {
      await press(page, 'k');
      await page.waitForTimeout(150);
      const k0 = await snap(page);
      await page.evaluate(() => document.querySelector('[data-k="pack"]').click());
      await page.waitForTimeout(100);
      const waiting = await snap(page);
      await press(page, 'Escape');
      await page.waitForTimeout(120);
      const cancelled = await snap(page);
      await shot(page, 'rebind-cancelled');
      /* 第二次：等待中直接关面板（点遮罩），再按 W */
      await page.evaluate(() => document.querySelector('[data-k="recycle"]').click());
      await page.waitForTimeout(100);
      const w1 = await snap(page);
      await page.evaluate(() => document.getElementById('ovbg').click());
      await page.waitForTimeout(120);
      const closed = await snap(page);
      const cursorBefore = closed.cursor;
      await press(page, 'w');
      await page.waitForTimeout(150);
      const afterW = await snap(page);
      await shot(page, 'rebind-leak');
      return { verdict: { keys0: k0.editableKeys, waitingVisible: waiting.waiting, escKeys: cancelled.editableKeys, escPanel: cancelled.panel, waitingOnClose: closed.waiting, panelOnClose: closed.panel, cursorBefore, cursorAfterW: afterW.cursor, keysAfterW: afterW.editableKeys, toasts: afterW.toasts } };
    }
  });

  await rec('B6', '年表筛选→关→再开', {
    opts: { land: true },
    async run(page, shot) {
      await page.evaluate(() => { for (let i = 0; i < 24; i++) window.__dbg.S.cmd.tick(1); window.__dbg.UI.updateHud(); });
      await press(page, 'c');
      await page.waitForTimeout(150);
      const a = await snap(page);
      const rowsAll = await page.$$eval('.tl .it', r => r.length);
      await page.evaluate(() => document.querySelector('[data-kf="气候"]').click());
      await page.waitForTimeout(150);
      const b = await snap(page);
      const rowsClimate = await page.$$eval('.tl .it', r => r.length);
      await press(page, 'Escape');
      await page.waitForTimeout(100);
      await press(page, 'c');
      await page.waitForTimeout(150);
      const c = await snap(page);
      await shot(page, 'chron-filter');
      return { verdict: { filterOnOpen: a.chronFilter, rowsAll, filterSet: b.chronFilter, rowsClimate, filterReopen: c.chronFilter, rowsReopen: await page.$$eval('.tl .it', r => r.length) } };
    }
  });

  /* ═════════ 世界 / 模式 ═════════ */

  await rec('C1', '摆件模式 → 开面板 → 退出摆件 / Esc / 拖手牌', {
    opts: { land: true },
    async run(page, shot) {
      await press(page, 'p');
      await page.waitForTimeout(420);
      const amb = await snap(page);
      const rects = await page.evaluate(() => {
        const h = document.querySelector('.handslot').getBoundingClientRect();
        const w = document.getElementById('handwrap').getBoundingClientRect();
        return { hand: [Math.round(h.x), Math.round(h.y), Math.round(h.width), Math.round(h.height)], wrap: [Math.round(w.y), Math.round(w.height)], vh: innerHeight };
      });
      await shot(page, 'ambient');
      await press(page, 'l');
      await page.waitForTimeout(200);
      const withPanel = await snap(page);
      await shot(page, 'ambient-panel');
      await press(page, 'p');
      await page.waitForTimeout(250);
      const back = await snap(page);
      /* 摆件 → Esc */
      await press(page, 'p');
      await page.waitForTimeout(420);
      await press(page, 'Escape');
      await page.waitForTimeout(250);
      const esc = await snap(page);
      /* 仅右栏 → P → P 是否回到仅右栏 */
      const modeSeq = await page.evaluate(() => {
        const UI = window.__dbg.UI;
        UI.setUiMode('仅右栏');
        UI.toggleAmbient();
        const a = UI.uiMode;
        UI.toggleAmbient();
        return [a, UI.uiMode];
      });
      return { verdict: { ambientMode: amb.uiMode, handRect: rects, panelWhileAmbient: withPanel.panel, afterP: back.uiMode, panelStill: back.panel, afterEsc: esc.uiMode, modeSeq } };
    }
  });

  await rec('C2', '摆件模式 → 拖手牌（是否可拖 / 拖到哪）', {
    opts: { land: true },
    async run(page, shot) {
      await press(page, 'p');
      await page.waitForTimeout(450);
      const r = await page.evaluate(() => {
        const h = document.querySelectorAll('.handslot')[0].getBoundingClientRect();
        return { x: h.x + h.width / 2, y: h.y + Math.min(h.height / 2, 12), w: h.width, h: h.height, top: h.top, bottom: h.bottom, vh: innerHeight };
      });
      const before = await snap(page);
      await page.mouse.move(r.x, r.y);
      await page.mouse.down();
      for (let i = 1; i <= 5; i++) { await page.mouse.move(r.x, r.y - i * 60); await page.waitForTimeout(30); }
      const mid = await snap(page);
      await shot(page, 'ambient-drag');
      await page.mouse.up();
      await page.waitForTimeout(200);
      const after = await snap(page);
      return { verdict: { handSlot: r, ghostWhileDrag: mid.ghost, handBefore: before.handN, handAfter: after.handN, cellsBefore: before.cells, cellsAfter: after.cells } };
    }
  });

  await rec('C3', '创世 → 返回当前界（世界是否完好 / RNG 是否被改）', {
    opts: { land: true },
    async run(page, shot) {
      await page.evaluate(() => { for (let i = 0; i < 6; i++) window.__dbg.S.cmd.tick(1); window.__dbg.UI.updateHud(); });
      const before = await snap(page);
      await press(page, 'g');
      await page.waitForTimeout(250);
      const inCreator = await snap(page);
      const hasBack = await page.$('[data-c="back"]') != null;
      await shot(page, 'creator-fromworld');
      await page.click('[data-c="back"]');
      await page.waitForTimeout(200);
      const back = await snap(page);
      /* 再来一次：重掷后返回 */
      await press(page, 'g');
      await page.waitForTimeout(200);
      await page.click('[data-c="reroll"]');
      await page.waitForTimeout(250);
      const rolled = await snap(page);
      await page.click('[data-c="back"]');
      await page.waitForTimeout(200);
      const back2 = await snap(page);
      return { verdict: { beforeDigest: before.digest, fromWorld: inCreator.fromWorld, hasBack, afterBackDigest: back.digest, createdWhileOn: inCreator.creator, digestAfterRerollBack: back2.digest, rngBefore: before.digest.rng, rngAfterReroll: rolled.digest.rng, rngAfterBack: back2.digest.rng } };
    }
  });

  await rec('C4', '空世界按 G：进得去出不来？', {
    opts: { land: false },
    async run(page, shot) {
      const before = await snap(page);
      await press(page, 'g');
      await page.waitForTimeout(250);
      const inC = await snap(page);
      const hasBack = await page.$('[data-c="back"]') != null;
      await shot(page, 'creator-emptyworld');
      await press(page, 'Escape');
      await page.waitForTimeout(200);
      const afterEsc = await snap(page);
      await press(page, 'Escape');
      await page.waitForTimeout(200);
      const afterEsc2 = await snap(page);
      return { verdict: { cells: before.cells, fromWorld: inC.fromWorld, hasBack, creatorAfterEsc: afterEsc.creator, creatorAfterEsc2: afterEsc2.creator } };
    }
  });

  await rec('C5', '创世 → 进入新区（旧世界替换 / 抽卡残局是否跟随）', {
    opts: { land: true },
    async run(page, shot) {
      await page.evaluate(() => window.__dbg.UI.openPanel('pack'));
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(150);
      const packed = await snap(page);
      await page.keyboard.press('Escape');
      await page.evaluate(() => { for (let i = 0; i < 3; i++) window.__dbg.S.cmd.tick(1); });
      await press(page, 'g');
      await page.waitForTimeout(250);
      const c1 = await snap(page);
      await page.click('[data-c="enter"]');
      await page.waitForTimeout(200);
      const armed = await snap(page);
      await page.click('[data-c="enter"]');
      await page.waitForTimeout(300);
      const world2 = await snap(page);
      await press(page, 'b');
      await page.waitForTimeout(200);
      const pack2 = await snap(page);
      await shot(page, 'creator-new-world-pack');
      return { verdict: { packStageBefore: packed.packStage, packCardsBefore: packed.packCards, creatorArmed: armed.armed, monthAfter: world2.month, cellsAfter: world2.cells, essenceAfter: world2.essence, gear: world2.gear, panel2: pack2.panel, stage2: pack2.packStage, cards2: pack2.packCards } };
    }
  });

  await rec('C6', '事件：三路各一次 / Esc / 再触发 / 与面板叠加', {
    opts: { land: true },
    async run(page, shot) {
      const out = [];
      /* 承受 */
      await press(page, 'e');
      await page.waitForTimeout(200);
      const e1 = await snap(page);
      await shot(page, 'event-bear');
      await page.click('[data-ev="bear"]');
      await page.waitForTimeout(200);
      out.push({ route: 'bear', opened: e1.event, after: (await snap(page)).event });
      /* 改写 */
      await press(page, 'e');
      await page.waitForTimeout(200);
      await page.click('[data-ev="rewrite"]');
      await page.waitForTimeout(200);
      out.push({ route: 'rewrite', after: (await snap(page)).event });
      /* Esc */
      await press(page, 'e');
      await page.waitForTimeout(200);
      await press(page, 'Escape');
      await page.waitForTimeout(150);
      out.push({ route: 'esc', after: (await snap(page)).event });
      /* 再触发 */
      await press(page, 'e');
      await page.waitForTimeout(200);
      const again = await snap(page);
      out.push({ route: 're-trigger', event: again.event });
      /* 事件开着开面板 */
      await press(page, 'l');
      await page.waitForTimeout(200);
      const stack = await snap(page);
      await shot(page, 'event-panel-stack');
      await press(page, 'Escape');
      await page.waitForTimeout(200);
      const esc2 = await snap(page);
      return { verdict: { out, stackEvent: stack.event, stackPanel: stack.panel, stackOverlay: stack.overlay, afterEscEvent: esc2.event, afterEscPanel: esc2.panel } };
    }
  });

  /* ═════════ 键盘 ═════════ */

  await rec('D1', 'WASD → Enter 施卡 → Esc / 面板里 WASD / 面板里空格', {
    opts: { land: true },
    async run(page, shot) {
      await page.evaluate(() => { const S = window.__dbg.S; S.W.cells.clear(); S.cmd.cast(window.__dbg.D.byId.get('terrain.earth.0.matter.land'), 0, 0); window.__dbg.UI.renderHand(); });
      const c0 = (await snap(page)).cursor;
      await press(page, 'w'); await press(page, 'w'); await press(page, 'd');
      await page.waitForTimeout(200);
      const moved = await snap(page);
      await shot(page, 'cursor-moved');
      const cellsBefore = moved.cells;
      await page.evaluate(() => { const S = window.__dbg.S; S.W.hand[0] = window.__dbg.D.byId.get('terrain.earth.0.matter.land'); S.W.sel = 0; window.__dbg.UI.renderHand(); });
      await press(page, 'Enter');
      await page.waitForTimeout(250);
      const casted = await snap(page);
      await press(page, 'Escape');
      await page.waitForTimeout(150);
      const esc = await snap(page);
      /* 面板里按 WASD / 空格 */
      await press(page, 'l');
      await page.waitForTimeout(150);
      const inPanel = await snap(page);
      await press(page, 'w'); await press(page, 's');
      await page.waitForTimeout(200);
      const movedInPanel = await snap(page);
      const cellsPreSpace = movedInPanel.cells, handPre = movedInPanel.handN;
      await press(page, ' ');
      await page.waitForTimeout(250);
      const spaceInPanel = await snap(page);
      await shot(page, 'panel-space-leak');
      return { verdict: { cursor0: c0, cursorMoved: moved.cursor, cellsBefore, cellsAfterEnter: casted.cells, cursorInPanel: inPanel.cursor, cursorAfterWASDinPanel: movedInPanel.cursor, panelStill: movedInPanel.panel, cellsPreSpace, cellsPostSpace: spaceInPanel.cells, handPre, handPost: spaceInPanel.handN } };
    }
  });

  await rec('D2', 'Tab 顺序：面板开着时焦点是否跑到背景', {
    opts: { land: true },
    async run(page, shot) {
      await press(page, 'l');
      await page.waitForTimeout(200);
      const seq = [];
      for (let i = 0; i < 14; i++) {
        await page.keyboard.press('Tab');
        await page.waitForTimeout(40);
        seq.push(await page.evaluate(() => {
          const a = document.activeElement;
          if (!a) return '-';
          const inPanel = !!a.closest('#panel');
          return (a.id || a.className || a.tagName).toString().slice(0, 26) + (inPanel ? '@panel' : '@bg');
        }));
      }
      await shot(page, 'tab-order');
      return { verdict: { seq, inPanel: seq.filter(s => s.includes('@panel')).length, bg: seq.filter(s => s.includes('@bg')).length } };
    }
  });

  await rec('D3', '拖拽中按功能键（Space/X/B/G/P/1）', {
    opts: { land: true },
    async run(page, shot) {
      async function startDrag() {
        const r = await page.evaluate(() => {
          const el = document.querySelectorAll('.handslot')[0];
          const b = el.getBoundingClientRect();
          return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
        });
        await page.mouse.move(r.x, r.y);
        await page.mouse.down();
        for (let i = 1; i <= 4; i++) { await page.mouse.move(r.x + i * 20, r.y - i * 50); await page.waitForTimeout(25); }
        return r;
      }
      const out = [];
      /* Space：拖拽中随机释放 */
      let before = await snap(page);
      await startDrag();
      await press(page, ' ');
      await page.waitForTimeout(220);
      const mid = await snap(page);
      await page.mouse.move(VW / 2, VH * 0.4);
      await page.waitForTimeout(80);
      await page.mouse.up();
      await page.waitForTimeout(250);
      const after = await snap(page);
      await shot(page, 'drag-space');
      out.push({ key: 'Space', handBefore: before.handN, handMid: mid.handN, handAfter: after.handN, cellsBefore: before.cells, cellsMid: mid.cells, cellsAfter: after.cells, ghostMid: mid.ghost, ghostAfter: after.ghost });
      /* X：拖拽中回收 */
      before = after;
      await startDrag();
      await press(page, 'x');
      await page.waitForTimeout(200);
      const midX = await snap(page);
      await page.mouse.move(VW / 2, VH * 0.42);
      await page.mouse.up();
      await page.waitForTimeout(250);
      const afterX = await snap(page);
      out.push({ key: 'X', handBefore: before.handN, handMid: midX.handN, handAfter: afterX.handN, cellsAfter: afterX.cells, sel: afterX.sel, ghostAfter: afterX.ghost });
      /* B：拖拽中开面板 */
      before = afterX;
      await startDrag();
      await press(page, 'b');
      await page.waitForTimeout(200);
      const midB = await snap(page);
      await page.mouse.move(VW / 2, VH * 0.42);
      await page.mouse.up();
      await page.waitForTimeout(250);
      const afterB = await snap(page);
      out.push({ key: 'B', panelMid: midB.panel, handBefore: before.handN, handAfter: afterB.handN, cellsBefore: before.cells, cellsAfter: afterB.cells, panelAfter: afterB.panel, ghostAfter: afterB.ghost });
      await shot(page, 'drag-b');
      return { verdict: out };
    }
  });

  await rec('D4', '改绑到 WASD / 1 / E 的冲突检查', {
    opts: { land: true },
    async run(page, shot) {
      await press(page, 'k');
      await page.waitForTimeout(150);
      async function rebind(act, key) {
        await page.evaluate(a => document.querySelector('[data-k="' + a + '"]').click(), act);
        await page.waitForTimeout(80);
        await page.keyboard.press(key);
        await page.waitForTimeout(180);
        return page.evaluate(() => {
          const UI = window.__dbg.UI;
          return { pack: UI.KEYS.find(k => k.act === 'pack').key, recycle: UI.KEYS.find(k => k.act === 'recycle').key, toasts: [...document.querySelectorAll('#phead .toast, #toasts .toast')].map(t => t.textContent.trim()) };
        });
      }
      const r1 = await rebind('pack', 'w');
      const r2 = await rebind('recycle', '1');
      await shot(page, 'rebind-conflict');
      /* 关面板后按 W / 1，看新键是否生效 */
      await press(page, 'Escape');
      await page.waitForTimeout(120);
      const c0 = (await snap(page)).cursor;
      await press(page, 'w');
      await page.waitForTimeout(150);
      const c1 = (await snap(page)).cursor;
      const p0 = (await snap(page)).panel;
      await press(page, '1');
      await page.waitForTimeout(150);
      const s1 = await snap(page);
      return { verdict: { rebindToW: r1, rebindTo1: r2, cursorBeforeW: c0, cursorAfterW: c1, panelAfterW: p0, selAfterKey1: s1.sel } };
    }
  });

  /* ═════════ 时间 / 数值 ═════════ */

  await rec('E1', '暂停 → 施卡 → 恢复', {
    opts: { land: true, gear: 0 },
    async run(page, shot) {
      const a = await snap(page);
      const res = await page.evaluate(() => {
        const { S, UI } = window.__dbg;
        const i = S.W.hand.findIndex(c => c.sim.op === 'land');
        const before = { month: S.W.month, hand: S.W.hand.length };
        const r = UI.cast(i >= 0 ? i : 0, null, { x: 8, y: 8 });
        return { before, monthAfter: S.W.month, handAfter: S.W.hand.length, r };
      });
      await page.waitForTimeout(400);
      const b = await snap(page);
      await shot(page, 'paused-cast');
      await page.evaluate(() => { window.__dbg.S.setGear(2); window.__dbg.UI.syncGears(); });
      await page.waitForTimeout(400);
      const c = await snap(page);
      return { verdict: { gear0: a.gear, paused: a.paused, monthBefore: a.month, monthAfterCast: res.monthAfter, monthAfterWait: b.month, resumeGear: c.gear, monthResumed: c.month } };
    }
  });

  await rec('E2', '4× → 连续施卡', {
    opts: { land: true, gear: 4 },
    async run(page, shot) {
      const a = await snap(page);
      const out = await page.evaluate(() => {
        const { S, UI } = window.__dbg;
        const rows = [];
        for (let k = 0; k < 6; k++) {
          const i = S.W.hand.length ? k % S.W.hand.length : -1;
          if (i < 0) break;
          const r = S.cmd.playSlot(i, 6 + k, 6);
          rows.push({ k, ok: r.ok, msg: r.msg || '', month: S.W.month, hand: S.W.hand.length });
          UI.renderHand(); UI.updateHud();
        }
        return rows;
      });
      await page.waitForTimeout(400);
      const b = await snap(page);
      await shot(page, 'speed4-cast');
      return { verdict: { gear: a.gear, rows: out, monthDelta: b.month - a.month, handAfter: b.handN } };
    }
  });

  await rec('E3', '时素归零 → 还能做什么', {
    opts: { land: true, essence: 0 },
    async run(page, shot) {
      const a = await snap(page);
      const ui = await page.evaluate(() => ({
        packBtn: { text: document.getElementById('bPack').textContent, disabled: document.getElementById('bPack').disabled },
        packNextDisabled: (() => { window.__dbg.UI.openPanel('pack'); const b = document.querySelector('[data-act="packNext"]'); return b && b.disabled; })(),
        rerollTitle: document.querySelector('[data-act="reroll"]').title
      }));
      await page.evaluate(() => { const b = document.querySelector('[data-act="packNext"]'); if (b && !b.disabled) b.click(); });
      await page.waitForTimeout(150);
      const after = await snap(page);
      const rerollRes = await page.evaluate(() => { window.__dbg.UI.reroll(); return window.__dbg.S.W.essence; });
      await page.keyboard.press('Escape');
      await page.waitForTimeout(100);
      const evDisabled = await page.evaluate(() => { window.__dbg.UI.fireEvent(); const b = document.querySelector('[data-ev="rewrite"]'); return b ? b.disabled : 'no-event'; });
      await page.evaluate(() => window.__dbg.UI.closeEvent());
      await shot(page, 'zero-essence');
      return { verdict: { essence: a.essence, ui, panelAfterClick: after.panel, stage: after.packStage, essenceAfterReroll: rerollRes, rewriteDisabled: evDisabled } };
    }
  });

  await rec('E4', '牌池抽空后抽卡', {
    opts: { land: true },
    async run(page, shot) {
      const setup = await page.evaluate(() => {
        const { S, UI } = window.__dbg;
        const pool = S.drawPool();
        S.W.hand = pool.slice();          // 池里的卡全在手牌里 → drawOne 永远返回 null
        S.W.sel = 0;
        UI.renderHand(); UI.updateHud();
        return { poolN: pool.length, handN: S.W.hand.length, essence: S.W.essence };
      });
      await page.evaluate(() => window.__dbg.UI.openPanel('pack'));
      const txt = await page.textContent('#pbody');
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(200);
      const opened = await snap(page);
      const label = await page.textContent('[data-act="packNext"]');
      const picks = await page.$$eval('.packpick', e => e.length);
      await shot(page, 'empty-pool');
      await page.click('[data-act="packNext"]');
      await page.waitForTimeout(150);
      const skipped = await snap(page);
      await page.evaluate(() => window.__dbg.UI.openPanel('pack'));
      await page.waitForTimeout(150);
      const re = await snap(page);
      return { verdict: { poolN: setup.poolN, handN: setup.handN, essenceBefore: setup.essence, essenceAfter: opened.essence, stage: opened.packStage, cards: opened.packCards, picks, label, afterSkipPanel: skipped.panel, afterSkipStage: skipped.packStage, reopenStage: re.packStage, poolText: (txt.match(/池\s*(\d+)\s*\/\s*(\d+)/) || []).slice(1) } };
    }
  });

  fs.writeFileSync(path.join(__dirname, 'state-probe.json'), JSON.stringify(results, null, 1));
  const bad = results.filter(r => !r.ok);
  log('---');
  log('scenarios', results.length, 'flagged', bad.length);
  bad.forEach(b => log('  ✗', b.id, b.title, b.err ? b.err.split('\n')[0] : ''));
  await browser.close();
})();
