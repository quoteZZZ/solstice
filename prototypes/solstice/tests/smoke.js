/* 至日 · 合并原型自检
   跑法（Node 18+）：node tests/smoke.js
   用本机 Edge（playwright-core 不下载浏览器）：file:// 直接开，检查控制台报错、施卡链路、
   生态 tick、面板开关与卡表校验。 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', 'index.html')).href;

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1680, height: 1000 } });
  const errors = [];
  page.on('console', m => { if (m.type() === 'error') errors.push('[console] ' + m.text()); });
  page.on('pageerror', e => errors.push('[PAGEERROR] ' + e.message));

  await page.goto(PAGE);
  await page.waitForTimeout(1800);

  /* 1. 卡表装配与校验 */
  const table = await page.evaluate(() => {
    const D = window.SOL.data;
    const cls = { matter: 0, rite: 0, law: 0 };
    D.cards.forEach(c => cls[c.class]++);
    return {
      total: D.total, cls,
      warn: D.warn,
      ids: new Set(D.cards.map(c => c.id)).size,
      numbered: D.cards.every(c => /^№ \d{4} \/ \d{4}$/.test(c.number)),
      reclaimOk: D.cards.every(c => c.tier === 0 ? c.reclaim === 0 : c.reclaim === Math.round(c.value * 0.15))
    };
  });

  /* 2. 进界 + 造世界 */
  const world = await page.evaluate(() => {
    const { UI, S, W, D } = window.__dbg;
    UI.enterWorld();
    S.setGear(3);                     // 2×
    const land = D.cards.find(c => c.sim.op === 'land' && c.tier === 0);
    const sun = D.byId.get('sky.light.0.matter.sun');
    const water = D.byId.get('terrain.mineral.0.matter.water');
    const grass = D.byId.get('flora.wood.1.matter.grass');
    const rabbit = D.byId.get('fauna.earth.1.matter.rabbit');
    const wolf = D.byId.get('fauna.dark.2.matter.wolf');
    const tree = D.byId.get('flora.wood.2.matter.tree');

    const r1 = S.cast(land, 0, 0);
    const r2 = S.cast(sun, 0, 0);
    const r3 = S.cast(water, 0, -1);
    const r4 = S.cast(grass, 1, 0);
    const r5 = S.cast(tree, -1, 0);
    const gate = S.cast(rabbit, 1, 1);            // 有草 → 应成功
    const gated = S.cast(wolf, 2, 2);              // 无猎物也能落（食肉只判生命层）
    const rain = S.cast(D.byId.get('rite.water.2.rite.rain'), 0, 0);
    const beast = S.cast(D.byId.get('rite.light.8.rite.awaken'), 1, 1);
    return {
      cells: W.cells.size, sun: W.sun,
      results: [r1.ok, r2.ok, r3.ok, r4.ok, r5.ok, gate.ok, gated.ok, rain.ok, beast.ok],
      beastMsg: beast.msg,
      W00: +W.cells.get('0,0').W.toFixed(2),
      ents: W.cells.get('1,0').ents.map(e => e.name),
      traces: W.traces.length
    };
  });

  await page.waitForTimeout(400);
  await page.screenshot({ path: path.join(__dirname, 'shot-world.png') });

  /* 3. 跑 24 个月 + 确定性摘要 */
  const sim = await page.evaluate(() => {
    const { S, W } = window.__dbg;
    for (let i = 0; i < 24; i++) S.tick();
    const d1 = S.digest();
    for (let i = 0; i < 6; i++) S.tick();
    return { tick: d1, month: W.month, ess: W.essence, life: S.lifeStats() };
  });

  /* 4. 面板全开一遍 */
  const panels = await page.evaluate(() => {
    const { UI } = window.__dbg;
    const out = {};
    for (const p of ['pack', 'codex', 'chron', 'settings', 'keys', 'help']) {
      UI.openPanel(p);
      out[p] = document.querySelector('#panel #pbody').children.length > 0;
    }
    UI.closePanel();
    UI.openCard('fauna.water.1.matter.nightmarsh_bird');
    out.codexCard = !!document.querySelector('#panel .card');
    UI.closePanel();
    /* 卡库第二面：图鉴墙（6 条判定） */
    UI.openPanel('codex');
    UI.state.codexTab = 'ach';
    UI.renderOverlay();
    out.dexCells = document.querySelectorAll('#panel .axc').length;
    out.dexHead = document.getElementById('phead').textContent.indexOf('图鉴') >= 0;
    UI.state.codexTab = 'cards';
    UI.closePanel();
    UI.fireEvent('灾变');
    out.event = document.getElementById('eventwrap').classList.contains('on');
    UI.closeEvent();
    return out;
  });

  await page.waitForTimeout(300);
  await page.screenshot({ path: path.join(__dirname, 'shot-panel.png'), fullPage: false });

  /* 5. 手牌 + 卡面尺寸 */
  const cards = await page.evaluate(() => {
    const { W, UI } = window.__dbg;
    W.hand = ['terrain.earth.0.matter.land', 'flora.wood.1.matter.grass', 'rite.water.2.rite.rain']
      .map(id => window.SOL.data.byId.get(id));
    W.sel = 0;
    UI.renderHand();
    const el = document.querySelector('.handslot .card');
    const r = el.getBoundingClientRect();
    return {
      slots: document.querySelectorAll('.handslot').length,
      cardBox: [Math.round(r.width), Math.round(r.height)],
      ringOn: el.querySelectorAll('.ringbar i.on').length,
      hasNumber: /№/.test(el.querySelector('.foot').textContent),
      fxLen: el.querySelector('.fx').textContent.length
    };
  });
  await page.evaluate(() => document.getElementById('handwrap').scrollIntoView());
  const hand = await page.$('#handwrap');
  if (hand) await hand.screenshot({ path: path.join(__dirname, 'shot-hand.png') });

  const report = { table, world, sim, panels, cards, errors };
  fs.writeFileSync(path.join(__dirname, 'report.json'), JSON.stringify(report, null, 2));
  console.log(JSON.stringify(report, null, 1));

  await browser.close();
  if (errors.length) process.exitCode = 1;
})();
