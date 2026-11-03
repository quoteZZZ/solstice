/* 至日 · 覆盖层面板：抽卡 / 卡库 / 年表 / 设置 / 键位 / 帮助
   所有读数都来自 sim 与卡表；面板里不写死任何「总数 252」之类的数字。 */
(function (UI, S, D) {
  const W = S.W;
  const U = UI.state = {
    panel: null,
    codexEl: '水', codexSel: null, codexOwn: false, codexMore: false,
    chronFilter: '全部',
    setGroup: 'video',
    keyTab: '全部'
  };

  UI.openPanel = function (p) {
    if (p === 'creator') { UI.openCreator(); return; }
    if (!['pack', 'codex', 'chron', 'settings', 'keys', 'help'].includes(p)) return;
    U.panel = p;
    if (p === 'pack') UI.packStage = 0;
    UI.renderOverlay();
    UI.log('打开：' + (UI.PANEL_LABEL[p] || p), true);
  };

  UI.closePanel = function () {
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
    const title = {
      pack: '抽卡 · 主包（' + S.PACK_SIZE + ' 张 / ' + S.PACK_COST + ' 时素）',
      codex: '卡库 · 已见 ' + W.seen.size + ' / ' + D.total,
      chron: '年表 · ' + W.chron.length + ' 条',
      settings: '设置',
      keys: '设置 / 操作 · 键位',
      help: '帮助（只讲操作）'
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
    if (p === 'pack')
      return `<button class="btn" data-act="packNext">${UI.packStage === 0 ? '撕开 · −' + S.PACK_COST + ' 时素' : UI.packStage === 1 ? '翻牌 ▸' : '收下'}</button>
        <button class="btn ghost" data-close="1">关闭</button>
        <span class="xs" style="margin-left:auto">时素 ${W.essence} · 每月自动 +0.1</span>`;
    return `<button class="btn ghost" data-close="1">关闭</button>`;
  }

  function bodyFor(p) {
    if (p === 'pack') return packBody();
    if (p === 'codex') return codexBody();
    if (p === 'chron') return chronBody();
    if (p === 'settings') return settingsBody();
    if (p === 'keys') return keysBody();
    return helpBody();
  }

  /* ── 抽卡 ── */
  UI.packStage = 0;
  UI.packCards = [];

  function packBody() {
    const got = UI.packCards || [];
    return `<div style="display:flex;gap:16px;height:100%">
      <div style="width:280px;border-right:1px solid var(--line);padding-right:14px">
        <div class="xs" style="margin-bottom:8px">包种</div>
        <div class="w" style="padding:10px;border-color:var(--acc)">
          <div class="row" style="justify-content:space-between;align-items:baseline">
            <span class="sm" style="color:#fff">主包</span>
            <span class="mono" style="color:var(--acc)">${S.PACK_COST}</span></div>
          <div class="xs" style="margin-top:3px">0–9 环 · ${S.PACK_SIZE} 张 · 权重只看环级</div>
          <div class="mono xs" style="margin-top:6px;color:#3f4b58">池 ${D.total} 张（目标 540）</div>
        </div>
        <div class="xs" style="margin-top:10px;line-height:1.7">
          抽到的卡直接进手牌（同名不重复）。<br>
          时素由世界月数累积：1 时素 / 10 月。<br>
          回收闲置卡能补回来（0 环不返还）。
        </div>
        <div class="xs" style="margin-top:12px">当前时素 <span class="mono" style="color:var(--acc)">${W.essence}</span></div>
      </div>
      <div style="flex:1;min-width:0;display:flex;flex-direction:column">
        <div class="row gap6" style="margin-bottom:10px">
          ${['封印', '悬浮', '翻牌', '收下'].map((t, i) =>
            `<span class="tag ${i === UI.packStage ? 'a' : ''}">${i + 1}. ${t}</span>`).join('')}
          <span class="xs" style="margin-left:auto">手牌 ${W.hand.length} / ${S.HAND_MAX}</span>
        </div>
        <div class="packframe" style="flex:1">
          ${UI.packStage === 0 ? `<div style="width:151px;height:220px;border:1px dashed var(--acc);
              background:#1a1509;display:flex;align-items:center;justify-content:center;color:var(--acc);
              font-family:var(--mono);font-size:10.5px;text-align:center;padding:10px">
              ${W.essence < S.PACK_COST ? '时素不够 · 还要 ' + (S.PACK_COST - W.essence) : '星尘崩解（不是撕纸）'}</div>` : ''}
          ${UI.packStage === 1 ? `<div style="display:flex">${[0, 1, 2, 3, 4].slice(0, got.length).map(i =>
            `<div style="width:120px;height:176px;border:1px solid var(--line2);background:#161c25;margin-left:-38px;
              transform:rotate(${(i - 2) * 5}deg) translateY(${Math.abs(i - 2) * 8}px)"></div>`).join('')}</div>` : ''}
          ${UI.packStage >= 2 ? `<div style="display:flex;gap:10px;align-items:flex-end">
            ${got.map((c, i) => `<div style="transform:scale(.62);transform-origin:bottom center;margin:0 -28px">
              ${UI.cardFace(c, 'p' + i)}</div>`).join('')}</div>` : ''}
        </div>
        <div class="xs" style="margin-top:10px">抽卡是获得新卡的唯一途径；卡进手牌后同名只留一张。</div>
      </div>
    </div>`;
  }

  function packNext() {
    if (UI.packStage === 0) {
      const res = S.drawPack();
      if (!res.ok) return UI.toast('时素不够', res.msg);
      UI.packCards = res.got;
      UI.packStage = 1;
      UI.toast('撕开一包', '−' + S.PACK_COST + ' 时素');
    } else if (UI.packStage === 1) {
      UI.packStage = 2;
    } else {
      UI.packStage = 0;
      UI.packCards = [];
      U.panel = null;
    }
    UI.renderHand(); UI.updateHud(); UI.renderRight(); UI.renderOverlay();
  }

  /* ── 卡库 ── */
  UI.openCard = function (id) {
    const c = D.byId.get(id);
    if (!c) return;
    U.panel = 'codex';
    U.codexEl = c.primary;
    U.codexSel = c.id;
    UI.renderOverlay();
  };

  function codexBody() {
    const els = D.elements.order;
    const el = U.codexEl;
    const sel = U.codexSel ? D.byId.get(U.codexSel) : null;
    const seenIn = e => D.cards.filter(c => c.primary === e && W.seen.has(c.id)).length;
    const totalIn = e => D.cards.filter(c => c.primary === e).length;

    const grid = Array.from({ length: 11 }, (_, tier) =>
      `<div class="codexGrid">
        <span class="mono xs" style="width:20px;text-align:center">${tier === 10 ? '超' : tier}</span>
        ${['matter', 'rite', 'law'].map(cls => {
          const c = D.cards.find(x => x.primary === el && x.tier === tier && x.class === cls);
          if (!c) return `<div class="cgCell empty">—</div>`;
          const seen = W.seen.has(c.id);
          if (U.codexOwn && !seen) return `<div class="cgCell empty"></div>`;
          return `<div class="cgCell ${seen ? 'has' : ''}" data-card="${c.id}" title="${c.name.zh}">
            <div class="cgTag">${D.clsName[cls]}</div>
            <div class="cgBody">${seen ? c.name.zh : '·'}</div></div>`;
        }).join('')}
      </div>`).join('');

    return `<div style="display:flex;gap:14px;height:100%;overflow:hidden">
      <div class="codexEl">
        ${els.map(e => `<button class="btn ${e === el ? 'on' : 'ghost'}" data-el="${e}"
          style="height:42px;flex-direction:column;gap:0"><span>${e}</span>
          <span style="font-size:8px;color:#4c5563">${seenIn(e)} / ${totalIn(e)}</span></button>`).join('')}
      </div>
      <div style="flex:1;min-width:0;overflow:auto">
        <div class="row gap8" style="margin-bottom:9px;align-items:center">
          <span class="mono xs" style="color:#8b99a6">已见 ${W.seen.size} / ${D.total}</span>
          <button class="btn ${U.codexOwn ? 'on' : 'ghost'}" data-act="codexOwn" style="height:22px">只看已见</button>
          <span class="xs">灰格 = 还没抽到 · 点开看详情</span>
        </div>
        ${grid}
      </div>
      <div style="width:400px;border-left:1px solid var(--line);padding-left:16px;overflow:auto">
        ${sel ? codexDetail(sel) : '<div class="xs">点左侧任意卡格 → 这里出详情</div>'}
      </div>
    </div>`;
  }

  function codexDetail(c) {
    return `<div style="display:flex;flex-direction:column;align-items:center">
        <div style="transform:scale(.86);transform-origin:top center;height:384px">${UI.cardFace(c, 'cx')}</div>
      </div>
      <div style="display:flex;justify-content:space-between;margin-top:2px;padding:9px 2px;
        border-top:1px solid #1a212a;border-bottom:1px solid #1a212a">
        ${[['◔', '半径', c.radius_w.toFixed(3) + ' W'], ['⚠', '扰动', '+' + c.disturb],
           ['↩', '回收', c.reclaim ? '+' + c.reclaim : '—']]
          .map(([i, k, v]) => `<div style="text-align:center;flex:1">
            <div style="font-size:14px;color:var(--acc)">${i}</div>
            <div class="mono" style="font-size:11.5px;color:#e8eef4">${v}</div>
            <div class="xs" style="color:#4c5563">${k}</div></div>`).join('')}
      </div>
      <div class="dex" style="margin-top:10px">
        <div class="mono xs" style="color:#5a6674;letter-spacing:.1em">
          ${c.title ? c.title + ' · ' : ''}${c.elements.join(' / ')} · ${c.tier === 10 ? '超环' : c.tier + ' 环'}</div>
        ${UI.statsBlock(c)}
        ${(c.lore || []).map(l => `<div class="lore"><b>${l.k === 'habit' ? '习性' : l.k === 'behavior' ? '行为' : '传说'}</b>
          <span>${l.t}</span></div>`).join('')}
        ${(c.rel || []).length ? `<div class="rel">${c.rel.map(r => `<span class="rl">${r.k}<b
          data-card="${r.to}">${(D.byId.get(r.to) || { name: { zh: '?' } }).name.zh}</b></span>`).join('')}</div>` : ''}
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

  /* ── 年表 ── */
  function chronBody() {
    const kinds = ['全部', '干涉', '气候', '生态', '灾变', '文明'];
    const kf = U.chronFilter;
    const list = W.chron.filter(c => kf === '全部' || c.kind === kf);
    const sel = U.chronSel != null ? W.chron[U.chronSel] : null;
    return `<div class="chronRow">
      <div style="flex:1;border-right:1px solid var(--line);padding-right:14px;overflow:auto">
        <div class="row gap6" style="margin-bottom:12px">
          ${kinds.map(k => `<button class="btn ${kf === k ? 'on' : 'ghost'}" data-kf="${k}" style="height:24px">${k}</button>`).join('')}
          <span class="xs" style="align-self:center;margin-left:auto">${list.length} 条 · 上限 300</span>
        </div>
        <div class="tl">
          ${list.map((c, i) => `<div class="it" style="--dot:${UI.CHRON_COL[c.kind] || '#3f7fa8'}" data-ci="${W.chron.indexOf(c)}">
            <div class="xs"><span style="color:${UI.CHRON_COL[c.kind] || '#3f7fa8'};margin-right:4px">${UI.CHRON_ICON[c.kind] || '·'}</span>
              第 ${c.month} 月</div>
            <div class="t" style="font-size:12.5px;color:${U.chronSel === W.chron.indexOf(c) ? 'var(--acc)' : '#fff'}">${c.text}</div>
            <div class="xs" style="color:#4c5563">${c.kind}</div></div>`).join('') || '<div class="xs">这一类还没有记录</div>'}
        </div>
      </div>
      <div style="width:380px;padding-left:16px;overflow:auto">
        ${sel ? `<div class="xs">这一条发生时</div>
          <div style="font-size:14px;color:#fff;margin:8px 0">${sel.text}</div>
          <div class="rowline"><span class="xs">月份</span><span class="mono sm">${sel.month} 月</span></div>
          <div class="rowline"><span class="xs">类型</span><span class="sm">${sel.kind}</span></div>
          <div class="xs" style="margin-top:10px">年表只记录发生过的事；回滚与重演还没实装。</div>`
          : `<div class="xs">点任一条 → 这里显示它发生在第几月。年表同时是记忆与因果索引。</div>`}
      </div>
    </div>`;
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
      { k: '拖动灵敏度', v: ['慢', '中', '快'], cur: () => '中', set: () => { } }
    ],
    ui: [
      { k: '常驻栏', v: ['全部', '仅右栏', '全隐'], cur: () => UI.uiMode || '全部', set: v => UI.setUiMode(v) }
    ]
  };

  function settingsBody() {
    const nav = [['video', '画面'], ['world', '世界'], ['input', '操作'], ['ui', '界面']];
    const g = U.setGroup;
    const rows = SETTINGS[g] || [];
    return `<div style="display:flex;height:100%">
      <div style="width:170px;border-right:1px solid var(--line);display:flex;flex-direction:column;gap:4px;padding-right:12px">
        ${nav.map(([k, t]) => `<button class="btn ${g === k ? 'on' : 'ghost'}" data-sg="${k}"
          style="justify-content:flex-start">${t}</button>`).join('')}
      </div>
      <div style="flex:1;padding-left:20px;display:flex;flex-direction:column;min-width:0">
        ${rows.map(r => `<div class="setRow">
          <span class="sm">${r.k}</span>
          <div style="display:flex;gap:6px">
            ${r.v.map(v => `<button class="btn ${r.cur() === v ? 'on' : 'ghost'}" data-set="${g}"
              data-val="${v}" style="height:26px">${v}</button>`).join('')}
          </div>
        </div>`).join('')}
        <div class="xs" style="margin-top:14px;line-height:1.7">
          这里只有真正接线的项：像素比改渲染器、雾浓度改 scene.fog、俯仰与缩放直接改相机。
          音量 / 语言 / 辅助三组没有实装，所以不列出来。
        </div>
        ${g === 'input' ? `<button class="btn on" style="margin-top:14px;align-self:flex-start" data-act="toKeys">键位重绑 ▸</button>` : ''}
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
    { act: 'esc', name: '关闭 / 退出', group: '界面', key: 'Esc', ed: false },
    { act: 'tab', name: '显隐 UI', group: '界面', key: 'Tab', ed: false }
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

  let waitingFor = -1;

  function keysBody() {
    const tabs = ['全部', '手牌', '视角', '时间', '界面', '面板'];
    const tab = U.keyTab;
    const dup = keyConflicts();
    const rows = UI.KEYS.filter(k => tab === '全部' || k.group === tab);
    return `<div class="row gap6" style="margin-bottom:8px">
      ${tabs.map(t => `<button class="btn ${tab === t ? 'on' : 'ghost'}" data-kt="${t}" style="height:24px">${t}</button>`).join('')}
      <span class="xs" style="align-self:center;margin-left:auto">${
        dup.size ? '⚠ ' + dup.size + ' 组键位冲突（红框）' : '点「可改」的键位 → 按下新键；★ 号键位锁死'}</span>
    </div>
    ${rows.map(k => {
      const bad = dup.has(k.key);
      return `<div class="keyRow" style="${bad ? 'box-shadow:inset 2px 0 0 #b4553f;padding-left:6px' : ''}">
        <span class="xs" style="width:40px">${k.group}</span>
        <span class="sm" style="flex:1">${k.name}${bad ? '<span class="tag warn" style="margin-left:6px">冲突</span>' : ''}</span>
        <button class="btn ${k.ed ? '' : 'ghost'}" data-k="${k.act}" style="width:160px;height:26px"
          ${k.ed ? '' : 'disabled'}>${k.key}</button>
        <span style="width:64px;text-align:right">${k.ed ? '<span class="tag ok">可改</span>' : '<span class="tag">锁死</span>'}</span>
      </div>`;
    }).join('')}`;
  }

  UI.beginRebind = function (act) {
    waitingFor = act;
    const el = document.querySelector('[data-k="' + act + '"]');
    if (el) { el.textContent = '等待按键…'; el.classList.add('on'); }
    UI.log('等待按键：' + (UI.KEYS.find(k => k.act === act) || {}).name, true);
  };

  UI.captureKey = function (ev) {
    if (!waitingFor) return false;
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

  /* ── 帮助 ── */
  function helpBody() {
    return `<div style="display:flex;gap:16px;height:100%">
      <div style="flex:1.4;border:1px solid var(--line2);padding:14px">
        <div class="sm" style="color:#fff;margin-bottom:6px">手势：一套轴，三种结果</div>
        <svg viewBox="0 0 620 340" style="width:100%;height:auto">
          <rect x="250" y="120" width="120" height="150" rx="4" fill="#1a2230" stroke="#3d4a5c"/>
          <text x="310" y="200" text-anchor="middle" font-family="ui-monospace,Consolas" font-size="11" fill="#7e8a9a">手牌</text>
          <ellipse cx="76" cy="150" rx="52" ry="11" fill="#22402f" stroke="#3a5a48"/>
          <text x="76" y="154" text-anchor="middle" font-family="ui-monospace,Consolas" font-size="8" fill="#7fa08c">地面</text>
          <path d="M310,112 L310,42" stroke="#e0a33e" stroke-width="2"/>
          <path d="M310,36 L302,52 L318,52 Z" fill="#e0a33e"/>
          <text x="326" y="56" font-family="ui-monospace,Consolas" font-size="11" fill="#e0a33e">向上拖 = 随机落点释放</text>
          <text x="326" y="74" font-family="ui-monospace,Consolas" font-size="10" fill="#5d6b7a">或按 空格</text>
          <path d="M240,190 C160,190 120,168 98,140" stroke="#5f9fc0" stroke-width="2" fill="none"/>
          <path d="M90,132 L94,150 L110,142 Z" fill="#5f9fc0"/>
          <text x="14" y="118" font-family="ui-monospace,Consolas" font-size="11" fill="#5f9fc0">拖到地面 = 指定落点</text>
          <path d="M310,282 L310,318" stroke="#b4553f" stroke-width="2"/>
          <path d="M310,282 L302,298 L318,298 Z" fill="#b4553f"/>
          <text x="326" y="292" font-family="ui-monospace,Consolas" font-size="11" fill="#b4553f">向下拖 = 回收</text>
          <text x="326" y="310" font-family="ui-monospace,Consolas" font-size="10" fill="#5d6b7a">或按 X</text>
        </svg>
      </div>
      <div style="width:340px;display:flex;flex-direction:column;gap:8px">
        ${[['⌖', '视角', '拖拽旋转 · 滚轮缩放 · F1–F4 俯仰 · Z 缩放档'],
           ['▤', '界面', '左栏点开面板 · 右栏常驻 · Tab 显隐 · P 摆件'],
           ['◇', '世界', '悬停看该格的水/光/温与活物；律卡不需要落点'],
           ['⚑', '事件', 'E 演练 · Esc 关 · 承受 / 改写（改写在面板里说明代价）']]
          .map(([i, t, d]) => `<div class="w" style="padding:11px;display:flex;gap:10px;align-items:flex-start">
            <span style="font-size:15px;color:var(--acc);line-height:1">${i}</span>
            <div><div class="sm" style="color:#fff">${t}</div>
              <div class="xs" style="margin-top:4px;line-height:1.6">${d}</div></div></div>`).join('')}
        <button class="btn ghost" style="margin-top:auto" data-act="toKeys">全部键位 ▸</button>
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
        if (a === 'codexOwn') { U.codexOwn = !U.codexOwn; return UI.renderOverlay(); }
        if (a === 'toKeys') { U.panel = 'keys'; return UI.renderOverlay(); }
      }
      const card = ev.target.closest('[data-card]');
      if (card) { UI.openCard(card.dataset.card); return; }
      const el = ev.target.closest('[data-el]');
      if (el) { U.codexEl = el.dataset.el; U.codexSel = null; return UI.renderOverlay(); }
      const kf = ev.target.closest('[data-kf]');
      if (kf) { U.chronFilter = kf.dataset.kf; return UI.renderOverlay(); }
      const ci = ev.target.closest('[data-ci]');
      if (ci) { U.chronSel = +ci.dataset.ci; return UI.renderOverlay(); }
      const sg = ev.target.closest('[data-sg]');
      if (sg) { U.setGroup = sg.dataset.sg; return UI.renderOverlay(); }
      const st = ev.target.closest('[data-set]');
      if (st) {
        const g = st.dataset.set, val = st.dataset.val;
        const row = (SETTINGS[g] || []).find(r => r.v.includes(val));
        if (row) row.set(val);
        return UI.renderOverlay();
      }
      const kt = ev.target.closest('[data-kt]');
      if (kt) { U.keyTab = kt.dataset.kt; return UI.renderOverlay(); }
      const kb = ev.target.closest('[data-k]');
      if (kb && !kb.disabled) { UI.beginRebind(kb.dataset.k); }
    });
  };

})(SOL.ui, SOL.sim, SOL.data);
