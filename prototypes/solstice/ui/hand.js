/* 至日 · 手牌与拖放
   一套轴三种结果：拖到地面 = 指定落点；向上拖 / 空格 = 随机落点；向下拖 / X = 回收。
   落点换算只走 render.cam（射线），UI 不碰投影。 */
(function (UI, S, D) {
  const W = S.W;
  let drag = null;

  UI.renderHand = function () {
    const box = document.getElementById('hand');
    if (!box) return;
    const n = W.hand.length, mid = (n - 1) / 2;
    const sp = n > 6 ? 116 : 128;
    box.innerHTML = W.hand.map((card, i) => {
      const d = i - mid;
      const ang = (d * 2.4).toFixed(2);
      const lift = Math.round(Math.abs(d) * Math.abs(d) * 2.2);
      const z = (i === W.sel ? 200 : 100 - Math.round(Math.abs(d) * 3));
      return `<div class="handslot ${i === W.sel ? 'sel' : ''}" data-i="${i}"
        style="left:calc(50% + ${(d * sp).toFixed(1)}px - 78px);bottom:${6 + lift}px;z-index:${z}">
        <div class="cardwrap" style="transform:rotate(${ang}deg)">
          ${UI.cardFace(card, 'h' + i)}
        </div>
      </div>`;
    }).join('');

    const deck = document.getElementById('deck');
    deck.innerHTML = `<div class="dk" title="手牌 / 上限"><b>${W.hand.length}</b><span>手</span></div>
      <div class="dk" title="卡池总数（目标 540，已填 ${D.total}）"><b>${D.total}</b><span>池</span></div>`;

    const act = document.getElementById('actbar');
    act.innerHTML = `
      <div class="ab" data-act="random" title="随机落点释放（空格）">✦</div>
      <div class="ab" data-act="recycle" title="回收选中的卡（X）">↩</div>
      <div class="ab" data-act="reroll" title="重抽一手（5 时素 · R）">↻</div>
      <div class="ab" data-act="gear" title="档位（J）" style="font-size:11px">${S.GEARS[S.gear].label}</div>`;
  };

  UI.select = function (i) {
    W.sel = i;
    UI.renderHand();
  };

  /* ── 拖放 ── */
  function beginDrag(ev, i) {
    const card = W.hand[i];
    if (!card) return;
    ev.preventDefault();
    UI.select(i);
    drag = { i: i, x0: ev.clientX, y0: ev.clientY, moved: false, mode: 'idle', cell: null, pt: null };

    const wrap = document.getElementById('ghostwrap');
    const g = document.createElement('div');
    g.id = 'ghost';
    g.innerHTML = UI.cardFace(card, 'g') + '<div class="tip">拖到地面指定落点 · 向上随机 · 向下回收</div>';
    wrap.appendChild(g);
    moveGhost(ev.clientX, ev.clientY);

    document.addEventListener('pointermove', onDrag);
    document.addEventListener('pointerup', endDrag, { once: true });
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
    } else if (dy < -40 && drag.moved) {
      drag.mode = 'random';
      g && g.classList.remove('rec');
      if (tip) tip.textContent = '松手释放（随机落点）';
      SOL.render.cam.setHover(null);
    } else if (overCanvas && drag.moved) {
      drag.mode = 'cast';
      g && g.classList.remove('rec');
      const cell = SOL.render.cam.pickAt(ev.clientX, ev.clientY);
      const pt = cell ? { x: cell.x, y: cell.y } : SOL.render.cam.planeAt(ev.clientX, ev.clientY);
      drag.cell = cell; drag.pt = pt;
      SOL.render.cam.setHover(cell);
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
    }
  }

  function endDrag(ev) {
    document.removeEventListener('pointermove', onDrag);
    const g = document.getElementById('ghost');
    if (g) g.remove();
    SOL.render.cam.setHover(null);
    const d = drag; drag = null;
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
      const size = card.sim.size == null ? 2 : card.sim.size;
      if (cell) { x = cell.x + size + 1; y = cell.y; }
      else if (pt) { x = pt.x; y = pt.y; }
      else return UI.toast('要指一个落点', '土地落在虚空上');
    } else if (op === 'sun') {
      if (!W.cells.size) { x = 0; y = 0; } else { x = 0; y = 0; }
    } else {
      if (!cell) return UI.toast('要放在地块上', '虚空承不住这张卡');
      x = cell.x; y = cell.y;
    }

    const res = S.cast(card, x, y);
    if (!res.ok) { UI.toast('放不下', res.msg || ''); return; }

    S.consume(i);
    W.sel = Math.min(W.sel, Math.max(0, W.hand.length - 1));
    UI.renderHand();
    UI.updateHud();
    UI.renderRight();
    if (pt) UI.flyTo(x, y, '+' + (res.gain || 0) || '');
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
    const res = S.recycle(W.sel);
    if (!res.ok) return UI.toast('不能回收', res.msg);
    UI.toast('回收', '+' + res.gain + ' 时素');
    UI.renderHand(); UI.updateHud(); UI.renderRight();
  };

  UI.reroll = function () {
    if (W.essence < 5) return UI.toast('时素不够', '重抽要 5 时素');
    W.essence -= 5;
    S.dealHand();
    UI.renderHand(); UI.updateHud(); UI.renderRight();
    UI.log('重抽一手 −5 时素', true);
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
      else if (a === 'gear') { S.nextGear(); UI.syncGears(); UI.renderHand(); }
    });
  };

})(SOL.ui, SOL.sim, SOL.data);
