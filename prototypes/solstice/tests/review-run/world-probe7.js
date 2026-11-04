/* Solstice · 探针 7：岛缘低角 / 阴影空跑成本 / 轴对齐方位 */
const path = require('path'), fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'world-shots');
const M = { shots: [], failures: [] };
const shot = async (page, name, clip) => {
  await page.waitForTimeout(240);
  await page.screenshot({ path: path.join(OUT, name + '.png'), clip });
  M.shots.push(name); console.log('shot', name);
};

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
    for (let x = -5; x <= 5; x++) for (let y = -5; y <= 5; y++) S.addCell(x, y, { h: 0.30 });
    for (let x = -5; x <= 5; x++) S.cmd.cast(g('terrain.mineral.0.matter.water'), x, 0);
    for (let x = -5; x <= 2; x++) for (let y = -5; y <= 5; y++) {
      const c = S.at(x, y); if (!c || c.kind === 'water') continue;
      S.spawn(g('flora.wood.1.matter.grass'), c, {});
    }
    for (const [x, y] of [[-1, 2], [0, 2], [1, 2], [-2, -2]]) S.spawn(g('flora.wood.2.matter.tree'), S.at(x, y), {});
    for (const c of S.W.cells.values()) S.classifyAround(c);
    S.W.sun = 2; S.W.essence = 999;
    UI.updateHud(); UI.renderRight(); UI.renderHand();
  });
  await page.waitForTimeout(700);

  /* 岛缘：镜头在岛外，看侧壁与虚空水面的接法 */
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.yaw = 0; R.cam.pitch = 0.18; R.cam.dist = 9.5;
    R.cam.target.set(4.5, 0.22, 0); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-island-rim');
  await page.screenshot({ path: path.join(OUT, 'w-island-rim-crop.png'), clip: { x: 500, y: 300, width: 800, height: 470 } });
  M.shots.push('w-island-rim-crop');

  /* 轴对齐方位（yaw 0）F1：最废的一档 */
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.yaw = 0; R.cam.pitch = 0.26; R.cam.dist = 8.6;
    R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-yaw0-F1');

  /* 水面默认机位 */
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.52; R.cam.dist = 8.6;
    R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-water-default');
  await page.screenshot({ path: path.join(OUT, 'w-water-default-crop.png'), clip: { x: 620, y: 330, width: 520, height: 330 } });
  M.shots.push('w-water-default-crop');

  /* 空阴影通道的成本：开/关各测 20 帧 CPU */
  const cpu = () => page.evaluate(() => {
    const { R } = window.__dbg;
    const t0 = performance.now();
    for (let i = 0; i < 20; i++) R.frame(t0 + i * 16, 0);
    return +((performance.now() - t0) / 20).toFixed(2);
  });
  M.shadow = { on: await cpu() };
  await page.evaluate(() => window.__dbg.R.setShadows(false));
  M.shadow.off = await cpu();
  await page.evaluate(() => window.__dbg.R.setShadows(true));

  /* 岛底与虚空水面：几何落差 */
  M.rim = await page.evaluate(() => {
    const { S, R } = window.__dbg;
    const minTop = Math.min(...[...S.W.cells.values()].map(c => R.hOf(c)));
    let plane = null;
    R.scene.traverse(o => {
      if (o.isMesh && o.geometry && o.geometry.type === 'PlaneGeometry' && o.geometry.parameters.width > 200)
        plane = { y: +o.position.y.toFixed(2), size: o.geometry.parameters.width };
    });
    return { minCellTop: +minTop.toFixed(3), voidPlane: plane, gap: plane ? +(minTop - plane.y).toFixed(3) : null };
  });

  fs.writeFileSync(path.join(OUT, 'metrics7.json'), JSON.stringify(M, null, 1));
  console.log(JSON.stringify(M, null, 1));
  await browser.close();
})();
