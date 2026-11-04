/* 探针 5：落点预览的可见性（近景 + 差分放大图）
   node tests/review-run/regress-probe5.js */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const ROOT = path.join(__dirname, '..', '..');
const PAGE = pathToFileURL(path.join(ROOT, 'index.html')).href;
const OUT = path.join(__dirname, 'regress-probe5');
fs.mkdirSync(OUT, { recursive: true });

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1366, height: 800 }, deviceScaleFactor: 1 });
  await page.goto(PAGE);
  await page.waitForTimeout(1500);
  await page.evaluate(() => { window.__dbg.UI.enterFromCreator(); window.__dbg.UI.updateHud(); window.__dbg.UI.renderHand(); });
  await page.waitForTimeout(300);

  await page.evaluate(() => {
    const { S, D, R, UI } = window.__dbg;
    const land = D.byId.get('terrain.earth.0.matter.land');
    for (let x = -4; x <= 4; x++) for (let y = -4; y <= 4; y++) S.cmd.cast(land, x, y);
    S.W.traces = []; R.trace.reset && R.trace.reset(); R.trace.sync && R.trace.sync();
    S.W.hand[0] = land; UI.select(0); UI.updateHud(); UI.renderHand();
    R.cam.setZoom(4); R.cam.setPitch(1);
  });
  await page.waitForTimeout(1600);
  await page.evaluate(() => { const C = window.__dbg.R.cam; C.snapGoal(); C.place(); });
  await page.waitForTimeout(200);

  const cell = { x: 0, y: 0 };
  const center = await page.evaluate(c => { const p = window.__dbg.R.cam.screenOf(c.x, c.y, 0.32); return { x: Math.round(p.x), y: Math.round(p.y) }; }, cell);
  const clip = { x: Math.max(0, center.x - 220), y: Math.max(0, center.y - 150), width: 440, height: 300 };

  await page.evaluate(() => window.__dbg.R.marks.clear());
  await page.waitForTimeout(200);
  const off = await page.screenshot({ path: path.join(OUT, 'off-close.png'), clip });
  await page.evaluate(c => {
    const { S, D } = window.__dbg;
    SOL.render.marks.set(S.castRange(D.byId.get('terrain.earth.0.matter.land'), S.at(c.x, c.y)));
  }, cell);
  await page.waitForTimeout(250);
  const on = await page.screenshot({ path: path.join(OUT, 'on-close.png'), clip });

  const res = await page.evaluate(async ([a, b]) => {
    const load = src => new Promise(r => { const im = new Image(); im.onload = () => r(im); im.src = src; });
    const [ia, ib] = await Promise.all([load(a), load(b)]);
    const w = ia.width, h = ia.height;
    const mk = im => { const c = document.createElement('canvas'); c.width = w; c.height = h; const g = c.getContext('2d'); g.drawImage(im, 0, 0); return g; };
    const ga = mk(ia), gb = mk(ib);
    const da = ga.getImageData(0, 0, w, h).data, db = gb.getImageData(0, 0, w, h).data;
    let sum = 0, max = 0, changed = 0;
    const out = gb.createImageData(w, h);
    for (let i = 0; i < da.length; i += 4) {
      const dr = Math.abs(da[i] - db[i]), dg = Math.abs(da[i + 1] - db[i + 1]), dbl = Math.abs(da[i + 2] - db[i + 2]);
      const d = dr + dg + dbl;
      sum += d; if (d > max) max = d; if (d > 6) changed++;
      const v = Math.min(255, d * 6);
      out.data[i] = out.data[i + 3] = v; out.data[i + 1] = 0; out.data[i + 2] = 0;
    }
    const c2 = document.createElement('canvas'); c2.width = w; c2.height = h;
    c2.getContext('2d').putImageData(out, 0, 0);
    return { mean: +(sum / (da.length / 4)).toFixed(2), max, changed, total: da.length / 4,
      diffPng: c2.toDataURL('image/png') };
  }, ['data:image/png;base64,' + off.toString('base64'), 'data:image/png;base64,' + on.toString('base64')]);

  fs.writeFileSync(path.join(OUT, 'diff-x6.png'), Buffer.from(res.diffPng.split(',')[1], 'base64'));
  delete res.diffPng;
  console.log(JSON.stringify({ center, clip, diff: res }, null, 1));
  await browser.close();
})().catch(e => { console.error('FAIL', e); process.exit(1); });
