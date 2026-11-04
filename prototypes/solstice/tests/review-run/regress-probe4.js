/* 探针 4：落点预览到底看不看得见（同一机位，仅开/关预览，做像素差分）
   node tests/review-run/regress-probe4.js */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const ROOT = path.join(__dirname, '..', '..');
const PAGE = pathToFileURL(path.join(ROOT, 'index.html')).href;
const OUT = path.join(__dirname, 'regress-probe4');
fs.mkdirSync(OUT, { recursive: true });

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1366, height: 800 }, deviceScaleFactor: 1 });
  await page.goto(PAGE);
  await page.waitForTimeout(1500);
  await page.evaluate(() => { window.__dbg.UI.enterFromCreator(); window.__dbg.UI.updateHud(); window.__dbg.UI.renderHand(); });
  await page.waitForTimeout(300);

  /* 造一片地、选一张土地卡、把镜头摆固定 */
  const info = await page.evaluate(() => {
    const { S, D, UI, R } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -2; x <= 2; x++) for (let y = -2; y <= 2; y++) S.cmd.cast(land, x, y);
    S.W.traces = [];                      // 去掉痕迹环，避免干扰对比
    R.trace.reset && R.trace.reset();
    R.trace.sync && R.trace.sync();
    S.W.hand[0] = land; UI.select(0);
    UI.updateHud(); UI.renderHand(); UI.renderRight(true);
    R.cam.setZoom(2); R.cam.setPitch(1);
    return { cells: S.W.cells.size };
  });
  await page.waitForTimeout(1400);
  /* 镜头必须彻底停住：缓动没走完就截图，两次图会差在机位上而不是预览上 */
  await page.evaluate(() => { const C = window.__dbg.R.cam; C.snapGoal(); C.place(); });

  /* 目标格：正中央那格 */
  const cell = { x: 0, y: 0 };
  const center = await page.evaluate(c => {
    const { R } = window.__dbg;
    const p = R.cam.screenOf(c.x, c.y, 0.32);
    return { x: Math.round(p.x), y: Math.round(p.y) };
  }, cell);

  const clip = { x: Math.max(0, center.x - 150), y: Math.max(0, center.y - 110), width: 300, height: 220 };
  const camA = await page.evaluate(() => { const C = window.__dbg.R.cam; return [+C.dist.toFixed(3), +C.pitch.toFixed(3), +C.yaw.toFixed(3), +C.target.x.toFixed(3), +C.target.z.toFixed(3)]; });
  const bufA = await page.screenshot({ clip });
  await page.evaluate(c => {
    const { S, D } = window.__dbg;
    SOL.render.marks.set(S.castRange(D.byId.get('terrain.earth.0.matter.land'), S.at(c.x, c.y)));
  }, cell);
  await page.waitForTimeout(250);
  const camB = await page.evaluate(() => { const C = window.__dbg.R.cam; return [+C.dist.toFixed(3), +C.pitch.toFixed(3), +C.yaw.toFixed(3), +C.target.x.toFixed(3), +C.target.z.toFixed(3)]; });
  const bufB = await page.screenshot({ clip });
  const marks = await page.evaluate(() => window.__dbg.R.marks.count());
  await page.screenshot({ path: path.join(OUT, 'preview-on.png'), clip });
  await page.evaluate(() => window.__dbg.R.marks.clear());
  await page.waitForTimeout(250);
  await page.screenshot({ path: path.join(OUT, 'preview-off.png'), clip });

  /* 在页面里做像素差分（同一 canvas 尺寸） */
  const diff = await page.evaluate(async ([a, b]) => {
    const load = src => new Promise(res => { const im = new Image(); im.onload = () => res(im); im.src = src; });
    const [ia, ib] = await Promise.all([load(a), load(b)]);
    const w = ia.width, h = ia.height;
    const mk = im => { const c = document.createElement('canvas'); c.width = w; c.height = h; const g = c.getContext('2d'); g.drawImage(im, 0, 0); return g.getImageData(0, 0, w, h).data; };
    const da = mk(ia), db = mk(ib);
    let sum = 0, max = 0, cnt = 0;
    for (let i = 0; i < da.length; i += 4) {
      const d = Math.abs(da[i] - db[i]) + Math.abs(da[i + 1] - db[i + 1]) + Math.abs(da[i + 2] - db[i + 2]);
      sum += d; if (d > max) max = d; if (d > 6) cnt++;
    }
    return { w, h, meanDeltaPerPx: +(sum / (da.length / 4)).toFixed(2), maxDelta: max, changedPx: cnt, totalPx: da.length / 4 };
  }, ['data:image/png;base64,' + bufA.toString('base64'), 'data:image/png;base64,' + bufB.toString('base64')]);

  console.log(JSON.stringify({ info, marks, center, clip, camA, camB, diff }, null, 1));
  await browser.close();
})().catch(e => { console.error('FAIL', e); process.exit(1); });
