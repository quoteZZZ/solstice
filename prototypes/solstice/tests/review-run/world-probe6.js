/* Solstice · 探针 6：按投影坐标精确裁切「痕迹 + 预览 + 扩散环」同屏证据 */
const path = require('path'), fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'world-shots');
const M = { shots: [], failures: [] };
const shot = async (page, name, clip) => {
  await page.waitForTimeout(120);
  await page.screenshot({ path: path.join(OUT, name + '.png'), clip });
  M.shots.push(name); console.log('shot', name, clip ? JSON.stringify(clip) : '');
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
    for (let x = -4; x <= 4; x++) for (let y = -4; y <= 4; y++) S.addCell(x, y, { h: 0.30 });
    const grass = D.byId.get('flora.wood.1.matter.grass');
    const water = D.byId.get('terrain.mineral.0.matter.water');
    const rock = D.byId.get('terrain.mineral.0.matter.rock');
    for (const [x, y] of [[-2, 1], [-2, 2], [1, 2], [2, 2], [2, 1]]) S.spawn(grass, S.at(x, y), {});
    S.addTrace(grass, S.at(0, 0)); S.addTrace(grass, S.at(0, 0));
    S.addTrace(water, S.at(0, 0)); S.addTrace(rock, S.at(0, 0));
    S.W.sun = 2; S.W.essence = 999;
    UI.updateHud(); UI.renderRight(); UI.renderHand();
  });
  await page.waitForTimeout(600);

  const setup = () => page.evaluate(() => {
    const { S, D, R, UI } = window.__dbg;
    R.trace.sync();
    const card = D.byId.get('terrain.earth.0.matter.land');
    S.W.hand.unshift(card); S.W.sel = 0;
    R.marks.set([{ x: 1, y: 0, h: 0.3 }, { x: 1, y: -1, h: 0.3 }]);
    R.marks.cursor({ x: -1, y: 0, h: 0.3 });
    UI.renderHand();
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.85; R.cam.dist = 5.0;
    R.cam.target.set(0, 0.25, 0); R.cam.place(); R.cam.snapGoal();
  });
  const rect = () => page.evaluate(`(() => {
    const { R } = window.__dbg; const THREE = window.THREE;
    ${SYNC} sync();
    const r = R.canvas.getBoundingClientRect();
    const proj = (x,y,z) => { const v = new THREE.Vector3(x,y,z).project(R.camera);
      return { x: (v.x*.5+.5)*r.width + r.left, y: (-v.y*.5+.5)*r.height + r.top }; };
    const pts = [[-1.6,-1.6],[1.6,-1.6],[1.6,1.6],[-1.6,1.6]].map(([dx,dz]) => proj(dx, 0.3, dz));
    const x0 = Math.max(0, Math.min(...pts.map(p=>p.x))), x1 = Math.min(r.width, Math.max(...pts.map(p=>p.x)));
    const y0 = Math.max(56, Math.min(...pts.map(p=>p.y))), y1 = Math.min(r.top + r.height, Math.max(...pts.map(p=>p.y)));
    return { x: Math.round(x0), y: Math.round(y0), width: Math.round(x1-x0), height: Math.round(y1-y0) };
  })()`);

  await setup();
  await shot(page, 'w-ring-A-trace-preview');
  const rc = await rect();
  await page.screenshot({ path: path.join(OUT, 'w-ring-A-crop.png'), clip: rc });
  M.shots.push('w-ring-A-crop');

  await setup();
  await page.evaluate(() => { const { R } = window.__dbg; R.marks.pulse(0, 0, '#5e8f47', 0.3); });
  await page.waitForTimeout(140);
  await page.screenshot({ path: path.join(OUT, 'w-ring-B-trio.png'), clip: rc });
  M.shots.push('w-ring-B-trio');
  /* 同一瞬的脉冲透明度读数 */
  M.pulse = await page.evaluate(() => {
    const { R } = window.__dbg;
    const ps = [];
    R.scene.traverse(o => { if (o.isMesh && o.visible && o.geometry.type === 'RingGeometry' && o.renderOrder === 5)
      ps.push({ op: +o.material.opacity.toFixed(3), scale: +o.scale.x.toFixed(2), r: +o.geometry.parameters.outerRadius.toFixed(2) }); });
    return ps;
  });
  console.log('pulse', JSON.stringify(M.pulse), 'rect', JSON.stringify(rc));

  fs.writeFileSync(path.join(OUT, 'metrics6.json'), JSON.stringify(M, null, 1));
  console.log('failures', M.failures.length);
  await browser.close();
})();
