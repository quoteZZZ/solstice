/* Solstice · 世界画面审计探针 5：三种标记同屏（干净样本） */
const path = require('path'), fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'world-shots');
const M = { shots: [], failures: [] };
const shot = async (page, name, clip) => {
  await page.waitForTimeout(200);
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
    for (let x = -6; x <= 6; x++) for (let y = -6; y <= 6; y++) S.addCell(x, y, { h: 0.30 });
    const grass = D.byId.get('flora.wood.1.matter.grass');
    const water = D.byId.get('terrain.mineral.0.matter.water');
    const rock = D.byId.get('terrain.mineral.0.matter.rock');
    for (const [x, y] of [[-2, 2], [-2, 3], [-1, 2], [0, 2], [1, 2], [2, 2], [2, 3]]) S.spawn(grass, S.at(x, y), {});
    /* 目标格 (0,0)：三个元素的痕迹，不施卡（避免改地形） */
    S.addTrace(grass, S.at(0, 0));
    S.addTrace(grass, S.at(0, 0));
    S.addTrace(water, S.at(0, 0));
    S.addTrace(rock, S.at(0, 0));
    S.W.sun = 2; S.W.essence = 999;
    UI.updateHud(); UI.renderRight(); UI.renderHand();
  });
  await page.waitForTimeout(600);

  const setup = dist => page.evaluate(d => {
    const { R, S, D, UI } = window.__dbg;
    R.trace.sync();
    const card = D.byId.get('terrain.earth.0.matter.land');
    S.W.hand.unshift(card); S.W.sel = 0;
    R.marks.set([{ x: 0, y: 0, h: 0.3 }, { x: 1, y: 0, h: 0.3 }, { x: 0, y: 1, h: 0.3 }]);
    R.marks.cursor({ x: 2, y: 0, h: 0.3 });
    UI.renderHand();
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.95; R.cam.dist = d;
    R.cam.target.set(1, 0.25, 0.5); R.cam.place(); R.cam.snapGoal();
  }, dist);

  /* 只有痕迹 + 预览（静态） */
  await page.evaluate(() => { const { S, D, R } = window.__dbg; R.trace.sync(); });
  await setup(3.4);
  await shot(page, 'w-rings-three-preview');
  await page.screenshot({ path: path.join(OUT, 'w-rings-three-preview-crop.png'), clip: { x: 600, y: 240, width: 560, height: 460 } });
  M.shots.push('w-rings-three-preview-crop');

  /* 三者同屏：再点一次扩散环 */
  await setup(3.4);
  await page.evaluate(() => { const { R } = window.__dbg; R.marks.pulse(0, 0, '#5e8f47', 0.3); });
  await page.waitForTimeout(150);
  await shot(page, 'w-rings-trio');
  await page.screenshot({ path: path.join(OUT, 'w-rings-trio-crop.png'), clip: { x: 600, y: 240, width: 560, height: 460 } });
  M.shots.push('w-rings-trio-crop');

  /* 默认游玩机位：三者都出现，还看得见吗 */
  await page.waitForTimeout(900);
  await setup(8.6);
  await page.evaluate(() => { const { R } = window.__dbg; R.marks.pulse(0, 0, '#5e8f47', 0.3); });
  await page.waitForTimeout(150);
  await shot(page, 'w-rings-default-zoom');
  await page.screenshot({ path: path.join(OUT, 'w-rings-default-crop.png'), clip: { x: 700, y: 330, width: 500, height: 380 } });
  M.shots.push('w-rings-default-crop');

  /* F4 立牌压平特写 */
  await page.waitForTimeout(900);
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.marks.clear(); R.marks.cursor(null);
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 1.45; R.cam.dist = 4.2;
    R.cam.target.set(-1, 0.35, 2.5); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-f4-standee-flat');
  await page.screenshot({ path: path.join(OUT, 'w-f4-standee-flat-crop.png'), clip: { x: 560, y: 240, width: 620, height: 480 } });
  M.shots.push('w-f4-standee-flat-crop');

  /* 立牌根部黑饼特写（树 + 草同格） */
  await page.evaluate(() => {
    const { S, D, R } = window.__dbg;
    S.spawn(D.byId.get('flora.wood.2.matter.tree'), S.at(-2, 2), {});
    R.cam.yaw = 0.2; R.cam.pitch = 0.42; R.cam.dist = 3.0;
    R.cam.target.set(-2, 0.4, 2); R.cam.place(); R.cam.snapGoal();
  });
  await page.waitForTimeout(400);
  await shot(page, 'w-base-black');
  await page.screenshot({ path: path.join(OUT, 'w-base-black-crop.png'), clip: { x: 620, y: 260, width: 520, height: 440 } });
  M.shots.push('w-base-black-crop');

  fs.writeFileSync(path.join(OUT, 'metrics5.json'), JSON.stringify(M, null, 1));
  console.log('failures', M.failures.length, M.failures.slice(0, 3));
  await browser.close();
})();
