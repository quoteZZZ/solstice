/* 第二轮回归 · 新快照全量复测（只读源码；输出 regress2-audit/）
   node tests/review-run/regress2-audit.js */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const ROOT = path.join(__dirname, '..', '..');
const PAGE = pathToFileURL(path.join(ROOT, 'index.html')).href;
const OUT = path.join(__dirname, 'regress2-audit');
fs.mkdirSync(OUT, { recursive: true });
const R = { meta: {}, p: {}, shots: [] };

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1366, height: 800 }, deviceScaleFactor: 1 });
  const errs = [];
  page.on('pageerror', e => errs.push(String(e.message)));
  const snap = async name => { await page.waitForTimeout(260); await page.screenshot({ path: path.join(OUT, name + '.png') }); R.shots.push(name + '.png'); };

  await page.goto(PAGE);
  await page.waitForTimeout(1500);
  R.meta.files = fs.readdirSync(path.join(ROOT, 'ui')).map(f => f + ':' + fs.statSync(path.join(ROOT, 'ui', f)).mtime.toTimeString().slice(0, 8));

  /* 17b 顶栏第 0 年 */
  R.p['17_clockYear0'] = await page.evaluate(() => {
    const y = document.getElementById('cYear');
    return { text: document.getElementById('clock').textContent.replace(/\s+/g, ' ').trim(), yearDisplay: getComputedStyle(y).display, yearText: y.textContent };
  });
  await snap('17-clock-year0');

  await page.evaluate(() => { window.__dbg.UI.enterFromCreator(); window.__dbg.UI.updateHud(); window.__dbg.UI.renderHand(); });
  await page.waitForTimeout(500);

  /* 1 F14 */
  R.p['1_F14'] = await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -2; x <= 2; x++) for (let y = -2; y <= 2; y++) S.cmd.cast(land, x, y);
    document.getElementById('toasts').innerHTML = '';
    S.W.hand[0] = land; UI.select(0); UI.renderHand();
    const h0 = S.W.hand.length, c0 = S.W.cells.size, id0 = S.W.hand[0].id;
    const res = S.cmd.playSlot(0, 0, 0);
    return { res, hand: [h0, S.W.hand.length], cells: [c0, S.W.cells.size], keep: S.W.hand.some(c => c.id === id0),
      toasts: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.replace(/\s+/g, ' ').trim()) };
  });

  /* 5 U01 */
  R.p['5_U01'] = await page.evaluate(() => ({ btn: document.getElementById('bPack').textContent,
    cheapest: Math.min(...window.__dbg.S.packList().map(p => p.cost)) }));

  /* 20/23 U12/U18 空世界 */
  R.p['20_23_U12U18'] = await page.evaluate(() => {
    const { S, UI } = window.__dbg;
    const keep = new Map(S.W.cells); S.W.cells.clear(); UI.renderRight(true);
    const rp = [...document.querySelectorAll('#rightpanel .rp')];
    const out = { bars: rp[1].querySelectorAll('.fbar').length, envText: rp[1].textContent.replace(/\s+/g, ' ').trim(),
      grid: [...rp[2].querySelectorAll('div')].map(d => d.textContent.trim()).filter(t => t === '–' || t === '●').slice(0, 3) };
    keep.forEach((v, k) => S.W.cells.set(k, v)); UI.renderRight(true);
    return out;
  });

  /* 6 U19 */
  R.p['6_U19'] = await page.evaluate(() => {
    const { S, UI } = window.__dbg;
    const t0 = S.W.traces; S.W.traces = []; UI.renderRight(true);
    const txt = [...document.querySelectorAll('#rightpanel .rp')][3].textContent.replace(/\s+/g, ' ');
    S.W.traces = t0; UI.renderRight(true);
    return { cells: S.W.cells.size, dissolved: /最近的痕迹都已消散/.test(txt), neverTouched: /世界还没被碰过/.test(txt) };
  });

  /* 21 U24 */
  R.p['21_U24'] = await page.evaluate(() => { const h = document.querySelector('#rightpanel .rp .h');
    return { after: getComputedStyle(h, '::after').content, before: getComputedStyle(h, '::before').content }; });

  /* 22 U17 生态色块 */
  R.p['22_U17'] = await page.evaluate(() => [...document.querySelectorAll('#rightpanel .rp')[2].querySelectorAll('i')]
    .map(i => { const b = i.getBoundingClientRect(); return [+b.width.toFixed(1), +b.height.toFixed(1), getComputedStyle(i).backgroundColor]; }));

  /* 19 U09 左栏 */
  R.p['19_U09'] = await page.evaluate(() => { const s = document.querySelector('#leftrail .rail svg').getBoundingClientRect();
    return { restX: +s.x.toFixed(2) }; });
  await page.hover('#leftrail'); await page.waitForTimeout(420);
  Object.assign(R.p['19_U09'], await page.evaluate(() => {
    const s = document.querySelector('#leftrail .rail svg').getBoundingClientRect();
    const sp = document.querySelector('#leftrail .rail span'); const b = sp.getBoundingClientRect();
    return { hoverX: +s.x.toFixed(2), labelLeft: +b.x.toFixed(1), labelW: +b.width.toFixed(1), labelOp: getComputedStyle(sp).opacity };
  }));
  await page.mouse.move(700, 300); await page.waitForTimeout(350);

  /* 15 U22 / 16 U32 */
  R.p['15_U22'] = await page.evaluate(() => {
    const { UI } = window.__dbg; UI.openPanel('codex');
    const cd = document.querySelector('.cdscroll');
    const a = cd ? getComputedStyle(cd, '::after') : null;
    const g = getComputedStyle(document.querySelector('#rightpanel .rp.grow'), '::after');
    const pb = getComputedStyle(document.getElementById('pbody'), '::after');
    UI.closePanel();
    return { cd: a && { h: a.height, bg: a.backgroundImage.slice(0, 30) }, growH: g.height, pbodyH: pb.height };
  });
  R.p['16_U32'] = await page.evaluate(() => {
    const { UI } = window.__dbg; UI.closePanel();
    for (let i = 0; i < 3; i++) UI.log('土地铺开 ' + (12 + i) + ' 格', i === 0);
    const el = document.getElementById('log'); const cs = getComputedStyle(el);
    return { display: cs.display, bg: cs.backgroundImage.slice(0, 60), fs: cs.fontSize, color: cs.color, lines: el.children.length };
  });
  await snap('16-log');

  /* 2/3/4/12 抽卡 */
  R.p['2_3_4_12'] = await page.evaluate(() => {
    const { UI, S } = window.__dbg; S.W.essence = 200; UI.updateHud(); UI.openPanel('pack');
    const rows = [...document.querySelectorAll('.packrow')];
    rows.find(r => /进阶/.test(r.textContent)).click();
    const p01 = { title: document.querySelector('#phead .sm').textContent,
      foot: document.querySelector('#pfoot [data-act="packNext"]').textContent,
      disabled: document.querySelector('#pfoot [data-act="packNext"]').disabled };
    document.querySelector('#pfoot [data-act="packNext"]').click();
    const p04 = { picks: document.querySelectorAll('.packpick').length,
      faces: document.querySelectorAll('.packpick .card').length,
      deal: document.querySelectorAll('.dealcard').length,
      steps: [...document.querySelectorAll('#pbody .tag')].map(t => t.textContent.trim()).filter(t => /^\d\./.test(t)) };
    const btn = document.querySelector('#pfoot [data-act="packNext"]');
    const p03 = { label: btn.textContent, cls: btn.className };
    const before = UI.packCards.length;
    [...document.querySelectorAll('.packrow')].find(r => /基础/.test(r.textContent)).click();
    const p02 = { sel: UI.packSel, stage: UI.packStage, cards: UI.packCards.length, before,
      toast: [...document.querySelectorAll('#phead .toast.ih, #toasts .toast')].map(t => t.textContent.replace(/\s+/g, ' ').trim()) };
    return { p01, p04, p03, p02, pickSizes: [...document.querySelectorAll('.packpick')].slice(0, 2).map(p => { const b = p.getBoundingClientRect(); return [Math.round(b.width), Math.round(b.height)]; }) };
  });
  await snap('12-pack-deal');
  R.p['2_3_4_12_afford'] = await page.evaluate(() => {
    const { UI, S } = window.__dbg; S.W.essence = 15; UI.updateHud(); UI.openPanel('pack');
    const rd = () => { const b = document.querySelector('#pfoot [data-act="packNext"]'); return { t: b.textContent, d: b.disabled }; };
    document.querySelector('.packrow').click(); const basic = rd();
    [...document.querySelectorAll('.packrow')].find(r => /进阶/.test(r.textContent)).click(); const mid = rd();
    UI.closePanel();
    return { basic, mid, top: document.getElementById('bPack').textContent };
  });

  /* 13/14 F12 F13 P40 */
  R.p['13_F12'] = await page.evaluate(() => {
    const { UI } = window.__dbg; const box = document.getElementById('toasts'); box.innerHTML = '';
    for (let i = 0; i < 4; i++) UI.toast('同一句', '同细节');
    const dup = box.children.length;
    for (let i = 0; i < 6; i++) UI.toast('不同 ' + i);
    return { dup, cap: box.children.length };
  });
  R.p['14_P40'] = await page.evaluate(() => {
    const { UI } = window.__dbg; const box = document.getElementById('toasts');
    UI.toast('旧提示'); const before = box.children.length;
    UI.openPanel('chron'); const cleared = box.children.length;
    UI.toast('面板内提示', '行内');
    const c = document.querySelector('#phead .toast.ih'); const r = c && c.getBoundingClientRect();
    const body = document.getElementById('pbody').getBoundingClientRect();
    UI.closePanel();
    return { before, cleared, inline: !!c, top: r && Math.round(r.top), bottom: r && Math.round(r.bottom), bodyTop: Math.round(body.top) };
  });
  await snap('14-p40');

  /* 17 U21 年表 + 事件 */
  R.p['17_U21'] = await page.evaluate(() => {
    const { S, UI } = window.__dbg;
    for (let i = 0; i < 30; i++) S.cmd.tick(1);
    UI.updateHud(); UI.renderRight(true); UI.openPanel('chron');
    const chron = document.getElementById('pbody').textContent.replace(/\s+/g, ' ');
    const clock = document.getElementById('clock').textContent.replace(/\s+/g, ' ').trim();
    const bad = { diNian: /第\s*\d+\s*年/.test(chron), decimal: /\d+\.\d+\s*年/.test(chron), diYue: /第\s*\d+\s*月/.test(chron) };
    UI.closePanel();
    UI.fireEvent();
    const ev = document.querySelector('#eventcard .row span:last-child');
    const evTxt = ev ? ev.textContent.trim() : null;
    UI.closeEvent();
    return { clock, month: S.W.month, bad, samples: (chron.match(/\d+\s*年\s*\d+\s*月|\d+\s*月/g) || []).slice(0, 8), eventMonth: evTxt };
  });
  await snap('17-u21');

  /* 18 E13 */
  R.p['18_E13'] = await page.evaluate(() => {
    const { UI } = window.__dbg; const box = document.getElementById('log'); box.innerHTML = '';
    ['chron', 'codex', 'settings'].forEach(p => { UI.openPanel(p); UI.closePanel(); });
    return { lines: box.children.length, text: box.textContent };
  });

  /* 26 P11 */
  R.p['26_P11'] = await page.evaluate(() => {
    const { UI } = window.__dbg; UI.openPanel('codex');
    const t = document.getElementById('pbody').textContent.replace(/\s+/g, ' ');
    const seen = (t.match(/已见\s*\d+\s*\/\s*\d+/g) || []);
    UI.closePanel();
    return { seen, pool: /池\s*\d+\s*\/\s*\d+/.test(t) };
  });

  /* 27/29 P28 键位 */
  R.p['27_29_P28'] = await page.evaluate(() => {
    const { UI } = window.__dbg; UI.openPanel('keys');
    const rows = [...document.querySelectorAll('.keyRow')];
    const data = rows.map(r => { const n = r.querySelector('.sm'), b = r.querySelector('button');
      const nb = n.getBoundingClientRect(), bb = b.getBoundingClientRect();
      return { t: n.textContent.trim(), h: Math.round(nb.height), btn: Math.round(bb.width), sw: n.scrollWidth, cw: n.clientWidth, key: b.textContent.trim() }; });
    UI.closePanel();
    return { n: rows.length, maxH: Math.max(...data.map(d => d.h)), btnW: [...new Set(data.map(d => d.btn))],
      overflow: data.filter(d => d.sw > d.cw + 1).length, space: (data.find(d => d.t === '随机释放') || {}).key };
  });
  await snap('27-keys');

  /* 24/25 P20 F32 */
  R.p['24_25_P20'] = await page.evaluate(() => {
    const { D, UI } = window.__dbg;
    const withRel = D.cards.find(c => (c.rel || []).some(r => r.note));
    UI.openCard(withRel.id);
    const d = document.querySelector('.cdscroll');
    const txt = d.textContent.replace(/\s+/g, ' ');
    const rels = [...document.querySelectorAll('.dex .rel .rl')].map(r => r.textContent.replace(/\s+/g, ' ').trim());
    const out = { card: withRel.name.zh, weiDing: /未定/.test(txt),
      dashCtx: (txt.match(/.{0,6}—.{0,6}/g) || []).slice(0, 3),
      rels, bare: (txt.match(/\b(sym|foe|req|where|food)\b/g) || []) };
    UI.closePanel();
    return out;
  });
  await snap('24-codex');

  /* 30 U23 */
  R.p['30_U23'] = await page.evaluate(() => {
    const { UI } = window.__dbg; UI.closePanel(); UI.renderRight(true);
    const panel = document.getElementById('rightpanel');
    const segs = [...document.querySelectorAll('#rightpanel .rp')].map(r => { const b = r.getBoundingClientRect();
      return { h: Math.round(b.height), sh: r.scrollHeight, ch: r.clientHeight }; });
    return { panelH: Math.round(panel.getBoundingClientRect().height), segs, sum: segs.reduce((a, s) => a + s.h, 0),
      overflow: panel.scrollHeight - panel.clientHeight };
  });

  /* 31 U31 */
  R.p['31_U31'] = await page.evaluate(() => {
    const { S, R } = window.__dbg;
    const rings = R.traceGroup.children.filter(m => m.geometry.type === 'RingGeometry');
    return { traces: S.W.traces.length, rings: rings.length, opacity: rings[0] && rings[0].material.opacity,
      r: rings[0] && +rings[0].geometry.parameters.outerRadius.toFixed(3) };
  });

  /* 28 E16 */
  R.meta.handScale = await page.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--hand-scale').trim());
  R.meta.errs = errs;
  fs.writeFileSync(path.join(OUT, 'audit.json'), JSON.stringify(R, null, 1));
  console.log(JSON.stringify(R.p, null, 1));
  console.log('meta', JSON.stringify(R.meta));
  await browser.close();
})().catch(e => { console.error('FAIL', e); process.exit(1); });
