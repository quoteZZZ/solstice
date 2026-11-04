/* Solstice · 手牌与拖放
   一套轴三种结果：拖到地面 = 指定落点；向上拖 / 空格 = 随机落点；向下拖 / X = 回收。
   落点换算只走 render.cam（射线），UI 不碰投影。 */
(function (UI, S, D) {
  const W = S.W;
  let drag = null;

  /* 手牌宽度 136（.card.h 的设计尺寸，原生 1:1，不再整体缩放）。
     步长按可用宽度收：cap 取卡宽本身 —— 只要屏幕放得下就整排不重叠，
     邻卡盖住的那 20 多像素正好是文字被切掉的地方（REVIEW-DEMO T-3 / C-5）。 */
  const CW = 136;

  /* 视觉尺寸的唯一真源是 CSS 的 --hand-scale（窄屏会把它调小）。
     这里读同一个变量来算步长：两边各写一个数字的话，改断点时必然只改一边，
     结果就是「牌缩小了但间距没缩」或者反过来。 */
  UI.handScale = function () {
    const v = parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--hand-scale'));
    return v > 0 ? v : 1;
  };

  UI.renderHand = function () {
    const box = document.getElementById('hand');
    if (!box) return;
    const n = W.hand.length, mid = (n - 1) / 2;
    /* 手牌一次要问八张「现在能不能打」，每次 playable 都从头扫一遍世界。
       统计一次传进去：八张卡共用同一份读数，也保证同一帧里八张卡的判断不会互相矛盾。 */
    const life = S.lifeStats();
    const wrap = document.getElementById('handwrap');
    const avail = ((wrap && wrap.clientWidth) || 900) - 24;
    const cw = CW * UI.handScale();
    const sp = n > 1
      ? Math.max(78, Math.min(cw, (avail - cw) / (n - 1)))
      : 0;
    box.innerHTML = W.hand.map((card, i) => {
      const d = i - mid;
      const ang = (d * 2.4).toFixed(2);
      const lift = Math.round(Math.abs(d) * Math.abs(d) * 2.2);
      const z = (i === W.sel ? 200 : 100 - Math.round(Math.abs(d) * 3));
      /* 扇形的抬升走 --lift 变量，不写死 bottom：写死之后 `.handslot:hover{bottom:…}` 永远被
         内联样式盖住，悬停只剩投影变化，卡本身不动。 */
      return `<div class="handslot ${i === W.sel ? 'sel' : ''}" data-i="${i}" tabindex="0" role="button"
        style="left:calc(50% + ${(d * sp).toFixed(1)}px - ${cw / 2}px);--lift:${lift}px;z-index:${z}">
        <div class="cardwrap" style="transform:rotate(${ang}deg)">
          ${UI.cardFace(card, 'h' + i, 'h', life)}
        </div>
      </div>`;
    }).join('');

    const deck = document.getElementById('deck');
    deck.innerHTML = `<div class="dk" title="手牌 / 上限"><b>${W.hand.length}</b><span>手</span></div>
      <div class="dk" title="卡池总数（目标 540，已填 ${D.total}）"><b>${D.total}</b><span>池</span></div>`;

    const act = document.getElementById('actbar');
    act.innerHTML = `
      <div class="ab" data-act="random" tabindex="0" role="button" title="随机落点释放（空格）">✦</div>
      <div class="ab" data-act="recycle" tabindex="0" role="button" title="回收选中的卡（X）">↩</div>
      <div class="ab" data-act="reroll" tabindex="0" role="button" title="重抽一手（${S.cmd.rerollCost()} 时素 · R · 连抽翻倍）">↻</div>
      <div class="ab" data-act="gear" tabindex="0" role="button" title="档位（J）" style="font-size:11px">${S.GEARS[S.gear].label}</div>`;
  };

  UI.select = function (i) {
    S.cmd.select(i);
    UI.renderHand();
  };

  /* ── 拖放 ── */
  function beginDrag(ev, i) {
    const card = W.hand[i];
    if (!card) return;
    /* 上一次拖拽没收干净就再拖：会造出第二张 #ghost，而 getElementById 只删得掉第一张，
       剩下那张钉死在 left:0/top:0，永久压住左上角的品牌与左栏（UI 评审 H04 / E01）。 */
    if (drag) endDrag(null);
    ev.preventDefault();
    UI.select(i);
    drag = { i: i, x0: ev.clientX, y0: ev.clientY, moved: false, mode: 'idle', cell: null, pt: null };

    const wrap = document.getElementById('ghostwrap');
    const g = document.createElement('div');
    g.id = 'ghost';
    g.innerHTML = UI.cardFace(card, 'g', 'h') + '<div class="tip">拖到地面指定落点 · 向上随机 · 向下回收</div>';
    wrap.appendChild(g);
    moveGhost(ev.clientX, ev.clientY);

    document.addEventListener('pointermove', onDrag);
    document.addEventListener('pointerup', endDrag, { once: true });
    /* 指针在窗口外松开 / 触控被系统取消时 pointerup 不会来。
       没有兜底，ghost 会留在屏幕上、document 上的 pointermove 永久驻留（REVIEW-DEMO X-11）。 */
    const cancel = () => endDrag(null);
    drag.cancel = cancel;
    document.addEventListener('pointercancel', cancel, { once: true });
    window.addEventListener('blur', cancel, { once: true });
  }

  function moveGhost(x, y) {
    const g = document.getElementById('ghost');
    if (!g) return;
    g.style.left = x + 'px';
    g.style.top = y + 'px';
  }

  function onDrag(ev) {
    if (!drag) return;
    const dy = ev.clientY - drag.y0;
    if (Math.abs(ev.clientX - drag.x0) + Math.abs(dy) > 6) drag.moved = true;
    moveGhost(ev.clientX, ev.clientY);

    const g = document.getElementById('ghost');
    const tip = g && g.querySelector('.tip');
    const card = W.hand[drag.i];
    const cvRect = SOL.render.canvas.getBoundingClientRect();
    const overCanvas = ev.clientX > cvRect.left && ev.clientX < cvRect.right &&
      ev.clientY > cvRect.top && ev.clientY < cvRect.bottom;

    drag.cell = null; drag.pt = null;

    if (dy > 90) {
      drag.mode = 'recycle';
      g && g.classList.add('rec');
      if (tip) tip.textContent = card.reclaim ? '松手回收 +' + card.reclaim + ' 时素' : '松手回收（0 环不返还）';
      SOL.render.cam.setHover(null);
      SOL.render.marks.clear();
    } else if (dy < -40 && drag.moved) {
      drag.mode = 'random';
      g && g.classList.remove('rec');
      if (tip) tip.textContent = '松手释放（随机落点）';
      SOL.render.cam.setHover(null);
      SOL.render.marks.clear();
    } else if (overCanvas && drag.moved) {
      drag.mode = 'cast';
      g && g.classList.remove('rec');
      const cell = SOL.render.cam.pickAt(ev.clientX, ev.clientY);
      const pt = cell ? { x: cell.x, y: cell.y } : SOL.render.cam.planeAt(ev.clientX, ev.clientY);
      drag.cell = cell; drag.pt = pt;
      SOL.render.cam.setHover(cell);
      SOL.render.marks.set(SOL.sim.castRange(card, cell));
      if (tip) {
        const op = card.sim.op;
        tip.textContent = op === 'land' ? '松手在这片虚空造陆'
          : op === 'law' ? '松手生效（律作用于全图）'
          : cell ? '松手放在 (' + cell.x + ', ' + cell.y + ')' : '要放在已有地块上';
      }
    } else {
      drag.mode = 'idle';
      g && g.classList.remove('rec');
      if (tip) tip.textContent = '拖到地面指定落点 · 向上随机 · 向下回收';
      SOL.render.cam.setHover(null);
      SOL.render.marks.clear();
    }
  }

  function endDrag(ev) {
    const d = drag; drag = null;
    document.removeEventListener('pointermove', onDrag);
    document.removeEventListener('pointerup', endDrag);
    if (d && d.cancel) {
      document.removeEventListener('pointercancel', d.cancel);
      window.removeEventListener('blur', d.cancel);
    }
    /* 用 querySelectorAll 而不是 getElementById：同 id 的残影可能不止一张 */
    document.querySelectorAll('#ghost').forEach(g => g.remove());
    SOL.render.cam.setHover(null);
    SOL.render.marks.clear();
    if (!d) return;
    if (d.mode === 'recycle') return UI.recycle();
    if (d.mode === 'cast') return UI.cast(d.i, d.cell, d.pt);
    if (d.mode === 'random') return UI.castRandom();
  }

  /* ── 落卡 ── */
  UI.cast = function (i, cell, pt) {
    const card = W.hand[i];
    if (!card) return;
    let x, y;
    const op = card.sim.op;
    if (op === 'law') { x = 0; y = 0; }
    else if (op === 'land') {
      /* 落在已有地块上就绕它长：原来是 cell.x + size + 1，地在右边三格外，
         与 ghost 上写的「松手在这片虚空造陆」不符（REVIEW-DEMO X-13）。 */
      if (cell) { x = cell.x; y = cell.y; }
      else if (pt) { x = pt.x; y = pt.y; }
      else return UI.toast('要指一个落点', '土地落在虚空上');
    } else if (op === 'sun') {
      x = 0; y = 0;
    } else {
      if (!cell) return UI.toast('要放在地块上', '虚空承不住这张卡');
      x = cell.x; y = cell.y;
    }

    const res = S.cmd.playSlot(i, x, y);
    if (!res.ok) { UI.toast('放不下', res.msg || ''); return; }

    UI.renderHand();
    UI.updateHud();
    UI.renderRight();
    SOL.render.marks.clear();
    /* 原式 '+'(gain||0) || '' 前半永远是真值串，于是每张无收益的卡都飘一个「+0」 */
    if (pt && res.gain) UI.flyTo(x, y, '+' + res.gain);
    if (op === 'land') setTimeout(() => SOL.render.cam.recenter(), 30);
  };

  UI.castRandom = function () {
    const card = W.hand[W.sel];
    if (!card) return;
    if (card.sim.op === 'land') {
      const cells = [...W.cells.values()];
      if (!cells.length) return UI.cast(W.sel, null, { x: 0, y: 0 });
      const c = cells[(Math.random() * cells.length) | 0];
      return UI.cast(W.sel, c, null);
    }
    if (card.sim.op === 'sun' || card.sim.op === 'law') return UI.cast(W.sel, null, null);
    const cells = [...W.cells.values()];
    if (!cells.length) return UI.toast('世界是空的', '先打一张「土地」');
    const c = cells[(Math.random() * cells.length) | 0];
    return UI.cast(W.sel, c, null);
  };

  UI.recycle = function () {
    const res = S.cmd.recycle(W.sel);
    if (!res.ok) return UI.toast('不能回收', res.msg);
    const at = res.at, id = res.id;
    UI.toast('回收', '+' + res.gain + ' 时素', {
      label: '撤销',
      fn() {
        const r = S.cmd.unrecycle(at, id);
        if (!r.ok) return UI.toast('收不回来', r.msg);
        UI.renderHand(); UI.updateHud(); UI.renderRight();
      }
    });
    UI.renderHand(); UI.updateHud(); UI.renderRight();
  };

  UI.reroll = function () {
    const res = S.cmd.reroll();
    if (!res.ok) return UI.toast('时素不够', res.msg);
    UI.renderHand(); UI.updateHud(); UI.renderRight();
    UI.log('重抽一手 −' + res.cost + ' 时素', true);
  };

  /* 世界坐标上的飘字（土地 / 钟声的收益反馈） */
  UI.flyTo = function (x, y, text) {
    if (!text) return;
    let el = document.getElementById('gainfly');
    if (!el) {
      el = document.createElement('div');
      el.id = 'gainfly';
      document.getElementById('stage').appendChild(el);
    }
    const p = SOL.render.cam.screenOf(x, y, 0.4);
    const stage = document.getElementById('stage').getBoundingClientRect();
    el.textContent = text;
    el.style.left = (p.x - stage.left) + 'px';
    el.style.top = (p.y - stage.top - 10) + 'px';
    el.style.opacity = 1;
    el.animate([{ transform: 'translateY(0)', opacity: 1 }, { transform: 'translateY(-44px)', opacity: 0 }],
      { duration: 900, easing: 'ease-out' });
    setTimeout(() => { el.style.opacity = 0; }, 900);
  };

  UI.bindHand = function () {
    const hand = document.getElementById('hand');
    hand.addEventListener('pointerdown', ev => {
      const slot = ev.target.closest('.handslot');
      if (!slot) return;
      beginDrag(ev, +slot.dataset.i);
    });
    hand.addEventListener('pointerover', ev => {
      const slot = ev.target.closest('.handslot');
      if (!slot || drag) return;
      const card = W.hand[+slot.dataset.i];
      if (card) UI.showTip(ev, card);
    });
    hand.addEventListener('pointerout', ev => {
      if (ev.target.closest('.handslot')) UI.hideTip();
    });

    document.getElementById('actbar').addEventListener('click', ev => {
      const b = ev.target.closest('[data-act]');
      if (!b) return;
      const a = b.dataset.act;
      if (a === 'random') UI.castRandom();
      else if (a === 'recycle') UI.recycle();
      else if (a === 'reroll') UI.reroll();
      else if (a === 'gear') { S.cmd.nextGear(); UI.syncGears(); UI.renderHand(); }
    });
  };

})(SOL.ui, SOL.sim, SOL.data);
