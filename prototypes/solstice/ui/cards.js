/* Solstice · 卡面渲染（唯一一份）
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
    /* 画芯之前读起来是一块近黑的空框：palette 的三个色本来就都是暗值（#0d1712 / #3c7048 / #131c17），
       再压一层 0.26 的黑底渐变，中间那一点元素色也被吃掉了。
       这版加了三层，让它在 136×60 的手牌窗口里也看得出是「画」：
       顶部受光（白 0.10 → 透明）、元素光晕提到 0.34、墨线放大到 1.02 并压一层极淡的影。 */
    return `<svg viewBox="0 0 100 100" preserveAspectRatio="xMidYMid slice">
      <defs>
        <linearGradient id="bg${uid}" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0" stop-color="${pal[0]}"/><stop offset=".45" stop-color="${pal[1]}"/><stop offset="1" stop-color="${pal[2]}"/>
        </linearGradient>
        <radialGradient id="gl${uid}" cx="${f.x}" cy="${f.y}" r=".72">
          <stop offset="0" stop-color="${D.elColor(card.primary)}" stop-opacity=".34"/>
          <stop offset="1" stop-color="${D.elColor(card.primary)}" stop-opacity="0"/>
        </radialGradient>
        <linearGradient id="lt${uid}" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0" stop-color="#fff" stop-opacity=".10"/>
          <stop offset=".55" stop-color="#fff" stop-opacity="0"/>
        </linearGradient>
      </defs>
      <rect width="100" height="100" fill="url(#bg${uid})"/>
      <g opacity=".30" fill="#a8ccdd">
        <circle cx="14" cy="12" r=".7"/><circle cx="32" cy="7" r=".5"/><circle cx="52" cy="15" r=".8"/>
        <circle cx="72" cy="9" r=".55"/><circle cx="88" cy="18" r=".6"/><circle cx="22" cy="24" r=".45"/>
      </g>
      <rect width="100" height="100" fill="url(#gl${uid})"/>
      <g style="color:${D.elColor(card.primary)}" transform="translate(50 52) scale(1.02) translate(-50 -50)"
         opacity=".95">${sig}</g>
      <rect width="100" height="100" fill="url(#lt${uid})"/>
      <path d="M0 82 C22 76 44 86 66 80 C80 76 90 80 100 78 L100 100 L0 100 Z"
        fill="#000" opacity=".18"/>
    </svg>`;
  }

  /* 手牌专用版式（152×216 原生尺寸，不缩放）。
     原来是 302×440 的整卡 push 到 scale(.5)：fx 13px 缩成 6.5px、cond 4.75px、编号 4.25px，
     手里八张卡实际上读不出任何一句话（REVIEW-DEMO T-3）。这里按「一眼能判断」重排：
     名 + 环条 / 画芯 / 三行效果 / 回收值，完整效果交给悬浮（UI.showTip）。
     不可打的卡降饱和 —— 开局世界是空的时候，这一步就是「先打土地」的无声引导（R-5 / F-1）。 */
  /* 手牌卡面只回答三件事：这是什么（类 / 环）、能放在哪（cond）、做了什么（效果句）。
     效果句是作者写的整句，读起来慢但不会漏；能放在哪是唯一一件要逐张扫的事，给它一枚角标。
     cond 是数据里写好的落点约束，不另造说法；「需已有地块」是默认前提（11 张里 8 张都写它），
     每张都挂就等于没挂——角标只留给例外：虚空、湿土、夜间、有灌木之类。 */
  const PLAIN_COND = '需已有地块';

  function handFace(card, uid, can) {
    const ringc = RINGC[Math.min(10, card.tier)];
    const bars = Array.from({ length: 12 }, (_, i) => `<i class="${i <= card.tier ? 'on' : ''}"></i>`).join('');
    const need = (card.line && card.line.cond || [])[0];
    const showNeed = need && need !== PLAIN_COND;
    return `<div class="card h ${card.class} ${D.elCls(card.primary)}${can ? '' : ' cant'}"
      data-card="${card.id}" style="--ringc:${ringc};--el:${D.elRgb(card.primary)}">
      <div class="hband">
        <span class="ctag">${D.clsName[card.class] || ''}</span>
        <h3>${esc(card.name.zh)}</h3>
      </div>
      <div class="hringbar">${bars}</div>
      <div class="hwin">${artSVG(card, uid)}${showNeed ? `<div class="hreq">${esc(need)}</div>` : ''}</div>
      <div class="hsum"><span>${highlight(card.line.fx, card.line.hl)}</span></div>
      <div class="hfoot">
        <span class="helem" style="--el:${D.elRgb(card.primary)}">
          <svg viewBox="0 0 100 100">${D.sigil(card.primary)}</svg></span>
        <span class="hrec">${card.reclaim ? '↩ ' + card.reclaim : '↩ —'}</span>
      </div>
    </div>`;
  }

  /* 卡库的收藏格要一块缩略插画，跟卡面同一套（同一个函数，不另画一种图） */
  UI.artSVG = artSVG;

  UI.cardFace = function (card, uid, mode, life) {
    uid = uid || (++UI._uid);
    /* 手牌按「此刻能不能打」降饱和；抽包面板里不这么做——
       抽包时玩家判断的是这张卡值不值，不是现在打不打得了（REVIEW-DEMO #13）。 */
    if (mode === 'h') return handFace(card, uid, SOL.sim.playable(card, life));
    if (mode === 'p') return handFace(card, uid, true);
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
    /* disturb / cooldown_s 在 schema 里是必填，但 55 张卡一张都没写。
       缺就先不印，别把 undefined 印到卡上（数值要不要补，等数据拍板）。 */
    const dist = card.disturb == null ? '' : ' · ⚠ +' + card.disturb;
    const cd = card.cooldown_s == null ? '' : ' · ⧗ ' + card.cooldown_s + ' 秒';
    tip.innerHTML = `<div class="t1">${esc(card.name.zh)}</div>
      <div class="t2">${card.elements.join(' + ')} · ${card.tier === 10 ? '超环' : card.tier + ' 环'} · ${D.clsName[card.class]} · ${esc(card.rarity_word)}</div>
      <div class="t3">${esc(card.line.fx)}</div>
      <div class="t4">◔ ${card.radius_w.toFixed(3)} W${dist}${cd}
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
