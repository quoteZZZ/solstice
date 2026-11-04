/* Solstice · 世界事件
   事件不是失败判定，是「要不要接住它」：承受 = 让它发生；改写 = 花时素换一种后果。
   两种结果都真的改世界（改在格子的数值与实体上），不是只写一行年表。 */
(function (UI, S, D) {
  const W = S.W;
  const U = UI.state;

  /* 事件表与后果都在 sim/events.js；这里只读，不持有。 */
  const EVENTS = UI.EVENTS = S.EVENTS;

  /* 改写价与「抽哪一张」：价由 sim 定，抽牌用 Math.random 也安全——它只决定给你看哪张，不改世界。 */
  UI.REWRITE_COST = S.REWRITE_COST;

  UI.fireEvent = function (kind) {
    if (!W.cells.size) { UI.toast('世界是空的', '先造出一点地，再来事件'); return; }
    U.event = S.cmd.pickEvent(kind);
    U.panel = null;
    UI.renderOverlay();
    UI.renderEvent();
  };


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
        <button class="btn ghost" data-ev="rewrite"${W.essence < UI.REWRITE_COST ? ' disabled' : ''}
          title="${W.essence < UI.REWRITE_COST ? '还差 ' + (UI.REWRITE_COST - W.essence) + ' 时素' : ''}"
          >改写 · ${UI.REWRITE_COST} 时素</button>
        <span class="xs" style="align-self:center;margin-left:auto">时素 ${W.essence} · Esc 关闭</span>
      </div>
    </div>`;

    box.onclick = ev => {
      if (ev.target.id === 'eventwrap') return UI.closeEvent();
      const b = ev.target.closest('[data-ev]');
      if (!b) return;
      if (b.dataset.ev === 'bear') {
        S.cmd.bearEvent(e);
        UI.toast('已承受', '「' + e.t + '」写进年表');
      } else {
        const res = S.cmd.rewriteEvent(e);
        if (!res.ok) { UI.toast('时素不够', '改写要 ' + UI.REWRITE_COST); return; }
        UI.toast('已改写', '−' + res.cost + ' 时素 · 后果换了');
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
        <rect x="26" y="82" width="10" height="10" fill="var(--ink-4)"/><rect x="44" y="82" width="10" height="10" fill="var(--ink-4)"/>
        <rect x="62" y="82" width="10" height="10" fill="var(--ink-4)"/><rect x="80" y="82" width="10" height="10" fill="var(--ink-4)"/>`,
      script: `<rect x="30" y="46" width="60" height="76" fill="#6b7887"/>
        <path d="M40,62 L80,62 M40,74 L74,74 M40,86 L80,86 M40,98 L64,98" stroke="#cfd8e2" stroke-width="2" opacity=".35"/>
        <path d="M52,72 L60,60 L68,72 L64,84 L56,84 Z" fill="none" stroke="#e0a33e" stroke-width="2"/>`
    };
    return base + (art[a] || '') + '</svg>';
  }

})(SOL.ui, SOL.sim, SOL.data);
