const { chromium } = require('playwright');

(async () => {
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 1600, height: 1000 } });
  p.on('pageerror', e => console.log('[ERR] ' + e.message));
  await p.goto('file:///e:/solstice/demo/index.html');
  await p.waitForTimeout(2000);

  await p.evaluate(() => {
    const d = window.__dbg;
    d.W.paused = true;
    d.playCard('土地', 0, 0);
    d.playCard('太阳', 0, 0);
    d.playCard('水', 0, -1);
    d.playCard('草', 1, 0);
    d.playCard('猴', 0, 1);
    for (let i = 0; i < 20; i++) d.tick();
    window.__render();
  });
  await p.waitForTimeout(900);

  const r = await p.evaluate(() => {
    const H = innerHeight, W = innerWidth;
    const g = s => {
      const e = document.querySelector(s);
      if (!e) return null;
      const b = e.getBoundingClientRect();
      return { top: Math.round(b.top), h: Math.round(b.height), w: Math.round(b.width), pctH: +(b.height / H * 100).toFixed(1) };
    };

    // 世界内容在屏幕上的包围盒
    const d = window.__dbg;
    let minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    d.W.cells.forEach(c => {
      const s = d.screenOf(c.x, c.y);
      minX = Math.min(minX, s.x); maxX = Math.max(maxX, s.x);
      minY = Math.min(minY, s.y); maxY = Math.max(maxY, s.y);
    });

    const card = document.querySelector('.slot > .card').getBoundingClientRect();
    const slot = document.querySelector('.slot').getBoundingClientRect();
    const cv = document.querySelector('#cv').getBoundingClientRect();

    return {
      viewport: { W, H },
      header: g('header'),
      main: g('main'),
      footer: g('footer'),
      handInner: g('#hand'),
      card: { w: Math.round(card.width), h: Math.round(card.height), pctH: +(card.height / H * 100).toFixed(1) },
      slotCount: document.querySelectorAll('.slot').length,
      handScrollW: document.querySelector('#hand').scrollWidth,
      canvas: { w: Math.round(cv.width), h: Math.round(cv.height) },
      world: {
        w: Math.round(maxX - minX), h: Math.round(maxY - minY),
        pctOfCanvasW: +((maxX - minX) / cv.width * 100).toFixed(1),
        pctOfCanvasH: +((maxY - minY) / cv.height * 100).toFixed(1),
        centerX: Math.round((minX + maxX) / 2),
        centerY: Math.round((minY + maxY) / 2),
        canvasCenterX: Math.round(cv.left + cv.width / 2),
        canvasCenterY: Math.round(cv.top + cv.height / 2),
      },
    };
  });

  console.log(JSON.stringify(r, null, 1));
  await p.screenshot({ path: 'e:/solstice/demo/shot-full.png' });
  await b.close();
})();
