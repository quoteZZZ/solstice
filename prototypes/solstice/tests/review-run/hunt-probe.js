/* 新问题猎手取证脚本：只读页面，不改源码。
   node tests/review-run/hunt-probe.js --w=1080 --h=700 --tag=p1 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const argv = process.argv.slice(2);
const arg = (k, d) => {
  const eq = argv.find(a => a.startsWith('--' + k + '='));
  return eq ? eq.slice(k.length + 3) : d;
};
const VW = +arg('w', 1080), VH = +arg('h', 700), TAG = arg('tag', 'p1');
const OUT = path.join(__dirname, 'review-run', 'hunt-shots');
fs.mkdirSync(OUT, { recursive: true });
const REP = { tag: TAG, viewport: [VW, VH], steps: [], console: [] };

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
  page.on('pageerror', e => REP.console.push('[PAGEERROR] ' + e.message));
  page.on('console', m => { if (m.type() === 'error') REP.console.push('[console] ' + m.text()); });

  let n = 0;
  const shot = async (name, clip) => {
    n++;
    const f = TAG + '-' + String(n).padStart(2, '0') + '-' + name + '.png';
    const opts = { path: path.join(OUT, f) };
    if (clip) opts.clip = clip;
    await page.screenshot(opts);
    return f;
  };
  const rec = async (name, fn) => {
    try {
      const data = await fn();
      REP.steps.push({ name, data });
      console.log('OK  ', name, data === undefined ? '' : JSON.stringify(data).slice(0, 300));
      return data;
    } catch (e) {
      REP.steps.push({ name, error: String(e && e.message) });
      console.log('ERR ', name, e && e.message);
    }
  };
  const ev = (fn, a) => page.evaluate(fn, a);

  await page.goto(PAGE);
  await page.waitForTimeout(1500);
  await ev(() => { const U = window.__dbg.UI; U.enterFromCreator(); U.updateHud(); U.renderHand(); });
  await page.waitForTimeout(350);

  /* 造一片地 + 999 时素 */
  await ev(() => {
    const { S, D, UI } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -3; x <= 3; x++) for (let y = -3; y <= 3; y++) S.cmd.cast(land, x, y);
    S.W.essence = 999;
    UI.updateHud(); UI.renderRight(); UI.renderHand();
  });
  await page.waitForTimeout(300);

  const state = () => ev(() => {
    const { S, W } = window.__dbg;
    const g = document.querySelector('#ghost');
    const slots = [...document.querySelectorAll('.handslot')].map(e => {
      const b = e.getBoundingClientRect(); return { x: Math.round(b.x), y: Math.round(b.y), w: Math.round(b.width), h: Math.round(b.height) };
    });
    const rp = document.getElementById('rightpanel').getBoundingClientRect();
    const lr = document.getElementById('leftrail').getBoundingClientRect();
    const hw = document.getElementById('handwrap').getBoundingClientRect();
    return {
      cells: S.W.cells.size, hand: S.W.hand.length, sel: S.W.sel, month: S.W.month, ess: S.W.essence,
      ghost: document.querySelectorAll('#ghost').length,
      ghostTip: g ? (g.querySelector('.tip') || {}).textContent : null,
      ghostRec: g ? g.classList.contains('rec') : null,
      toastTop: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()),
      toastHead: [...document.querySelectorAll('#phead .toast.ih')].map(e => e.textContent.trim()),
      panel: window.__dbg.UI.state.panel, stage: window.__dbg.UI.packStage,
      packCards: (window.__dbg.UI.packCards || []).length,
      slots, handwrap: { x: Math.round(hw.x), y: Math.round(hw.y), w: Math.round(hw.width), h: Math.round(hw.height) },
      right: { x: Math.round(rp.x), w: Math.round(rp.width) },
      rail: { x: Math.round(lr.x), w: Math.round(lr.width) },
      sw: document.getElementById('handwrap').scrollWidth,
      cw: document.getElementById('handwrap').clientWidth
    };
  });

  const slotCenter = async (i = 0) => ev(i => {
    const e = document.querySelectorAll('.handslot')[i];
    const b = e.getBoundingClientRect();
    return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
  }, i);

  const forceLand = async () => ev(() => {
    const { S, D, UI } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    S.W.hand[0] = land;
    S.W.sel = 0;
    UI.renderHand();
  });
  const findPoint = async (want) => ev(want => {
    const { R } = window.__dbg;
    const rc = document.getElementById('cv').getBoundingClientRect();
    const hw = document.getElementById('handwrap').getBoundingClientRect();
    for (let gy = 0.1; gy < 0.95; gy += 0.04) {
      for (let gx = 0.06; gx < 0.94; gx += 0.04) {
        const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy;
        if (y > hw.top - 14) continue;
        const hit = !!R.cam.pickAt(x, y);
        if (hit === want) return { x: Math.round(x), y: Math.round(y) };
      }
    }
    return null;
  }, want);

  const dragTo = async (to, opts = {}) => {
    await forceLand();
    await page.waitForTimeout(60);
    const c = await slotCenter(0);
    await page.mouse.move(c.x, c.y);
    await page.mouse.down();
    await page.mouse.move(to.x, to.y, { steps: 8 });
    await page.waitForTimeout(140);
    const mid = await state();
    let file = null;
    if (opts.shot) file = await shot(opts.shot, opts.clip);
    if (opts.beforeUp) await opts.beforeUp();
    await page.mouse.up();
    await page.waitForTimeout(380);
    const after = await state();
    return { mid, after, file, from: c };
  };

  /* ───────────── 1. 手牌 1080 布局 ───────────── */
  await rec('hand-layout-1080', async () => {
    await forceLand();
    const st = await state();
    const file = await shot('hand-layout', { x: 620, y: 440, width: 300, height: 260 });
    const z = await ev(() => {
      const hw = document.getElementById('handwrap').getBoundingClientRect();
      const rp = document.getElementById('rightpanel').getBoundingClientRect();
      const slots = [...document.querySelectorAll('.handslot')];
      const last = slots[slots.length - 1].getBoundingClientRect();
      const first = slots[0].getBoundingClientRect();
      const pts = [
        [rp.left + 2, hw.top + 30], [rp.left + 10, hw.top + 60],
        [hw.left + 2, hw.top + 40], [hw.left + 8, hw.top + 120]
      ].map(([x, y]) => {
        const el = document.elementFromPoint(x, y);
        return { x, y, el: el ? (el.id || el.className || el.tagName) : null };
      });
      return { first: { x: Math.round(first.x), right: Math.round(first.right) }, last: { x: Math.round(last.x), right: Math.round(last.right) }, rpLeft: Math.round(rp.left), pts };
    });
    return { slots: st.slots, sw: st.sw, cw: st.cw, right: st.right, z, file };
  });

  /* ───────────── 2. 摆件模式残留 ───────────── */
  await rec('ambient-residue', async () => {
    await ev(() => window.__dbg.UI.toggleAmbient());
    await page.waitForTimeout(900);
    const st = await state();
    const hw = await ev(() => {
      const e = document.getElementById('handwrap');
      return { transform: getComputedStyle(e).transform, top: e.getBoundingClientRect().top, h: e.getBoundingClientRect().height };
    });
    const a = await shot('ambient-full');
    const b = await shot('ambient-bl', { x: 0, y: 620, width: 260, height: 80 });
    const c = await shot('ambient-br', { x: 760, y: 620, width: 320, height: 80 });
    await ev(() => window.__dbg.UI.toggleAmbient());
    await page.waitForTimeout(600);
    return { hw, slots: st.slots, files: [a, b, c] };
  });

  /* ───────────── 3. 面板 toast 是否被 renderOverlay 洗掉 ───────────── */
  await rec('pack-toast-tear', async () => {
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(250);
    const before = await ev(() => ({ stage: window.__dbg.UI.packStage, ess: window.__dbg.S.W.essence }));
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    const immediate = await ev(() => ({
      head: [...document.querySelectorAll('#phead .toast.ih')].map(e => e.textContent),
      top: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent),
      stage: window.__dbg.UI.packStage, cards: window.__dbg.UI.packCards.length,
      ess: window.__dbg.S.W.essence
    }));
    await page.waitForTimeout(120);
    const later = await ev(() => ([...document.querySelectorAll('#phead .toast.ih')].length));
    const f = await shot('pack-deal-phead', { x: 40, y: 60, width: 1000, height: 120 });
    const f2 = await shot('pack-deal-full');
    return { before, immediate, later, files: [f, f2] };
  });

  await rec('pack-toast-pick', async () => {
    const st0 = await state();
    await ev(() => document.querySelector('.packpick').click());
    const immediate = await ev(() => ({
      head: [...document.querySelectorAll('#phead .toast.ih')].length,
      top: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent),
      panel: window.__dbg.UI.state.panel, hand: window.__dbg.S.W.hand.length,
      selName: window.__dbg.S.W.hand[window.__dbg.S.W.sel].name.zh
    }));
    await page.waitForTimeout(120);
    const f = await shot('pack-picked-full');
    return { before: st0, immediate, file: f };
  });

  /* 切包被拦截：toast 进 #phead，检查头部是否错位 */
  await rec('pack-block-switch-toast', async () => {
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(200);
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(200);
    const st = await ev(() => ({
      stage: window.__dbg.UI.packStage, cards: window.__dbg.UI.packCards.length
    }));
    await ev(() => document.querySelector('[data-pack="advanced"]')?.click() || document.querySelectorAll('[data-pack]')[1].click());
    await page.waitForTimeout(120);
    const head = await ev(() => {
      const h = document.getElementById('phead');
      const t = h.querySelector('.sm');
      const chip = h.querySelector('.toast.ih');
      const close = h.querySelector('.close');
      const r = e => e ? { x: Math.round(e.getBoundingClientRect().x), w: Math.round(e.getBoundingClientRect().width), h: Math.round(e.getBoundingClientRect().height) } : null;
      return { title: r(t), chip: r(chip), close: r(close), headH: Math.round(h.getBoundingClientRect().height), sh: h.scrollHeight, ch: h.clientHeight, text: chip ? chip.textContent : null, packSel: window.__dbg.UI.packSel, stage: window.__dbg.UI.packStage };
    });
    const f = await shot('pack-block-switch', { x: 40, y: 60, width: 1000, height: 130 });
    const f2 = await shot('pack-block-switch-full');
    return { st, head, files: [f, f2] };
  });

  /* 长 toast 撑高 #phead？ */
  await rec('phead-long-toast', async () => {
    await ev(() => {
      window.__dbg.UI.toast('图鉴点亮', '生态 · 「长夜里的第一株苔藓与它的伴生菌」　18 / 42');
      window.__dbg.UI.toast('图鉴点亮', '气候 · 「连续十二个月的雨季」　19 / 42');
      window.__dbg.UI.toast('图鉴点亮', '文明 · 「第一座聚落的第一口井」　20 / 42');
    });
    await page.waitForTimeout(100);
    const head = await ev(() => {
      const h = document.getElementById('phead');
      const r = e => e ? { x: Math.round(e.getBoundingClientRect().x), w: Math.round(e.getBoundingClientRect().width), h: Math.round(e.getBoundingClientRect().height), right: Math.round(e.getBoundingClientRect().right) } : null;
      return {
        title: r(h.querySelector('.sm')), chip: r(h.querySelector('.toast.ih')), close: r(h.querySelector('.close')),
        head: r(h), chips: h.querySelectorAll('.toast.ih').length,
        titleScrollW: h.querySelector('.sm').scrollWidth, titleClientW: h.querySelector('.sm').clientWidth,
        chipText: h.querySelector('.toast.ih') ? h.querySelector('.toast.ih').textContent : null
      };
    });
    const f = await shot('phead-long', { x: 40, y: 60, width: 1000, height: 140 });
    await ev(() => window.__dbg.UI.closePanel());
    return { head, file: f };
  });

  /* ───────────── 4. 抽卡阶段保持 / 跳过 ───────────── */
  await rec('pack-stage-esc-reopen', async () => {
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(150);
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(150);
    const afterTear = await ev(() => ({ stage: window.__dbg.UI.packStage, cards: window.__dbg.UI.packCards.length, ess: window.__dbg.S.W.essence }));
    await page.keyboard.press('Escape');
    await page.waitForTimeout(150);
    const closed = await ev(() => ({ panel: window.__dbg.UI.state.panel, stage: window.__dbg.UI.packStage, cards: window.__dbg.UI.packCards.length }));
    await page.keyboard.press('b');
    await page.waitForTimeout(200);
    const reopened = await ev(() => ({ panel: window.__dbg.UI.state.panel, stage: window.__dbg.UI.packStage, cards: window.__dbg.UI.packCards.length, picks: document.querySelectorAll('.packpick').length }));
    const f = await shot('pack-reopen');
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(100);
    const skipped = await ev(() => ({ panel: window.__dbg.UI.state.panel, stage: window.__dbg.UI.packStage, cards: window.__dbg.UI.packCards.length, ess: window.__dbg.S.W.essence, head: [...document.querySelectorAll('#phead .toast.ih')].length, top: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent) }));
    return { afterTear, closed, reopened, skipped, file: f };
  });

  await rec('pack-close-reopen-x', async () => {
    await ev(() => { window.__dbg.S.W.essence = 999; window.__dbg.UI.openPanel('pack'); });
    await page.waitForTimeout(150);
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(120);
    await ev(() => window.__dbg.UI.closePanel());
    await page.waitForTimeout(120);
    const a = await ev(() => ({ stage: window.__dbg.UI.packStage, cards: window.__dbg.UI.packCards.length, toasts: document.querySelectorAll('#toasts .toast').length }));
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(150);
    const b = await ev(() => ({ stage: window.__dbg.UI.packStage, cards: window.__dbg.UI.packCards.length, picks: document.querySelectorAll('.packpick').length, title: document.querySelector('#phead .sm').textContent }));
    await ev(() => window.__dbg.UI.closePanel());
    return { a, b };
  });

  /* 面板开着时按 X：带动作的 toast 仍发到 #toasts（z70 盖住面板 60） */
  await rec('act-toast-over-panel', async () => {
    await ev(() => { window.__dbg.S.W.essence = 999; window.__dbg.UI.openPanel('codex'); });
    await page.waitForTimeout(250);
    const before = await ev(() => ({ panel: window.__dbg.UI.state.panel, hand: window.__dbg.S.W.hand.length }));
    await page.keyboard.press('x');
    await page.waitForTimeout(180);
    const after = await ev(() => {
      const t = document.querySelector('#toasts .toast');
      const p = document.getElementById('panel');
      const r = e => e ? e.getBoundingClientRect() : null;
      const a = r(t), b = r(p);
      return {
        panel: window.__dbg.UI.state.panel, hand: window.__dbg.S.W.hand.length,
        toastTop: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()),
        overlap: !!(a && b && a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top),
        toast: a ? { x: Math.round(a.x), y: Math.round(a.y), w: Math.round(a.width), h: Math.round(a.height) } : null,
        panelRect: b ? { x: Math.round(b.x), y: Math.round(b.y) } : null
      };
    });
    const f = await shot('act-toast-over-panel');
    await ev(() => window.__dbg.UI.closePanel());
    return { before, after, file: f };
  });

  /* 面板开着时按空格：施卡到面板背后的世界 */
  await rec('space-cast-over-panel', async () => {
    await ev(() => {
      const { S, D, UI } = window.__dbg;
      S.W.hand[0] = D.byId.get('terrain.earth.0.matter.land');
      S.W.sel = 0; UI.renderHand(); UI.openPanel('pack');
    });
    await page.waitForTimeout(200);
    const before = await ev(() => ({ cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length }));
    await page.keyboard.press(' ');
    await page.waitForTimeout(300);
    const after = await ev(() => ({ cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length, panel: window.__dbg.UI.state.panel }));
    await ev(() => window.__dbg.UI.closePanel());
    return { before, after };
  });

  /* ───────────── 5. 月份写法 ───────────── */
  await rec('months', async () => {
    const out = [];
    for (const m of [0, 11, 12, 13, 23, 24]) {
      const row = await ev(m => {
        const { S, UI } = window.__dbg;
        S.W.month = m;
        UI.updateHud();
        UI.renderEvent && UI.fireEvent();
        const evText = document.querySelector('#eventcard .row .xs:last-child');
        const clock = document.getElementById('cYear').textContent + '年' + document.getElementById('cMonth').textContent + '月';
        const r = { m, mon: UI.mon(m), clock, event: evText ? evText.textContent.trim() : null, trace: null };
        UI.closeEvent();
        return r;
      }, m);
      out.push(row);
    }
    return out;
  });

  await rec('month-0-event-shot', async () => {
    await ev(() => { window.__dbg.S.W.month = 0; window.__dbg.UI.updateHud(); window.__dbg.UI.fireEvent(); });
    await page.waitForTimeout(150);
    const f = await shot('event-month0');
    await ev(() => window.__dbg.UI.closeEvent());
    return f;
  });

  /* 跨年纪年条目顺序 */
  await rec('chron-year-order', async () => {
    return ev(() => {
      const { S, UI } = window.__dbg;
      S.W.month = 11;
      S.cmd.tick(); // month 12
      UI.updateHud();
      const rows = S.W.chron.slice(0, 8).map(c => ({ m: c.month, mon: UI.mon(c.month), kind: c.kind, text: c.text }));
      const kinds = ['全部', '干涉', '气候', '生态', '灾变', '文明', '图鉴'];
      const sums = kinds.slice(1).reduce((a, k) => a + S.W.chron.filter(c => c.kind === k).length, 0);
      return { rows, total: S.W.chron.length, filterSum: sums, hasYearFilter: kinds.includes('纪年') };
    });
  });

  /* ───────────── 6. 土地卡打在已有地上：有无回执 ───────────── */
  await rec('land-touched-receipt', async () => {
    return ev(() => {
      const { S, D, UI } = window.__dbg;
      const till = D.byId.get('terrain.earth.1.matter.mound') || D.cards.find(c => c.sim.op === 'land' && c.sim.rise);
      S.W.hand[0] = till; S.W.sel = 0; UI.renderHand();
      S.W.essence = 999;
      const cell = S.at(0, 0);
      const before = { hand: S.W.hand.length, cells: S.W.cells.size };
      const gain = document.getElementById('gainfly');
      if (gain) { gain.textContent = ''; gain.style.opacity = 0; }
      UI.cast(0, cell, null);
      const log = document.getElementById('log');
      return {
        card: till.name.zh, op: till.sim.op,
        before, after: { hand: S.W.hand.length, cells: S.W.cells.size },
        gainfly: gain ? { text: gain.textContent, opacity: getComputedStyle(gain).opacity } : null,
        lastLog: log ? [...log.children].map(e => e.textContent).slice(-3) : null,
        logVisible: log ? getComputedStyle(log).display : null,
        toasts: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim())
      };
    });
  });

  /* ───────────── 7. 拖拽边界 ───────────── */
  const voidPt = await findPoint(false);
  const cellPt = await findPoint(true);
  REP.pts = { voidPt, cellPt };

  await rec('drag-control-world', async () => {
    const before = await state();
    const r = await dragTo(voidPt, { shot: 'drag-world' });
    return { before: { cells: before.cells, hand: before.hand }, mid: { mode: r.mid.ghostTip, rec: r.mid.ghostRec, ghost: r.mid.ghost }, after: { cells: r.after.cells, hand: r.after.hand, toasts: r.after.toastTop }, file: r.file };
  });

  await rec('drag-rightpanel', async () => {
    const before = await state();
    const r = await dragTo({ x: VW - 90, y: 300 }, { shot: 'drag-rightpanel' });
    return { before: { cells: before.cells, hand: before.hand }, mid: { tip: r.mid.ghostTip, mode: r.mid.ghostRec, ghost: r.mid.ghost }, after: { cells: r.after.cells, hand: r.after.hand, toastTop: r.after.toastTop, toastHead: r.after.toastHead }, file: r.file };
  });

  await rec('drag-leftrail', async () => {
    const r = await dragTo({ x: 26, y: 300 }, { shot: 'drag-leftrail' });
    return { mid: { tip: r.mid.ghostTip, ghost: r.mid.ghost }, after: { cells: r.after.cells, hand: r.after.hand, toastTop: r.after.toastTop }, file: r.file };
  });

  await rec('drag-topbar', async () => {
    const r = await dragTo({ x: VW / 2, y: 20 }, { shot: 'drag-topbar' });
    return { mid: { tip: r.mid.ghostTip, ghost: r.mid.ghost }, after: { cells: r.after.cells, hand: r.after.hand, toastTop: r.after.toastTop }, file: r.file };
  });

  await rec('drag-bottom-world', async () => {
    const hwTop = (await state()).handwrap.y;
    const r = await dragTo({ x: 700, y: hwTop - 2 }, { shot: 'drag-bottom-world' });
    return { hwTop, mid: { tip: r.mid.ghostTip, ghost: r.mid.ghost }, after: { cells: r.after.cells, hand: r.after.hand, toastTop: r.after.toastTop }, file: r.file };
  });

  await rec('drag-in-hand', async () => {
    const c = await slotCenter(0);
    const r = await dragTo({ x: c.x + 30, y: c.y - 40 }, { shot: 'drag-in-hand' });
    return { mid: { tip: r.mid.ghostTip, ghost: r.mid.ghost }, after: { cells: r.after.cells, hand: r.after.hand, toastTop: r.after.toastTop }, file: r.file };
  });

  await rec('drag-down-recycle', async () => {
    const r = await dragTo({ x: VW / 2, y: VH - 30 }, { shot: 'drag-down' });
    return { mid: { tip: r.mid.ghostTip, rec: r.mid.ghostRec }, after: { cells: r.after.cells, hand: r.after.hand, ess: r.after.ess, toastTop: r.after.toastTop }, file: r.file };
  });

  await rec('drag-esc-then-again', async () => {
    await forceLand();
    const c = await slotCenter(0);
    await page.mouse.move(c.x, c.y); await page.mouse.down();
    await page.mouse.move(voidPt.x, voidPt.y, { steps: 8 });
    await page.waitForTimeout(100);
    const beforeEsc = await state();
    await page.keyboard.press('Escape');
    await page.waitForTimeout(120);
    const afterEsc = await state();
    await page.mouse.up();
    await page.waitForTimeout(150);
    const afterUp = await state();
    const r2 = await dragTo(voidPt, { shot: 'drag-after-esc' });
    return { beforeEsc: { ghost: beforeEsc.ghost, cells: beforeEsc.cells, hand: beforeEsc.hand }, afterEsc: { ghost: afterEsc.ghost, cells: afterEsc.cells, hand: afterEsc.hand }, afterUp: { ghost: afterUp.ghost, cells: afterUp.cells, hand: afterUp.hand }, again: { after: { cells: r2.after.cells, hand: r2.after.hand } }, file: r2.file };
  });

  await rec('drag-p-ambient', async () => {
    await forceLand();
    const c = await slotCenter(0);
    const before = await state();
    await page.mouse.move(c.x, c.y); await page.mouse.down();
    await page.mouse.move(voidPt.x, voidPt.y, { steps: 8 });
    await page.waitForTimeout(100);
    await page.keyboard.press('p');
    await page.waitForTimeout(500);
    const mid = await state();
    const f = await shot('drag-p-ambient');
    await page.mouse.up();
    await page.waitForTimeout(400);
    const after = await state();
    await ev(() => { if (window.__dbg.UI.state.uiMode !== '全部') window.__dbg.UI.setUiMode('全部'); });
    return { before: { cells: before.cells, hand: before.hand }, mid: { ghost: mid.ghost, tip: mid.ghostTip, panel: mid.panel, handwrap: mid.handwrap }, after: { cells: after.cells, hand: after.hand, ghost: after.ghost, toastTop: after.toastTop }, file: f };
  });

  await rec('drag-resize', async () => {
    await forceLand();
    const c = await slotCenter(0);
    await page.mouse.move(c.x, c.y); await page.mouse.down();
    await page.mouse.move(voidPt.x, voidPt.y, { steps: 8 });
    await page.waitForTimeout(100);
    await page.setViewportSize({ width: 900, height: 620 });
    await page.waitForTimeout(400);
    const mid = await state();
    const f = await shot('drag-resized');
    await page.mouse.move(400, 300, { steps: 4 });
    await page.mouse.up();
    await page.waitForTimeout(400);
    const after = await state();
    await page.setViewportSize({ width: VW, height: VH });
    await page.waitForTimeout(400);
    return { mid: { ghost: mid.ghost, tip: mid.ghostTip, slots: mid.slots.slice(0, 2) }, after: { cells: after.cells, hand: after.hand, ghost: after.ghost, toastTop: after.toastTop }, file: f };
  });

  /* ───────────── 8. 左栏展开（需要 >1080 才看得到 log） ───────────── */
  await rec('rail-hover-1440', async () => {
    await page.setViewportSize({ width: 1440, height: 900 });
    await page.waitForTimeout(700);
    await ev(() => { const { UI } = window.__dbg; UI.renderHand(); UI.updateHud(); UI.renderRight(); });
    await page.waitForTimeout(300);
    const read = () => ev(() => {
      const lr = document.getElementById('leftrail').getBoundingClientRect();
      const log = document.getElementById('log').getBoundingClientRect();
      const label = document.querySelector('#leftrail .rail span');
      const cs = label ? getComputedStyle(label) : null;
      return { railW: Math.round(lr.width * 10) / 10, logLeft: Math.round(log.left * 10) / 10, labelW: cs ? cs.width : null, labelOp: cs ? cs.opacity : null, disp: getComputedStyle(document.getElementById('log')).display, active: document.activeElement ? (document.activeElement.className || document.activeElement.id) : null };
    });
    const before = await read();
    await page.mouse.move(30, 400);
    const t0 = await read();
    await page.waitForTimeout(90);
    const t90 = await read();
    const a = await shot('rail-hover-90');
    await page.waitForTimeout(400);
    const t490 = await read();
    const b = await shot('rail-hover-490');
    await page.mouse.move(700, 300);
    await page.waitForTimeout(500);
    const after = await read();
    /* Tab 到左栏 */
    await ev(() => document.activeElement && document.activeElement.blur());
    const focuses = [];
    for (let i = 0; i < 5; i++) {
      await page.keyboard.press('Tab');
      await page.waitForTimeout(60);
      focuses.push(await read());
    }
    const c = await shot('rail-tab-focus');
    return { before, t0, t90, t490, after, focuses, files: [a, b, c] };
  });

  /* ───────────── 9. 滚动渐隐 ───────────── */
  await rec('scroll-fade-codex', async () => {
    await ev(() => window.__dbg.UI.openPanel('codex'));
    await page.waitForTimeout(350);
    const box = await ev(() => {
      const el = document.querySelector('.cdscroll');
      const b = el.getBoundingClientRect();
      return { x: Math.round(b.x), y: Math.round(b.y), w: Math.round(b.width), h: Math.round(b.height), sh: el.scrollHeight, ch: el.clientHeight, st: el.scrollTop };
    });
    const a = await shot('codex-scroll-top');
    await ev(() => { const el = document.querySelector('.cdscroll'); el.scrollTop = el.scrollHeight; });
    await page.waitForTimeout(250);
    const bottom = await ev(() => {
      const el = document.querySelector('.cdscroll');
      return { st: el.scrollTop, max: el.scrollHeight - el.clientHeight, atEnd: el.scrollTop + el.clientHeight >= el.scrollHeight - 1 };
    });
    const b = await shot('codex-scroll-bottom', { x: box.x - 4, y: box.y + box.h - 90, width: box.w + 8, height: 100 });
    return { box, bottom, files: [a, b] };
  });

  await rec('scroll-fade-rightpanel', async () => {
    await ev(() => window.__dbg.UI.closePanel());
    await page.waitForTimeout(200);
    await ev(() => { const el = document.querySelector('#rightpanel .rp.grow'); if (el) el.scrollTop = el.scrollHeight; });
    await page.waitForTimeout(300);
    const st = await ev(() => {
      const el = document.querySelector('#rightpanel .rp.grow');
      if (!el) return null;
      const b = el.getBoundingClientRect();
      return { sh: el.scrollHeight, ch: el.clientHeight, st: el.scrollTop, atEnd: el.scrollTop + el.clientHeight >= el.scrollHeight - 1, x: Math.round(b.x), y: Math.round(b.y), w: Math.round(b.width), h: Math.round(b.height) };
    });
    const f = await shot('right-scroll-bottom', st ? { x: st.x, y: st.y + st.h - 70, width: st.w, height: 80 } : undefined);
    return { st, file: f };
  });

  await rec('panel-fade-pack', async () => {
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(300);
    const f = await shot('panel-pack-bottom', { x: 40, y: 600, width: 1000, height: 70 });
    await ev(() => window.__dbg.UI.closePanel());
    return f;
  });

  /* ───────────── 10. 图鉴关系链 / 指标条 ───────────── */
  await rec('codex-rel-labels', async () => {
    await ev(() => window.__dbg.UI.openPanel('codex'));
    await page.waitForTimeout(300);
    await ev(() => { const el = document.querySelector('.cdscroll'); el.scrollTop = el.scrollHeight; });
    await page.waitForTimeout(250);
    const data = await ev(() => {
      const el = document.querySelector('.cdscroll');
      const rels = [...el.querySelectorAll('.dex .rl')].map(e => e.textContent.trim());
      const rows = el.querySelectorAll('div[style*="border-top"] > div').length;
      const metricRow = el.querySelector('div[style*="justify-content:space-between"]');
      const cells = [...el.querySelectorAll('div[style*="text-align:center"]')].map(e => e.textContent.replace(/\s+/g, ' ').trim());
      return { rels, metricCells: cells.slice(0, 4), colCount: rows, metricW: metricRow ? Math.round(metricRow.getBoundingClientRect().width) : null };
    });
    const f = await shot('codex-detail-bottom');
    /* 换一张有 reclaim 的卡，看指标条 3 格 */
    const f2 = await ev(() => {
      const { D } = window.__dbg;
      const c = D.cards.find(x => x.reclaim && x.disturb != null);
      if (c) window.__dbg.UI.openCard(c.id);
      return c ? c.name.zh : null;
    });
    await page.waitForTimeout(300);
    await ev(() => { const el = document.querySelector('.cdscroll'); if (el) el.scrollTop = el.scrollHeight; });
    await page.waitForTimeout(200);
    const f3 = await shot('codex-detail-reclaim');
    const data2 = await ev(() => {
      const el = document.querySelector('.cdscroll');
      const cells = [...el.querySelectorAll('div[style*="text-align:center"]')].map(e => e.textContent.replace(/\s+/g, ' ').trim());
      return { metricCells: cells.slice(0, 4), name: el.querySelector('.cgtName') ? el.querySelector('.cgtName').textContent : null };
    });
    await ev(() => window.__dbg.UI.closePanel());
    return { data, cardWithReclaim: f2, data2, files: [f, f3] };
  });

  REP.final = await state();
  fs.writeFileSync(path.join(OUT, TAG + '-report.json'), JSON.stringify(REP, null, 1));
  console.log('--- steps', REP.steps.length, 'console', REP.console.length);
  console.log(JSON.stringify(REP.console.slice(0, 5)));
  await browser.close();
})();
