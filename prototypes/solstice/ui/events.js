/* 至日 · 世界事件
   事件不是失败判定，是「要不要接住它」：承受 = 让它发生；改写 = 花时素换一种后果。
   两种结果都真的改世界（改在格子的数值与实体上），不是只写一行年表。 */
(function (UI, S, D) {
  const W = S.W;
  const U = UI.state;

  const EVENTS = [
    { k: '禁物', t: '锈蚀之核', a: 'rust',
      b: '界缘裂开一线，一枚不属于这里的核落进来。它把周围的金属慢慢吃成锈，但锈土里会长出别处没有的赤色苔。',
      tags: ['只启不养', '不可回收', '同时只许一件'],
      apply: () => { const c = anyCell(); if (c) { c.flags.metal = true; c.W = Math.max(0, c.W - 0.6); } },
      rewrite: () => { gainEssence(4); } },
    { k: '禁物', t: '无光之钉', a: 'nail',
      b: '一枚钉子插进夜里，钉子上方三尺没有星。它不是黑暗，是把光让开。',
      tags: ['只启不养', '不可回收', '夜间更暗'],
      apply: () => { const c = anyCell(); if (c) c.mods.l -= 1; },
      rewrite: () => { const c = anyCell(); if (c) c.mods.l -= 0.4; } },
    { k: '禁物', t: '逆流之钟', a: 'bell',
      b: '钟自己响了。此后这一带的水先往山上走，再落下来——落下来的水更清。',
      tags: ['只启不养', '不可回收', '水往高处'],
      apply: () => { for (const c of someCells(5)) c.W = Math.min(3, c.W + 0.6); },
      rewrite: () => { gainEssence(6); } },
    { k: '灾变', t: '山火过境', a: 'fire',
      b: '火从西坡往上走，走到雪线自己停了。焦土是黑的，但三年后这里会比别处更肥。',
      tags: ['留下焦土', '灰是肥料', '兽群迁移'],
      apply: () => { for (const c of someCells(3)) { c.ents = c.ents.filter(e => e.ground); c.W = Math.max(0, c.W - 0.5); c.fertile = 0.3; } },
      rewrite: () => { const c = anyCell(); if (c) { c.ents = c.ents.filter(e => e.ground); c.fertile = 0.3; } } },
    { k: '灾变', t: '陨石', a: 'meteor',
      b: '一道白线，然后是一个坑。坑底的石头化了又凝，像一块没人磨过的玻璃。',
      tags: ['留下陨坑', '玻璃质', '扬尘 10 日'],
      apply: () => { const c = anyCell(); if (c) { c.ents = c.ents.filter(e => e.ground); c.W = 0; c.mods.l -= 0.3; } },
      rewrite: () => { gainEssence(5); } },
    { k: '灾变', t: '冰川期', a: 'ice',
      b: '雪线压到海面，整界安静下来。冰退的时候，会把山磨成峡湾。',
      tags: ['雪线下降', '数十日', '留下冰碛'],
      apply: () => { for (const c of W.cells.values()) c.mods.t -= 1; },
      rewrite: () => { for (const c of W.cells.values()) c.mods.t -= 0.3; } },
    { k: '文明', t: '第一座聚落', a: 'hut',
      b: '三间屋子围着一口井。它们不声不响地出现了，像本来就该在那里。',
      tags: ['需已有植物', 'Z3 可见', '可被卡改写'],
      apply: () => { W.sapient = Math.max(1, W.sapient); },
      rewrite: () => { S.drawPack(); } },
    { k: '文明', t: '第一道城墙', a: 'wall',
      b: '有人把石头垒成一条线。线内是「我们」，线外还是世界。',
      tags: ['疆域', '不可越过', '可被攻破'],
      apply: () => { const c = anyCell(); if (c) S.spawn(D.byId.get('terrain.mineral.0.matter.rock'), c, { ground: true }); },
      rewrite: () => { gainEssence(3); } },
    { k: '文明', t: '第一部文字', a: 'script',
      b: '有人在石头上刻了一道。从这一刻起，年表开始自己说话。',
      tags: ['年表自动记录', '记忆可继承'],
      apply: () => { W.sapient = Math.max(1, W.sapient); S.addChron('文明', '第一道文字被刻下'); },
      rewrite: () => { S.addChron('文明', '有人把第一道文字磨掉了'); } }
  ];

  UI.EVENTS = EVENTS;

  function anyCell() {
    const cells = [...W.cells.values()];
    return cells.length ? cells[(Math.random() * cells.length) | 0] : null;
  }
  function someCells(n) {
    const cells = [...W.cells.values()];
    const out = [];
    for (let i = 0; i < n && cells.length; i++) out.push(cells[(Math.random() * cells.length) | 0]);
    return out;
  }
  function gainEssence(n) { W.essence += n; }

  UI.fireEvent = function (kind) {
    if (!W.cells.size) { UI.toast('世界是空的', '先造出一点地，再来事件'); return; }
    const pool = kind ? EVENTS.filter(e => e.k === kind) : EVENTS;
    U.event = pool[(Math.random() * pool.length) | 0];
    U.panel = null;
    UI.renderOverlay();
    UI.renderEvent();
  };

  UI.REWRITE_COST = 60;

  UI.renderEvent = function () {
    const box = document.getElementById('eventwrap');
    if (!box) return;
    const e = U.event;
    box.classList.toggle('on', !!e);
    if (!e) { box.innerHTML = ''; return; }
    box.innerHTML = `<div id="eventcard">
      <div class="band"></div>
      <div class="row" style="justify-content:space-between">
        <span class="mono xs" style="letter-spacing:.18em;color:var(--warn)">世界事件 · ${e.k}</span>
        <span class="xs">第 ${W.month} 月</span>
      </div>
      <div class="row" style="gap:18px;margin-top:14px;align-items:flex-start">
        ${evArt(e.a)}
        <div style="flex:1;min-width:0">
          <div class="ttl">${e.t}</div>
          <div class="body">${e.b}</div>
          <div class="tags">${e.tags.map(t => `<span class="tag warn">${t}</span>`).join('')}</div>
        </div>
      </div>
      <div class="row gap8" style="margin-top:18px">
        <button class="btn on" data-ev="bear">承受</button>
        <button class="btn ghost" data-ev="rewrite">改写 · ${UI.REWRITE_COST} 时素</button>
        <span class="xs" style="align-self:center;margin-left:auto">时素 ${W.essence} · Esc 关闭</span>
      </div>
    </div>`;

    box.onclick = ev => {
      if (ev.target.id === 'eventwrap') return UI.closeEvent();
      const b = ev.target.closest('[data-ev]');
      if (!b) return;
      if (b.dataset.ev === 'bear') {
        e.apply();
        S.addChron(e.k === '文明' ? '文明' : '灾变', e.t + '：被承受');
        UI.toast('已承受', '「' + e.t + '」写进年表');
      } else {
        if (W.essence < UI.REWRITE_COST) { UI.toast('时素不够', '改写要 ' + UI.REWRITE_COST); return; }
        W.essence -= UI.REWRITE_COST;
        e.rewrite();
        S.addChron('干涉', '改写了「' + e.t + '」');
        UI.toast('已改写', '−' + UI.REWRITE_COST + ' 时素 · 后果换了');
      }
      UI.closeEvent();
    };
  };

  UI.closeEvent = function () {
    U.event = null;
    UI.renderEvent();
    UI.renderHand();
    UI.updateHud();
    UI.renderRight();
  };

  /* 事件缩略图：120×168 */
  function evArt(a) {
    const base = `<svg viewBox="0 0 120 168" style="width:120px;height:168px;flex:0 0 120px;
      border:1px solid #33404f;background:radial-gradient(120% 90% at 50% 10%,#1b2733 0%,#0c1218 70%)">`;
    const art = {
      rust: `<circle cx="60" cy="84" r="34" fill="none" stroke="#b4553f" stroke-width="3" opacity=".8"/>
        <circle cx="60" cy="84" r="22" fill="#5a2c22" opacity=".7"/>
        <path d="M42,72 L78,96 M46,96 L74,70" stroke="#c4643f" stroke-width="2" opacity=".7"/>`,
      nail: `<path d="M60,30 L60,130" stroke="#8f9aa6" stroke-width="4"/>
        <path d="M48,30 L72,30 L60,44 Z" fill="#8f9aa6"/>
        <circle cx="60" cy="30" r="17" fill="#06090d"/>`,
      bell: `<path d="M36,104 Q40,56 60,52 Q80,56 84,104 Z" fill="#6b5a3a"/>
        <rect x="34" y="104" width="52" height="8" fill="#8a7448"/>
        <path d="M60,116 L60,130" stroke="#8a7448" stroke-width="3"/>`,
      fire: `<path d="M60,34 C74,60 92,70 82,96 C75,114 60,124 48,116 C32,104 42,80 52,72 C52,90 60,88 60,78 C60,62 56,48 60,34 Z"
        fill="#b4432e" opacity=".85"/>`,
      meteor: `<ellipse cx="60" cy="112" rx="40" ry="16" fill="#1c232b"/>
        <ellipse cx="60" cy="112" rx="22" ry="8" fill="#2b333d"/>
        <path d="M92,26 L66,74" stroke="#e8d9b0" stroke-width="3" opacity=".8"/>`,
      ice: `<path d="M20,120 L60,60 L100,120 Z" fill="#7fa3b8" opacity=".55"/>
        <path d="M34,120 L60,80 L86,120 Z" fill="#cfe4ef" opacity=".8"/>`,
      hut: `<path d="M40,104 L60,84 L80,104 L80,124 L40,124 Z" fill="#5a4a38"/>
        <path d="M34,106 L60,80 L86,106" fill="none" stroke="#8a7448" stroke-width="4"/>
        <rect x="54" y="110" width="12" height="14" fill="#2a2118"/>`,
      wall: `<rect x="26" y="88" width="68" height="32" fill="#5b6673"/>
        <rect x="26" y="82" width="10" height="10" fill="#6d7987"/><rect x="44" y="82" width="10" height="10" fill="#6d7987"/>
        <rect x="62" y="82" width="10" height="10" fill="#6d7987"/><rect x="80" y="82" width="10" height="10" fill="#6d7987"/>`,
      script: `<rect x="30" y="46" width="60" height="76" fill="#3a4652"/>
        <path d="M40,62 L80,62 M40,74 L74,74 M40,86 L80,86 M40,98 L64,98" stroke="#cfd8e2" stroke-width="2" opacity=".35"/>
        <path d="M52,72 L60,60 L68,72 L64,84 L56,84 Z" fill="none" stroke="#e0a33e" stroke-width="2"/>`
    };
    return base + (art[a] || '') + '</svg>';
  }

})(SOL.ui, SOL.sim, SOL.data);
