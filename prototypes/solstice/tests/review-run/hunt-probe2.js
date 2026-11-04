/* 取证 2：修正上一轮被前序状态污染的抽卡流程，并补拖拽/键盘/toast/跨世界。
   node tests/review-run/hunt-probe2.js --w=1080 --h=700 --tag=p2 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const argv = process.argv.slice(2);
const arg = (k, d) => { const eq = argv.find(a => a.startsWith('--' + k + '=')); return eq ? eq.slice(k.length + 3) : d; };
const VW = +arg('w', 1080), VH = +arg('h', 700), TAG = arg('tag', 'p2');
const OUT = path.join(__dirname, 'hunt-shots');
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
    await page.screenshot(clip ? { path: path.join(OUT, f), clip } : { path: path.join(OUT, f) });
    return f;
  };
  const rec = async (name, fn) => {
    try { const d = await fn(); REP.steps.push({ name, data: d }); console.log('OK  ', name, JSON.stringify(d).slice(0, 400)); return d; }
    catch (e) { REP.steps.push({ name, error: String(e && e.message) }); console.log('ERR ', name, e && e.message); }
  };
  const ev = (f, a) => page.evaluate(f, a);

  await page.goto(PAGE); await page.waitForTimeout(1500);
  await ev(() => {
    const { S, D, UI } = window.__dbg;
    UI.enterFromCreator(); UI.updateHud(); UI.renderHand();
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -3; x <= 3; x++) for (let y = -3; y <= 3; y++) S.cmd.cast(land, x, y);
    S.W.essence = 999; UI.updateHud(); UI.renderRight(); UI.renderHand();
    window.__calls = [];
    const orig = UI.cast;
    UI.cast = function (...a) {
      window.__calls.push({ args: a.map(v => v && typeof v === 'object' ? JSON.stringify(v) : v), hand0: S.W.hand[0] && S.W.hand[0].name.zh, stack: new Error().stack.split('\n').slice(1, 5).join(' | ') });
      return orig.apply(this, a);
    };
  });
  await page.waitForTimeout(350);

  const clearToasts = async () => ev(() => { const t = document.getElementById('toasts'); if (t) t.innerHTML = ''; const h = document.getElementById('phead'); if (h) h.querySelectorAll('.toast.ih').forEach(e => e.remove()); });
  const resetPack = async () => ev(() => {
    const { UI, S } = window.__dbg;
    UI.closePanel(); UI.packStage = 0; UI.packCards = []; UI.packSel = 'basic'; UI.renderOverlay(); S.W.essence = 999;
  });
  const state = () => ev(() => {
    const { S } = window.__dbg;
    const g = document.querySelector('#ghost');
    return {
      cells: S.W.cells.size, hand: S.W.hand.length, sel: S.W.sel, ess: S.W.essence,
      handNames: S.W.hand.map(c => c.name.zh),
      ghost: document.querySelectorAll('#ghost').length,
      tip: g ? (g.querySelector('.tip') || {}).textContent : null,
      toastTop: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()),
      toastHead: [...document.querySelectorAll('#phead .toast.ih')].map(e => e.textContent.trim()),
      panel: window.__dbg.UI.state.panel, stage: window.__dbg.UI.packStage,
      cards: (window.__dbg.UI.packCards || []).length, sel2: window.__dbg.UI.packSel
    };
  });
  const slotCenter = async (i = 0) => ev(i => { const e = document.querySelectorAll('.handslot')[i]; const b = e.getBoundingClientRect(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; }, i);
  const put = async (id) => ev(id => {
    const { S, D, UI } = window.__dbg;
    const c = D.byId.get(id) || D.cards.find(x => x.id === id);
    S.W.hand[0] = c; S.W.sel = 0; UI.renderHand(); return c.name.zh;
  }, id);
  const landId = 'terrain.earth.0.matter.land';

  /* ── 1. 抽卡：撕开 → Esc 关闭 → 重开，阶段是否保住 ── */
  await rec('pack-tear-esc-reopen', async () => {
    await resetPack(); await clearToasts();
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(200);
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(200);
    const torn = await state();
    const f1 = await shot('pack-torn-full');
    await page.keyboard.press('Escape');
    await page.waitForTimeout(200);
    const closed = await state();
    await page.keyboard.press('b');
    await page.waitForTimeout(250);
    const reopened = await state();
    const picks = await ev(() => document.querySelectorAll('.packpick').length);
    const f2 = await shot('pack-reopened');
    return { torn, closed, reopened: { ...reopened, picks }, files: [f1, f2] };
  });

  /* ── 2. 跳过这一包 ── */
  await rec('pack-skip', async () => {
    await resetPack(); await clearToasts();
    await ev(() => { window.__dbg.UI.openPanel('pack'); });
    await page.waitForTimeout(150);
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(150);
    const torn = await state();
    const btn = await ev(() => { const b = document.querySelector('[data-act="packNext"]'); return b ? { text: b.textContent, cls: b.className } : null; });
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(300);
    const after = await state();
    const f = await shot('pack-after-skip');
    return { torn: { stage: torn.stage, ess: torn.ess, cards: torn.cards }, btn, after, file: f };
  });

  /* ── 3. 撕开→关→换界→新世界→再开抽卡：上一包的牌还在？ ── */
  await rec('pack-cross-world', async () => {
    await resetPack(); await clearToasts();
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(150);
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(200);
    const torn = await state();
    await ev(() => window.__dbg.UI.closePanel());
    await page.waitForTimeout(150);
    /* 走换界：G 打开创世，再进入（第二次才是真替换，这里已有世界） */
    await page.keyboard.press('g');
    await page.waitForTimeout(300);
    await ev(() => { const { UI } = window.__dbg; UI.enterFromCreator(); UI.enterFromCreator(); });
    await page.waitForTimeout(500);
    const world = await state();
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(300);
    const reopened = await state();
    const picks = await ev(() => [...document.querySelectorAll('.packpick')].map(e => e.textContent.trim().slice(0, 10)));
    const f = await shot('pack-cross-world');
    return { torn: { stage: torn.stage, cards: torn.cards, ess: torn.ess }, world: { cells: world.cells, ess: world.ess, stage: world.stage, cards: world.cards }, reopened: { stage: reopened.stage, cards: reopened.cards, picks }, file: f };
  });

  /* ── 4. 面板开着时按 X（可回收的卡）：toast 是否压在面板上 ── */
  await rec('x-toast-over-panel', async () => {
    await resetPack(); await clearToasts();
    await ev(() => {
      const { S, D, UI } = window.__dbg;
      const c = D.cards.find(x => x.reclaim > 0);
      S.W.hand[0] = c; S.W.sel = 0; S.W.essence = 999;
      UI.renderHand(); UI.openPanel('codex');
    });
    await page.waitForTimeout(350);
    const before = await state();
    await page.keyboard.press('x');
    await page.waitForTimeout(200);
    const after = await ev(() => {
      const t = document.querySelector('#toasts .toast');
      const p = document.getElementById('panel');
      const r = e => e ? e.getBoundingClientRect() : null;
      const a = r(t), b = r(p);
      return {
        hand: window.__dbg.S.W.hand.length, panel: window.__dbg.UI.state.panel,
        toastTop: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()),
        headChip: [...document.querySelectorAll('#phead .toast.ih')].map(e => e.textContent.trim()),
        overlap: !!(a && b && a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top),
        toast: a ? { x: Math.round(a.x), y: Math.round(a.y), w: Math.round(a.width), h: Math.round(a.height) } : null,
        panelRect: b ? { x: Math.round(b.x), y: Math.round(b.y), w: Math.round(b.width), h: Math.round(b.height) } : null
      };
    });
    const f = await shot('x-toast-over-panel');
    await ev(() => window.__dbg.UI.closePanel());
    return { before: { hand: before.hand, sel: before.sel, name: before.handNames[0] }, after, file: f };
  });

  /* ── 5. 面板开着时空格：是否在面板背后施卡 ── */
  await rec('space-behind-panel', async () => {
    await resetPack(); await clearToasts();
    const name = await put('flora.wood.1.matter.grass');
    await ev(() => { const { S, UI } = window.__dbg; S.W.essence = 999; UI.updateHud(); UI.openPanel('pack'); });
    await page.waitForTimeout(300);
    const before = await state();
    const entsBefore = await ev(() => { let n = 0; for (const c of window.__dbg.S.W.cells.values()) n += c.ents.length; return n; });
    await page.keyboard.press(' ');
    await page.waitForTimeout(350);
    const after = await state();
    const entsAfter = await ev(() => { let n = 0; for (const c of window.__dbg.S.W.cells.values()) n += c.ents.length; return n; });
    const f = await shot('space-behind-panel');
    await ev(() => window.__dbg.UI.closePanel());
    return { card: name, before: { hand: before.hand, cells: before.cells, ents: entsBefore }, after: { hand: after.hand, cells: after.cells, ents: entsAfter, toastTop: after.toastTop, toastHead: after.toastHead, panel: after.panel }, file: f };
  });

  /* ── 6. 重抽/换档/事件 是否也穿透面板（一次汇总） ── */
  await rec('keys-through-panel', async () => {
    await resetPack(); await clearToasts();
    await ev(() => { const { S, UI } = window.__dbg; S.W.essence = 999; S.W.rerollN = 0; UI.openPanel('pack'); });
    await page.waitForTimeout(250);
    const a = await state();
    await page.keyboard.press('r');
    await page.waitForTimeout(250);
    const b = await state();
    await page.keyboard.press('j');
    await page.waitForTimeout(150);
    const c = await state();
    await page.keyboard.press('e');
    await page.waitForTimeout(250);
    const d = await ev(() => ({ event: window.__dbg.UI.state.event ? window.__dbg.UI.state.event.t : null, panel: window.__dbg.UI.state.panel, eventWrap: document.getElementById('eventwrap').classList.contains('on') }));
    const f = await shot('keys-through-panel-event');
    await ev(() => { window.__dbg.UI.closeEvent(); window.__dbg.UI.closePanel(); });
    return { a: { ess: a.ess, hand: a.hand, panel: a.panel }, b: { ess: b.ess, hand: b.hand, toastHead: b.toastHead }, c: { ess: c.ess }, d, file: f };
  });

  /* ── 7. 拖拽：Esc 取消 / 双施 / 落到底部带宽 ── */
  const dragWithLog = async (to, beforeUp) => {
    await clearToasts();
    await put(landId);
    await page.waitForTimeout(80);
    await ev(() => { window.__calls = []; });
    const c = await slotCenter(0);
    await page.mouse.move(c.x, c.y); await page.mouse.down();
    await page.mouse.move(to.x, to.y, { steps: 8 });
    await page.waitForTimeout(150);
    const mid = await state();
    if (beforeUp) await beforeUp();
    await page.mouse.up();
    await page.waitForTimeout(450);
    const calls = await ev(() => window.__calls.map(c => ({ hand0: c.hand0, args: c.args, stack: c.stack })));
    return { mid, after: await state(), calls };
  };
  const voidPt = await ev(() => {
    const { R } = window.__dbg; const rc = document.getElementById('cv').getBoundingClientRect();
    for (let gy = .12; gy < .5; gy += .03) for (let gx = .1; gx < .9; gx += .03) {
      const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy;
      if (!R.cam.pickAt(x, y)) return { x: Math.round(x), y: Math.round(y) };
    }
    return null;
  });
  REP.voidPt = voidPt;

  await rec('drag-esc-cast', async () => {
    const r = await dragWithLog(voidPt, async () => { await page.keyboard.press('Escape'); await page.waitForTimeout(200); });
    const f = await shot('drag-esc-after');
    return { voidPt, mid: { cells: r.mid.cells, hand: r.mid.hand, ghost: r.mid.ghost }, after: { cells: r.after.cells, hand: r.after.hand, names: r.after.handNames, ghost: r.after.ghost, toastTop: r.after.toastTop }, calls: r.calls, file: f };
  });

  await rec('drag-double-cast', async () => {
    const r = await dragWithLog(voidPt);
    const f = await shot('drag-double-after');
    return { voidPt, mid: { cells: r.mid.cells, hand: r.mid.hand, tip: r.mid.tip }, after: { cells: r.after.cells, hand: r.after.hand, names: r.after.handNames, toastTop: r.after.toastTop }, calls: r.calls, file: f };
  });

  await rec('drag-blur-cast', async () => {
    const r = await dragWithLog(voidPt, async () => { await ev(() => window.dispatchEvent(new Event('blur'))); await page.waitForTimeout(250); });
    return { mid: { cells: r.mid.cells, hand: r.mid.hand, ghost: r.mid.ghost }, after: { cells: r.after.cells, hand: r.after.hand, ghost: r.after.ghost }, calls: r.calls };
  });

  await rec('drag-bottom-band', async () => {
    await clearToasts();
    await put(landId);
    const hw = await ev(() => document.getElementById('handwrap').getBoundingClientRect().top);
    const r = await dragWithLog({ x: 700, y: hw - 2 });
    return { hwTop: hw, mid: { cells: r.mid.cells, tip: r.mid.tip, ghost: r.mid.ghost }, after: { cells: r.after.cells, hand: r.after.hand, toastTop: r.after.toastTop }, calls: r.calls };
  });

  /* ── 8. 面板内连续两条不同 toast：只留最后一条 ── */
  await rec('phead-two-toasts', async () => {
    await resetPack(); await clearToasts();
    await ev(() => { window.__dbg.UI.openPanel('codex'); });
    await page.waitForTimeout(250);
    await ev(() => {
      const { UI } = window.__dbg;
      UI.toast('图鉴点亮', '生态 · 芦苇　11 / 42');
      UI.toast('图鉴点亮', '气候 · 长夜　12 / 42');
    });
    await page.waitForTimeout(150);
    const st = await ev(() => ({
      chips: [...document.querySelectorAll('#phead .toast.ih')].map(e => e.textContent.trim()),
      top: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim())
    }));
    const f = await shot('phead-two-toasts');
    await ev(() => window.__dbg.UI.closePanel());
    return { ...st, file: f };
  });

  /* ── 9. 回收的撤销 toast 被 openPanel 清空 ── */
  await rec('undo-toast-cleared', async () => {
    await resetPack(); await clearToasts();
    await ev(() => {
      const { S, D, UI } = window.__dbg;
      const c = D.cards.find(x => x.reclaim > 0);
      S.W.hand[0] = c; S.W.sel = 0; S.W.essence = 999;
      UI.renderHand(); UI.recycle();
    });
    await page.waitForTimeout(200);
    const a = await ev(() => ({ toasts: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()), hand: window.__dbg.S.W.hand.length }));
    await page.keyboard.press('b');
    await page.waitForTimeout(250);
    const b = await ev(() => ({ toasts: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()), panel: window.__dbg.UI.state.panel }));
    const f = await shot('undo-cleared');
    await ev(() => window.__dbg.UI.closePanel());
    return { beforeOpen: a, afterOpen: b, file: f };
  });

  /* ── 10. Tab 到左栏：标签是否出现 ── */
  await rec('rail-tab', async () => {
    await ev(() => { window.__dbg.UI.closePanel(); document.activeElement && document.activeElement.blur(); });
    await page.setViewportSize({ width: 1440, height: 900 });
    await page.waitForTimeout(600);
    let hit = null;
    for (let i = 0; i < 20; i++) {
      await page.keyboard.press('Tab');
      await page.waitForTimeout(50);
      hit = await ev(() => {
        const ae = document.activeElement;
        if (!ae || !ae.classList || !ae.classList.contains('rail')) return null;
        const lr = document.getElementById('leftrail').getBoundingClientRect();
        const lab = ae.querySelector('span');
        const cs = getComputedStyle(lab);
        return { cls: ae.className, railW: Math.round(lr.width), labelW: cs.width, labelOp: cs.opacity, focusVisible: ae.matches(':focus-visible'), text: lab.textContent };
      });
      if (hit) break;
    }
    const f = await shot('rail-tab-focus2', { x: 0, y: 56, width: 260, height: 400 });
    return { hit, file: f };
  });

  /* ── 11. 1080 手牌：最后一格与右栏的几何 ── */
  await rec('hand-geometry', async () => {
    await page.setViewportSize({ width: 1080, height: 700 });
    await page.waitForTimeout(600);
    await ev(() => { const { UI } = window.__dbg; UI.renderHand(); UI.updateHud(); });
    await page.waitForTimeout(300);
    return ev(() => {
      const hw = document.getElementById('handwrap');
      const hb = hw.getBoundingClientRect();
      const rp = document.getElementById('rightpanel').getBoundingClientRect();
      const slots = [...document.querySelectorAll('.handslot')];
      const b = i => slots[i].getBoundingClientRect();
      const cards = [...document.querySelectorAll('.handslot .card')].map(e => e.getBoundingClientRect());
      return {
        hw: { x: Math.round(hb.x), w: Math.round(hb.width), sw: hw.scrollWidth, cw: hw.clientWidth },
        rpLeft: Math.round(rp.left),
        first: { x: Math.round(b(0).x), right: Math.round(b(0).right) },
        last: { x: Math.round(b(slots.length - 1).x), right: Math.round(b(slots.length - 1).right) },
        cardFirstLeft: Math.round(cards[0].left), cardLastRight: Math.round(cards[cards.length - 1].right),
        mid: Math.round((b(0).left + b(slots.length - 1).right) / 2), center: Math.round(hb.x + hb.width / 2)
      };
    });
  });

  fs.writeFileSync(path.join(OUT, TAG + '-report.json'), JSON.stringify(REP, null, 1));
  console.log('--- steps', REP.steps.length, 'console', REP.console.length, JSON.stringify(REP.console.slice(0, 4)));
  await browser.close();
})();
