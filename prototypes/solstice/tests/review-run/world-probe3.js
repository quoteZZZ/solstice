/* Solstice · 世界画面审计探针 3：干净对照（隐去立牌看格线 / 同格痕迹分状态对比） */
const path = require('path'), fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'world-shots');
const M = { shots: [], failures: [] };
const shot = async (page, name, clip) => {
  await page.waitForTimeout(220);
  await page.screenshot({ path: path.join(OUT, name + '.png'), clip });
  M.shots.push(name); console.log('shot', name);
};
const SYNC = `const sync = () => { const c = R.camera; c.updateMatrixWorld(true);
  c.matrixWorldInverse.copy(c.matrixWorld).invert(); c.updateProjectionMatrix(); };`;

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1680, height: 1000 }, deviceScaleFactor: 1 });
  page.on('pageerror', e => M.failures.push('[pageerror] ' + e.message));
  await page.goto(PAGE);
  await page.waitForTimeout(1400);

  await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    UI.enterFromCreator();
    const g = id => D.byId.get(id);
    const water = g('terrain.mineral.0.matter.water');
    const rock = g('terrain.mineral.0.matter.rock');
    const grass = g('flora.wood.1.matter.grass');
    const tree = g('flora.wood.2.matter.tree');
    for (let x = -6; x <= 6; x++) for (let y = -6; y <= 6; y++) S.addCell(x, y, { h: 0.30 });
    for (let x = -6; x <= 6; x++) S.cmd.cast(water, x, -2);
    for (let x = 4; x <= 6; x++) for (let y = 3; y <= 6; y++) S.cmd.cast(rock, x, y);
    for (let x = -6; x <= 2; x++) for (let y = -6; y <= 6; y++) {
      const c = S.at(x, y); if (!c || c.kind === 'water' || c.kind === 'rock') continue;
      S.spawn(grass, c, {});
    }
    for (const [x, y] of [[-1, 1], [0, 1], [1, 1], [-2, 2]]) S.spawn(tree, S.at(x, y), {});
    for (const c of S.W.cells.values()) S.classifyAround(c);
    S.W.sun = 2; S.W.essence = 999;
    UI.updateHud(); UI.renderRight(); UI.renderHand();
  });
  await page.waitForTimeout(700);

  /* ── A. 隐去立牌，只留地块：格界到底可不可见 ── */
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.standeeGroup.visible = false;
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.52; R.cam.dist = 8.6;
    R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-ground-only-F2');
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.pitch = 1.45; R.cam.dist = 8.6; R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-ground-only-F4');
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.pitch = 0.26; R.cam.dist = 8.6; R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-ground-only-F1');
  await page.evaluate(() => { window.__dbg.R.standeeGroup.visible = true; });

  /* ── B. 同格痕迹：无 / 1 环 / 3 环 / +预览 / +扩散环，同机位 ── */
  const setupCell = async (x, y, dist, pitch) => page.evaluate(({ x, y, dist, pitch }) => {
    const { R } = window.__dbg;
    R.cam.yaw = Math.PI / 4; R.cam.pitch = pitch; R.cam.dist = dist;
    R.cam.target.set(x, 0.25, y); R.cam.place(); R.cam.snapGoal();
  }, { x, y, dist, pitch });
  const cellPx = () => page.evaluate(`(() => {
    const { R, S } = window.__dbg; const THREE = window.THREE;
    ${SYNC}
    sync();
    const r = R.canvas.getBoundingClientRect();
    const proj = (x,y,z) => { const v = new THREE.Vector3(x,y,z).project(R.camera);
      return { x: Math.round((v.x*.5+.5)*r.width + r.left), y: Math.round((-v.y*.5+.5)*r.height + r.top) }; };
    const c = { x: 4, y: 0 };
    const a = proj(c.x-0.5, 0.30, c.y-0.5), b = proj(c.x+0.5, 0.30, c.y-0.5);
    const d = proj(c.x-0.5, 0.30, c.y+0.5);
    return { a, b, d, wPx: Math.round(Math.hypot(b.x-a.x,b.y-a.y)), hPx: Math.round(Math.hypot(d.x-a.x,d.y-a.y)) };
  })()`);

  await page.evaluate(() => { const { S, R } = window.__dbg; S.W.traces = []; R.trace.sync(); });
  await setupCell(4, 0, 2.6, 1.45);
  await shot(page, 'w-t4-none');
  M.t4 = { none: await cellPx() };

  await page.evaluate(() => {
    const { S, D, R } = window.__dbg;
    S.cmd.cast(D.byId.get('flora.wood.1.matter.grass'), 4, 0);
    R.trace.sync();
  });
  await shot(page, 'w-t4-one');
  await page.evaluate(() => {
    const { S, D, R } = window.__dbg;
    S.cmd.cast(D.byId.get('terrain.mineral.0.matter.water'), 4, 0);
    S.cmd.cast(D.byId.get('terrain.mineral.0.matter.rock'), 4, 0);
    R.trace.sync();
  });
  await shot(page, 'w-t4-three');

  await page.evaluate(() => {
    const { S, D, R, UI } = window.__dbg;
    const card = D.byId.get('terrain.mineral.0.matter.water');
    S.W.hand.unshift(card); S.W.sel = 0;
    R.marks.set(S.castRange(card, S.at(4, 0)));
    R.marks.cursor(S.at(4, 0));
    UI.renderHand();
  });
  await shot(page, 'w-t4-preview');
  await page.evaluate(() => { const { R } = window.__dbg; R.marks.pulse(4, 0, '#7fd8f0', 0.30); });
  await page.waitForTimeout(200);
  await shot(page, 'w-t4-pulse');
  await page.waitForTimeout(900);
  await page.evaluate(() => { const { R } = window.__dbg; R.marks.clear(); R.marks.cursor(null); });

  /* 默认游玩机位下的痕迹（46px 直径），并给放大图 */
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.52; R.cam.dist = 8.6;
    R.cam.target.set(1, 0.25, 0); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-t4-default-zoom');
  await page.screenshot({ path: path.join(OUT, 'w-t4-default-crop.png'), clip: { x: 700, y: 380, width: 420, height: 300 } });
  M.shots.push('w-t4-default-crop');

  /* 草地上的痕迹（最常用场景：绿环落绿草） */
  await page.evaluate(() => {
    const { S, D, R } = window.__dbg;
    S.W.traces = [];
    S.cmd.cast(D.byId.get('flora.wood.1.matter.grass'), -4, 2);
    R.trace.sync();
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.95; R.cam.dist = 4.0;
    R.cam.target.set(-4, 0.25, 2); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-t4-on-grass');
  await page.screenshot({ path: path.join(OUT, 'w-t4-on-grass-crop.png'), clip: { x: 690, y: 330, width: 300, height: 280 } });
  M.shots.push('w-t4-on-grass-crop');

  /* ── C. 水面颜色拼块：13 个水格中心色 ── */
  M.waterCells = await page.evaluate(`(() => {
    const { R, S } = window.__dbg; const THREE = window.THREE;
    ${SYNC}
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 1.45; R.cam.dist = 10;
    R.cam.target.set(0, 0.1, -2); R.cam.place(); R.cam.snapGoal(); sync();
    const r = R.canvas.getBoundingClientRect();
    const proj = (x,y,z) => { const v = new THREE.Vector3(x,y,z).project(R.camera);
      return { x: Math.round((v.x*.5+.5)*r.width + r.left), y: Math.round((-v.y*.5+.5)*r.height + r.top) }; };
    const out = [];
    for (let x = -6; x <= 6; x++) {
      const c = S.at(x, -2);
      out.push({ x, kind: c.kind, p: proj(x, 0.116, -2), top: +R.hOf(c).toFixed(3) });
    }
    return out;
  })()`);
  await shot(page, 'w-water-patchwork');

  fs.writeFileSync(path.join(OUT, 'metrics3.json'), JSON.stringify(M, null, 1));
  console.log('failures', M.failures.length, M.failures.slice(0, 3));
  await browser.close();
})();
