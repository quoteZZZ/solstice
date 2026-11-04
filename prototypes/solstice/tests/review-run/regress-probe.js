/* 回归验证用临时探针（只读：不改任何源码）
   跑法：node tests/review-run/regress-probe.js
   产出：tests/review-run/regress-probe/*.png + probes.json */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const ROOT = path.join(__dirname, '..', '..');
const PAGE = pathToFileURL(path.join(ROOT, 'index.html')).href;
const OUT = path.join(__dirname, 'regress-probe');
fs.mkdirSync(OUT, { recursive: true });

const VW = 1080, VH = 700;
const out = { meta: { viewport: [VW, VH] }, probes: {}, shots: [] };
let n = 0;

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
  const errs = [];
  page.on('pageerror', e => errs.push(String(e.message)));

  let shotN = 0;
  async function shot(name) {
    shotN++;
    const file = String(shotN).padStart(2, '0') + '-' + name + '.png';
    await page.waitForTimeout(260);
    await page.screenshot({ path: path.join(OUT, file) });
    out.shots.push(file);
    return file;
  }
  const ev = fn => page.evaluate(fn);
  const P = (k, v) => { out.probes[k] = v; };

  await page.goto(PAGE);
  await page.waitForTimeout(1500);

  /* 进入世界 */
  await ev(() => { window.__dbg.UI.enterFromCreator(); window.__dbg.UI.updateHud(); window.__dbg.UI.renderHand(); });
  await page.waitForTimeout(500);

  /* ───── 1. F14 空施吃牌 ───── */
  P('F14', await ev(() => {
    const { S, D, UI } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -2; x <= 2; x++) for (let y = -2; y <= 2; y++) S.cmd.cast(land, x, y);
    const h0 = S.W.hand.length;
    /* 找一张土地卡放进手牌，直接打在已有地块中心 */
    let idx = S.W.hand.findIndex(c => c.sim.op === 'land');
    if (idx < 0) { S.W.hand[0] = land; idx = 0; }
    UI.renderHand();
    const before = S.W.cells.size;
    const res = UI.cast(idx, { x: 0, y: 0 }, null);
    return {
      handBefore: h0, handAfter: S.W.hand.length,
      cellsBefore: before, cellsAfter: S.W.cells.size,
      toast: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.trim())
    };
  }));
  await shot('f14-land-on-land');

  /* ───── 5. U01 顶栏抽包价 ───── */
  P('U01', await ev(() => {
    const { S } = window.__dbg;
    return {
      btn: document.getElementById('bPack').textContent,
      disabled: document.getElementById('bPack').disabled,
      cheapest: Math.min(...S.packList().map(p => p.cost)),
      essence: S.W.essence
    };
  }));

  /* ───── 20. U12 空世界环境段 / 23. U18 三宫格 0 值 ───── */
  P('U12U18', await ev(() => {
    const { S, UI } = window.__dbg;
    const saved = new Map(S.W.cells);
    S.W.cells.clear();
    UI.renderRight(true);
    const rp = [...document.querySelectorAll('#rightpanel .rp')];
    const envRp = rp[1];
    const bars = envRp.querySelectorAll('.fbar').length;
    const txt = envRp.textContent.replace(/\s+/g, ' ').trim();
    const zero = [...rp[2].querySelectorAll('div')].filter(d => d.style.fontSize === '11px').map(d => d.textContent.trim()).slice(0, 3);
    /* 痕跡空态 */
    const traceTxt = rp[3].textContent.replace(/\s+/g, ' ').trim().slice(0, 40);
    S.W.cells.clear(); saved.forEach((v, k) => S.W.cells.set(k, v));
    UI.renderRight(true);
    return { bars, envText: txt, zero: zero.join('|'), traceEmptyNoCells: traceTxt };
  }));

  /* ───── 6. U19 有格但痕迹空 ───── */
  P('U19', await ev(() => {
    const { S, UI } = window.__dbg;
    const saved = S.W.traces;
    S.W.traces = [];
    UI.renderRight(true);
    const rp = [...document.querySelectorAll('#rightpanel .rp')];
    const t = rp[3].textContent.replace(/\s+/g, ' ');
    const m = /最近的痕迹都已消散/.test(t);
    const bad = /世界还没被碰过/.test(t);
    S.W.traces = saved;
    UI.renderRight(true);
    return { cells: S.W.cells.size, saysDissolved: m, saysNeverTouched: bad };
  }));

  /* ───── 21. U24 段标题右侧孤线 ───── */
  P('U24', await ev(() => {
    const h = document.querySelector('#rightpanel .rp .h');
    const cs = getComputedStyle(h, '::after');
    const before = getComputedStyle(h, '::before');
    return { afterContent: cs.content, afterW: cs.width, beforeContent: before.content, paddingRight: getComputedStyle(h).paddingRight };
  }));

  /* ───── 22. U17 图例色块 ───── */
  P('U17', await ev(() => {
    /* 图例在创世页；先把它渲染出来量一次，再关掉 */
    const { UI } = window.__dbg;
    UI.openCreator();
    const i = document.querySelector('#creator .legend i');
    if (!i) return { found: false };
    const b = i.getBoundingClientRect();
    const res = { found: true, w: +b.width.toFixed(2), h: +b.height.toFixed(2), css: getComputedStyle(i).width + ' / ' + getComputedStyle(i).height };
    if (UI.closeCreator) UI.closeCreator();
    return res;
  }));
  await shot('u17-legend');
  await ev(() => { try { const c = document.getElementById('creator'); if (c && c.classList.contains('on')) window.__dbg.UI.closeCreator(); } catch (e) {} });
  await page.waitForTimeout(300);

  /* ───── 19. U09 左栏图标不横移 ───── */
  P('U09', await ev(() => {
    const rail = document.getElementById('leftrail');
    const btn = rail.querySelector('.rail');
    const svg = btn.querySelector('svg');
    const before = svg.getBoundingClientRect();
    const restRailW = rail.getBoundingClientRect().width;
    return { restIconX: +before.x.toFixed(2), restIconW: +before.width.toFixed(2), restRailW: +restRailW.toFixed(2) };
  }));
  await page.hover('#leftrail');
  await page.waitForTimeout(450);
  Object.assign(out.probes.U09, await ev(() => {
    const rail = document.getElementById('leftrail');
    const svg = rail.querySelector('.rail svg');
    const b = svg.getBoundingClientRect();
    const sp = rail.querySelector('.rail span');
    const s = sp.getBoundingClientRect();
    const cs = getComputedStyle(sp);
    return { hoverIconX: +b.x.toFixed(2), hoverRailW: +rail.getBoundingClientRect().width.toFixed(2),
      labelX: +s.x.toFixed(2), labelOpacity: cs.opacity, labelWidth: +s.width.toFixed(1) };
  }));
  await shot('u09-rail-hover');
  await page.mouse.move(600, 300);
  await page.waitForTimeout(400);

  /* ───── 2/3/4/12. 抽卡：P01 P02 P03 P04 ───── */
  await ev(() => { window.__dbg.S.W.essence = 200; window.__dbg.UI.updateHud(); window.__dbg.UI.openPanel('pack'); });
  await page.waitForTimeout(300);
  P('P01', await ev(() => {
    const rows = [...document.querySelectorAll('.packrow')];
    const mid = rows.find(r => /进阶/.test(r.textContent));
    mid.click();
    return {
      title: document.querySelector('#phead .sm').textContent,
      footBtn: document.querySelector('#pfoot [data-act="packNext"]').textContent,
      footDisabled: document.querySelector('#pfoot [data-act="packNext"]').disabled,
      rows: rows.map(r => r.textContent.replace(/\s+/g, ' ').trim())
    };
  }));
  await shot('p01-mid-pack');

  P('P02P03P04', await ev(() => {
    const next = document.querySelector('#pfoot [data-act="packNext"]');
    next.click();                       // 撕开进阶包
    const stage2 = {
      stage: window.__dbg.UI.packStage,
      picks: document.querySelectorAll('.packpick').length,
      faces: [...document.querySelectorAll('.packpick .card')].length,
      blankRects: document.querySelectorAll('.dealcard').length,
      steps: [...document.querySelectorAll('.packframe')].length
    };
    const tags = [...document.querySelectorAll('#pbody .tag')].map(t => t.textContent.trim());
    const btn = document.querySelector('#pfoot [data-act="packNext"]');
    const btnInfo = { label: btn.textContent, cls: btn.className };
    /* P02：撕开后点别的包种 */
    const cardsBefore = window.__dbg.UI.packCards.length;
    const basicRow = [...document.querySelectorAll('.packrow')].find(r => /基础/.test(r.textContent));
    basicRow.click();
    const afterSwitch = {
      panel: window.__dbg.UI.state.panel,
      stage: window.__dbg.UI.packStage,
      cards: window.__dbg.UI.packCards.length,
      sel: window.__dbg.UI.packSel,
      toasts: [...document.querySelectorAll('#toasts .toast, #phead .toast.ih')].map(t => t.textContent.trim())
    };
    return { stage2, tags, btnInfo, cardsBefore, afterSwitch };
  }));
  await shot('p02-p03-p04-deal');

  /* P04 单独截图：6 张牌面 */
  P('P04_faces', await ev(() => {
    const picks = [...document.querySelectorAll('.packpick')];
    return {
      count: picks.length,
      sizes: picks.slice(0, 2).map(p => { const b = p.getBoundingClientRect(); return [Math.round(b.width), Math.round(b.height)]; }),
      nameTexts: picks.slice(0, 3).map(p => p.textContent.replace(/\s+/g, ' ').trim().slice(0, 16)),
      hasArt: picks.every(p => !!p.querySelector('svg'))
    };
  }));

  /* ───── 13. F12/F13 toast 去重 + 上限 3 ───── */
  P('F12F13', await ev(() => {
    const { UI } = window.__dbg;
    UI.closePanel();
    const box = document.getElementById('toasts');
    box.innerHTML = '';
    for (let i = 0; i < 4; i++) UI.toast('同一句话', '同样的细节');
    const dupCount = box.querySelectorAll('.toast').length;
    for (let i = 0; i < 6; i++) UI.toast('不同的 ' + i);
    const capCount = box.children.length;
    return { dupCount, capCount };
  }));
  await shot('f12-toast-dedupe');

  /* ───── 14. P40 面板打开时提示进 #phead ───── */
  P('P40', await ev(() => {
    const { UI } = window.__dbg;
    const box = document.getElementById('toasts');
    UI.toast('面板外的旧提示');           // 先制造一条全屏提示
    const beforeOpen = box.children.length;
    UI.openPanel('chron');
    const afterOpenStack = box.children.length;
    UI.toast('面板里的提示', '行内');
    const head = document.getElementById('phead');
    const c = head.querySelector('.toast.ih');
    const r = c ? c.getBoundingClientRect() : null;
    const foot = document.getElementById('pfoot').getBoundingClientRect();
    const steps = document.querySelector('#pbody');
    return {
      beforeOpen, stackAfterOpen: afterOpenStack,
      inline: !!c, inlineText: c ? c.textContent.trim() : '',
      inlineTop: r ? Math.round(r.top) : null, inlineBottom: r ? Math.round(r.bottom) : null,
      bodyTop: steps ? Math.round(steps.getBoundingClientRect().top) : null,
      footTop: Math.round(foot.top)
    };
  }));
  await shot('p40-inline-toast');

  /* ───── 17. U21 月份写法：年表 / 右栏 / 图鉴 ───── */
  P('U21', await ev(() => {
    const { S, UI } = window.__dbg;
    for (let i = 0; i < 26; i++) S.cmd.tick(1);
    UI.updateHud(); UI.renderRight(true);
    UI.openPanel('chron');
    const chronTxt = document.getElementById('pbody').textContent.replace(/\s+/g, ' ');
    const rightTxt = document.getElementById('rightpanel').textContent.replace(/\s+/g, ' ');
    const topClock = document.getElementById('clock').textContent.replace(/\s+/g, ' ').trim();
    const bad1 = /第\s*\d+\s*年/.test(chronTxt) || /第\s*\d+\s*年/.test(rightTxt);
    const bad2 = /\d+\.\d+\s*年/.test(chronTxt) || /\d+\.\d+\s*年/.test(rightTxt);
    const bad3 = /第\s*\d+\s*月/.test(chronTxt);
    const samples = (chronTxt.match(/\d+(?:\.\d+)?\s*(?:年|月)/g) || []).slice(0, 14);
    return { topClock, month: S.W.month, badDiNian: bad1, badDecimalYear: bad2, badDiYue: bad3, samples };
  }));
  await shot('u21-chron');

  /* ───── 18. E13/U13 开面板不写 log ───── */
  P('E13', await ev(async () => {
    const { UI } = window.__dbg;
    UI.closePanel();
    const box = document.getElementById('log');
    box.innerHTML = '';
    ['chron', 'codex', 'settings'].forEach(p => { UI.openPanel(p); UI.closePanel(); });
    const txt = box.textContent;
    return { logLines: box.children.length, text: txt, hasOpen: /打开/.test(txt) };
  }));

  /* ───── 26. P11 卡库一个分母 ───── */
  P('P11', await ev(() => {
    const { UI } = window.__dbg;
    UI.openPanel('codex');
    const rows = [...document.querySelectorAll('#pbody .row')].map(r => r.textContent.replace(/\s+/g, ' ').trim()).filter(Boolean);
    const txt = document.getElementById('pbody').textContent.replace(/\s+/g, ' ');
    return { rows: rows.slice(0, 4), hasPoolDenom: /池\s*\d+\s*\/\s*\d+/.test(txt), seen: txt.match(/已见\s*\d+\s*\/\s*\d+/g) };
  }));

  /* ───── 27/29. P28/E12 键位表 + 空格键 ───── */
  P('P28', await ev(() => {
    const { UI } = window.__dbg;
    UI.openPanel('keys');
    const rows = [...document.querySelectorAll('.keyRow')];
    const data = rows.map(r => {
      const name = r.querySelector('.sm');
      const btn = r.querySelector('button');
      const nb = name.getBoundingClientRect(), bb = btn.getBoundingClientRect();
      return { t: name.textContent.trim(), nameW: Math.round(nb.width), nameH: Math.round(nb.height),
        btnW: Math.round(bb.width), btnText: btn.textContent.trim(), sw: name.scrollWidth, cw: name.clientWidth };
    });
    const maxBtn = Math.max(...data.map(d => d.btnW));
    const twoLine = data.filter(d => d.nameH > 24).map(d => d.t);
    const space = data.find(d => d.t === '随机释放');
    return { count: rows.length, maxBtnW: maxBtn, twoLineNames: twoLine, spaceBtnText: space && space.btnText,
      sample: data.slice(0, 4) };
  }));
  await shot('p28-keys');

  /* ───── 24/25. P20 指标占位符 + §1.5 关系链键名 ───── */
  P('P20_F32', await ev(() => {
    const { UI } = window.__dbg;
    UI.openPanel('codex');
    const d = document.querySelector('.cdscroll');
    const txt = d ? d.textContent.replace(/\s+/g, ' ') : '';
    const rels = [...document.querySelectorAll('.dex .rel .rl')].map(r => r.textContent.replace(/\s+/g, ' ').trim());
    const bareKeys = (txt.match(/\b(sym|foe|req|where|food)\b/g) || []);
    return {
      hasWeiDing: /未定/.test(txt), hasDash: /—/.test(txt),
      rels, bareKeys, relCount: rels.length,
      noteShown: rels.some(r => /[a-zA-Z\u4e00-\u9fa5]{2,}\s*[·]?\s*$/ && r.length > 4)
    };
  }));
  await shot('p20-codex-detail');

  /* 换一张有 rel 的卡，确保关系链真的渲染过 */
  P('P20_F32_rel', await ev(() => {
    const { D, UI } = window.__dbg;
    const withRel = D.cards.find(c => (c.rel || []).some(r => r.note));
    if (!withRel) return { found: false };
    UI.openCard(withRel.id);
    const rels = [...document.querySelectorAll('.dex .rel .rl')].map(r => r.textContent.replace(/\s+/g, ' ').trim());
    const raw = withRel.rel.map(r => r.k + '→' + r.to + (r.note ? '|' + r.note : ''));
    return { found: true, card: withRel.name.zh, rels, raw };
  }));
  await shot('p20-rel');

  /* ───── 15. U22/P12 底部渐隐 ───── */
  P('U22', await ev(() => {
    const { UI } = window.__dbg;
    UI.openPanel('codex');
    const cd = document.querySelector('.cdscroll');
    const cs = cd ? getComputedStyle(cd, '::after') : null;
    UI.closePanel();
    const grow = document.querySelector('#rightpanel .rp.grow');
    const g2 = grow ? getComputedStyle(grow, '::after') : null;
    const pb = document.querySelector('#pbody');
    return {
      cdAfter: cs ? { content: cs.content, h: cs.height, bg: cs.backgroundImage.slice(0, 40) } : null,
      growAfter: g2 ? { content: g2.content, h: g2.height } : null,
      pbodyAfter: pb ? getComputedStyle(pb, '::after').content : null
    };
  }));

  /* ───── 16. U32 log 底板 ───── */
  P('U32', await ev(() => {
    const el = document.getElementById('log');
    const cs = getComputedStyle(el);
    return { bg: cs.backgroundImage, color: cs.color, fs: cs.fontSize, display: cs.display,
      shadow: cs.textShadow.slice(0, 40) };
  }));

  /* ───── 30. U23 右栏四段高度 vs 面板高度 ───── */
  P('U23', await ev(() => {
    const { UI } = window.__dbg;
    UI.closePanel();
    UI.renderRight(true);
    const panel = document.getElementById('rightpanel').getBoundingClientRect();
    const rps = [...document.querySelectorAll('#rightpanel .rp')];
    const seg = rps.map(r => {
      const b = r.getBoundingClientRect();
      return { cls: r.className, h: Math.round(b.height), sh: r.scrollHeight, ch: r.clientHeight,
        cs: getComputedStyle(r).flex, ov: getComputedStyle(r).overflowY };
    });
    const sum = seg.reduce((a, s) => a + s.h, 0);
    return { panelH: Math.round(panel.height), segs: seg, sum, gap: Math.round(panel.height - sum),
      docOverflow: document.getElementById('rightpanel').scrollHeight - document.getElementById('rightpanel').clientHeight };
  }));

  /* ───── 7/8/9/10/11. 拖拽 ───── */
  /* 重置：给世界一片地，选一张土地卡 */
  const setup = await ev(() => {
    const { S, D, UI, R } = window.__dbg;
    S.W.essence = 300;
    UI.renderHand();
    /* 保证手牌里有一张 land 并选中 */
    let i = S.W.hand.findIndex(c => c.sim.op === 'land');
    if (i < 0) { S.W.hand[0] = D.byId.get('terrain.earth.0.matter.land'); i = 0; }
    UI.select(i);
    const rc = R.canvas.getBoundingClientRect();
    const hand = document.getElementById('handwrap').getBoundingClientRect();
    const slot = document.querySelector('.handslot');
    const sb = slot.getBoundingClientRect();
    return { sel: i, cells: S.W.cells.size, canvas: [Math.round(rc.left), Math.round(rc.top), Math.round(rc.width), Math.round(rc.height)],
      handTop: Math.round(hand.top), slot: [Math.round(sb.x + sb.width / 2), Math.round(sb.y + sb.height / 2)] };
  });
  P('dragSetup', setup);

  /* 找一个「虚空」点（pickAt 返回 null），用平面点做落点 */
  const target = await ev(() => {
    const { R } = window.__dbg;
    const rc = R.canvas.getBoundingClientRect();
    const hand = document.getElementById('handwrap').getBoundingClientRect();
    for (let gy = 0.18; gy < 0.7; gy += 0.04) {
      for (let gx = 0.30; gx < 0.75; gx += 0.04) {
        const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy;
        if (y > hand.top - 30) continue;
        if (!R.cam.pickAt(x, y)) {
          const p = R.cam.planeAt(x, y);
          return { x: Math.round(x), y: Math.round(y), plane: p };
        }
      }
    }
    return null;
  });
  P('dragTarget', target);

  async function realDrag(tx, ty, opts = {}) {
    const s = await ev(() => {
      const slot = document.querySelector('.handslot');
      const b = slot.getBoundingClientRect();
      return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
    });
    await page.mouse.move(s.x, s.y);
    await page.mouse.down();
    for (let i = 1; i <= 8; i++) {
      await page.mouse.move(s.x + (tx - s.x) * i / 8, s.y + (ty - s.y) * i / 8);
      await page.waitForTimeout(25);
    }
    await page.waitForTimeout(120);
    const mid = await ev(() => {
      const g = document.getElementById('ghost');
      const tip = g && g.querySelector('.tip');
      const m = window.__dbg.R.marks.count();
      return {
        ghost: !!g, ghosts: document.querySelectorAll('#ghost').length,
        tip: tip ? tip.textContent : null,
        marks: m, hover: !!window.__dbg.R.cam.hover,
        cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length
      };
    });
    return mid;
  }

  /* 7. H01 拖到世界 */
  const d1 = await realDrag(target.x, target.y);
  const f1 = await shot('h01-drag-over-world');
  const before = await ev(() => ({ cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length }));
  await page.mouse.up();
  await page.waitForTimeout(420);
  const after1 = await ev(() => ({
    cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length,
    ghosts: document.querySelectorAll('#ghost').length,
    gainfly: (document.getElementById('gainfly') || {}).textContent || null,
    flyOpacity: document.getElementById('gainfly') ? getComputedStyle(document.getElementById('gainfly')).opacity : null,
    toasts: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.trim())
  }));
  P('H01', { drag: d1, shot: f1, before, after: after1 });
  await shot('h01-after-cast');

  /* 8. H02 向下拖 40–90px */
  const d2 = await realDrag(target.x, target.y + 0);
  await page.mouse.up();          // 先清干净上一次
  await page.waitForTimeout(250);
  const s2 = await ev(() => {
    const slot = document.querySelector('.handslot');
    const b = slot.getBoundingClientRect();
    return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
  });
  const pre2 = await ev(() => ({ cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length }));
  await page.mouse.move(s2.x, s2.y);
  await page.mouse.down();
  for (let i = 1; i <= 6; i++) { await page.mouse.move(s2.x + 6, s2.y + 62 * i / 6); await page.waitForTimeout(30); }
  await page.waitForTimeout(150);
  const mid2 = await ev(() => {
    const g = document.getElementById('ghost');
    return { ghost: !!g, tip: g && g.querySelector('.tip') ? g.querySelector('.tip').textContent : null,
      marks: window.__dbg.R.marks.count(), cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length };
  });
  const f2 = await shot('h02-drag-down-62px');
  await page.mouse.up();
  await page.waitForTimeout(350);
  const post2 = await ev(() => ({ cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length,
    ghosts: document.querySelectorAll('#ghost').length }));
  P('H02', { pre: pre2, mid: mid2, post: post2, shot: f2, dy: 62 });

  /* 9. U38 拖拽中按 Esc */
  const s3 = await ev(() => {
    const slot = document.querySelector('.handslot');
    const b = slot.getBoundingClientRect();
    return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
  });
  const pre3 = await ev(() => ({ cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length }));
  await page.mouse.move(s3.x, s3.y);
  await page.mouse.down();
  for (let i = 1; i <= 6; i++) { await page.mouse.move(s3.x, s3.y - 130 * i / 6); await page.waitForTimeout(30); }
  const beforeEsc = await ev(() => ({ ghosts: document.querySelectorAll('#ghost').length,
    tip: document.querySelector('#ghost .tip') ? document.querySelector('#ghost .tip').textContent : null }));
  await page.keyboard.press('Escape');
  await page.waitForTimeout(220);
  const afterEsc = await ev(() => ({ ghosts: document.querySelectorAll('#ghost').length,
    cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length }));
  const f3 = await shot('u38-esc-cancelled');
  await page.mouse.up();
  await page.waitForTimeout(200);
  const post3 = await ev(() => ({ cells: window.__dbg.S.W.cells.size, hand: window.__dbg.S.W.hand.length,
    ghosts: document.querySelectorAll('#ghost').length }));
  P('U38', { pre: pre3, beforeEsc, afterEsc, postMouseUp: post3, shot: f3 });

  /* 10. H04/E01 连续两次 pointerdown 不残留 #ghost */
  const s4 = await ev(() => {
    const slot = document.querySelector('.handslot');
    const b = slot.getBoundingClientRect();
    return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
  });
  await page.mouse.move(s4.x, s4.y);
  await page.mouse.down();
  await page.mouse.move(s4.x + 3, s4.y - 60);
  await page.waitForTimeout(80);
  const ghost1 = await ev(() => document.querySelectorAll('#ghost').length);
  /* 第二次 pointerdown：不松手，直接在手牌另一张上再按下 */
  const s5 = await ev(() => {
    const slots = document.querySelectorAll('.handslot');
    const b = slots[Math.min(1, slots.length - 1)].getBoundingClientRect();
    return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
  });
  await page.mouse.move(s5.x, s5.y);
  await page.waitForTimeout(60);
  await page.mouse.up();
  await page.waitForTimeout(120);
  await page.mouse.down();
  await page.mouse.move(s5.x + 2, s5.y - 40);
  await page.waitForTimeout(120);
  const ghost2 = await ev(() => ({ ghosts: document.querySelectorAll('#ghost').length,
    firstGhostLeft: document.getElementById('ghost') ? getComputedStyle(document.getElementById('ghost')).left : null }));
  const f4 = await shot('h04-double-pointerdown');
  await page.mouse.up();
  await page.waitForTimeout(250);
  const ghost3 = await ev(() => ({ ghosts: document.querySelectorAll('#ghost').length, cells: window.__dbg.S.W.cells.size,
    hand: window.__dbg.S.W.hand.length }));
  P('H04', { ghostAfterOne: ghost1, ghostAfterTwo: ghost2, ghostAfterRelease: ghost3, shot: f4 });

  /* 11. H06 成功回执：mark pulse + flyTo 文本 */
  P('H06', await ev(() => {
    const { S, D, UI, R } = window.__dbg;
    UI.closePanel();
    let i = S.W.hand.findIndex(c => c.sim.op === 'land');
    if (i < 0) return { ok: false };
    /* 找一块空地 */
    let spot = null;
    for (let r = 8; r < 30 && !spot; r++) {
      for (let x = -r; x <= r && !spot; x++) for (let y = -r; y <= r; y++) if (!S.at(x, y)) { spot = { x: x, y: y }; break; }
    }
    const before = S.W.cells.size;
    const res = UI.cast(i, null, spot);
    const fly = document.getElementById('gainfly');
    const pulses = (R.marks && R.marks.__pulses) ? null : null;
    return { before, after: S.W.cells.size, spot,
      flyText: fly ? fly.textContent : null, flyOpacity: fly ? getComputedStyle(fly).opacity : null,
      toast: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.trim()) };
  }));
  await page.waitForTimeout(180);
  await shot('h06-cast-receipt');

  /* 28/29 已在 P28 覆盖；E16 从 log 取 */
  P('errs', errs);
  fs.writeFileSync(path.join(OUT, 'probes.json'), JSON.stringify(out, null, 1));
  console.log(JSON.stringify(out.probes, null, 1));
  await browser.close();
})().catch(e => { console.error('PROBE FAIL', e); process.exit(1); });
