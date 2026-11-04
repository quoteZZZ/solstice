/* Solstice · 启动与键盘
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

  /* ── 格子游标：WASD / 方向键移动，Enter 在游标处施卡 ──
     相机可以绕着世界转，所以「按 W」不能直接当成「世界 −y」：
     先把屏幕上想去的方向投到世界 XZ，再吸到最近的 8 个格子方向之一。
     这样无论从哪个方位看，W 都是屏幕上的「向里」。 */
  const DIRS = {
    w: [0, -1], a: [-1, 0], s: [0, 1], d: [1, 0],
    ArrowUp: [0, -1], ArrowLeft: [-1, 0], ArrowDown: [0, 1], ArrowRight: [1, 0]
  };

  let cursor = null;   // {x, y}，键盘游标所在格；null = 还没动过键盘

  function screenToGrid(sx, sy) {
    const yaw = R.cam.yaw;
    const vx = Math.sin(yaw) * sx + Math.cos(yaw) * sy;
    const vy = -Math.cos(yaw) * sx + Math.sin(yaw) * sy;
    const k = Math.round(Math.atan2(vy, vx) / (Math.PI / 4));
    return [Math.round(Math.cos(k * Math.PI / 4)), Math.round(Math.sin(k * Math.PI / 4))];
  }

  /* 游标停在哪就画哪：高亮框 + 落点预览 + 该格读数，与鼠标悬停同一套 */
  function syncCursor(anchor) {
    const cell = S.at(cursor.x, cursor.y);
    const spot = cell || { x: cursor.x, y: cursor.y, h: 0.30 };
    R.cam.setHover(spot, true);
    R.marks.cursor(spot);
    const card = W.hand[W.sel];
    if (card) R.marks.set(S.castRange(card, spot));
    else R.marks.clear();
    /* 读数框固定在左上角，不跟着光标跑——键盘没有指针位置可跟 */
    UI.hoverInfo(cell, anchor || { clientX: 128, clientY: 96 });
    follow();
  }

  /* 游标走远就把镜头带过去。键盘没有「看着哪里」这层反馈，
     游标一旦走到手牌或右栏底下，屏幕上就什么都没发生。
     安全区按真实面板量：左栏右边、右栏左边、手牌上边——写死百分比会随窗口变。 */
  function follow() {
    const s = R.cam.screenOf(cursor.x, cursor.y, 0.32);
    const r = R.canvas.getBoundingClientRect();
    const rect = id => {
      const el = document.getElementById(id);
      if (!el) return null;
      const b = el.getBoundingClientRect();
      return b.width > 0 && b.height > 0 ? b : null;
    };
    const left = rect('leftrail'), right = rect('rightpanel'), hand = rect('handwrap');
    const x0 = left ? left.right + 24 : r.left + 60;
    const x1 = right ? right.left - 24 : r.right - 60;
    const y1 = hand ? hand.top - 16 : r.bottom - 40;
    if (s.x > x0 && s.x < x1 && s.y > r.top + 56 && s.y < y1) return;
    /* 走缓动而不是瞬移：键盘每按一下都要把镜头「拽」一次的话，
       连按 WASD 时画面是逐格抽搐的，看不出是自己走还是被打断。 */
    R.cam.glideTo(cursor.x, 0.22, cursor.y);
  }

  function moveCursor(sx, sy) {
    if (!cursor) cursor = { x: Math.round(R.cam.target.x), y: Math.round(R.cam.target.z) };
    const g = screenToGrid(sx, sy);
    cursor.x += g[0]; cursor.y += g[1];
    syncCursor();
  }

  function onKey(ev) {
    if (UI.captureKey(ev)) { ev.preventDefault(); return; }

    /* 拖拽进行中只认 Esc（取消）。原来空格/X/R/数字都能穿进来：
       空格再放一张、松手又放一张，一次手势最多烧 3 张牌（第二轮 S08）。 */
    if (UI.isDragging && UI.isDragging() && ev.key !== 'Escape') {
      ev.preventDefault();
      return;
    }
    /* 面板开着的时候，世界键（空格/X/R/J/1–8/WASD/Enter/F1–F4/Z/P/E/G/H/K）不该穿到背后去：
       原来面板里按空格会直接在世界里放一张牌（第二轮 S07 / K02）。 */
    if (UI.state.panel || UI.state.event) {
      const k = ev.key;
      const ae = document.activeElement;
      const inPanel = !!(ae && ae.closest && ae.closest('#panel, #eventcard'));
      /* 面板开关键不要手写：原来只放行 b/l/c/o，K 和 H 就成了死键（第二轮 S26）。
         直接查 UI.KEYS 里 group === '面板' 的那几条，键位改了也不用改这里。 */
      const isPanelKey = UI.KEYS.some(x => x.group === '面板' && x.key.length === 1 &&
        x.key.toLowerCase() === k.toLowerCase());
      /* 焦点在面板里时，Enter / Space 交给原生按钮，并且**必须直接 return**：
         只「放行」不返回的话，代码会继续走到下面的空格分支——空格被当成「随机释放」
         打一张牌，同时 preventDefault 又吃掉了按钮的 keyup 激活（第二轮 S25 补）。
         实测后果：焦点在「图鉴」页签上按空格，页签没切，世界里的太阳 +1。 */
      if (inPanel && (k === 'Enter' || k === ' ')) return;
      /* 事件是模态：它开着的时候不许再开面板（原来按 L 会把卡库叠在事件遮罩后面，
         Esc 只能一层层剥，玩家会以为自己卡住了）。事件自己用 Esc 关，或用三个选项回答。 */
      const pass = k === 'Escape' || k === 'Tab' || (!UI.state.event && isPanelKey);
      if (!pass) { ev.preventDefault(); return; }
    }

    /* 焦点在输入框里：除了 Esc 交出焦点，其余一律放给输入框。
       否则在种子输入框里打字会顺手触发施卡 / 开面板（REVIEW-DEMO X-7）。 */
    const t = ev.target;
    if (t && (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.isContentEditable)) {
      if (ev.key === 'Escape') t.blur();
      return;
    }

    /* div 做的按钮不会自己响应 Enter / 空格，这里补上——不然键盘用户 Tab 到了也按不动 */
    const ae = document.activeElement;
    if ((ev.key === 'Enter' || ev.key === ' ') && ae && ae.dataset &&
        (ae.dataset.act || ae.dataset.panel)) {
      ev.preventDefault(); ae.click(); return;
    }

    if (ev.key === 'Escape') {
      if (UI.state.event) return UI.closeEvent();
      if (UI.state.panel) return UI.closePanel();
      if (document.getElementById('creator').classList.contains('on')) {
        /* 已开局才允许退——第一次掷骰不能退（X-2） */
        if (UI.creator.fromWorld) UI.closeCreator();
        return;
      }
      if (UI.state.uiMode !== '全部') return UI.setUiMode('全部');
      return;
    }

    /* 创世页只认 Enter；走和按钮同一条路（已有世界时要先武装，不会再裸清空一次） */
    if (document.getElementById('creator').classList.contains('on')) {
      if (ev.key === 'Enter') UI.enterFromCreator();
      return;
    }

    /* Tab 归还浏览器：原先被钉死成「显隐 UI」，键盘用户从此再也到不了任何按钮（X-7）。
       摆件模式本来就有 P，键位表里写的也是 P，这条劫持一直是暗绑。 */
    if (ev.key === 'F1') return R.cam.setPitch(0);
    if (ev.key === 'F2') return R.cam.setPitch(1);
    if (ev.key === 'F3') return R.cam.setPitch(2);
    if (ev.key === 'F4') return R.cam.setPitch(3);
    if (/^[1-8]$/.test(ev.key)) {
      const i = Math.min(+ev.key - 1, W.hand.length - 1);
      if (i >= 0) UI.select(i);
      return;
    }

    /* 游标：WASD 与方向键同一套。移动之后按 Enter 就把选中的卡打在游标那一格，
       不用鼠标也能走完「选卡 → 指格 → 落卡」整条路（REVIEW-DEMO F-3 / 排期 8）。
       面板/事件开着的时候不动游标——世界被盖住了，动了也看不见。 */
    const dir = DIRS[ev.key] || DIRS[String(ev.key).toLowerCase()];
    const blocked = !!(UI.state.panel || UI.state.event);
    if (dir && !blocked) { ev.preventDefault(); return moveCursor(dir[0], dir[1]); }
    if (ev.key === 'Enter' && cursor && !blocked) {
      ev.preventDefault();
      return UI.cast(W.sel, S.at(cursor.x, cursor.y), cursor);
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
      onHover: (cell, ev, pt) => {
        UI.hoverInfo(cell, ev);
        /* 鼠标扫过哪一格，游标就跟到哪一格：这样「指到哪、Enter 打哪」两种输入是同一条路 */
        if (cell) cursor = { x: cell.x, y: cell.y };
        R.marks.cursor(null);
        /* 落点预览：选中一张卡后，鼠标经过世界就把「这张卡会动到哪些格」画出来。
           这个游戏的主要施卡手势是「点世界」而不是「拖过去」（cam.onClick → UI.cast），
           所以预览必须挂在 hover 上，挂在 drag 上等于只覆盖一半（REVIEW-DEMO R-1 / #11）。 */
        const card = W.hand[W.sel];
        if (card && cell) R.marks.set(S.castRange(card, cell));
        else if (card && pt) {
          /* 虚空落点：没有格子可拾取，就按平面点算范围（土地开局的 5×5 预览） */
          const fake = { x: Math.round(pt.x), y: Math.round(pt.y), h: 0.30 };
          R.marks.set(S.castRange(card, fake));
        } else R.marks.clear();
      }
    });

    /* 手牌重排的 resize 监听放在 ui/hand.js —— 它和 renderHand 同作用域，
       不依赖 boot 的执行顺序（这里原先有一份，实测不生效）。 */

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
    let lastEss = -1;
    /* 主循环里任何一处抛异常，requestAnimationFrame 就再也不排下一帧了——
       画面停住、时钟停住，而屏幕上没有任何提示：玩家只会觉得「卡死了」。
       一圈一次 try：这帧出错就记一条、跳过这一帧的画，下一帧照排。
       连续错满 60 帧（约 1 秒）才真的停下，免得台上滚屏刷成一片。 */
    let failStreak = 0;
    function frame(now) {
      const dt = Math.min(80, now - last);
      last = now;
      try {
        /* 时素按真实时间涨，和世界档位无关（SETTING §3.1）：暂停时也要涨，
           否则「停下来想想」就是纯亏。世界那边照旧走 advance。 */
        S.cmd.essenceTick(dt);
        const ticks = S.cmd.advance(dt);
        if (W.essence !== lastEss) { lastEss = W.essence; UI.updateHud(); }
        R.frame(now, ticks);
        failStreak = 0;
      } catch (e) {
        failStreak++;
        if (failStreak === 1) {
          console.error('[frame]', e);
          try { UI.toast('画面出错', '世界仍在推进，可继续操作'); } catch (_) {}
        }
        if (failStreak > 60) {
          console.error('[frame] 连续 60 帧失败，主循环停下', e);
          return;
        }
      }
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
