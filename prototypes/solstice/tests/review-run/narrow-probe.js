/* 窄屏 / 键盘审计取证脚本（只读，不改源码）
   跑法：node tests/review-run/narrow-probe.js --w=1080 --h=700 --out=tests/review-run/narrow-1080/probe.json
   输出：手牌排布、右栏分段、顶栏、各面板溢出、焦点顺序、游标读数。 */
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
const VW = +arg('w', 1080), VH = +arg('h', 700);
const OUT = path.resolve(arg('out', path.join(__dirname, 'review-run', 'probe-' + VW + 'x' + VH + '.json')));
const MODE = arg('mode', 'all');

const R = r => ({ x: Math.round(r.x), y: Math.round(r.y), w: Math.round(r.width), h: Math.round(r.height), r: Math.round(r.right), b: Math.round(r.bottom) });

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
  const log = { viewport: [VW, VH], errors: [] };
  page.on('pageerror', e => log.errors.push(e.message));
  await page.goto(PAGE);
  await page.waitForTimeout(1400);
  await page.evaluate(() => {
    window.__dbg.UI.enterFromCreator();
    window.__dbg.UI.updateHud(); window.__dbg.UI.renderHand(); window.__dbg.UI.renderRight();
  });
  await page.waitForTimeout(500);

  if (MODE === 'all' || MODE === 'layout') {
    log.layout = await page.evaluate(() => {
      const rr = el => { const r = el.getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height, r: r.right, b: r.bottom }; };
      const handwrap = document.getElementById('handwrap');
      const hw = rr(handwrap);
      const slots = [...document.querySelectorAll('.handslot')].map((el, i) => {
        const b = rr(el);
        const card = el.querySelector('.card');
        const name = el.querySelector('.hband h3');
        const cb = card.getBoundingClientRect();
        const nb = name.getBoundingClientRect();
        // 名字中点被谁盖住
        const cx = nb.left + nb.width / 2, cy = nb.top + nb.height / 2;
        const topEl = document.elementFromPoint(cx, cy);
        const coveredByOther = topEl && !el.contains(topEl) && topEl.closest && topEl.closest('.handslot')
          ? +topEl.closest('.handslot').dataset.i : null;
        return {
          i, x: Math.round(b.x), y: Math.round(b.y), w: Math.round(b.w), h: Math.round(b.h), right: Math.round(b.r),
          card: { x: Math.round(cb.x), y: Math.round(cb.y), w: Math.round(cb.width), h: Math.round(cb.height), right: Math.round(cb.right) },
          name: { text: name.textContent, x: Math.round(nb.x), w: Math.round(nb.width), center: Math.round(cx) },
          nameCoveredBy: coveredByOther,
          clippedRight: Math.max(0, Math.round(cb.right - document.getElementById('rightpanel').getBoundingClientRect().left)),
          clippedViewport: Math.max(0, Math.round(cb.right - innerWidth))
        };
      });
      const pitch = slots.length > 1 ? +(slots[1].x - slots[0].x).toFixed(2) : 0;
      const overlap = slots.length > 1 ? +(slots[0].w - pitch).toFixed(2) : 0;
      const rp = document.getElementById('rightpanel');
      const rpB = rp.getBoundingClientRect();
      const sections = [...rp.querySelectorAll('.rp')].map(el => {
        const b = rr(el);
        const grow = el.classList.contains('grow');
        return { cls: el.className, h: b.h, y: b.y, sh: el.scrollHeight, ch: el.clientHeight, grow, title: (el.querySelector('.h') || {}).textContent };
      });
      const top = document.getElementById('top');
      const topKids = [...top.children].map(el => ({ id: el.id || el.className, ...rr(el) }));
      const bPack = document.getElementById('bPack').getBoundingClientRect();
      const gears = document.getElementById('gears').getBoundingClientRect();
      const hand = document.getElementById('hand');
      return {
        handScale: getComputedStyle(document.documentElement).getPropertyValue('--hand-scale').trim(),
        right: getComputedStyle(document.documentElement).getPropertyValue('--right').trim(),
        rail: getComputedStyle(document.documentElement).getPropertyValue('--rail').trim(),
        fs: getComputedStyle(document.documentElement).getPropertyValue('--fs').trim(),
        rootFont: getComputedStyle(document.body).fontSize,
        handwrap: { ...hw, cw: handwrap.clientWidth, sw: handwrap.scrollWidth },
        hand: { cw: hand.clientWidth, sw: hand.scrollWidth },
        slotCount: slots.length, pitch, overlap, slots,
        rp: { ...rr(rp), w: rpB.width },
        sections,
        growOverflow: (() => { const g = rp.querySelector('.rp.grow'); return g ? g.scrollHeight - g.clientHeight : null; })(),
        top: { ...rr(top), kids: topKids, bPack: rr(document.getElementById('bPack')), gears: { ...rr(document.getElementById('gears')), right: gears.right }, bPackClipped: Math.round(bPack.right - innerWidth) },
        stage: rr(document.getElementById('stage')),
        docW: document.documentElement.scrollWidth, docH: document.documentElement.scrollHeight
      };
    });
  }

  if (MODE === 'all' || MODE === 'panels') {
    log.panels = {};
    for (const p of ['pack', 'codex', 'chron', 'settings']) {
      await page.evaluate(id => window.__dbg.UI.openPanel(id), p);
      await page.waitForTimeout(260);
      if (p === 'settings') await page.evaluate(() => { window.__dbg.UI.openPanel('settings'); });
      log.panels[p] = await page.evaluate(() => {
        const rr = el => { const r = el.getBoundingClientRect(); return { x: Math.round(r.x), y: Math.round(r.y), w: Math.round(r.width), h: Math.round(r.height), r: Math.round(r.right), b: Math.round(r.bottom) }; };
        const panel = document.getElementById('panel');
        const pb = document.getElementById('pbody');
        const rows = [];
        // 找所有可滚动容器
        pb.querySelectorAll('*').forEach(el => {
          const cs = getComputedStyle(el);
          if ((cs.overflowY === 'auto' || cs.overflowY === 'scroll') && el.scrollHeight > el.clientHeight + 2) {
            rows.push({ tag: el.tagName + '.' + (el.className || ''), ...rr(el), sh: el.scrollHeight, ch: el.clientHeight, over: el.scrollHeight - el.clientHeight });
          }
        });
        const kids = [...pb.querySelectorAll('*')].map(el => el.getBoundingClientRect());
        const maxBottom = kids.length ? Math.max(...kids.map(b => b.bottom)) : 0;
        const maxRight = kids.length ? Math.max(...kids.map(b => b.right)) : 0;
        return {
          panel: rr(panel), pbody: rr(pb), bodyOverflowY: getComputedStyle(pb).overflowY,
          scrollables: rows,
          contentMaxBottom: Math.round(maxBottom), panelBottom: Math.round(panel.getBoundingClientRect().bottom),
          contentOverBottom: Math.round(maxBottom - panel.getBoundingClientRect().bottom),
          contentMaxRight: Math.round(maxRight), panelRight: Math.round(panel.getBoundingClientRect().right),
          contentOverRight: Math.round(maxRight - panel.getBoundingClientRect().right),
          closeVisible: !!document.querySelector('#phead .close'),
          closeRect: (() => { const c = document.querySelector('#phead .close'); return c ? rr(c) : null; })(),
          focusables: [...document.querySelectorAll('#panel button,#panel [tabindex]')].filter(e => !e.disabled).length
        };
      });
      await page.evaluate(() => window.__dbg.UI.closePanel());
      await page.waitForTimeout(120);
    }
  }

  if (MODE === 'all' || MODE === 'keyboard') {
    log.keyboard = {};
    await page.evaluate(() => document.activeElement && document.activeElement.blur());
    const order = [];
    for (let i = 0; i < 34; i++) {
      await page.keyboard.press('Tab');
      await page.waitForTimeout(35);
      const cur = await page.evaluate(() => {
        const a = document.activeElement;
        if (!a || a === document.body) return { tag: 'BODY' };
        const r = a.getBoundingClientRect();
        const cs = getComputedStyle(a);
        const par = a.parentElement;
        let clip = false;
        for (let e = a.parentElement; e && e !== document.body; e = e.parentElement) {
          const s = getComputedStyle(e);
          if ((s.overflow === 'hidden' || s.overflowX === 'hidden' || s.overflowY === 'hidden') && (e.scrollHeight > e.clientHeight + 2 || e.scrollWidth > e.clientWidth + 2)) {
            const er = e.getBoundingClientRect();
            if (r.left < er.left - 1 || r.right > er.right + 1 || r.top < er.top - 1 || r.bottom > er.bottom + 1) clip = true;
          }
        }
        return {
          tag: a.tagName, cls: a.className, id: a.id, act: a.dataset && (a.dataset.act || a.dataset.panel || a.dataset.ctab || a.dataset.el || a.dataset.pack || a.dataset.card),
          text: (a.innerText || '').replace(/\s+/g, ' ').slice(0, 24),
          rect: { x: Math.round(r.x), y: Math.round(r.y), w: Math.round(r.width), h: Math.round(r.height) },
          outline: cs.outlineStyle + ' ' + cs.outlineWidth + ' ' + cs.outlineColor,
          focusVisible: a.matches(':focus-visible'),
          clipped: clip,
          inPanel: !!a.closest('#overlay')
        };
      });
      order.push(cur);
    }
    log.keyboard.tabOrder = order;

    // 手牌焦点 + Enter / 方向键
    log.keyboard.hand = await page.evaluate(async () => {
      const slot = document.querySelector('.handslot');
      if (!slot) return null;
      slot.focus();
      const before = { sel: window.__dbg.S.W.sel, hand: window.__dbg.S.W.hand.length, cells: window.__dbg.S.W.cells.size };
      const rect = slot.getBoundingClientRect();
      return { before, rect: { x: rect.x, y: rect.y, w: rect.width, h: rect.height }, focusVisible: slot.matches(':focus-visible') };
    });
    // 方向键在焦点于手牌时是否移动游标
    const cursorBefore = await page.evaluate(() => ({ hover: document.getElementById('hoverinfo') && getComputedStyle(document.getElementById('hoverinfo')).display, text: (document.getElementById('hoverinfo') || {}).innerText }));
    await page.keyboard.press('d');
    await page.keyboard.press('d');
    await page.waitForTimeout(300);
    const cursorAfter = await page.evaluate(() => ({
      hover: document.getElementById('hoverinfo') && getComputedStyle(document.getElementById('hoverinfo')).display,
      text: (document.getElementById('hoverinfo') || {}).innerText,
      marks: document.querySelectorAll('canvas').length
    }));

    // 在世界里按 WASD：游标是否可见、有无读数
    await page.evaluate(() => document.activeElement && document.activeElement.blur());
    await page.keyboard.press('w');
    await page.waitForTimeout(500);
    const worldCursor = await page.evaluate(() => {
      const hi = document.getElementById('hoverinfo');
      const r = hi && hi.getBoundingClientRect();
      return { hiDisplay: hi && getComputedStyle(hi).display, hiText: hi && hi.innerText, hiRect: r && { x: Math.round(r.x), y: Math.round(r.y), w: Math.round(r.width), h: Math.round(r.height) }, sel: window.__dbg.S.W.sel };
    });
    log.keyboard.cursor = { before: cursorBefore, afterMoveWhileHandFocused: cursorAfter, world: worldCursor };

    // Enter 施卡（先在世界里把一张土地选上）
    log.keyboard.enterCast = await page.evaluate(async () => {
      const { S, UI } = window.__dbg;
      const i = S.W.hand.findIndex(c => c.sim.op === 'land');
      if (i >= 0) UI.select(i);
      return { sel: S.W.sel, cells: S.W.cells.size, hand: S.W.hand.length };
    });
    await page.keyboard.press('Enter');
    await page.waitForTimeout(400);
    log.keyboard.enterCast.after = await page.evaluate(() => ({ cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length, toast: (document.querySelector('.toast') || {}).innerText }));

    // 面板里的焦点与 Esc
    log.keyboard.panel = await page.evaluate(async () => {
      window.__dbg.UI.openPanel('pack');
      return { open: window.__dbg.UI.state.panel };
    });
    await page.waitForTimeout(250);
    const focusablesInPanel = await page.evaluate(() => {
      const list = [...document.querySelectorAll('#panel button,#panel [tabindex]')].filter(e => !e.disabled);
      return { n: list.length, first: list[0] && list[0].innerText, last: list[list.length - 1] && list[list.length - 1].innerText, backgroundFocusable: [...document.querySelectorAll('#leftrail .rail,#top .btn,#top .gears')].filter(e => e.offsetParent).length };
    });
    await page.keyboard.press('Tab');
    await page.keyboard.press('Tab');
    await page.keyboard.press('Tab');
    const afterTabs = await page.evaluate(() => {
      const a = document.activeElement;
      return { tag: a.tagName, cls: a.className, text: (a.innerText || '').slice(0, 20), inPanel: !!a.closest('#panel'), inOverlay: !!a.closest('#overlay') };
    });
    await page.keyboard.press('Escape');
    await page.waitForTimeout(200);
    log.keyboard.panelFocus = { focusablesInPanel, afterTabs, closedByEsc: await page.evaluate(() => !window.__dbg.UI.state.panel) };
  }

  fs.writeFileSync(OUT, JSON.stringify(log, null, 1));
  console.log('wrote', OUT);
  await browser.close();
})();
