/* 第二轮修复后的当前 HEAD 复核：只测仍然存在的问题 + 新回归。
   node tests/review-run/hunt-probe4.js --tag=p4 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const TAG = 'p4';
const OUT = path.join(__dirname, 'hunt-shots');
fs.mkdirSync(OUT, { recursive: true });
const REP = { tag: TAG, steps: [], console: [] };
(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1080, height: 700 }, deviceScaleFactor: 1 });
  page.on('pageerror', e => REP.console.push('[PAGEERROR] ' + e.message));
  page.on('console', m => { if (m.type() === 'error') REP.console.push('[console] ' + m.text()); });
  let n = 0;
  const shot = async (name, clip) => { n++; const f = TAG + '-' + String(n).padStart(2, '0') + '-' + name + '.png'; await page.screenshot(clip ? { path: path.join(OUT, f), clip } : { path: path.join(OUT, f) }); return f; };
  const rec = async (name, fn) => { try { const d = await fn(); REP.steps.push({ name, data: d }); console.log('OK  ', name, JSON.stringify(d).slice(0, 300)); return d; } catch (e) { REP.steps.push({ name, error: String(e && e.message) }); console.log('ERR ', name, e && e.message); } };
  const ev = (f, a) => page.evaluate(f, a);

  await page.goto(PAGE); await page.waitForTimeout(1500);
  await ev(() => {
    const { S, D, UI } = window.__dbg;
    UI.enterFromCreator(); UI.updateHud(); UI.renderHand();
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -3; x <= 3; x++) for (let y = -3; y <= 3; y++) S.cmd.cast(land, x, y);
    S.W.essence = 999; UI.updateHud(); UI.renderRight(); UI.renderHand();
    window.__calls = [];
    const o = UI.cast;
    UI.cast = function (...a) { window.__calls.push({ hand0: S.W.hand[0] && S.W.hand[0].name.zh, stack: new Error().stack.split('\n').slice(1, 5).join(' | ') }); return o.apply(this, a); };
  });
  await page.waitForTimeout(400);

  const clearToasts = () => ev(() => { const t = document.getElementById('toasts'); if (t) t.innerHTML = ''; const h = document.getElementById('phead'); if (h) h.querySelectorAll('.toast.ih').forEach(e => e.remove()); });
  const resetPack = () => ev(() => { const { UI, S } = window.__dbg; UI.closePanel(); UI.packStage = 0; UI.packCards = []; UI.packSel = 'basic'; UI.renderOverlay(); S.W.essence = 999; });
  const st = () => ev(() => {
    const { S, UI } = window.__dbg;
    return {
      cells: S.W.cells.size, hand: S.W.hand.length, sel: S.W.sel, ess: S.W.essence, month: S.W.month,
      panel: UI.state.panel, stage: UI.packStage, cards: (UI.packCards || []).length,
      head: [...document.querySelectorAll('#phead .toast.ih')].map(e => e.textContent.trim()),
      top: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()),
      event: UI.state.event ? UI.state.event.t : null,
      gear: S.gear, dragging: UI.isDragging ? UI.isDragging() : null
    };
  });

  /* ── 控件：第二轮修复是否生效 ── */
  await rec('fixed-esc-drag', async () => {
    await clearToasts();
    await ev(() => { const { S, D, UI } = window.__dbg; S.W.hand[0] = D.byId.get('terrain.earth.0.matter.land'); S.W.sel = 0; UI.renderHand(); window.__calls = []; });
    const c = await ev(() => { const b = document.querySelector('.handslot').getBoundingClientRect(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; });
    const pt = await ev(() => { const { R } = window.__dbg; const rc = document.getElementById('cv').getBoundingClientRect(); for (let gy = .12; gy < .5; gy += .03) for (let gx = .1; gx < .9; gx += .03) { const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy; if (!R.cam.pickAt(x, y)) return { x: Math.round(x), y: Math.round(y) }; } return null; });
    await page.mouse.move(c.x, c.y); await page.mouse.down(); await page.mouse.move(pt.x, pt.y, { steps: 8 }); await page.waitForTimeout(120);
    const before = await st();
    await page.keyboard.press('Escape'); await page.waitForTimeout(200);
    const afterEsc = await ev(() => ({ calls: window.__calls.length, cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length, ghosts: document.querySelectorAll('#ghost').length, dragging: window.__dbg.UI.isDragging() }));
    await page.mouse.up(); await page.waitForTimeout(300);
    const afterUp = await ev(() => ({ calls: window.__calls.length, cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length, toasts: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()) }));
    return { before: { cells: before.cells, hand: before.hand }, afterEsc, afterUp, pt };
  });

  await rec('fixed-one-drag-one-card', async () => {
    await clearToasts();
    await ev(() => { const { S, D, UI } = window.__dbg; S.W.hand[0] = D.byId.get('terrain.earth.0.matter.land'); S.W.sel = 0; UI.renderHand(); window.__calls = []; });
    const c = await ev(() => { const b = document.querySelector('.handslot').getBoundingClientRect(); return { x: b.x + b.width / 2, y: b.y + b.height / 2 }; });
    const pt = await ev(() => { const { R } = window.__dbg; const rc = document.getElementById('cv').getBoundingClientRect(); for (let gy = .12; gy < .5; gy += .03) for (let gx = .1; gx < .9; gx += .03) { const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy; if (!R.cam.pickAt(x, y)) return { x: Math.round(x), y: Math.round(y) }; } return null; });
    await page.mouse.move(c.x, c.y); await page.mouse.down(); await page.mouse.move(pt.x, pt.y, { steps: 8 }); await page.waitForTimeout(120);
    await page.mouse.up(); await page.waitForTimeout(400);
    return { pt, calls: await ev(() => window.__calls.map(c => ({ hand0: c.hand0, stack: c.stack }))), cells: (await st()).cells, hand: (await st()).hand, toasts: (await st()).top };
  });

  await rec('fixed-keys-panel', async () => {
    await clearToasts();
    await ev(() => { const { S, D, UI } = window.__dbg; S.W.hand[0] = D.byId.get('flora.wood.1.matter.grass'); S.W.sel = 0; S.W.essence = 999; UI.renderHand(); UI.openPanel('pack'); });
    await page.waitForTimeout(250);
    const before = await st();
    await page.keyboard.press('r');
    await page.keyboard.press(' ');
    await page.keyboard.press('x');
    await page.keyboard.press('j');
    await page.waitForTimeout(300);
    const after = await st();
    await ev(() => window.__dbg.UI.closePanel());
    return { before: { ess: before.ess, hand: before.hand, gear: before.gear, panel: before.panel }, after: { ess: after.ess, hand: after.hand, gear: after.gear, panel: after.panel } };
  });

  /* ── 新回归：面板内按钮的 Enter / Space 被面板守卫拦掉 ── */
  await rec('panel-button-keyboard', async () => {
    await resetPack(); await clearToasts();
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(250);
    const before = await st();
    await ev(() => document.querySelector('[data-act="packNext"]').focus());
    const focused = await ev(() => document.activeElement ? document.activeElement.textContent : null);
    await page.keyboard.press('Enter'); await page.waitForTimeout(300);
    const afterEnter = await st();
    await ev(() => document.querySelector('[data-act="packNext"]') && document.querySelector('[data-act="packNext"]').focus());
    await page.keyboard.press(' '); await page.waitForTimeout(300);
    const afterSpace = await st();
    const f = await shot('panel-btn-keyboard');
    await ev(() => window.__dbg.UI.closePanel());
    return { before: { stage: before.stage, ess: before.ess }, focused, afterEnter: { stage: afterEnter.stage, ess: afterEnter.ess }, afterSpace: { stage: afterSpace.stage, ess: afterSpace.ess }, file: f };
  });

  /* ── 新回归：面板打开时 Tab 焦点落到背后的世界 ── */
  await rec('panel-tab-focus', async () => {
    await resetPack(); await clearToasts();
    await ev(() => { window.__dbg.UI.openPanel('codex'); document.activeElement && document.activeElement.blur(); });
    await page.waitForTimeout(300);
    const seq = [];
    for (let i = 0; i < 6; i++) {
      await page.keyboard.press('Tab'); await page.waitForTimeout(60);
      seq.push(await ev(() => {
        const ae = document.activeElement;
        const r = ae ? ae.getBoundingClientRect() : null;
        const inside = !!(ae && ae.closest && ae.closest('#panel'));
        return { tag: ae ? ae.tagName : null, cls: ae ? ae.className : null, text: ae ? (ae.textContent || '').trim().slice(0, 12) : null, insidePanel: inside, x: r ? Math.round(r.x) : null, y: r ? Math.round(r.y) : null, visible: r ? !!(r.width && r.height) : false };
      }));
    }
    const f = await shot('panel-tab-focus');
    await ev(() => window.__dbg.UI.closePanel());
    return { seq, file: f };
  });

  /* ── 抽卡：撕开 / 收下的 toast 是否还在 ── */
  await rec('pack-toast-lost', async () => {
    await resetPack(); await clearToasts();
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(200);
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(250);
    const torn = await st();
    const f1 = await shot('pack-torn-current');
    await ev(() => document.querySelector('.packpick').click());
    await page.waitForTimeout(250);
    const picked = await st();
    const f2 = await shot('pack-picked-current');
    return { torn: { stage: torn.stage, ess: torn.ess, head: torn.head, top: torn.top }, picked: { stage: picked.stage, panel: picked.panel, head: picked.head, top: picked.top, hand: picked.hand }, files: [f1, f2] };
  });

  /* ── 换界后旧包还在 ── */
  await rec('pack-cross-world', async () => {
    await resetPack(); await clearToasts();
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(200);
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(200);
    const torn = await st();
    await ev(() => window.__dbg.UI.closePanel());
    await page.keyboard.press('g'); await page.waitForTimeout(300);
    await ev(() => { const { UI } = window.__dbg; UI.enterFromCreator(); UI.enterFromCreator(); });
    await page.waitForTimeout(600);
    const world = await st();
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(300);
    const reopened = await st();
    const picks = await ev(() => document.querySelectorAll('.packpick').length);
    const f = await shot('pack-cross-world-current');
    await ev(() => window.__dbg.UI.closePanel());
    return { torn: { stage: torn.stage, cards: torn.cards, ess: torn.ess }, world: { cells: world.cells, ess: world.ess, stage: world.stage, cards: world.cards }, reopened: { stage: reopened.stage, cards: reopened.cards, picks }, file: f };
  });

  /* ── 跳过这一包 ── */
  await rec('pack-skip', async () => {
    await resetPack(); await clearToasts();
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(200);
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(200);
    const torn = await st();
    const btn = await ev(() => { const b = document.querySelector('[data-act="packNext"]'); return { text: b.textContent, cls: b.className }; });
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(300);
    const after = await st();
    return { torn: { stage: torn.stage, ess: torn.ess }, btn, after: { stage: after.stage, cards: after.cards, ess: after.ess, panel: after.panel, head: after.head, top: after.top } };
  });

  /* ── 顶部抽包在欠费 + 有未取牌时禁用 ── */
  await rec('top-pack-disabled-pending', async () => {
    await resetPack(); await clearToasts();
    await ev(() => window.__dbg.UI.openPanel('pack'));
    await page.waitForTimeout(200);
    await ev(() => document.querySelector('[data-act="packNext"]').click());
    await page.waitForTimeout(250);
    const r = await ev(() => { const { S, UI } = window.__dbg; UI.closePanel(); S.W.essence = 0; UI.updateHud(); const b = document.getElementById('bPack'); return { stage: UI.packStage, cards: UI.packCards.length, text: b.textContent, disabled: b.disabled }; });
    const f = await shot('top-pack-disabled', { x: 700, y: 0, width: 380, height: 56 });
    return { ...r, file: f };
  });

  /* ── 锁定包仍可选中 ── */
  await rec('locked-pack-select', async () => {
    await resetPack(); await clearToasts();
    await ev(() => { window.__dbg.S.W.essence = 999; window.__dbg.UI.openPanel('pack'); });
    await page.waitForTimeout(250);
    const rows = await ev(() => [...document.querySelectorAll('.packrow')].map(r => ({ pack: r.dataset.pack, lock: r.classList.contains('lock') })));
    const r = await ev(() => { const row = [...document.querySelectorAll('.packrow')].find(r => r.classList.contains('lock')); if (!row) return null; row.click(); return { sel: window.__dbg.UI.packSel, on: [...document.querySelectorAll('.packrow.on')].map(r => r.dataset.pack), btn: (() => { const b = document.querySelector('[data-act="packNext"]'); return b ? { text: b.textContent, disabled: b.disabled } : null; })() }; });
    const f = await shot('locked-pack-current');
    await ev(() => window.__dbg.UI.closePanel());
    return { rows, after: r, file: f };
  });

  /* ── 月份：事件弹窗仍用原始月 ── */
  await rec('month-event', async () => {
    const out = [];
    for (const m of [0, 11, 12, 13, 23, 24]) {
      out.push(await ev(m => {
        const { S, UI } = window.__dbg; S.W.month = m; UI.updateHud(); UI.fireEvent();
        const e = document.querySelector('#eventcard .row .xs:last-child');
        const clock = document.getElementById('cYear').textContent + '年' + document.getElementById('cMonth').textContent + '月';
        const r = { m, mon: UI.mon(m), clock, event: e ? e.textContent.trim() : null };
        UI.closeEvent(); return r;
      }, m));
    }
    await ev(() => { window.__dbg.S.W.month = 0; window.__dbg.UI.updateHud(); window.__dbg.UI.fireEvent(); });
    await page.waitForTimeout(150);
    const f = await shot('event-month0-current');
    await ev(() => window.__dbg.UI.closeEvent());
    return { out, file: f };
  });

  /* ── 年表筛选缺纪年 ── */
  await rec('chron-filter', async () => {
    return ev(() => {
      const { S, UI } = window.__dbg; S.W.month = 11; S.cmd.tick(); UI.updateHud();
      const kinds = ['全部', '干涉', '气候', '生态', '灾变', '文明', '图鉴'];
      return { total: S.W.chron.length, sum: kinds.slice(1).reduce((a, k) => a + S.W.chron.filter(c => c.kind === k).length, 0), hasYear: kinds.includes('纪年'), yearRows: S.W.chron.filter(c => c.kind === '纪年').map(c => ({ m: c.month, mon: UI.mon(c.month), icon: UI.CHRON_ICON[c.kind] || null, col: UI.CHRON_COL[c.kind] || null })) };
    });
  });

  /* ── 回收撤销 toast 被 openPanel 清掉 ── */
  await rec('undo-cleared', async () => {
    await resetPack(); await clearToasts();
    await ev(() => { const { S, D, UI } = window.__dbg; const c = D.cards.find(x => x.reclaim > 0); S.W.hand[0] = c; S.W.sel = 0; S.W.essence = 999; UI.renderHand(); UI.recycle(); });
    await page.waitForTimeout(200);
    const a = await ev(() => ({ top: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()), hand: window.__dbg.S.W.hand.length }));
    await page.keyboard.press('b'); await page.waitForTimeout(250);
    const b = await ev(() => ({ top: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()), panel: window.__dbg.UI.state.panel }));
    await ev(() => window.__dbg.UI.closePanel());
    return { before: a, after: b };
  });

  /* ── #phead 只留最后一条 ── */
  await rec('phead-two', async () => {
    await resetPack(); await clearToasts();
    await ev(() => window.__dbg.UI.openPanel('codex'));
    await page.waitForTimeout(250);
    await ev(() => { const { UI } = window.__dbg; UI.toast('图鉴点亮', '生态 · 芦苇　11 / 42'); UI.toast('图鉴点亮', '气候 · 长夜　12 / 42'); });
    await page.waitForTimeout(150);
    return ev(() => ({ chips: [...document.querySelectorAll('#phead .toast.ih')].map(e => e.textContent.trim()), top: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()) }));
  });

  /* ── 土地垫高：无文字回执，log 在 1080 隐藏 ── */
  await rec('land-touched', async () => {
    return ev(() => {
      const { S, D, UI } = window.__dbg;
      const till = D.cards.find(c => c.sim.op === 'land' && c.sim.rise);
      S.W.hand[0] = till; S.W.sel = 0; S.W.essence = 999; UI.renderHand();
      const g = document.getElementById('gainfly'); if (g) { g.textContent = ''; g.style.opacity = 0; }
      const before = { hand: S.W.hand.length, cells: S.W.cells.size };
      UI.cast(0, S.at(0, 0), null);
      const log = document.getElementById('log');
      return { card: till.name.zh, before, after: { hand: S.W.hand.length, cells: S.W.cells.size }, gainfly: g ? { text: g.textContent, op: getComputedStyle(g).opacity } : null, log: log ? { display: getComputedStyle(log).display, last: [...log.children].map(e => e.textContent).slice(-2) } : null, toasts: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()) };
    });
  });

  /* ── 左栏 Tab：标签仍不可见 ── */
  await rec('rail-tab', async () => {
    await ev(() => { window.__dbg.UI.closePanel(); document.activeElement && document.activeElement.blur(); });
    await page.setViewportSize({ width: 1440, height: 900 }); await page.waitForTimeout(600);
    let hit = null;
    for (let i = 0; i < 25; i++) {
      await page.keyboard.press('Tab'); await page.waitForTimeout(40);
      hit = await ev(() => { const ae = document.activeElement; if (!ae || !ae.classList || !ae.classList.contains('rail')) return null; const lr = document.getElementById('leftrail').getBoundingClientRect(); const lab = ae.querySelector('span'); const cs = getComputedStyle(lab); return { railW: Math.round(lr.width), labelW: cs.width, labelOp: cs.opacity, focusVisible: ae.matches(':focus-visible'), text: lab.textContent }; });
      if (hit) break;
    }
    const f = await shot('rail-tab-current', { x: 0, y: 56, width: 240, height: 380 });
    return { hit, file: f };
  });

  /* ── 卡库详情滚到底的最后一行 ── */
  await rec('scroll-fade', async () => {
    await page.setViewportSize({ width: 1080, height: 700 }); await page.waitForTimeout(500);
    await ev(() => window.__dbg.UI.openPanel('codex'));
    await page.waitForTimeout(350);
    const box = await ev(() => { const el = document.querySelector('.cdscroll'); const b = el.getBoundingClientRect(); return { x: Math.round(b.x), y: Math.round(b.y), w: Math.round(b.width), h: Math.round(b.height), sh: el.scrollHeight, ch: el.clientHeight }; });
    await ev(() => { const el = document.querySelector('.cdscroll'); el.scrollTop = el.scrollHeight; });
    await page.waitForTimeout(250);
    const end = await ev(() => { const el = document.querySelector('.cdscroll'); return { st: el.scrollTop, max: el.scrollHeight - el.clientHeight, atEnd: el.scrollTop + el.clientHeight >= el.scrollHeight - 1 }; });
    const f = await shot('codex-bottom-current', { x: box.x - 4, y: box.y + box.h - 70, width: box.w + 8, height: 80 });
    await ev(() => window.__dbg.UI.closePanel());
    return { box, end, file: f };
  });

  /* ── 关系链 note ── */
  await rec('rel-note', async () => {
    await ev(() => window.__dbg.UI.openPanel('codex'));
    await page.waitForTimeout(300);
    await ev(() => { const el = document.querySelector('.cdscroll'); el.scrollTop = el.scrollHeight; });
    await page.waitForTimeout(200);
    const r = await ev(() => { const el = document.querySelector('.cdscroll'); return [...el.querySelectorAll('.dex .rl')].map(e => ({ text: e.textContent.trim(), html: e.innerHTML })); });
    const f = await shot('codex-rel-current');
    await ev(() => window.__dbg.UI.closePanel());
    return { rel: r, file: f };
  });

  /* ── 摆件模式残留 toast ── */
  await rec('ambient-toast', async () => {
    await ev(() => { window.__dbg.UI.closePanel(); window.__dbg.UI.toast('图鉴点亮', '生态 · 芦苇　11 / 42'); window.__dbg.UI.toggleAmbient(); });
    await page.waitForTimeout(500);
    const r = await ev(() => ({ top: [...document.querySelectorAll('#toasts .toast')].map(e => e.textContent.trim()), mode: window.__dbg.UI.state.uiMode }));
    const f = await shot('ambient-toast-current');
    await ev(() => { if (window.__dbg.UI.state.uiMode !== '全部') window.__dbg.UI.setUiMode('全部'); });
    return { ...r, file: f };
  });

  /* ── 设置：阴影开关是否还有视觉作用（当前默认关） ── */
  await rec('shadow-setting', async () => {
    return ev(() => ({ shadows: window.__dbg.R.shadows, casters: (() => { let n = 0; window.__dbg.R.scene.traverse(o => { if (o.isMesh && o.castShadow) n++; }); return n; })() }));
  });

  /* ── 1080 当前手牌几何（-34 之后） ── */
  await rec('hand-current', async () => {
    await page.setViewportSize({ width: 1080, height: 700 }); await page.waitForTimeout(500);
    await ev(() => { window.__dbg.UI.renderHand(); });
    await page.waitForTimeout(300);
    return ev(() => {
      const hw = document.getElementById('handwrap'); const hb = hw.getBoundingClientRect();
      const slots = [...document.querySelectorAll('.handslot')].map(e => { const b = e.getBoundingClientRect(); return { x: Math.round(b.x), right: Math.round(b.right) }; });
      const cards = [...document.querySelectorAll('.handslot .card')].map(e => { const b = e.getBoundingClientRect(); return { left: Math.round(b.left), right: Math.round(b.right) }; });
      const ab = document.getElementById('actbar').getBoundingClientRect(), dk = document.getElementById('deck').getBoundingClientRect(), rp = document.getElementById('rightpanel').getBoundingClientRect();
      const hit = (x, y) => { const e = document.elementFromPoint(x, y); return { x, y, el: e ? (e.id || (e.className && e.className.baseVal) || e.className) : null, inAct: !!(e && e.closest && e.closest('#actbar')), inCard: !!(e && e.closest && e.closest('.handslot')) }; };
      return { hw: { x: Math.round(hb.x), w: Math.round(hw.clientWidth), sw: hw.scrollWidth }, first: slots[0], last: slots[slots.length - 1], firstCard: cards[0], lastCard: cards[cards.length - 1], rpLeft: Math.round(rp.left), actbar: { x: Math.round(ab.x), right: Math.round(ab.right), y: Math.round(ab.y), bottom: Math.round(ab.bottom) }, deck: { x: Math.round(dk.x), right: Math.round(dk.right), y: Math.round(dk.y) }, hits: [hit(ab.x + 18, ab.y + 18), hit(ab.x + 18, ab.y + 60), hit(ab.x + 18, ab.bottom - 18), hit(dk.x + 20, dk.y + 20), hit(dk.x + 60, dk.y + 20), hit(rp.left + 4, 520) ] };
    });
  });

  fs.writeFileSync(path.join(OUT, TAG + '-report.json'), JSON.stringify(REP, null, 1));
  console.log('--- steps', REP.steps.length, 'console', REP.console.length, JSON.stringify(REP.console.slice(0, 4)));
  await browser.close();
})();
