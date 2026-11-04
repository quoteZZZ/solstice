/* Solstice · 世界画面审计探针 4：痕迹环 / 预览 / 扩散环的「同色地形」对比度
   不施卡，只调 S.addTrace —— 避免施卡本身改变地形（水卡会把格子变成水），
   这样测到的只是「痕迹层 vs 地形」的对比。 */
const path = require('path'), fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'world-shots');
const M = { cases: [], shots: [], failures: [] };
const shot = async (page, name, clip) => {
  await page.waitForTimeout(200);
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
    for (let x = -6; x <= 6; x++) for (let y = -6; y <= 6; y++) S.addCell(x, y, { h: 0.30 });
    for (let x = -6; x <= 6; x++) S.cmd.cast(water, x, -2);
    for (let x = 4; x <= 6; x++) for (let y = 3; y <= 6; y++) S.cmd.cast(rock, x, y);
    for (let x = -6; x <= 2; x++) for (let y = -6; y <= 6; y++) {
      const c = S.at(x, y); if (!c || c.kind === 'water' || c.kind === 'rock') continue;
      S.spawn(grass, c, {});
    }
    for (const c of S.W.cells.values()) S.classifyAround(c);
    S.W.sun = 2; S.W.essence = 999;
    UI.updateHud(); UI.renderRight(); UI.renderHand();
  });
  await page.waitForTimeout(700);

  /* 四个对照点：裸地(3,0) / 草地(-4,2) / 水格(0,-2) / 岩格(5,4)
     每个点：先拍基线，再 addTrace(对应元素) 拍一次。 */
  const CASES = [
    { id: 'wood-on-land', cell: [3, 0], card: 'flora.wood.1.matter.grass', el: '木' },
    { id: 'wood-on-grass', cell: [-4, 2], card: 'flora.wood.1.matter.grass', el: '木' },
    { id: 'water-on-water', cell: [0, -2], card: 'terrain.mineral.0.matter.water', el: '水' },
    { id: 'earth-on-rock', cell: [5, 4], card: 'terrain.mineral.0.matter.rock', el: '土' }
  ];
  for (const c of CASES) {
    await page.evaluate(({ cell }) => {
      const { S, R } = window.__dbg;
      S.W.traces = []; R.trace.sync();
      R.cam.yaw = Math.PI / 4; R.cam.pitch = 1.45; R.cam.dist = 2.4;
      R.cam.target.set(cell[0], 0.25, cell[1]); R.cam.place(); R.cam.snapGoal();
    }, c);
    await shot(page, 'w-c-' + c.id + '-base');
    const geo = await page.evaluate(`(() => {
      const { R, S } = window.__dbg; const THREE = window.THREE;
      ${SYNC}
      sync();
      const r = R.canvas.getBoundingClientRect();
      const proj = (x,y,z) => { const v = new THREE.Vector3(x,y,z).project(R.camera);
        return { x: +((v.x*.5+.5)*r.width + r.left).toFixed(1), y: +((-v.y*.5+.5)*r.height + r.top).toFixed(1) }; };
      return { c: proj(${c.cell[0]}, 0.3, ${c.cell[1]}),
        edge: proj(${c.cell[0]}-0.5, 0.3, ${c.cell[1]}-0.5),
        kind: S.at(${c.cell[0]}, ${c.cell[1]}).kind, top: +R.hOf(S.at(${c.cell[0]}, ${c.cell[1]})).toFixed(3) };
    })()`);
    await page.evaluate(({ card, cell }) => {
      const { S, D, R } = window.__dbg;
      S.addTrace(D.byId.get(card), S.at(cell[0], cell[1]));
      R.trace.sync();
    }, c);
    const ring = await page.evaluate(() => {
      const { S } = window.__dbg;
      const t = S.W.traces[0];
      return t ? { r: +(0.14 + Math.min(4, t.count) * 0.018 + t.tier * 0.006).toFixed(3), tier: t.tier, el: t.el } : null;
    });
    await shot(page, 'w-c-' + c.id + '-trace');
    /* 预览 / 扩散环叠在同一格 */
    await page.evaluate(({ cell }) => {
      const { S, D, R, UI } = window.__dbg;
      const card = D.byId.get('terrain.earth.0.matter.land');
      S.W.hand.unshift(card); S.W.sel = 0;
      R.marks.set([{ x: cell[0], y: cell[1], fresh: true, h: 0.3 }]);
      R.marks.cursor({ x: cell[0], y: cell[1], h: 0.3 });
      UI.renderHand();
    }, c);
    await shot(page, 'w-c-' + c.id + '-preview');
    await page.evaluate(({ cell }) => {
      const { R } = window.__dbg;
      R.marks.pulse(cell[0], cell[1], '#ffffff', 0.3);
    }, c);
    await page.waitForTimeout(200);
    await shot(page, 'w-c-' + c.id + '-pulse');
    await page.waitForTimeout(900);
    await page.evaluate(() => { const { R } = window.__dbg; R.marks.clear(); R.marks.cursor(null); });
    M.cases.push({ id: c.id, geo, ring, pxPerCell: Math.round(Math.hypot(geo.edge.x - geo.c.x, geo.edge.y - geo.c.y) * 2) });
  }

  fs.writeFileSync(path.join(OUT, 'metrics4.json'), JSON.stringify(M, null, 1));
  console.log('failures', M.failures.length, M.failures.slice(0, 3));
  await browser.close();
})();
