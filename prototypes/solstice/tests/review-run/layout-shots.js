/* 当前树（2d347da）八视口世界截图 + 遮挡读数 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const VIEWS = [
  ['1080', 1080, 700], ['1366', 1366, 860], ['1281', 1281, 860], ['1280', 1280, 860],
  ['1440', 1440, 900], ['1680x600', 1680, 600], ['900x1200', 900, 1200], ['2560', 2560, 1440]
];

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const out = {};
  for (const [label, w, h] of VIEWS) {
    const page = await browser.newPage({ viewport: { width: w, height: h }, deviceScaleFactor: 1 });
    await page.goto(pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href);
    await page.waitForTimeout(1400);
    await page.evaluate(() => {
      const { S, D, UI } = window.__dbg;
      UI.enterFromCreator();
      const land = D.byId.get('terrain.earth.0.matter.land');
      for (let x = 4; x <= 12; x++) for (let y = 4; y <= 12; y++) S.cmd.cast(land, x, y);
      S.W.essence = 0;
      UI.updateHud(); UI.renderRight(); UI.renderHand();
    });
    await page.waitForTimeout(500);
    out[label] = await page.evaluate(() => {
      const rr = el => { const r = el.getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height, r: r.right, b: r.bottom }; };
      const rp = document.getElementById('rightpanel').getBoundingClientRect();
      const hw = document.getElementById('handwrap');
      const slots = [...document.querySelectorAll('.handslot')].map(el => {
        const s = rr(el), c = rr(el.querySelector('.card'));
        return { i: +el.dataset.i, x: s.x, w: s.w, r: s.r, cardR: c.r, cardB: c.b, overRP: Math.round(c.r - rp.left) };
      });
      const act = [...document.querySelectorAll('#actbar .ab')].map(a => {
        const b = a.getBoundingClientRect();
        const top = document.elementFromPoint(b.x + b.width / 2, b.y + b.height / 2);
        return { act: a.dataset.act, rect: [Math.round(b.x), Math.round(b.y)], covered: !top || !(top === a || a.contains(top)), top: top ? (top.className || top.tagName) : null };
      });
      const deck = [...document.querySelectorAll('#deck .dk')].map(d => {
        const b = d.getBoundingClientRect();
        const top = document.elementFromPoint(b.x + b.width / 2, b.y + b.height / 2);
        return { text: d.innerText.replace(/\n/g, '/'), rect: [Math.round(b.x), Math.round(b.y)], covered: !top || !(top === d || d.contains(top)) };
      });
      const railLast = [...document.querySelectorAll('#leftrail .rail')].pop();
      const rb = railLast.getBoundingClientRect();
      const rTop = document.elementFromPoint(rb.x + rb.width / 2, rb.y + rb.height / 2);
      const sections = [...document.querySelectorAll('#rightpanel .rp')].map(el => ({ title: (el.querySelector('.h') || {}).textContent, h: Math.round(rr(el).h), sh: el.scrollHeight, ch: el.clientHeight }));
      return {
        handScale: getComputedStyle(document.documentElement).getPropertyValue('--hand-scale').trim(),
        right: getComputedStyle(document.documentElement).getPropertyValue('--right').trim(),
        rail: getComputedStyle(document.documentElement).getPropertyValue('--rail').trim(),
        handwrap: { cw: hw.clientWidth, sw: hw.scrollWidth }, slots, pitch: +(slots[1].x - slots[0].x).toFixed(2),
        act, deck, railLast: { rect: [Math.round(rb.x), Math.round(rb.y), Math.round(rb.width), Math.round(rb.height)], covered: !rTop || !(rTop === railLast || railLast.contains(rTop)) },
        sections, rp: { w: Math.round(rp.width), h: Math.round(rp.height) }
      };
    });
    await page.screenshot({ path: path.resolve(path.join(__dirname, 'narrow-shots', 'w' + label + '.png')) });
    console.log(label, 'pitch', out[label].pitch, 'slotR', Math.round(out[label].slots[7].r), 'cardR', Math.round(out[label].slots[7].cardR), 'overRP', out[label].slots[7].overRP, 'actCovered', out[label].act.filter(a => a.covered).length, 'deckCovered', out[label].deck.filter(d => d.covered).length, 'railCovered', out[label].railLast.covered);
    await page.close();
  }
  fs.writeFileSync(path.resolve(path.join(__dirname, 'narrow-shots', 'layout-r2.json')), JSON.stringify(out, null, 1));
  await browser.close();
})();
