const { chromium } = require('playwright');

(async () => {
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 1600, height: 1000 } });
  const logs = [];
  p.on('console', m => { if (m.type() === 'error') logs.push('[console] ' + m.text()); });
  p.on('pageerror', e => logs.push('[PAGEERROR] ' + e.message));

  await p.goto('file:///e:/solstice/demo/index.html');
  await p.waitForTimeout(2200);

  // 造世界 + 验证术/律
  const info = await p.evaluate(() => {
    const d = window.__dbg;
    d.W.paused = true;
    d.playCard('土地', 0, 0);
    d.playCard('太阳', 0, 0);
    d.playCard('水', 0, -1);
    d.playCard('草', 1, 0);
    d.playCard('猴', 0, 1);
    d.playCard('岩石', -1, 0);
    d.playCard('树', -1, 1);

    const before = d.W.cells.get('0,0').W;
    const okRain = d.playCard('雨', 0, 0);            // 术
    const after = d.W.cells.get('0,0').W;

    const okGrass = d.playCard('肥土', 1, 0);          // 术：立刻长草
    const grassN = d.W.cells.get('1,0').ents.filter(e => e.name === '草').length;

    const okLaw = d.playCard('常春', 0, 0);            // 律
    const lifeX = d.W.law.lifeX;

    for (let i = 0; i < 20; i++) d.tick();
    return {
      cells: d.W.cells.size,
      standees: d.standeeGroup.children.length,
      rain: { okRain, before: +before.toFixed(2), after: +after.toFixed(2) },
      fert: { okGrass, grassN },
      law: { okLaw, lifeX, evapX: d.W.law.evapX, lightOff: d.W.law.lightOff },
      sun: d.W.sun,
    };
  });
  logs.push('WORLD ' + JSON.stringify(info));
  await p.waitForTimeout(700);
  await p.screenshot({ path: 'e:/solstice/demo/shot-world.png' });

  // 三类 × 多元素并排
  await p.evaluate(() => {
    const d = window.__dbg;
    d.W.hand = ['雨', '火', '常春', '长夜', '太阳', '水', '草', '树'];
    d.W.sel = -1;
    window.__render();
  });
  await p.waitForTimeout(600);
  const hand = await p.$('footer');
  if (hand) await hand.screenshot({ path: 'e:/solstice/demo/shot-hand.png' });

  const m = await p.evaluate(() => {
    const rows = [];
    document.querySelectorAll('.slot > .card').forEach(card => {
      const name = card.querySelector('h3')?.textContent || '?';
      const box = card.querySelector('.box');
      const note = card.querySelector('.note1');
      const bb = box.getBoundingClientRect(), nb = note.getBoundingClientRect();
      rows.push({
        name,
        ringBars: card.querySelectorAll('.ringbar i.on').length,
        hasSeal: !!card.querySelector('.clseal svg'),
        elems: card.querySelectorAll('.elem').length,
        noteClipped: Math.round(nb.bottom - (bb.bottom - 10)),
        ruleSize: getComputedStyle(card.querySelector('.rule1')).fontSize,
      });
    });
    return rows;
  });
  logs.push('CARDS ' + JSON.stringify(m, null, 1));

  console.log(logs.join('\n') || '(clean)');
  await b.close();
})();
