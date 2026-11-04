/* Solstice · 世界画面审计探针 2：补拍 + 像素级量化
   （水陆交界正对取景、相机真正进入地形、痕迹单格三环放大、
    摆件模式岛屿像素包围盒、格线可见度、痕迹/预览/扩散环的像素对比度） */
const path = require('path'), fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const { loadPNG, region, scan, px, luma, hex } = require('./pngstat');

const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'world-shots');
const M = { shots: [], failures: [] };
const shot = async (page, name, clip) => {
  await page.waitForTimeout(240);
  const f = path.join(OUT, name + '.png');
  await page.screenshot({ path: f, clip });
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
    const shrub = g('flora.wood.1.matter.shrub');
    for (let x = -6; x <= 6; x++) for (let y = -6; y <= 6; y++) S.addCell(x, y, { h: 0.30 });
    for (let x = -6; x <= 6; x++) S.cmd.cast(water, x, -2);
    for (let x = 4; x <= 6; x++) for (let y = 3; y <= 6; y++) S.cmd.cast(rock, x, y);
    for (let x = -6; x <= 2; x++) for (let y = -6; y <= 6; y++) {
      const c = S.at(x, y); if (!c || c.kind === 'water' || c.kind === 'rock') continue;
      S.spawn(grass, c, {});
    }
    for (const [x, y] of [[-1, 1], [0, 1], [1, 1], [-2, 2], [3, -4], [3, -3]]) S.spawn(tree, S.at(x, y), {});
    for (const [x, y] of [[2, 3], [2, 4], [-3, -1], [-4, 0]]) S.spawn(shrub, S.at(x, y), {});
    for (const c of S.W.cells.values()) S.classifyAround(c);
    S.W.sun = 2; S.W.essence = 999;
    UI.updateHud(); UI.renderRight(); UI.renderHand();
  });
  await page.waitForTimeout(700);

  /* ── 1. 水陆交界：镜头正对水带侧壁 ── */
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.pitch = 0.20; R.cam.dist = 6.0; R.cam.yaw = 0;   // 正对 -z 方向的水带
    R.cam.target.set(0, 0.10, -2); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-water-wall', { x: 300, y: 280, width: 900, height: 470 });
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.pitch = 0.42; R.cam.dist = 4.6; R.cam.yaw = 0.1;
    R.cam.target.set(0, 0.1, -2); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-water-top', { x: 300, y: 220, width: 900, height: 560 });

  /* ── 2. 相机真正进入地形（9°/3.4 → 视点 0.762 < 岩顶 0.986） ── */
  await page.evaluate(() => {
    const { S, D } = window.__dbg;
    const hill = D.byId.get('terrain.earth.5.matter.hill');
    for (let i = 0; i < 24; i++) S.cmd.cast(hill, 0, 0);
  });
  await page.waitForTimeout(500);
  const pen = await page.evaluate(`(() => {
    const { S, R } = window.__dbg; const THREE = window.THREE;
    ${SYNC}
    R.cam.yaw = 0; R.cam.pitch = 0.16; R.cam.dist = 3.4;
    R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal(); sync();
    const c = S.at(0, 0);
    const cam = R.camera.position;
    const inside = cam.y < R.hOf(c) && Math.abs(cam.x) < 0.5 + c.x && Math.abs(cam.z) < 0.5 + c.y;
    /* 近处几格的顶面高度与相机高度差 */
    const near = [];
    for (const [dx, dz] of [[0,0],[1,0],[0,1],[-1,0],[0,-1]]) {
      const cc = S.at(dx, dz); if (!cc) continue;
      near.push({ cell: [dx, dz], kind: cc.kind, top: +R.hOf(cc).toFixed(3), camAbove: +(cam.y - R.hOf(cc)).toFixed(3) });
    }
    return { camY: +cam.y.toFixed(3), inside, near };
  })()`);
  M.penetrationShots = pen;
  await shot(page, 'w-cam-in-terrain');
  await page.screenshot({ path: path.join(OUT, 'w-cam-in-terrain-crop.png'), clip: { x: 400, y: 200, width: 800, height: 560 } });
  M.shots.push('w-cam-in-terrain-crop');

  /* ── 3. 单格三元素痕迹放大 ── */
  await page.evaluate(() => {
    const { S, D, R } = window.__dbg;
    S.W.traces = [];
    const cast = (id, x, y) => S.cmd.cast(D.byId.get(id), x, y);
    cast('flora.wood.1.matter.grass', 0, 0);
    cast('flora.wood.1.matter.grass', 0, 0);
    cast('terrain.mineral.0.matter.water', 0, 0);
    cast('terrain.mineral.0.matter.rock', 0, 0);
    R.trace.sync();
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.95; R.cam.dist = 3.2;
    R.cam.target.set(0, 0.25, 0); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-trace-stack3');
  await page.screenshot({ path: path.join(OUT, 'w-trace-stack3-crop.png'), clip: { x: 660, y: 300, width: 420, height: 400 } });
  M.shots.push('w-trace-stack3-crop');

  /* 同一格：只有痕迹 / 痕迹+预览 / 痕迹+扩散环 —— 三张同机位对比 */
  await shot(page, 'w-trace-only');
  await page.evaluate(() => {
    const { S, D, R, UI } = window.__dbg;
    const card = D.byId.get('terrain.mineral.0.matter.water');
    S.W.hand.unshift(card); S.W.sel = 0;
    R.marks.set(S.castRange(card, S.at(0, 0)));
    UI.renderHand();
  });
  await shot(page, 'w-trace-plus-preview');
  await page.evaluate(() => { const { R } = window.__dbg; R.marks.pulse(0, 0, '#7fd8f0', 0.08); });
  await page.waitForTimeout(160);
  await shot(page, 'w-trace-plus-pulse');
  await page.evaluate(() => { const { R } = window.__dbg; R.marks.clear(); R.marks.cursor(null); });
  await page.waitForTimeout(900);

  /* ── 4. 摆件模式：岛屿像素包围盒（用画面自己算，不靠投影） ── */
  await page.evaluate(() => {
    const { R, UI } = window.__dbg;
    if (UI.state.uiMode === '全隐') UI.toggleAmbient();
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.52; R.cam.dist = 8.6;
    R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-amb2-before');
  await page.evaluate(() => window.__dbg.UI.toggleAmbient());
  await page.waitForTimeout(700);
  await shot(page, 'w-amb2-after');

  /* ── 5. 格线可见度：F4 正上方扫一条横线（跨 6 格草地顶面） ── */
  await page.evaluate(() => {
    const { R, UI } = window.__dbg;
    if (UI.state.uiMode === '全隐') UI.toggleAmbient();
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 1.45; R.cam.dist = 5.2;
    R.cam.target.set(-3, 0.3, 2); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-grid-F4');
  M.grid = await page.evaluate(`(() => {
    const { R, S } = window.__dbg; const THREE = window.THREE;
    ${SYNC}
    sync();
    const r = R.canvas.getBoundingClientRect();
    const proj = (x,y,z) => { const v = new THREE.Vector3(x,y,z).project(R.camera);
      return { x: Math.round((v.x*.5+.5)*r.width + r.left), y: Math.round((-v.y*.5+.5)*r.height + r.top) }; };
    const rows = [];
    for (let x = -6; x <= -1; x++) {
      const c = S.at(x, 2); if (!c) continue;
      const a = proj(c.x - 0.5, R.hOf(c), c.y - 0.5), b = proj(c.x + 0.5, R.hOf(c), c.y - 0.5);
      rows.push({ x, top: +R.hOf(c).toFixed(3), a, b, wPx: Math.round(Math.hypot(b.x-a.x, b.y-a.y)) });
    }
    return rows;
  })()`);

  /* ── 6. 水面横扫：跨 8 个水格找接缝 ── */
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 1.30; R.cam.dist = 5.0;
    R.cam.target.set(-1, 0.1, -2); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-water-scan');
  M.waterPts = await page.evaluate(`(() => {
    const { R } = window.__dbg; const THREE = window.THREE;
    ${SYNC}
    sync();
    const r = R.canvas.getBoundingClientRect();
    const proj = (x,y,z) => { const v = new THREE.Vector3(x,y,z).project(R.camera);
      return { x: Math.round((v.x*.5+.5)*r.width + r.left), y: Math.round((-v.y*.5+.5)*r.height + r.top) }; };
    return { left: proj(-6.5, 0.116, -2), right: proj(6.5, 0.116, -2) };
  })()`);

  fs.writeFileSync(path.join(OUT, 'metrics2.json'), JSON.stringify(M, null, 1));
  console.log('failures', M.failures.length, M.failures.slice(0, 3));
  await browser.close();
})();
