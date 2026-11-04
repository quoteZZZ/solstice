/* Solstice · 评审用「实际游玩」脚本
   跑法：node tests/review-run.js --out=<目录> [--scenario=full|stress|keyboard|narrow] [--w=1680] [--h=1000]

   这不是回归，是给评审取证：脚本真的按玩家路径走一遍（点、拖、按键、开面板、开包、抽牌、
   触发事件、切摆件模式、窄窗重排），每一步截一张图，并把那一刻的 DOM 读数一起存成 log.json。
   图给人看，log 给「这一块是不是被裁了 / 字号对不对 / 有没有溢出」提供数字。

   几条硬约定：
   · 任何一步抛异常都不中断，记进 log.failures，继续走下一步——评审要的是「还有哪些能跑」；
   · 不 mock 世界：所有格子、卡牌、月份都由 sim 正常推进，只有时素直接给（开包要花 12/24/48，
     等它自然涨要 2 分钟，那是等，不是测）；
   · 每张图都记下 viewport 与 --hand-scale，窄屏与宽屏的结论不能混。 */
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');

const PAGE = pathToFileURL(path.join(__dirname, '..', 'index.html')).href;

const argv = process.argv.slice(2);
/* 同时认 --k v 和 --k=v：Git Bash 会把带盘符的参数重写成 POSIX 路径，
   只有 `=` 形式能完整传进来，别只支持空格分隔。 */
const arg = (k, d) => {
  const eq = argv.find(a => a.startsWith('--' + k + '='));
  if (eq) return eq.slice(k.length + 3);
  const i = argv.indexOf('--' + k);
  return i >= 0 && argv[i + 1] && !argv[i + 1].startsWith('--') ? argv[i + 1] : d;
};
const SCEN = arg('scenario', 'full');
const VW = +arg('w', 1680);
const VH = +arg('h', 1000);
const OUT = path.resolve(arg('out', path.join(__dirname, 'review-run', SCEN + '-' + VW)));

fs.mkdirSync(OUT, { recursive: true });

/* DOM 读数：只量「可能出问题」的东西——宽度是否超出容器、字号、被裁的 scrollHeight。 */
const SNAP = () => {
  const sel = [
    '#top', '#leftrail', '#rightpanel', '.rp.grow', '#handwrap', '.handslot', '.card.h',
    '#phead', '#pbody', '#pfoot', '#panel', '.packframe', '.packseal', '.cgWall .cgt',
    '.axWall .axc', '.setRow', '.keyRow', '.tchip', '.toast', '#cardtip', '#hoverinfo',
    '#eventcard', '#creator', '#cLeft', '#cRight', '#cCenter', '#cutaway'
  ];
  const out = {};
  for (const s of sel) {
    const el = document.querySelector(s);
    if (!el) continue;
    const b = el.getBoundingClientRect();
    const cs = getComputedStyle(el);
    out[s] = {
      x: Math.round(b.x), y: Math.round(b.y), w: Math.round(b.width), h: Math.round(b.height),
      sh: el.scrollHeight, ch: el.clientHeight, sw: el.scrollWidth, cw: el.clientWidth,
      fs: cs.fontSize, ov: cs.overflow, vis: cs.display
    };
  }
  const cs = getComputedStyle(document.documentElement);
  return {
    viewport: [innerWidth, innerHeight],
    docW: document.documentElement.scrollWidth,
    handScale: cs.getPropertyValue('--hand-scale').trim(),
    right: cs.getPropertyValue('--right').trim(),
    rail: cs.getPropertyValue('--rail').trim(),
    handCount: document.querySelectorAll('.handslot').length,
    canCount: document.querySelectorAll('.card.h:not(.cant)').length,
    cells: window.__dbg ? window.__dbg.S.W.cells.size : -1,
    month: window.__dbg ? window.__dbg.S.W.month : -1,
    essence: window.__dbg ? window.__dbg.S.W.essence : -1,
    panel: window.__dbg ? window.__dbg.UI.state.panel : null,
    event: window.__dbg ? window.__dbg.UI.state.event : null,
    box: out
  };
};

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
  const log = { scenario: SCEN, viewport: [VW, VH], shots: [], failures: [], console: [] };
  page.on('pageerror', e => log.console.push('[PAGEERROR] ' + e.message));
  page.on('console', m => { if (m.type() === 'error') log.console.push('[console] ' + m.text()); });

  let n = 0;
  async function shot(name, note) {
    n++;
    const file = String(n).padStart(2, '0') + '-' + name + '.png';
    try {
      await page.waitForTimeout(320);
      const st = await page.evaluate(SNAP);
      await page.screenshot({ path: path.join(OUT, file) });
      log.shots.push({ file, name, note: note || '', state: st });
      console.log('shot', file);
    } catch (e) {
      log.failures.push({ step: name, msg: String(e && e.message) });
      console.log('FAIL', name, e && e.message);
    }
  }
  async function run(name, fn, note) {
    try { await fn(); } catch (e) {
      log.failures.push({ step: name, msg: String(e && e.message) });
      console.log('FAIL(step)', name, e && e.message);
    }
  }

  await page.goto(PAGE);
  await page.waitForTimeout(1600);

  /* 一次拖拽：真的按住手牌、拖到画面中间、松手。合成事件测不出 pointer capture 的问题。 */
  async function dragCardTo(x, y) {
    const r = await page.evaluate(() => {
      const el = document.querySelector('.handslot');
      if (!el) return null;
      const b = el.getBoundingClientRect();
      return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
    });
    if (!r) return null;
    await page.mouse.move(r.x, r.y);
    await page.mouse.down();
    for (let i = 1; i <= 6; i++) {
      await page.mouse.move(r.x + (x - r.x) * i / 6, r.y + (y - r.y) * i / 6);
      await page.waitForTimeout(30);
    }
    return { from: r };
  }
  async function dropAt() { await page.mouse.up(); }

  /* ── 开场 ── */
  await shot('creator', '创世页首屏');
  await run('creator-hover', async () => {
    const el = await page.$('#cutaway');
    if (el) await el.hover();
  });
  await shot('creator-hover', '悬停剖面图');

  await run('reroll', async () => { await page.evaluate(() => window.__dbg.UI.rerollCreator()); });
  await shot('creator-reroll', '重掷一次后的创世页');

  await run('enter', async () => {
    await page.evaluate(() => { window.__dbg.UI.enterFromCreator(); window.__dbg.UI.updateHud(); window.__dbg.UI.renderHand(); });
  });
  await shot('enter-empty', '刚进界：空世界 + 手牌');

  await run('tip', async () => {
    await page.evaluate(() => {
      const el = document.querySelector('.handslot');
      const card = window.__dbg.S.W.hand[0];
      if (el && card) window.__dbg.UI.showTip({ target: el }, card);
    });
  });
  await shot('cardtip', '手牌悬浮详情');

  await run('hide-tip', async () => { await page.evaluate(() => window.__dbg.UI.hideTip()); });

  /* 点一张不能打的卡，看拒绝路径 */
  await run('cast-void', async () => {
    await page.evaluate(() => {
      const { S, D, UI } = window.__dbg;
      const card = S.W.hand.find(c => c.sim.op !== 'land' && c.sim.op !== 'sun' && c.sim.op !== 'law');
      if (card) UI.cast(S.W.hand.indexOf(card), null, null);
    });
  });
  await shot('toast-reject', '在虚空上打一张需要地块的卡');

  /* 造一小片地：直接走 sim，省掉手牌不一定有土地卡的偶然 */
  await run('seed-land', async () => {
    await page.evaluate(() => {
      const { S, D, UI } = window.__dbg;
      const land = D.byId.get('terrain.earth.0.matter.land');
      for (let x = -3; x <= 3; x++) for (let y = -3; y <= 3; y++) S.cmd.cast(land, x, y);
      UI.updateHud(); UI.renderRight(); UI.renderHand();
    });
  });
  await shot('landed', '打下一片地之后');

  /* 点世界施卡：这是主要手势（cam.onClick → UI.cast），和拖放是两条路。
     落点不能随便挑：点到已经长好的陆地上，土地卡会「放不下」而什么都不发生——
     那不是 bug 也不是我们要看的画面。这里先扫出「虚空点」和「已有地块点」各一个。 */
  async function findPoint(wantCell) {
    return page.evaluate((want) => {
      const { R } = window.__dbg;
      const rc = document.getElementById('cv').getBoundingClientRect();
      const hand = document.getElementById('handwrap').getBoundingClientRect();
      for (let gy = 0.18; gy < 0.72; gy += 0.06) {
        for (let gx = 0.22; gx < 0.86; gx += 0.06) {
          const x = rc.left + rc.width * gx, y = rc.top + rc.height * gy;
          if (y > hand.top - 20) continue;
          const hit = !!R.cam.pickAt(x, y);
          if (hit === want) return { x, y };
        }
      }
      return null;
    }, wantCell);
  }

  const voidPt = await findPoint(false);
  await run('click-cast', async () => {
    await page.evaluate(() => {
      const { S, UI } = window.__dbg;
      const i = S.W.hand.findIndex(c => c.sim.op === 'land');
      UI.select(i >= 0 ? i : 0);
    });
    if (!voidPt) throw new Error('no void point');
    await page.mouse.move(voidPt.x, voidPt.y);
    await page.waitForTimeout(160);
    await page.mouse.click(voidPt.x, voidPt.y);
  });
  await shot('click-cast', '在虚空上点一下：土地铺开（主要施卡手势）');

  const cellPt = await findPoint(true);
  await run('click-cast-cell', async () => {
    await page.evaluate(() => {
      const { S, UI } = window.__dbg;
      const i = S.W.hand.findIndex(c => c.sim.op === 'land');
      UI.select(i >= 0 ? i : 0);
    });
    if (!cellPt) throw new Error('no cell point');
    await page.mouse.move(cellPt.x, cellPt.y);
    await page.waitForTimeout(160);
    await page.mouse.click(cellPt.x, cellPt.y);
  });
  await shot('click-cast-onland', '在有地块的格子上点一下：会被怎么拒');

  /* 真实拖拽一次。注意：手牌在屏幕底部，拖到世界的任何一格都是「向上」，
     所以这一步同时是在测「向上拖 = 随机落点」这条分支会不会盖住「拖到指定格」。 */
  await run('drag', async () => { await dragCardTo(VW / 2, VH * 0.42); });
  await shot('drag-over', '拖牌到世界上方：落点预览');

  /* 向下拖 = 回收 */
  await run('drag-recycle', async () => { await dragCardTo(VW / 2, VH - 40); });
  await shot('drag-recycle', '拖牌往下：回收提示');
  await run('drag-cancel', async () => { await page.keyboard.press('Escape'); await page.mouse.up(); });
  await run('drop', async () => { await dropAt(); });
  await shot('dropped', '松手之后');

  /* 悬停一格：读数框 + 覆盖层 */
  await run('hover', async () => { await page.mouse.move(VW * 0.5, VH * 0.4); });
  await shot('hover-cell', '悬停一格：地块读数');

  /* 世界长大 */
  await run('grow', async () => {
    await page.evaluate(() => {
      const { S, D, UI } = window.__dbg;
      const g = id => D.byId.get(id);
      const put = (id, cells) => cells.forEach(([x, y]) => S.cmd.cast(g(id), x, y));
      S.cmd.cast(g('sky.light.0.matter.sun'), 0, 0);
      put('terrain.mineral.0.matter.water', [[0, -1], [1, -1], [2, -1]]);
      put('flora.wood.1.matter.grass', [[1, 0], [2, 0], [3, 0], [-1, 1], [-2, 1], [0, 2]]);
      put('flora.wood.2.matter.tree', [[-1, 0], [-2, 0], [3, 1]]);
      S.W.essence = 999;
      for (let i = 0; i < 18; i++) S.cmd.tick(1);
      UI.updateHud(); UI.renderRight(); UI.renderHand();
    });
  });
  await shot('world-18m', '18 个月后的世界');

  /* 抽卡全流程 */
  await run('pack', async () => { await page.evaluate(() => window.__dbg.UI.openPanel('pack')); });
  await shot('pack-seal', '抽卡面板 · 封印');
  await run('pack-next', async () => {
    const b = await page.$('[data-act="packNext"]');
    if (b) await b.click();
  });
  await shot('pack-deal', '抽卡面板 · 翻牌');
  await run('pack-pick', async () => {
    const c = await page.$('.packpick');
    if (c) await c.click();
  });
  await shot('pack-picked', '选下一张之后');
  await run('pack-close', async () => { await page.evaluate(() => window.__dbg.UI.closePanel()); });

  /* 卡库 / 图鉴 / 年表 / 设置 / 键位 / 帮助 */
  for (const p of ['codex', 'chron', 'settings', 'keys', 'help']) {
    await run('open-' + p, async () => { await page.evaluate(id => window.__dbg.UI.openPanel(id), p); });
    await shot('panel-' + p, '面板：' + p);
    await run('close-' + p, async () => { await page.evaluate(() => window.__dbg.UI.closePanel()); });
  }

  /* 图鉴里点一张卡 → 详情 */
  await run('codex-card', async () => {
    await page.evaluate(() => window.__dbg.UI.openPanel('codex'));
    await page.waitForTimeout(200);
    const el = await page.$('.cgt');
    if (el) await el.click();
  });
  await shot('codex-detail', '图鉴 · 卡详情');
  await run('codex-close', async () => { await page.evaluate(() => window.__dbg.UI.closePanel()); });

  /* 世界事件 */
  await run('event', async () => { await page.evaluate(() => window.__dbg.UI.fireEvent()); });
  await shot('event', '世界事件弹窗');
  await run('event-close', async () => { await page.evaluate(() => window.__dbg.UI.closeEvent()); });

  /* 摆件模式 */
  await run('ambient', async () => { await page.evaluate(() => window.__dbg.UI.toggleAmbient()); });
  await shot('ambient', '摆件模式（UI 全隐）');
  await run('ambient-off', async () => { await page.evaluate(() => window.__dbg.UI.toggleAmbient()); });

  if (SCEN === 'stress') {
    /* 压力：连续打不出去、重抽到贵、回收再看撤销、手牌塞满 */
    await run('stress-fail', async () => {
      await page.evaluate(() => {
        const { S, UI } = window.__dbg;
        S.W.essence = 300;
        for (let k = 0; k < 8; k++) {
          UI.cast(k % S.W.hand.length, null, { x: 99, y: 99 });
        }
      });
    });
    await shot('stress-reject', '连续 8 次无效施放');
    await run('stress-reroll', async () => {
      await page.evaluate(() => { for (let i = 0; i < 3; i++) window.__dbg.UI.reroll(); });
    });
    await shot('stress-reroll', '连抽三次（梯价 5/10/20）');
    await run('stress-recycle', async () => {
      await page.evaluate(() => { window.__dbg.UI.select(0); window.__dbg.UI.recycle(); });
    });
    await shot('stress-recycle', '回收 + 撤销 toast');
    await run('stress-pack-empty', async () => {
      await page.evaluate(() => { window.__dbg.S.W.essence = 0; window.__dbg.UI.openPanel('pack'); });
    });
    await shot('stress-pack-poor', '时素为 0 时的抽卡面板');
    await run('stress-close', async () => { await page.evaluate(() => window.__dbg.UI.closePanel()); });
  }

  if (SCEN === 'keyboard') {
    const keys = ['F1', 'F2', 'F3', 'F4', 'z', 'w', 'w', 'd', 'Enter', 'Escape', 'l', 'k', 'j', 'p', 'b', 'x'];
    for (const k of keys) {
      await run('key-' + k, async () => { await page.keyboard.press(k); });
      await shot('key-' + k.replace(/[^a-zA-Z0-9]/g, '_'), '按键 ' + k + ' 之后');
      await run('key-esc', async () => { await page.keyboard.press('Escape'); });
    }
  }

  if (SCEN === 'narrow') {
    await run('handgrown', async () => {
      await page.evaluate(() => {
        const { S, D } = window.__dbg;
        const land = D.byId.get('terrain.earth.0.matter.land');
        for (let x = 4; x <= 12; x++) for (let y = 4; y <= 12; y++) S.cmd.cast(land, x, y);
        S.W.essence = 0;
        window.__dbg.UI.updateHud(); window.__dbg.UI.renderRight(); window.__dbg.UI.renderHand();
      });
    });
    await shot('narrow-world', '窄窗 · 世界');
    for (const p of ['codex', 'settings', 'keys']) {
      await run('n-open-' + p, async () => { await page.evaluate(id => window.__dbg.UI.openPanel(id), p); });
      await shot('narrow-' + p, '窄窗面板：' + p);
      await run('n-close-' + p, async () => { await page.evaluate(() => window.__dbg.UI.closePanel()); });
    }
  }

  if (SCEN === 'full') {
    /* 长跑：三个月一跳，看年表 / 痕迹 / 生态读数在长局里怎么滚 */
    await run('long', async () => {
      await page.evaluate(() => {
        const { S, UI } = window.__dbg;
        for (let i = 0; i < 36; i++) S.cmd.tick(1);
        UI.updateHud(); UI.renderRight(); UI.renderHand();
      });
    });
    await shot('long-run', '再跑 36 个月');
    await run('empty-hand', async () => {
      await page.evaluate(() => {
        const { S, UI } = window.__dbg;
        S.W.hand.length = 0;
        UI.renderHand(); UI.updateHud();
      });
    });
    await shot('empty-hand', '手牌空了');
  }

  fs.writeFileSync(path.join(OUT, 'log.json'), JSON.stringify(log, null, 1));
  console.log('---');
  console.log('shots', log.shots.length, 'failures', log.failures.length, 'console', log.console.length);
  if (log.failures.length) console.log(JSON.stringify(log.failures.slice(0, 6)));
  if (log.console.length) console.log(JSON.stringify(log.console.slice(0, 6)));
  await browser.close();
})();
