/* Solstice · 覆盖层面板：抽卡 / 卡库 / 年表 / 设置 / 键位 / 帮助
   所有读数都来自 sim 与卡表；面板里不写死任何「总数 252」之类的数字。 */
(function (UI, S, D) {
  const W = S.W;
  const U = UI.state = {
    panel: null,
    codexTab: 'cards', codexEl: '水', codexSel: null, codexOwn: false, codexMore: false,
    dexGrp: '全部',
    chronFilter: '全部',
    setGroup: 'video'
  };

  UI.openPanel = function (p) {
    /* 世界里攒下的提示属于世界上下文：面板一开它们会被 z-index 抬到面板之上，
       横在标题与步骤条之间（UI 评审 P40）。开面板时清掉，面板内的提示另走 #phead。 */
    const stack0 = document.getElementById('toasts');
    if (stack0) stack0.innerHTML = '';
    if (p === 'creator') { UI.openCreator(); return; }
    /* 键位与操作速查不再是独立面板，并进「设置 → 操作」：
       左栏因此只留 年表 / 卡库 / 换界 / 设置 / 摆件 五项（REVIEW-DEMO #12）。 */
    if (p === 'keys') { p = 'settings'; U.setGroup = 'keys'; }
    if (p === 'help') { p = 'settings'; U.setGroup = 'input'; }
    if (!['pack', 'codex', 'chron', 'settings'].includes(p)) return;
    U.panel = p;
    /* 已经撕开、还没挑的那一包不能被「关掉再打开」洗掉：
       时素在撕开的那一刻就扣了，重开面板把 packStage 归 0 等于把包和钱一起吞了。
       只有在没有待挑的牌时才回到封口状态。 */
    if (p === 'pack' && !(UI.packCards && UI.packCards.length)) UI.packStage = 0;
    waitingFor = null;   // 换面板 / 关面板 = 放弃改绑（第二轮 S04）
    UI.renderOverlay();
    /* 原来这里往世界 log 里写一条「打开：年表」。世界 log 是「刚刚世界上发生了什么」，
       开面板是玩家自己的操作，写进去会把真实事件挤掉（UI 评审 E13 / U13）。
       开没开面板，面板本身就是反馈。 */
  };

  UI.closePanel = function () {
    /* 改绑等待态只在 captureKey 里清，面板一关就变成「下一次按任何键都静默改键」，
       W 会被绑给「回收选中」而 WASD 又抢回去，等于造出一个死键（第二轮 S04）。 */
    waitingFor = null;
    U.panel = null;
    UI.renderOverlay();
  };

  UI.renderOverlay = function () {
    const box = document.getElementById('overlay');
    if (!box) return;
    box.classList.toggle('show', !!U.panel);
    box.innerHTML = U.panel ? panelHTML() : '';
    document.querySelectorAll('#leftrail .rail').forEach(r =>
      r.classList.toggle('on', r.dataset.panel === U.panel));
  };

  function panelHTML() {
    const pk = S.packOf(UI.packSel);
    const title = {
      pack: '抽卡 · ' + pk.name + '（' + S.PACK_SIZE + ' 张 / ' + pk.cost + ' 时素）',
      codex: U.codexTab === 'ach'
        ? '图鉴 · 已点亮 ' + S.codexCount() + ' / ' + S.CODEX.length
        : '卡库 · 已见 ' + W.seen.size + ' / ' + D.total,
      chron: '年表 · ' + W.chron.length + ' 条',
      settings: '设置'
    }[U.panel];
    return `<div id="ovbg" data-close="1"></div>
      <div id="panel">
        <div id="phead"><span class="sm">${title}</span>
          <span class="close xs" data-close="1">Esc ✕</span></div>
        <div id="pbody">${bodyFor(U.panel)}</div>
        <div id="pfoot">${footFor(U.panel)}</div>
      </div>`;
  }

  function footFor(p) {
    if (p === 'pack') {
      const pk = S.packOf(UI.packSel);
      const open = S.packOpen(pk), afford = W.essence >= pk.cost;
      /* 第三阶段是「必须挑一张」，主按钮原来却写「不必挑」——语义正好相反（UI 评审 P03）。
         它是「整包不要了」的退路，所以降到 ghost，主按钮位留给卡本身。 */
      const skip = UI.packStage >= 2;
      const label = UI.packStage === 0 ? '撕开 · −' + pk.cost + ' 时素' : '跳过这一包';
      return `<button class="btn${skip ? ' ghost' : ''}" data-act="packNext"${UI.packStage === 0 && (!open || !afford) ? ' disabled' : ''}>${label}</button>
        <button class="btn ghost" data-close="1">关闭</button>
        <span class="xs" style="margin-left:auto">时素 ${W.essence} · 每 10 秒 +1</span>`;
    }
    return `<button class="btn ghost" data-close="1">关闭</button>`;
  }

  function bodyFor(p) {
    if (p === 'pack') return packBody();
    if (p === 'codex') return codexPanel();
    if (p === 'chron') return chronBody();
    if (p === 'settings') return settingsBody();
    return '';
  }

  /* ── 抽卡 ── */
  UI.packStage = 0;
  UI.packCards = [];
  UI.packSel = 'basic';

  /* 封印状态的那一张：一包多少钱、现在有多少，用一条横条说，
     而不是在空荡荡的框中央写一句「时素不够 · 还要 28」（REVIEW-DEMO C-4）。 */
  function sealFace() {
    const p = S.packOf(UI.packSel);
    const open = S.packOpen(p);
    const pct = Math.max(0, Math.min(100, W.essence / p.cost * 100));
    const ok = open && W.essence >= p.cost;
    return `<div class="packseal${ok ? ' ok' : ''}${open ? '' : ' lock'}">
      <div class="psName">${p.name}</div>
      <div class="psSize">${S.PACK_SIZE} 张 · ${p.lo}–${p.hi} 环</div>
      <div class="psSize">池 ${S.poolIn(p).length} / ${S.poolTotal()}</div>
      <div class="psEss"><i style="width:${pct.toFixed(1)}%"></i></div>
      <div class="psNum"><b>${W.essence}</b> / ${p.cost}</div>
    </div>`;
  }

  /* 包种列表：价格、环区间、还差几条图鉴——一行说完，不写句子 */
  function packRows() {
    return S.packList().map(p => {
      const open = S.packOpen(p);
      const on = p.id === UI.packSel;
      const n = S.poolIn(p).length;
      return `<button class="packrow${on ? ' on' : ''}${open ? '' : ' lock'}" data-pack="${p.id}">
        <span class="prn">${p.name}</span>
        <span class="prc mono">${p.cost}</span>
        <span class="prr mono">${p.lo}–${p.hi} 环 · ${n} 张</span>
        ${open ? '' : `<span class="prl mono">图鉴 ${S.codexLit()} / ${p.need}</span>`}
      </button>`;
    }).join('');
  }

  function packBody() {
    const got = UI.packCards || [];
    const full = W.hand.length >= S.HAND_MAX;
    const swap = full ? W.hand[Math.max(0, Math.min(W.hand.length - 1, W.sel))] : null;
    return `<div style="display:flex;gap:var(--s-6);height:100%">
      <div style="width:230px;flex:0 0 230px;border-right:1px solid var(--line-1);padding-right:var(--s-5)">
        <div class="xs" style="margin-bottom:8px">包种</div>
        ${packRows()}
        <div class="mono xs" style="margin-top:10px;color:var(--ink-4)">池 ${S.drawPool().length} / ${S.poolTotal()} · ${S.PACK_SIZE} 张 / 包</div>
        ${swap ? `<div class="xs" style="margin-top:12px;color:var(--ink-3)">手牌满 · 收下的那张换掉
          <span class="mono" style="color:var(--ink-1)">${swap.name.zh}</span></div>` : ''}
      </div>
      <div style="flex:1;min-width:0;display:flex;flex-direction:column">
        <div class="row gap6" style="margin:0 auto var(--s-4);max-width:720px;width:100%">
          ${['封印', '挑选'].map((t, i) =>
            `<span class="tag ${(i === 0 ? UI.packStage === 0 : UI.packStage >= 2) ? 'a' : ''}">${i + 1}. ${t}</span>`).join('')}
          <span class="xs" style="margin-left:auto;color:var(--ink-3)">手牌 ${W.hand.length} / ${S.HAND_MAX}</span>
        </div>
        <div class="packframe" style="flex:1">
          ${UI.packStage === 0 ? sealFace() : ''}
          ${UI.packStage >= 2 ? `<div style="display:flex;gap:10px;align-items:flex-end;flex-wrap:wrap;justify-content:center">
            ${got.map((c, i) => `<div class="packpick" data-act="packPick" data-i="${i}">
              ${UI.cardFace(c, 'p' + i, 'p')}</div>`).join('')}</div>` : ''}
        </div>
      </div>
    </div>`;
  }

  function packNext() {
    if (UI.packStage === 0) {
      const res = S.cmd.drawPack(UI.packSel);
      if (!res.ok) return UI.toast('抽不了', res.msg);
      UI.packCards = res.got;
      /* 撕开就直接见牌。原来中间还有一站「翻牌」，画面上是 6 张**完全空白的卡背**
         （`.dealcard` 里没有内容、没有点击），玩家看不出那是牌，只能再按一次按钮
         （UI 评审 P04 / F26 / §1.2）。这一站没承载任何信息，并进挑选。 */
      UI.packStage = 2;
      UI.toast('撕开「' + res.pack.name + '」', '−' + res.pack.cost + ' 时素');
    } else {
      UI.packStage = 0;
      UI.packCards = [];
      U.panel = null;
    }
    UI.renderHand(); UI.updateHud(); UI.renderRight(); UI.renderOverlay();
  }

  /* 从摊开的包里挑一张：手牌没满就进手牌，满了就换掉选中的那张。
     挑完即收摊——一包只取一张，剩下的说明「不必挑」，不留残局。 */
  function packPick(i) {
    const c = (UI.packCards || [])[i];
    if (!c) return;
    const res = S.cmd.takeFromPack(c);
    if (!res.ok) return UI.toast('收不下', res.msg);
    UI.toast(res.added ? '收下「' + c.name.zh + '」' : '换入「' + c.name.zh + '」',
      res.added ? '' : '换下「' + res.replaced + '」');
    UI.packStage = 0;
    UI.packCards = [];
    U.panel = null;
    UI.renderHand(); UI.updateHud(); UI.renderRight(); UI.renderOverlay();
  }

  /* ── 卡库 / 图鉴：两面收藏墙，一面收卡一面收世界状态 ── */
  UI.openCard = function (id) {
    const c = D.byId.get(id);
    if (!c) return;
    U.panel = 'codex';
    U.codexTab = 'cards';
    U.codexEl = c.primary;
    U.codexSel = c.id;
    UI.renderOverlay();
  };

  function codexPanel() {
    const tabs = [
      ['cards', '卡 ' + W.seen.size + '/' + D.total],
      ['ach', '图鉴 ' + S.codexCount() + '/' + S.CODEX.length]
    ];
    return `<div style="display:flex;flex-direction:column;height:100%">
      <div class="row gap6" style="margin-bottom:11px">
        ${tabs.map(([k, t]) => `<button class="btn ${U.codexTab === k ? 'on' : 'ghost'}"
          data-ctab="${k}" style="height:24px">${t}</button>`).join('')}
      </div>
      <div style="flex:1;min-height:0;overflow:hidden">
        ${U.codexTab === 'ach' ? dexBody() : codexBody()}</div>
    </div>`;
  }

  function dexBody() {
    const grp = U.dexGrp;
    const list = grp === '全部' ? S.CODEX : S.codexOf(grp);
    const litIn = g => S.codexOf(g).filter(a => S.codex.on.has(a.id)).length;
    const nav = [['全部', S.CODEX.length, S.codexCount()]]
      .concat(S.CODEX_GROUPS.map(g => [g, S.codexOf(g).length, litIn(g)]));
    return `<div style="display:flex;gap:14px;height:100%;overflow:hidden">
      <div class="codexEl">
        ${nav.map(([g, all, lit]) => `<button class="btn ${g === grp ? 'on' : 'ghost'}" data-dg="${g}"
          style="height:42px;flex-direction:column;gap:0"><span>${g}</span>
          <span style="font-size:11px;color:var(--ink4)">${lit} / ${all}</span></button>`).join('')}
      </div>
      <div style="flex:1;min-width:0;overflow:auto">
        <div class="row gap8" style="margin-bottom:10px;align-items:center">
          <span class="mono xs" style="color:#8b99a6">已点亮 ${S.codexCount()} / ${S.CODEX.length}</span>
        </div>
        <div class="axWall">${list.map(a => dexCell(a)).join('')}</div>
      </div>
    </div>`;
  }

  function dexCell(a) {
    const lit = S.codex.on.has(a.id);
    const at = S.codex.at[a.id];
    /* need 型条目 sim 里逐月数着（A.st），界面原来只写「未点亮」——
       玩家凑「长夜 · 连续 12 月」时完全不知道自己到第几个月。 */
    const st = lit ? a.need : (S.codex.st[a.id] || 0);
    const pct = a.need ? Math.min(100, st / a.need * 100) : 0;
    return `<div class="axc ${lit ? 'on' : ''}">
      <div class="axh"><span>${a.grp}</span>
        <span class="axb">${a.need ? '连续 ' + a.need + ' 月' : '当月'}</span></div>
      <div class="axn">${a.name}</div>
      <div class="axj">${a.hint}</div>
      ${a.need ? `<div class="axp"><i style="width:${pct.toFixed(1)}%"></i></div>` : ''}
      <div class="axf">${lit ? UI.mon(at.month) + ' · ' + at.world
        : a.need ? st + ' / ' + a.need + ' 月' : '未点亮'}</div>
    </div>`;
  }

  function codexBody() {
    const els = D.elements.order;
    const el = U.codexEl;
    /* 详情列默认落在当前元素的头一张上：空着一整列去写「点左侧任意卡格」，
       既占了 400px 又只是在教人怎么点（守「界面不写说明句」）。 */
    const sel = (U.codexSel && D.byId.get(U.codexSel)) || D.cards.find(x => x.primary === el) || null;
    const seenIn = e => D.cards.filter(c => c.primary === e && W.seen.has(c.id)).length;
    const totalIn = e => D.cards.filter(c => c.primary === e).length;

    /* 卡库是「收藏墙」，不是环级矩阵。矩阵 11 环 × 3 类 = 33 格，一个元素只有 2–8 张卡，
       撑满 600px 宽就是一大片空格；按环高 74 又装不下 11 行。改成流动卡格：
       只画真有的卡，按可用宽度自动铺 4–5 列，空格自然消失（C-4 的同一句话）。 */
    const grid = `<div class="cgWall">${D.cards.filter(c => c.primary === el).map((c, i) => {
      const seen = W.seen.has(c.id);
      if (U.codexOwn && !seen) return '';
      /* 没有一档买得到的包覆盖这一环 = 图鉴还没点亮到那儿，虚框且不可点开。 */
      const locked = !S.tierOpen(c.tier);
      return `<div class="cgt${seen ? ' has' : ''}${locked ? ' lock' : ''}" data-card="${c.id}"
        title="${c.name.zh}${locked ? ' · 未解锁' : seen ? '' : ' · 未抽到'}">
        <div class="cgtArt">${UI.artSVG(c, 'cg' + i)}</div>
        <div class="cgtName">${seen ? c.name.zh : '·'}</div>
        <div class="cgtMeta mono"><span>${c.tier === 10 ? '超' : c.tier}</span>环 · ${D.clsName[c.class]}</div>
      </div>`;
    }).join('')}</div>`;

    return `<div style="display:flex;gap:14px;height:100%;overflow:hidden">
      <div class="codexEl">
        ${els.map(e => `<button class="btn ${e === el ? 'on' : 'ghost'}" data-el="${e}"
          style="height:42px;flex-direction:column;gap:0"><span>${e}</span>
          <span style="font-size:11px;color:var(--ink4)">${seenIn(e)} / ${totalIn(e)}</span></button>`).join('')}
      </div>
      <div style="flex:1;min-width:0;overflow:auto">
        <div class="row gap8" style="margin-bottom:9px;align-items:center">
          <span class="mono xs" style="color:#8b99a6">已见 ${W.seen.size} / ${D.total}</span>
          <button class="btn sm ${U.codexOwn ? 'on' : 'ghost'}" data-act="codexOwn">只看已见</button>
          <!-- 「池 50 / 53」原来和「已见 15 / 55」并排：两个总数（55 与 53）差 2 张，
               界面上没有任何地方解释为什么，读者只会以为其中一个错了（UI 评审 P11）。
               牌池是「抽卡」这件事的数字，放回抽卡面板；卡库只留一个分母。 -->
        </div>
        ${grid}
      </div>
      <div class="cdscroll" style="width:400px;border-left:1px solid var(--line);padding-left:16px;overflow:auto">
        ${sel ? codexDetail(sel) : ''}
      </div>
    </div>`;
  }

  /* 关系链的键名只有五个，全部来自 data/*.js 的 rel[].k */
  const REL_LABEL = { sym: '共生', foe: '相冲', food: '食物', req: '需要', where: '落点' };

  function codexDetail(c) {
    return `<div style="display:flex;flex-direction:column;align-items:center">
        <div style="transform:scale(.86);transform-origin:top center;height:384px">${UI.cardFace(c, 'cx')}</div>
      </div>
      <div style="display:flex;justify-content:space-between;margin-top:2px;padding:9px 2px;
        border-top:1px solid var(--line-1);border-bottom:1px solid var(--line-1)">
        ${(() => {
          /* 55 张卡里 disturb / cooldown 一张没填，界面却照着三格排版，
             于是每个玩家都会看到「未定」「—」这两个占位符，还放大成主视觉（UI 评审 P20）。
             有值才出行；一格时居中，不硬撑三栏。 */
          const cells = [['◔', '半径', c.radius_w.toFixed(3) + ' W']];
          if (c.disturb != null) cells.push(['⚠', '扰动', '+' + c.disturb]);
          if (c.reclaim) cells.push(['↩', '回收', '+' + c.reclaim]);
          return cells;
        })()
          .map(([i, k, v]) => `<div style="text-align:center;flex:1">
            <div style="font-size:14px;color:var(--acc)">${i}</div>
            <div class="mono" style="font-size:12px;color:#e8eef4">${v}</div>
            <div class="xs" style="color:var(--ink4)">${k}</div></div>`).join('')}
      </div>
      <div class="dex" style="margin-top:10px">
        <div class="mono xs" style="color:#5a6674;letter-spacing:.1em">
          ${c.title ? c.title + ' · ' : ''}${c.elements.join(' / ')} · ${c.tier === 10 ? '超环' : c.tier + ' 环'}</div>
        ${UI.statsBlock(c)}
        ${(c.lore || []).map(l => `<div class="lore"><b>${l.k === 'habit' ? '习性' : l.k === 'behavior' ? '行为' : '传说'}</b>
          <span>${l.t}</span></div>`).join('')}
        ${(c.rel || []).length ? `<div class="rel">${c.rel.map(r => {
          /* 原来直接把 r.k 印出来，玩家看到的是「sym 产氧」「req 湿地」——数据里的键名（UI 评审 §1.5 / F32）。
             note 字段一直有数据却从没渲染过，一并补上。 */
          const lab = REL_LABEL[r.k];
          const nm = (D.byId.get(r.to) || { name: { zh: '?' } }).name.zh;
          return `<span class="rl">${lab ? '<em>' + lab + '</em>' : ''}<b data-card="${r.to}">${nm}</b>` +
            (r.note ? '<i>' + r.note + '</i>' : '') + '</span>';
        }).join('')}</div>` : ''}
      </div>
      <div class="cdx">
        <div class="mono xs" style="color:var(--acc);letter-spacing:.1em">完整效果</div>
        ${(c.effect || []).map(e => `<div class="cdtxt"><b style="color:#dfe7ee;font-weight:400">${e.k}</b>　${e.v}
          ${e.req ? `<span class="tag" style="margin-left:4px">${e.req}</span>` : ''}</div>`).join('')}
        ${c.evolve ? `<div class="mono xs" style="color:var(--ok);margin-top:8px">
          ${c.evolve.from ? '来源 ▸ ' + (D.byId.get(c.evolve.from) || {}).name.zh + ' · ' : ''}
          ${c.evolve.to ? '进化 ▸ ' + (D.byId.get(c.evolve.to) || {}).name.zh + ' · ' + (c.evolve.req || '') : '链条末端'}</div>` : ''}
        <div class="cdfl">“${c.flavor ? c.flavor.zh : ''}”</div>
        <div class="xs" style="margin-top:8px">用法 · ${c.usage || ''}</div>
        ${(c.tips || []).map(t => `<div class="xs" style="margin-top:3px">· ${t}</div>`).join('')}
      </div>
      <div class="xs" style="margin-top:9px">id <span class="mono">${c.id}</span></div>`;
  }

  /* ── 年表 ──
     骨架与卡库同一套：左一列筛选、中间主体、右侧详情（REVIEW-DEMO C-1/C-3）。
     五个抽屉只有这一套骨架，玩家不用每次重新判断「我在哪一层」。 */
  function chronBody() {
    const kinds = ['全部', '干涉', '气候', '生态', '灾变', '文明', '图鉴'];
    const kf = U.chronFilter;
    const list = W.chron.filter(c => kf === '全部' || c.kind === kf);
    const countOf = k => k === '全部' ? W.chron.length : W.chron.filter(c => c.kind === k).length;
    const rows = foldChron(list);
    /* 选中项存对象而不是下标：年表会 unshift+pop 截断（上限 300），
       存下标的话截断一次就会「点 A 显示 B」（REVIEW-DEMO X-15）。
       没选就落到最新一条上——空着一整列去写「点任一条」，等于花 380px 教人怎么点。 */
    const sel = U.chronSel && W.chron.indexOf(U.chronSel) >= 0 ? U.chronSel : (list[0] || null);
    const selRow = sel ? rows.find(r => r.items.indexOf(sel) >= 0) : null;
    return `<div style="display:flex;gap:14px;height:100%;overflow:hidden">
      <div class="codexEl">
        ${kinds.map(k => `<button class="btn ${kf === k ? 'on' : 'ghost'}" data-kf="${k}"
          style="height:42px;flex-direction:column;gap:0"><span>${k}</span>
          <span style="font-size:11px;color:var(--ink4)">${countOf(k)}</span></button>`).join('')}
      </div>
      <div style="flex:1;min-width:0;border-right:1px solid var(--line);padding-right:14px;overflow:auto">
        <div class="row gap6" style="margin-bottom:10px;align-items:center">
          <span class="mono xs" style="color:var(--ink3)">${rows.length} 组 · ${list.length} 条 · 上限 300</span>
        </div>
        <div class="tl">
          ${rows.map(r => `<div class="it" style="--dot:${UI.CHRON_COL[r.kind] || '#3f7fa8'}"
              data-ci="${W.chron.indexOf(r.items[0])}">
            <div class="xs"><span style="color:${UI.CHRON_COL[r.kind] || '#3f7fa8'};margin-right:4px">${UI.CHRON_ICON[r.kind] || '·'}</span>
              ${monthSpan(r)}${r.n > 1 ? `<span class="itc mono">×${r.n}</span>` : ''}</div>
            <div class="t" style="font-size:13px;color:${selRow === r ? 'var(--acc)' : '#fff'}">${r.text}</div>
            <div class="xs" style="color:var(--ink4)">${r.kind}</div></div>`).join('') || '<div class="xs">这一类还没有记录</div>'}
        </div>
      </div>
      <div style="width:380px;overflow:auto">
        ${sel ? `<div style="font-size:14px;color:#fff;margin:2px 0 10px">${selRow ? selRow.text : sel.text}</div>
          <div class="rowline"><span class="xs">月份</span><span class="mono sm">${selRow ? monthSpan(selRow) : sel.month + ' 月'}</span></div>
          <div class="rowline"><span class="xs">类型</span><span class="sm">${sel.kind}</span></div>
          ${selRow && selRow.n > 1 ? `<div class="rowline"><span class="xs">合并</span><span class="mono sm">${selRow.n} 次相邻同类</span></div>` : ''}`
          : ''}
      </div>
    </div>`;
  }

  /* 相邻同类条目折成一条：一局里「土地落在 (x,y)」能占年表八成，
     真正的气候 / 生态 / 图鉴被埋在中间。只折「相邻且同一个动作」的，
     跨月穿插的事件不会被打乱顺序。 */
  function foldChron(list) {
    const keyOf = c => c.kind + '|' + c.text.replace(/[（(][^)）]*[)）]/g, '').replace(/落在.*$/, '').trim();
    const out = [];
    for (const c of list) {
      const k = keyOf(c);
      const last = out[out.length - 1];
      if (last && last.key === k) { last.n++; last.items.push(c); continue; }
      out.push({ key: k, kind: c.kind, text: c.text, n: 1, items: [c] });
    }
    /* 折叠后的条目不再挂某一次的坐标：三处不同落点合成一条时，
       写 (3, 3) 等于拿一处的细节代表三十九次。 */
    for (const r of out) if (r.n > 1) r.text = r.text.replace(/落在\s*[（(][^)）]*[)）]/, '落在 多处');
    return out;
  }

  function monthSpan(r) {
    const a = r.items[r.items.length - 1].month, b = r.items[0].month;
    return a === b ? UI.mon(a) : UI.mon(a) + '–' + UI.mon(b);
  }

  /* ── 设置：每一项都真的接线到渲染或 sim ── */
  const SETTINGS = {
    video: [
      { k: '像素比', v: ['1', '1.5', '2'], cur: () => String(SOL.render.pixelRatio || 1.5), set: v => SOL.render.setPixelRatio(+v) },
      { k: '阴影', v: ['开', '关'], cur: () => SOL.render.shadows ? '开' : '关', set: v => SOL.render.setShadows(v === '开') },
      { k: '雾浓度', v: ['无', '薄', '标准', '厚'], cur: () => SOL.render.fogLevel, set: v => SOL.render.setFog(v) }
    ],
    world: [
      { k: '默认俯仰', v: ['15°', '30°', '55°', '88°'], cur: () => SOL.render.cam.pitchDeg() + '°', set: v => SOL.render.cam.setPitch(['15', '30', '55', '88'].indexOf(v.replace('°', ''))) },
      { k: '缩放档', v: ['Z0', 'Z1', 'Z2', 'Z3', 'Z4'], cur: () => 'Z' + (SOL.render.cam.zoomIdx == null ? 2 : SOL.render.cam.zoomIdx), set: v => SOL.render.cam.setZoom(+v.slice(1)) }
    ],
    input: [
      { k: '拖动灵敏度', v: ['慢', '中', '快'], cur: () => SOL.render.cam.sensLabel || '中', set: v => SOL.render.cam.setSens(v) }
    ],
    /* 键位自成一栏：并进「操作」时下面还压着一张 340 高的手势图，
       二十条键位得先滚过图才看得见；而分组本身就是一个筛选，再套一层 tab 又多一层。 */
    keys: [],
    ui: [
      { k: '常驻栏', v: ['全部', '仅右栏', '全隐'], cur: () => UI.uiMode || '全部', set: v => UI.setUiMode(v) }
    ]
  };

  function settingsBody() {
    const nav = [['video', '画面'], ['world', '世界'], ['input', '操作'], ['keys', '键位'], ['ui', '界面']];
    const g = U.setGroup;
    const rows = SETTINGS[g] || [];
    return `<div style="display:flex;height:100%;min-height:0">
      <div style="width:170px;flex:0 0 170px;border-right:1px solid var(--line);display:flex;flex-direction:column;gap:4px;padding-right:12px">
        ${nav.map(([k, t]) => `<button class="btn ${g === k ? 'on' : 'ghost'}" data-sg="${k}"
          style="justify-content:flex-start">${t}</button>`).join('')}
      </div>
      <div class="setpane" style="flex:1;min-width:0;padding-left:20px;overflow:auto;display:flex;flex-direction:column">
        ${rows.map(r => `<div class="setRow">
          <span class="sm">${r.k}</span>
          <div style="display:flex;gap:6px">
            ${r.v.map(v => `<button class="btn ${r.cur() === v ? 'on' : 'ghost'}" data-set="${g}"
              data-val="${v}" style="height:26px">${v}</button>`).join('')}
          </div>
        </div>`).join('')}
        ${g === 'input' ? `<div style="margin-top:18px">${helpBlock()}</div>` : ''}
        ${g === 'keys' ? `<div style="margin-top:4px">${keysBody()}</div>` : ''}
      </div>
    </div>`;
  }

  /* ── 键位 ── */
  UI.KEYS = [
    { act: 'random', name: '随机释放', group: '手牌', key: ' ', ed: true },
    { act: 'recycle', name: '回收选中', group: '手牌', key: 'X', ed: true },
    { act: 'reroll', name: '重抽一手', group: '手牌', key: 'R', ed: true },
    { act: 'gear', name: '换档', group: '时间', key: 'J', ed: true },
    { act: 'ambient', name: '摆件模式', group: '界面', key: 'P', ed: true },
    { act: 'event', name: '演练事件', group: '界面', key: 'E', ed: true },
    { act: 'zoomIn', name: '放大一档', group: '视角', key: 'Z', ed: true },
    { act: 'zoomOut', name: '缩小一档', group: '视角', key: 'Shift+Z', ed: false },
    { act: 'pitch', name: '俯仰四档', group: '视角', key: 'F1–F4', ed: false },
    { act: 'pack', name: '抽卡', group: '面板', key: 'B', ed: true },
    { act: 'codex', name: '卡库', group: '面板', key: 'L', ed: true },
    { act: 'chron', name: '年表', group: '面板', key: 'C', ed: true },
    { act: 'creator', name: '创世', group: '面板', key: 'G', ed: true },
    { act: 'settings', name: '设置', group: '面板', key: 'O', ed: true },
    { act: 'keys', name: '键位', group: '面板', key: 'K', ed: true },
    { act: 'help', name: '帮助', group: '面板', key: 'H', ed: true },
    { act: 'select', name: '选卡 1–8', group: '手牌', key: '1 … 8', ed: false },
    { act: 'cursor', name: '移动格子游标', group: '世界', key: 'WASD', ed: false },
    { act: 'castAt', name: '在游标处施卡', group: '世界', key: 'Enter', ed: false },
    { act: 'esc', name: '关闭 / 退出', group: '界面', key: 'Esc', ed: false }
  ];

  UI.keyOf = act => (UI.KEYS.find(k => k.act === act) || {}).key;

  function keyConflicts() {
    const seen = {}, dup = new Set();
    UI.KEYS.forEach(k => {
      if (!k.key) return;
      if (seen[k.key]) { dup.add(k.key); }
      else seen[k.key] = k.name;
    });
    return dup;
  }

  /* null = 没在等；早先这里是 -1，而 -1 是真值，于是页面一加载就处于"等待绑键"状态：
     任何按键都走进重绑分支，Esc 永不生效并抛 undefined.key。见 REVIEW-DEMO-2026-11-03 X-1。 */
  let waitingFor = null;

  /* 键位表：是「设置 → 操作」里的一段，不是独立面板。
     原来顶上还有一排分组 tab，和卡库/年表的竖列筛选打架（REVIEW-DEMO C-3）；
     这里分组只分五段，加小标题比再加一个控件省事，也不多一层「我在哪」。 */
  function keysBody() {
    const dup = keyConflicts();
    const groups = [];
    for (const k of UI.KEYS) {
      const g = groups.find(x => x.name === k.group);
      if (g) g.rows.push(k);
      else groups.push({ name: k.group, rows: [k] });
    }
    return `<div class="row gap8" style="margin-bottom:10px;align-items:center">
        <span class="xs">${dup.size ? '<span style="color:#c8684f">⚠ ' + dup.size + ' 组键位冲突</span>' : '可改 · 锁死'}</span>
        <span class="xs" style="margin-left:auto">点右侧键名改绑 · Esc 取消</span>
      </div>
      ${groups.map(g => `<div class="keyGrp">${g.name}</div>
        ${g.rows.map(k => {
          const bad = dup.has(k.key);
          return `<div class="keyRow" style="${bad ? 'box-shadow:inset 2px 0 0 #b4553f;padding-left:6px' : ''}">
            <span class="sm" style="flex:1;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis">${k.name}${bad ? '<span class="tag warn" style="margin-left:6px">冲突</span>' : ''}</span>
            <button class="btn ${k.ed ? '' : 'ghost'}" data-k="${k.act}" style="width:132px;justify-content:center"
              ${k.ed ? '' : 'disabled'}>${k.key === ' ' ? '空格' : k.key}</button>
            <span style="width:64px;text-align:right">${k.ed ? '<span class="tag ok">可改</span>' : '<span class="tag">锁死</span>'}</span>
          </div>`;
        }).join('')}`).join('')}`;
  }

  UI.beginRebind = function (act) {
    waitingFor = act;
    const el = document.querySelector('[data-k="' + act + '"]');
    if (el) { el.textContent = '等待按键…'; el.classList.add('on'); }
    UI.log('等待按键：' + (UI.KEYS.find(k => k.act === act) || {}).name, true);
  };

  UI.captureKey = function (ev) {
    if (waitingFor === null) return false;
    /* 等待中按 Esc = 取消，不能把 Esc 绑上去 */
    if (ev.key === 'Escape') {
      waitingFor = null;
      UI.renderOverlay();
      UI.log('已取消改键', true);
      return true;
    }
    const k = ev.key.length === 1 ? ev.key.toUpperCase() : ev.key;
    const cur = UI.KEYS.find(x => x.act === waitingFor);
    const clash = UI.KEYS.find(x => x !== cur && x.key === k);
    if (clash) {
      UI.toast('键位冲突', '「' + k + '」已经被「' + clash.name + '」占用');
      waitingFor = null;
      UI.renderOverlay();
      return true;
    }
    cur.key = k;
    waitingFor = null;
    UI.renderOverlay();
    UI.toast('已改键', '「' + cur.name + '」→ ' + k);
    return true;
  };

  /* ── 操作速查：现在是「设置 → 操作」里的一段 ── */
  /* 只讲「手怎么动」。右侧原来还摆了四张卡（视角 / 界面 / 世界 / 事件），
     内容与「键位」那栏重复了八成，而且都是成句的说明——键位只在键位表里说一次。 */
  function helpBlock() {
    return `<div style="border:1px solid var(--line2);padding:14px;flex:1;display:flex;flex-direction:column;min-height:0">
      <div class="sm" style="color:#fff;margin-bottom:8px">手势：一套轴，三种结果</div>
      <div style="flex:1;min-height:0;display:flex;align-items:center;justify-content:center;overflow:hidden">
      <svg viewBox="0 0 620 340" preserveAspectRatio="xMidYMid meet"
        style="width:100%;height:auto;max-height:min(58vh,520px)">
        <rect x="250" y="120" width="120" height="150" rx="4" fill="#1a2230" stroke="#3d4a5c"/>
        <text x="310" y="200" text-anchor="middle" font-family="ui-monospace,Consolas" font-size="11" fill="#7e8a9a">手牌</text>
        <ellipse cx="76" cy="150" rx="52" ry="11" fill="#22402f" stroke="#3a5a48"/>
        <text x="76" y="154" text-anchor="middle" font-family="ui-monospace,Consolas" font-size="8" fill="#7fa08c">地面</text>
        <path d="M310,112 L310,42" stroke="#e0a33e" stroke-width="2"/>
        <path d="M310,36 L302,52 L318,52 Z" fill="#e0a33e"/>
        <text x="326" y="56" font-family="ui-monospace,Consolas" font-size="11" fill="#e0a33e">向上拖 = 随机落点释放</text>
        <text x="326" y="74" font-family="ui-monospace,Consolas" font-size="10" fill="#7e8a9a">或按 空格</text>
        <path d="M240,190 C160,190 120,168 98,140" stroke="#5f9fc0" stroke-width="2" fill="none"/>
        <path d="M90,132 L94,150 L110,142 Z" fill="#5f9fc0"/>
        <text x="14" y="118" font-family="ui-monospace,Consolas" font-size="11" fill="#5f9fc0">拖到地面 = 指定落点</text>
        <path d="M310,282 L310,318" stroke="#b4553f" stroke-width="2"/>
        <path d="M310,282 L302,298 L318,298 Z" fill="#b4553f"/>
        <text x="326" y="292" font-family="ui-monospace,Consolas" font-size="11" fill="#b4553f">向下拖 = 回收</text>
        <text x="326" y="310" font-family="ui-monospace,Consolas" font-size="10" fill="#7e8a9a">或按 X</text>
        <!-- 键盘那条路：不用鼠标也能落一张卡 -->
        <rect x="24" y="240" width="120" height="26" rx="3" fill="#141b23" stroke="#2d3844"/>
        <text x="84" y="257" text-anchor="middle" font-family="ui-monospace,Consolas" font-size="11" fill="#8b99a6">WASD → Enter</text>
        <text x="24" y="288" font-family="ui-monospace,Consolas" font-size="10" fill="#7e8a9a">走格 · 在游标处落卡</text>
      </svg>
      </div>
    </div>`;
  }

  /* ── 覆盖层事件 ── */
  UI.bindOverlay = function () {
    const ov = document.getElementById('overlay');
    ov.addEventListener('click', ev => {
      const close = ev.target.closest('[data-close]');
      if (close) return UI.closePanel();
      const act = ev.target.closest('[data-act]');
      if (act) {
        const a = act.dataset.act;
        if (a === 'packNext') return packNext();
        if (a === 'packPick') return packPick(+act.dataset.i);
        if (a === 'codexOwn') { U.codexOwn = !U.codexOwn; return UI.renderOverlay(); }
        if (a === 'toKeys') { U.panel = 'settings'; U.setGroup = 'input'; return UI.renderOverlay(); }
      }
      const pk = ev.target.closest('[data-pack]');
      if (pk) {
        /* 已经付费摊开的牌属于上一个包：切包会把它们连同时素一起吞掉，且不退款
           （UI 评审 P02，实测 12 时素买的 6 张牌凭空消失）。有未取走的牌时先不让切。 */
        if (UI.packStage >= 2 && (UI.packCards || []).length) {
          return UI.toast('这一包还没挑', '先收下一张，或点「跳过这一包」');
        }
        UI.packSel = pk.dataset.pack;
        /* 换包种等于重开：摊开的牌属于上一包，不能留着 */
        UI.packStage = 0; UI.packCards = [];
        return UI.renderOverlay();
      }
      const card = ev.target.closest('[data-card]');
      if (card) { UI.openCard(card.dataset.card); return; }
      const el = ev.target.closest('[data-el]');
      if (el) { U.codexEl = el.dataset.el; U.codexSel = null; return UI.renderOverlay(); }
      const ct = ev.target.closest('[data-ctab]');
      if (ct) { U.codexTab = ct.dataset.ctab; return UI.renderOverlay(); }
      const dg = ev.target.closest('[data-dg]');
      if (dg) { U.dexGrp = dg.dataset.dg; return UI.renderOverlay(); }
      const kf = ev.target.closest('[data-kf]');
      if (kf) { U.chronFilter = kf.dataset.kf; return UI.renderOverlay(); }
      const ci = ev.target.closest('[data-ci]');
      if (ci) { U.chronSel = W.chron[+ci.dataset.ci] || null; return UI.renderOverlay(); }
      const sg = ev.target.closest('[data-sg]');
      if (sg) waitingFor = null;   // 切分组同样放弃改绑（第二轮 S04）
      if (sg) { U.setGroup = sg.dataset.sg; return UI.renderOverlay(); }
      const st = ev.target.closest('[data-set]');
      if (st) {
        const g = st.dataset.set, val = st.dataset.val;
        const row = (SETTINGS[g] || []).find(r => r.v.includes(val));
        if (row) row.set(val);
        return UI.renderOverlay();
      }
      const kb = ev.target.closest('[data-k]');
      if (kb && !kb.disabled) { UI.beginRebind(kb.dataset.k); }
    });
  };

})(SOL.ui, SOL.sim, SOL.data);
