/* Solstice · 常驻 HUD：顶栏 / 左栏 / 右栏 / 日志 / toast
   右栏一屏三段（这一界 / 生态 / 刚刚发生），不切页签。 */
(function (UI, S, D) {
  const W = S.W;

  UI.GLYPH = {
    pack: '<rect x="2.5" y="5" width="7" height="9" rx="1" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M5.5 2.8h7.2v9.4" fill="none" stroke="currentColor" stroke-width="1.2" opacity=".55"/>',
    codex: '<path d="M2.6 3.2h4.6v9.6H2.6zM8.8 3.2h4.6v9.6H8.8z" fill="none" stroke="currentColor" stroke-width="1.1"/><path d="M7.2 3.2v9.6" stroke="currentColor" stroke-width="1" opacity=".6"/>',
    chron: '<rect x="3.5" y="2.5" width="9" height="11" rx="1" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M5.8 5.5h4.4M5.8 8h4.4M5.8 10.5h2.6" stroke="currentColor" stroke-width="1"/>',
    creator: '<rect x="2.6" y="2.6" width="10.8" height="10.8" rx="2" fill="none" stroke="currentColor" stroke-width="1.2"/><circle cx="5.9" cy="5.9" r="1" fill="currentColor"/><circle cx="10.1" cy="5.9" r="1" fill="currentColor"/><circle cx="5.9" cy="10.1" r="1" fill="currentColor"/><circle cx="10.1" cy="10.1" r="1" fill="currentColor"/>',
    settings: '<circle cx="8" cy="8" r="2.5" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M8 1.6v1.8M8 12.6v1.8M1.6 8h1.8M12.6 8h1.8M3.6 3.6l1.3 1.3M11.1 11.1l1.3 1.3M12.4 3.6l-1.3 1.3M4.9 11.1l-1.3 1.3" stroke="currentColor" stroke-width="1"/>',
    keys: '<rect x="2" y="4.5" width="12" height="7" rx="1.4" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M4.6 7h1.2M7.4 7h1.2M10.2 7h1.2M4.6 9.2h1.2M7.4 9.2h4" stroke="currentColor" stroke-width="1"/>',
    help: '<circle cx="8" cy="8" r="6" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M6.2 6.3a1.85 1.85 0 1 1 2.5 1.7c-.6.3-.8.7-.8 1.3" fill="none" stroke="currentColor" stroke-width="1.2"/><circle cx="8" cy="11.5" r=".7" fill="currentColor"/>'
  };
  UI.glyph = p => `<svg viewBox="0 0 16 16" width="15" height="15" style="display:block">${UI.GLYPH[p] || ''}</svg>`;

  const PANEL_LABEL = { pack: '抽卡', codex: '卡库', chron: '年表', settings: '设置', keys: '键位', help: '帮助', creator: '换界' };
  UI.PANEL_LABEL = PANEL_LABEL;

  UI.initHud = function () {
    /* 档位：暂停 / 0.5 / 1 / 2 / 4（TIME.md §2） */
    const gears = document.getElementById('gears');
    gears.innerHTML = S.GEARS.map((g, i) =>
      `<button class="btn" data-gear="${i}">${g.label}</button>`).join('');
    gears.addEventListener('click', e => {
      const b = e.target.closest('[data-gear]');
      if (!b) return;
      S.cmd.gear(+b.dataset.gear);
      UI.syncGears();
    });

    document.getElementById('bPack').onclick = () => UI.openPanel('pack');

    /* 左栏是唯一的页面入口：顶栏原来还有一排一模一样的图标，两处都点得到同一批面板
       （REVIEW-DEMO C-1 / #12）。现在只留左栏，抽包走顶栏按钮，
       键位与操作速查并进「设置 → 操作」，左栏 8 项收敛到 5 项。 */
    const rail = document.getElementById('leftrail');
    rail.innerHTML = ['chron', 'codex', 'creator', 'settings']
      .map(p => `<div class="rail" data-panel="${p}" tabindex="0" role="button" title="${PANEL_LABEL[p] || '创世'}">${UI.glyph(p)}<span>${PANEL_LABEL[p] || '创世'}</span></div>`)
      .join('') +
      `<div class="rail" data-act="ambient" tabindex="0" role="button" title="摆件模式（P）" style="margin-top:auto;margin-bottom:8px">
        <svg viewBox="0 0 16 16" width="15" height="15"><rect x="2.5" y="4" width="11" height="8" rx="1.6" fill="none" stroke="currentColor" stroke-width="1.2"/><circle cx="8" cy="8" r="1.6" fill="currentColor"/></svg><span>摆件</span></div>`;
    rail.addEventListener('click', e => {
      const b = e.target.closest('[data-panel],[data-act]');
      if (!b) return;
      if (b.dataset.act === 'ambient') UI.toggleAmbient();
      else UI.openPanel(b.dataset.panel);
    });

    S.on('log', UI.log);
    S.on('chron', () => UI.renderRight());
    /* 点亮那一刻只给一次 toast：图鉴入口在卡库里，不给入口提示玩家找不到 */
    S.on('codex', list => list.forEach(a =>
      UI.toast('图鉴点亮', a.grp + ' · ' + a.name + '　' + S.codexCount() + ' / ' + S.CODEX.length)));

    UI.syncGears();
    UI.renderRight();
  };

  UI.syncGears = function () {
    document.querySelectorAll('[data-gear]').forEach(b =>
      b.classList.toggle('on', +b.dataset.gear === S.gear));
    /* 底栏那颗档位按钮是 renderHand 拼的，不跟着改就会一直显示旧档（REVIEW-DEMO X-8） */
    const ab = document.querySelector('#actbar [data-act="gear"]');
    if (ab) ab.textContent = S.GEARS[S.gear].label;
  };

  /* ── 日志 ── */
  UI.log = function (msg, hot) {
    const box = document.getElementById('log');
    if (!box) return;
    const d = document.createElement('div');
    if (hot) d.className = 'hot';
    d.textContent = msg;
    box.appendChild(d);
    setTimeout(() => d.remove(), 4600);
    while (box.children.length > 8) box.removeChild(box.firstChild);
  };

  UI.toast = function (t, d, act) {
    /* 面板开着的时候不要发到全屏顶部：那一层 z-index 70 > 面板 60，
       会正好横在面板标题与步骤条之间（UI 评审 P40）。改成塞进 #phead 的一枚行内小条。 */
    const ov = document.getElementById('overlay');
    const head = document.getElementById('phead');
    if (ov && ov.classList.contains('show') && head && !act) {
      const chip = document.createElement('span');
      chip.className = 'toast ih';
      const i = document.createElement('i'); i.textContent = t;
      chip.appendChild(i);
      if (d) { const em = document.createElement('em'); em.textContent = d; chip.appendChild(em); }
      const close = head.querySelector('.close');
      head.insertBefore(chip, close || null);
      while (head.querySelectorAll('.toast.ih').length > 1) head.querySelector('.toast.ih').remove();
      setTimeout(() => chip.remove(), 2600);
      return;
    }
    const box = document.getElementById('toasts');
    if (!box) return;
    /* 同一句话不叠第二条：连续 8 次无效施放会在顶部堆 4 个一模一样的方框，
       最新那条反而被挤到最下面，还盖住面板标题（UI 评审 F12 / F13 / P40）。
       重复就只刷新计时并闪一下，不新增。 */
    const key = t + '|' + (d || '');
    for (const old of [...box.children]) {
      if (old.dataset.k !== key) continue;
      clearTimeout(old._t);
      old._t = setTimeout(() => old.remove(), 3800);
      if (old.animate) old.animate([{ opacity: .35 }, { opacity: 1 }], { duration: 200 });
      return;
    }
    const el = document.createElement('div');
    el.className = 'toast';
    el.innerHTML = `<div class="t">${t}</div>${d ? `<div class="d">${d}</div>` : ''}`;
    if (act) {
      const b = document.createElement('button');
      b.className = 'btn ghost toact';
      b.textContent = act.label;
      b.addEventListener('click', () => { el.remove(); act.fn(); });
      el.appendChild(b);
    }
    el.dataset.k = key;
    box.appendChild(el);
    el._t = setTimeout(() => el.remove(), 3800);
    while (box.children.length > 3) box.removeChild(box.firstChild);
  };

  /* ── 顶栏数值 ── */
  UI.updateHud = function () {
    const year = document.getElementById('cYear');
    if (!year) return;
    year.textContent = S.year();
    document.getElementById('cMonth').textContent = S.monthOfYear() + 1;
    document.getElementById('cSeason').textContent = S.season();
    document.getElementById('sCells').textContent = W.cells.size;
    document.getElementById('sEss').textContent = W.essence;
    document.getElementById('sLife').textContent = (() => {
      const l = S.lifeStats();
      return l.flora + l.fauna;
    })();
    document.querySelector('#essbar i').style.width = Math.round(W.essAcc * 100) + '%';
    /* 顶栏那颗「抽卡」：文案与可用性读同一个来源——当前开着的包里最便宜那档。
       原来按钮写死「抽包 30」（index.html），而基础包其实 12：玩家按 30 攒钱，
       12 就能买；凑到 30 又以为只够一包（UI 评审 U01）。 */
    const buyable = S.packList().filter(p => S.packOpen(p));
    const cheapest = buyable.reduce((a, p) => (!a || p.cost < a.cost ? p : a), null);
    const bPack = document.getElementById('bPack');
    bPack.textContent = cheapest ? '抽包 ' + cheapest.cost : '抽包';
    bPack.disabled = !buyable.some(p => W.essence >= p.cost);
    document.getElementById('hint').style.opacity = W.cells.size ? '0' : '1';
  };

  /* ── 右栏 ── */
  const KIND_LABEL = { land: '裸地', grass: '草原', forest: '林地', water: '水域', rock: '岩坡' };
  const KIND_COL = { land: '#5a5240', grass: '#5d8a4a', forest: '#2f5a3c', water: '#3f7fa8', rock: '#5b6b78' };

  /* 右栏每秒被 tick 叫一次。原来无条件 innerHTML 重建：
     玩家滚到「刚刚发生」想看条目，滚动位置每秒被清回顶部，点击也常被下一 tick 顶掉。
     现在先用一个便宜签名比一次，没变就不动 DOM；变了也只换内容、把滚动位置还回去。 */
  let rightSig = '';
  /* 水 / 光 / 温 三条横条：绿带是 sim 真正用的宜居区间（植物要 W≥1 且 L≥1），
     指针进带变绿、出带保持琥珀。温度没有生长阈值，只标 20 °C 基准刻度——
     不编一个「舒适区间」出来，那是假的信息（REVIEW-DEMO S-6 / R-3）。 */
  function fieldBars(f) {
    const rows = [
      { k: '水', v: f.w, lo: 0, hi: 3, good: [1, 3], fix: 2, unit: '' },
      { k: '光', v: f.l, lo: 0, hi: 3, good: [1, 3], fix: 2, unit: '' },
      { k: '温', v: f.t, lo: 10, hi: 30, mark: 20, fix: 1, unit: '°' }
    ];
    const pct = (v, lo, hi) => Math.max(0, Math.min(100, (v - lo) / (hi - lo) * 100));
    return rows.map(r => {
      /* 指针夹在轨道内：0.00 时 left:0% 会让 2px 指针有一半悬在圆头外面，像颗散落的点 */
      const at = Math.max(1.2, Math.min(98.8, pct(r.v, r.lo, r.hi)));
      const inBand = r.good ? r.v >= r.good[0] && r.v <= r.good[1] : null;
      return `<div class="fb">
        <span class="fbk">${r.k}</span>
        <div class="fbar">
          ${r.good ? `<i class="fband" style="left:${pct(r.good[0], r.lo, r.hi)}%;
            width:${pct(r.good[1], r.lo, r.hi) - pct(r.good[0], r.lo, r.hi)}%"></i>` : ''}
          ${r.mark != null ? `<i class="ftick" style="left:${pct(r.mark, r.lo, r.hi)}%"></i>` : ''}
          <i class="fpt${inBand ? ' ok' : ''}" style="left:${at}%"></i>
        </div>
        <span class="fbv">${r.v.toFixed(r.fix)}${r.unit || ''}</span>
      </div>`;
    }).join('');
  }

  UI.renderRight = function (force) {
    const box = document.getElementById('rightpanel');
    if (!box) return;
    const w = W.world || {};
    const life = S.lifeStats();

    /* 水/光/温均值也进签名：暂停时施一张引水卡，月份不变，横条得跟着动 */
    const field = S.fieldStats();
    const fsig = field ? [Math.round(field.w * 10), Math.round(field.l * 10), Math.round(field.t)].join(',') : '-';
    const sig = [fsig, W.month, W.cells.size, W.essence, W.sun, W.traces.length,
      W.traces.length ? W.traces[W.traces.length - 1].id + ':' + W.traces[W.traces.length - 1].count : 0,
      life.flora, life.fauna, life.sapient, life.floraSet.size, life.faunaSet.size,
      w.name, UI.state.selCell ? UI.state.selCell.x + ',' + UI.state.selCell.y : ''].join('|');
    if (!force && sig === rightSig) return;
    rightSig = sig;

    const grow = box.querySelector('.rp.grow');
    const keepTop = grow ? grow.scrollTop : 0;

    /* 生态构成用真实格数，不用写死的百分比 */
    const kinds = { land: 0, grass: 0, forest: 0, water: 0, rock: 0 };
    for (const c of W.cells.values()) kinds[c.kind] = (kinds[c.kind] || 0) + 1;
    const total = Math.max(1, W.cells.size);

    const species = [];
    for (const id of life.floraSet) species.push(D.byId.get(id).name.zh);
    for (const id of life.faunaSet) species.push(D.byId.get(id).name.zh);

    const traces = W.traces.slice(-4).reverse();

    box.innerHTML = `
      <div class="rp">
        <div class="h">这一界</div>
        <div class="big">${w.name || '—'}</div>
        <div class="xs" style="margin-top:2px">${w.form || '—'} · ${w.climate || '—'}</div>
        <div class="mono xs" style="margin-top:2px;color:var(--ink3)">${(w.seed || []).join('-')}</div>
        <div class="rowline" style="margin-top:5px"><span class="xs">时素</span>
          <span class="mono xs" style="color:var(--acc)">◈ ${W.essence}</span></div>
        <div class="rowline"><span class="xs">太阳 / 世界律</span>
          <span class="mono xs">☀ ${W.sun} · 寿命 ×${W.law.lifeX} · 繁 ×${W.law.propX}</span></div>
      </div>

      ${field ? `<div class="rp">
        <div class="h">环境</div>
        ${fieldBars(field)}
      </div>` : ''}

      <div class="rp">
        <div class="row" style="justify-content:space-between;align-items:baseline">
          <span class="h" style="margin:0">生态</span>
          <span class="tag ${life.flora ? 'ok' : ''}">${!W.cells.size ? '未成形' : !life.flora ? '荒界' : life.flora + life.fauna > 6 ? '丰饶' : '初绿'}</span>
        </div>
        <div style="margin-top:4px">
          ${Object.keys(kinds).map(k => `<div class="row" style="justify-content:space-between;padding:1px 0">
            <span class="xs"><i style="display:inline-block;width:8px;height:8px;border-radius:2px;
              background:${KIND_COL[k]};box-shadow:inset 0 0 0 1px rgba(255,255,255,.14);margin-right:5px"></i>${KIND_LABEL[k]}</span>
            <span class="mono xs" style="color:var(--ink-3)">${Math.round(kinds[k] / total * 100)}%</span></div>`).join('')}
        </div>
        <div class="row" style="justify-content:space-between;margin-top:8px;padding-top:7px;border-top:1px solid var(--line-1)">
          ${[['植物', life.flora, '#5e8f47'], ['动物', life.fauna, 'var(--gold)'], ['智慧', life.sapient, '#e0d6b8']]
            /* 0 值原来画一颗暗灰圆点，读起来像「还在加载」（UI 评审 U18）；改成一道短横。 */
            .map(([k, v, c]) => `<div style="text-align:center;flex:1">
              <div style="font-size:11px;color:${v ? c : 'var(--ink-4)'}">${v ? '●' : '–'}</div>
              <div class="mono" style="font-size:11px;color:${v ? 'var(--ink-1)' : 'var(--ink-4)'}">${v}</div>
              <div style="font-size:11px;color:var(--ink3)">${k}</div></div>`).join('')}
        </div>
        ${species.length ? `<div class="row" style="flex-wrap:wrap;gap:4px;margin-top:7px">
          ${species.slice(0, 10).map(n => `<span class="tag">${n}</span>`).join('')}</div>` : ''}
      </div>

      <div class="rp grow">
        <div class="row" style="justify-content:space-between;align-items:baseline">
          <span class="h" style="margin:0">痕迹 · 刚刚发生</span>
          <span class="xs" style="cursor:pointer" data-chron="1">年表 ▸</span>
        </div>
        <div style="margin:6px 0 9px">
          ${traces.length ? traces.map(t => `<div class="rowline" style="cursor:pointer" data-trace="${t.idCard}">
            <span class="xs"><i style="display:inline-block;width:5px;height:5px;border-radius:50%;
              background:${D.elColor(t.el)};margin-right:5px"></i>${t.name}${t.count > 1 ? ' ×' + t.count : ''}</span>
            <span class="mono xs" style="color:var(--ink4)">${t.life >= 999 ? '永久' : Math.max(0, t.life - (W.month - t.born)) + ' 月'}</span>
          </div>`).join('')
            /* 「还没有痕迹」只在世界真的没被碰过时成立。痕迹按月龄过期，
               146 格、4 年的世界里这段会变空，而空态还写着「没被碰过」——
               同一屏下方的年表却列着 6 条（UI 评审 U19）。 */
            : `<div class="xs">${W.cells.size ? '最近的痕迹都已消散 · 见年表 ▸' : '还没有痕迹——世界还没被碰过'}</div>`}
        </div>
        <div class="tl">
          ${(W.chron.slice(0, 6)).map(c => {
            const col = UI.CHRON_COL[c.kind] || '#3f7fa8';
            return `<div class="it" style="--dot:${col}" data-chron-item="${c.month}">
              <div class="xs"><span style="color:${col};margin-right:4px">${UI.CHRON_ICON[c.kind] || '·'}</span>
                第 ${c.month} 月</div>
              <div class="t" style="font-size:13px">${c.text}</div>
              <div class="xs" style="color:var(--ink4)">${c.kind}</div></div>`;
          }).join('') || '<div class="xs">年表还空着</div>'}
        </div>
      </div>`;

    box.onclick = e => {
      const t = e.target.closest('[data-trace]');
      if (t) { UI.openCard(t.dataset.trace); return; }
      if (e.target.closest('[data-chron]')) { UI.openPanel('chron'); return; }
      const it = e.target.closest('[data-chron-item]');
      if (it) UI.openPanel('chron');
    };

    /* 把滚动位置还回去，免得玩家正在读的条目被 tick 顶走 */
    const grow2 = box.querySelector('.rp.grow');
    if (grow2 && keepTop) grow2.scrollTop = keepTop;
  };

  UI.CHRON_ICON = { 干涉: '✦', 气候: '☁', 生态: '✿', 灾变: '⚠', 文明: '⌂', 图鉴: '❖' };
  UI.CHRON_COL = { 干涉: '#e0a33e', 气候: '#5f9fc0', 生态: '#6aa86a', 灾变: '#b4553f', 文明: 'var(--gold)', 图鉴: 'var(--gold)' };

  /* 悬停地块读数：真的读 sim 的格，不再用坐标哈希编 */
  UI.hoverInfo = function (cell, ev) {
    let box = document.getElementById('hoverinfo');
    if (!box) {
      box = document.createElement('div');
      box.id = 'hoverinfo';
      document.getElementById('stage').appendChild(box);
    }
    if (!cell) { box.style.display = 'none'; return; }
    const ents = cell.ents.filter(e => !e.ground).map(e => e.name);
    box.innerHTML = `<b>${KIND_LABEL[cell.kind] || cell.kind}</b> · ${cell.x}, ${cell.y}<br>
      水 ${cell.W.toFixed(2)} · 光 ${cell.L.toFixed(2)} · 温 ${cell.T.toFixed(1)}°C<br>
      ${ents.length ? '活物 ' + ents.slice(0, 4).join('/') : '空'}`;
    box.style.display = 'block';
    /* #hoverinfo 是 position:fixed，直接用视口坐标；原来减了 54（顶栏高），
       于是浮框整体飘在光标上方一格多（REVIEW-DEMO X-10）。 */
    box.style.left = Math.min(ev.clientX + 16, innerWidth - 210) + 'px';
    box.style.top = Math.min(ev.clientY + 16, innerHeight - 70) + 'px';
  };

})(SOL.ui, SOL.sim, SOL.data);
