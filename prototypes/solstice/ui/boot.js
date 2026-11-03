/* 至日 · 启动与键盘
   这里是唯一把 sim / render / ui 接在一起的地方（三层互不 import）。 */
(function (UI, S, D, R) {
  const W = S.W;

  UI.state.uiMode = '全部';

  UI.setUiMode = function (mode) {
    UI.state.uiMode = mode;
    const st = document.getElementById('stage');
    st.classList.toggle('hide-rail', mode === '仅右栏' || mode === '全隐');
    st.classList.toggle('hide-right', mode === '全隐');
    st.classList.toggle('ambient', mode === '全隐');
  };

  UI.toggleAmbient = function () {
    UI.setUiMode(UI.state.uiMode === '全隐' ? '全部' : '全隐');
    UI.renderOverlay();
  };

  /* ── 键盘 ── */
  function actOf(ev) {
    if (ev.shiftKey && (ev.key === 'Z' || ev.key === 'z')) return 'zoomOut';
    const k = ev.key;
    for (const b of UI.KEYS) {
      if (b.key === k) return b.act;
      if (b.key.length === 1 && b.key.toLowerCase() === String(k).toLowerCase()) return b.act;
    }
    return null;
  }

  function onKey(ev) {
    if (UI.captureKey(ev)) { ev.preventDefault(); return; }

    if (ev.key === 'Escape') {
      if (UI.state.event) return UI.closeEvent();
      if (UI.state.panel) return UI.closePanel();
      if (document.getElementById('creator').classList.contains('on')) return;
      if (UI.state.uiMode !== '全部') return UI.setUiMode('全部');
      return;
    }

    /* 创世页只认 Enter */
    if (document.getElementById('creator').classList.contains('on')) {
      if (ev.key === 'Enter') UI.enterWorld();
      return;
    }

    if (ev.key === 'Tab') { ev.preventDefault(); return UI.toggleAmbient(); }
    if (ev.key === 'F1') return R.cam.setPitch(0);
    if (ev.key === 'F2') return R.cam.setPitch(1);
    if (ev.key === 'F3') return R.cam.setPitch(2);
    if (ev.key === 'F4') return R.cam.setPitch(3);
    if (/^[1-8]$/.test(ev.key)) {
      const i = Math.min(+ev.key - 1, W.hand.length - 1);
      if (i >= 0) UI.select(i);
      return;
    }

    const act = actOf(ev);
    if (!act) return;
    if (act === 'random') { ev.preventDefault(); return UI.castRandom(); }
    if (act === 'recycle') return UI.recycle();
    if (act === 'reroll') return UI.reroll();
    if (act === 'gear') { S.cmd.nextGear(); UI.syncGears(); UI.renderHand(); return; }
    if (act === 'ambient') return UI.toggleAmbient();
    if (act === 'event') return UI.fireEvent();
    if (act === 'zoomIn') return R.cam.nudgeZoom(1);
    if (act === 'zoomOut') return R.cam.nudgeZoom(-1);
    if (act === 'pack') return UI.openPanel('pack');
    if (act === 'codex') return UI.openPanel('codex');
    if (act === 'chron') return UI.openPanel('chron');
    if (act === 'creator') return UI.openPanel('creator');
    if (act === 'settings') return UI.openPanel('settings');
    if (act === 'keys') return UI.openPanel('keys');
    if (act === 'help') return UI.openPanel('help');
  }

  /* ── 启动 ── */
  function boot() {
    R.init();
    R.cam.init();
    UI.initHud();
    UI.bindHand();
    UI.bindOverlay();

    R.cam.attach({
      onClick: (cell, pt) => {
        if (UI.state.panel || UI.state.event) return;
        if (document.getElementById('creator').classList.contains('on')) return;
        if (W.sel >= 0) UI.cast(W.sel, cell, pt);
      },
      onHover: (cell, ev) => UI.hoverInfo(cell, ev)
    });

    S.on('tick', () => {
      UI.updateHud();
      if (UI.state.panel === 'chron') UI.renderOverlay();
      UI.renderRight();
    });

    document.addEventListener('keydown', onKey);

    /* 创世先来：世界第一次必须由一次掷骰开始 */
    S.cmd.gear(0);
    UI.openCreator();
    UI.updateHud();

    /* 主循环归这一层：装配层拥有时钟（TECH.md §3.1）。render 不再自己 requestAnimationFrame，
       只被叫来画一帧。换前端 / 做回放 / 加存档，都只需要替换这里的这一个循环。 */
    let last = performance.now();
    function frame(now) {
      const dt = Math.min(80, now - last);
      last = now;
      const ticks = S.cmd.advance(dt);
      R.frame(now, ticks);
      requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);

    /* 调试口：给自动化测试用，不给玩家 */
    window.__dbg = {
      W: W, S: S, D: D, R: R, UI: UI,
      play(id, x, y) {
        const c = D.byId.get(id);
        if (!c) return { ok: false, msg: 'no card ' + id };
        const res = S.cmd.cast(c, x, y);
        if (res.ok) { UI.renderHand(); UI.updateHud(); UI.renderRight(); }
        return res;
      },
      tick(n) { S.cmd.tick(n); UI.updateHud(); UI.renderRight(); },
      digest: S.digest,
      seen: () => [...W.seen]
    };
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
  else boot();

})(SOL.ui, SOL.sim, SOL.data, SOL.render);
