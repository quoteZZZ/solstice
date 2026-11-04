/* 探针 8：同一格三元素痕迹（俯视近景，唯一目的：证据图） */
const path = require('path');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'world-shots');
(async () => {
  const b = await chromium.launch({ channel: 'msedge', headless: true });
  const p = await b.newPage({ viewport: { width: 1680, height: 1000 }, deviceScaleFactor: 1 });
  await p.goto(PAGE); await p.waitForTimeout(1400);
  await p.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    UI.enterFromCreator();
    for (let x = -3; x <= 3; x++) for (let y = -3; y <= 3; y++) S.addCell(x, y, { h: 0.30 });
    const g = id => D.byId.get(id);
    S.addTrace(g('flora.wood.1.matter.grass'), S.at(0, 0));
    S.addTrace(g('flora.wood.1.matter.grass'), S.at(0, 0));
    S.addTrace(g('terrain.mineral.0.matter.water'), S.at(0, 0));
    S.addTrace(g('terrain.mineral.0.matter.rock'), S.at(0, 0));
    S.W.sun = 2; S.W.essence = 999;
    UI.updateHud(); UI.renderRight(); UI.renderHand();
  });
  await p.waitForTimeout(600);
  await p.evaluate(() => {
    const { R } = window.__dbg; R.trace.sync();
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 1.45; R.cam.dist = 2.4;
    R.cam.target.set(0, 0.25, 0); R.cam.place(); R.cam.snapGoal();
  });
  await p.waitForTimeout(350);
  await p.screenshot({ path: path.join(OUT, 'w-stack3-clean.png'), clip: { x: 620, y: 300, width: 440, height: 440 } });
  console.log('ok');
  await b.close();
})();
