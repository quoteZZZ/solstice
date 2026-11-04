/* 回归验证探针 3：局部放大取证（U22 渐隐 / U21 顶栏 / H01 预览 / F14 toast / U31 痕迹环）
   node tests/review-run/regress-probe3.js */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const ROOT = path.join(__dirname, '..', '..');
const PAGE = pathToFileURL(path.join(ROOT, 'index.html')).href;
const OUT = path.join(__dirname, 'regress-probe3');
fs.mkdirSync(OUT, { recursive: true });
const out = { probes: {}, shots: [] };

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1366, height: 800 }, deviceScaleFactor: 2 });
  page.on('pageerror', e => out.probes.err = String(e.message));
  const clipShot = async (name, clip) => {
    await page.waitForTimeout(300);
    await page.screenshot({ path: path.join(OUT, name + '.png'), clip });
    out.shots.push(name + '.png');
  };

  await page.goto(PAGE);
  await page.waitForTimeout(1500);
  await page.evaluate(() => { window.__dbg.UI.enterFromCreator(); window.__dbg.UI.updateHud(); window.__dbg.UI.renderHand(); });
  await page.waitForTimeout(400);

  /* 顶栏第 0 年 */
  await clipShot('clock-year0', { x: 0, y: 0, width: 300, height: 40 });
  out.probes.clock = await page.evaluate(() => document.getElementById('clock').textContent.replace(/\s+/g, ' ').trim());

  /* F14：走 UI 路径看是否有说明 */
  out.probes.F14 = await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -2; x <= 2; x++) for (let y = -2; y <= 2; y++) S.cmd.cast(land, x, y);
    document.getElementById('toasts').innerHTML = '';
    S.W.hand[0] = land; UI.select(0);
    UI.renderHand();
    const h0 = S.W.hand.length, c0 = S.W.cells.size;
    UI.cast(0, { x: 0, y: 0 }, null);
    return { handBefore: h0, handAfter: S.W.hand.length, cellsBefore: c0, cellsAfter: S.W.cells.size,
      toasts: [...document.querySelectorAll('#toasts .toast')].map(t => t.textContent.replace(/\s+/g, ' ').trim()) };
  });
  await clipShot('f14-toast', { x: 300, y: 40, width: 700, height: 120 });

  /* H01：先把镜头拉近，再拖到已有格上，看预览是否肉眼可见 */
  await page.evaluate(() => {
    const { S, D, UI, R } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    S.W.hand[0] = land; UI.select(0); UI.renderHand();
    R.cam.setZoom(1);
    document.getElementById('toasts').innerHTML = '';
  });
  await page.waitForTimeout(900);
  const pt = await page.evaluate(() => {
    const { R } = window.__dbg;
    const rc = R.canvas.getBoundingClientRect();
    const hand = document.getElementById('handwrap').getBoundingClientRect();
    for (let gy = 0.25; gy < 0.7; gy += 0.03)
      for (let gx = 0.3; gx < 0.8; gx += 0.03) {
        const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy;
        if (y > hand.top - 40) continue;
        const c = R.cam.pickAt(x, y);
        if (c) return { x: Math.round(x), y: Math.round(y), cell: [c.x, c.y] };
      }
    return null;
  });
  out.probes.H01_pt = pt;
  if (pt) {
    const s = await page.evaluate(() => {
      const b = document.querySelector('.handslot').getBoundingClientRect();
      return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
    });
    await page.mouse.move(s.x, s.y);
    await page.mouse.down();
    for (let i = 1; i <= 8; i++) { await page.mouse.move(s.x + (pt.x - s.x) * i / 8, s.y + (pt.y - s.y) * i / 8); await page.waitForTimeout(25); }
    await page.waitForTimeout(200);
    out.probes.H01_marks = await page.evaluate(() => window.__dbg.R.marks.count());
    await page.screenshot({ path: path.join(OUT, 'h01-preview-zoom.png'), clip: { x: 380, y: 120, width: 620, height: 380 } });
    out.shots.push('h01-preview-zoom.png');
    await page.mouse.up();
    await page.waitForTimeout(400);
  }

  /* U31：痕迹环数量 + 一格环的半径 */
  out.probes.U31 = await page.evaluate(() => {
    const { S, R } = window.__dbg;
    const rings = R.traceGroup.children.filter(m => m.geometry.type === 'RingGeometry');
    const dots = R.traceGroup.children.filter(m => m.geometry.type !== 'RingGeometry');
    const one = rings[0];
    return { traces: S.W.traces.length, rings: rings.length, dots: dots.length,
      opacity: one ? one.material.opacity : null, ringR: one ? +one.geometry.parameters.outerRadius.toFixed(3) : null,
      sampleCoords: S.W.traces.slice(0, 4).map(t => [t.x, t.y, t.name, t.count]) };
  });

  /* U22：放大截详情列底部 70px */
  await page.evaluate(() => { window.__dbg.UI.openPanel('codex'); });
  await page.waitForTimeout(400);
  const geo = await page.evaluate(() => {
    const cd = document.querySelector('.cdscroll');
    const b = cd.getBoundingClientRect();
    const pb = document.getElementById('pbody').getBoundingClientRect();
    return { cd: [Math.round(b.x), Math.round(b.y), Math.round(b.width), Math.round(b.height)], pb: [Math.round(pb.x), Math.round(pb.y), Math.round(pb.width), Math.round(pb.height)],
      scrollable: cd.scrollHeight - cd.clientHeight };
  });
  out.probes.U22_geo = geo;
  await clipShot('u22-detail-bottom', { x: geo.cd[0], y: geo.cd[1] + geo.cd[3] - 64, width: geo.cd[2], height: 64 });
  await clipShot('u22-pbody-bottom', { x: geo.pb[0], y: geo.pb[1] + geo.pb[3] - 46, width: geo.pb[2], height: 46 });

  /* 右栏最下一段底部 */
  await page.evaluate(() => { window.__dbg.UI.closePanel(); window.__dbg.UI.renderRight(true); });
  await page.waitForTimeout(300);
  const rg = await page.evaluate(() => {
    const g = document.querySelector('#rightpanel .rp.grow');
    const b = g.getBoundingClientRect();
    return { x: Math.round(b.x), y: Math.round(b.y), w: Math.round(b.width), h: Math.round(b.height),
      scrollable: g.scrollHeight - g.clientHeight };
  });
  out.probes.U22_grow = rg;
  await clipShot('u22-grow-bottom', { x: rg.x, y: rg.y + rg.h - 70, width: rg.w, height: 70 });

  fs.writeFileSync(path.join(OUT, 'probes.json'), JSON.stringify(out, null, 1));
  console.log(JSON.stringify(out.probes, null, 1));
  await browser.close();
})().catch(e => { console.error('FAIL', e); process.exit(1); });
