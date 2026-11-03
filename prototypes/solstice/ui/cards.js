/* 至日 · 卡面渲染（唯一一份）
   卡面 / 悬浮 / 图鉴 / 抽卡结果都从这里出——同一个字段只有一个显示位置（CARD-SCHEMA §0.1）。 */
window.SOL = window.SOL || {};
SOL.ui = SOL.ui || {};
(function (UI, D) {
  const RINGC = ['#b8b0a0', '#b8b0a0', '#9caab8', '#9caab8', '#9caab8',
    '#d6ad5c', '#d6ad5c', '#d6ad5c', '#d6ad5c', '#e0a33e', '#9a7ae0'];
  const CLS_ICON = {
    matter: '<rect x="5" y="5" width="14" height="14" fill="currentColor"/>',
    rite: '<path d="M4 19A8 8 0 0 1 20 19" fill="none" stroke="currentColor" stroke-width="1.8"/><path d="M8 19A4 4 0 0 1 16 19" fill="none" stroke="currentColor" stroke-width="1.8"/>',
    law: '<path d="M12 3l8 4.6v8.8L12 21l-8-4.6V7.6z" fill="none" stroke="currentColor" stroke-width="1.8"/>'
  };

  const esc = s => String(s == null ? '' : s).replace(/[&<>]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c]));

  function highlight(text, terms) {
    let out = esc(text);
    for (const t of terms || []) {
      if (!t) continue;
      out = out.split(esc(t)).join('<b>' + esc(t) + '</b>');
    }
    return out;
  }

  /* 画芯：只在 schema 的 art.sigil / palette / focus 上做——没有专属图片就用元素纹样 */
  function artSVG(card, uid) {
    const pal = (card.art && card.art.palette) || ['#0c1218', '#12303c', '#0a1016'];
    const sig = D.sigil(card.primary);
    const f = (card.art && card.art.focus) || { x: 0.5, y: 0.5 };
    return `<svg viewBox="0 0 100 100" preserveAspectRatio="xMidYMid slice">
      <defs>
        <linearGradient id="bg${uid}" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0" stop-color="${pal[0]}"/><stop offset=".55" stop-color="${pal[1]}"/><stop offset="1" stop-color="${pal[2]}"/>
        </linearGradient>
        <radialGradient id="gl${uid}" cx="${f.x}" cy="${f.y}" r=".7">
          <stop offset="0" stop-color="${D.elColor(card.primary)}" stop-opacity=".22"/>
          <stop offset="1" stop-color="${D.elColor(card.primary)}" stop-opacity="0"/>
        </radialGradient>
      </defs>
      <rect width="100" height="100" fill="url(#bg${uid})"/>
      <g opacity=".35" fill="#a8ccdd">
        <circle cx="14" cy="12" r=".7"/><circle cx="32" cy="7" r=".5"/><circle cx="52" cy="15" r=".8"/>
        <circle cx="72" cy="9" r=".55"/><circle cx="88" cy="18" r=".6"/><circle cx="22" cy="24" r=".45"/>
      </g>
      <rect width="100" height="100" fill="url(#gl${uid})"/>
      <g style="color:${D.elColor(card.primary)}" transform="translate(50 52) scale(.86) translate(-50 -50)"
         opacity=".92">${sig}</g>
      <path d="M0 82 C22 76 44 86 66 80 C80 76 90 80 100 78 L100 100 L0 100 Z"
        fill="#000" opacity=".26"/>
    </svg>`;
  }

  UI.cardFace = function (card, uid) {
    uid = uid || (++UI._uid);
    const ringc = RINGC[Math.min(10, card.tier)];
    const bars = Array.from({ length: 12 }, (_, i) => `<i class="${i <= card.tier ? 'on' : ''}"></i>`).join('');
    const els = card.elements;
    const subs = els.slice(1);

    return `<div class="card ${card.class} ${D.elCls(card.primary)}" data-card="${card.id}"
      style="--ringc:${ringc};--el:${D.elRgb(card.primary)}">
      <div class="clseal"><svg viewBox="0 0 24 24">${CLS_ICON[card.class]}</svg></div>

      <div class="band">
        <h3>${esc(card.name.zh)}</h3>
        ${subs.map((e, i) => `<span class="elem sub ${i === 1 ? 'sub2' : ''}" style="--el:${D.elRgb(e)}">
          <svg viewBox="0 0 100 100">${D.sigil(e)}</svg></span>`).join('')}
        <span class="elem"><svg viewBox="0 0 100 100">${D.sigil(card.primary)}</svg></span>
      </div>

      <div class="ringbar">${bars}</div>

      <div class="win"><div class="pic">${artSVG(card, uid)}</div></div>

      <div class="box">
        <div class="fx">${highlight(card.line.fx, card.line.hl)}</div>
        ${(card.line.cond || []).length ? `<div class="cond">${(card.line.cond || []).map(esc).join(' · ')}</div>` : ''}
        <div class="num">${(card.line.num || []).map(n => `${esc(n.k)} <b>${esc(n.v)}</b>`).join('')}</div>
      </div>

      <div class="foot">
        <span>${esc(card.number)}</span>
        <span class="atk">ATK <b>${card.atk}</b> · DEF <b>${card.def}</b></span>
      </div>
    </div>`;
  };
  UI._uid = 0;

  /* 悬浮：卡上只写要点，这里给「要点 + 值 + 风味」，完整效果留给图鉴 */
  UI.showTip = function (ev, card) {
    const tip = document.getElementById('cardtip');
    if (!tip || !card) return;
    const life = SOL.sim.traceLife(card);
    tip.innerHTML = `<div class="t1">${esc(card.name.zh)}</div>
      <div class="t2">${card.elements.join(' + ')} · ${card.tier === 10 ? '超环' : card.tier + ' 环'} · ${D.clsName[card.class]} · ${esc(card.rarity_word)}</div>
      <div class="t3">${esc(card.line.fx)}</div>
      <div class="t4">◔ ${card.radius_w.toFixed(3)} W · ⚠ +${card.disturb} · ⧗ ${card.cooldown_s || '—'} 秒
        · 痕迹 ${life >= 999 ? '永久' : life + ' 月'}
        ${card.reclaim ? ' · 回收 +' + card.reclaim : ''}</div>
      ${card.evolve && card.evolve.to ? `<div class="t6">进化 ▸ ${esc((D.byId.get(card.evolve.to) || {}).name ? D.byId.get(card.evolve.to).name.zh : '')} · ${esc(card.evolve.req || '')}</div>` : ''}
      ${card.flavor ? `<div class="t5">“${esc(card.flavor.zh)}”</div>` : ''}
      <div class="t7">图鉴（L）▸ 完整效果</div>`;
    tip.style.display = 'block';
    const anchor = (ev.target.closest && ev.target.closest('.handslot')) || ev.currentTarget;
    const r = anchor.getBoundingClientRect();
    const w = 288, h = tip.offsetHeight;
    tip.style.left = Math.max(8, Math.min(r.left + r.width / 2 - w / 2, innerWidth - w - 10)) + 'px';
    tip.style.top = Math.max(8, r.top - h - 12) + 'px';
  };

  UI.hideTip = function () {
    const t = document.getElementById('cardtip');
    if (t) t.style.display = 'none';
  };

  /* 图鉴读数条：stats[] 六条（w = 绝对百分比，base = 同类中位线） */
  UI.statsBlock = function (card) {
    const col = D.elColor(card.primary);
    return (card.stats || []).map(s => `<div class="stat">
      <span class="sk">${esc(s.k)}</span>
      <div class="sbar"><i style="width:${s.w}%;background:${col}"></i></div>
      <span class="sv">${esc(s.v)}</span>
      ${s.t ? `<span class="st">${esc(s.t)}</span>` : ''}
    </div>`).join('');
  };

})(SOL.ui, SOL.data);
