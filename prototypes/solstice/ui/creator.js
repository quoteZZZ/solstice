/* 至日 · 创世（缺氧式：只能重掷，不能选）
   形制 / 气候 / 特征全部由系统掷出；同一坐标 = 同一个世界。
   创世结果只做三件事：定蓝图（3D 虚线上的轮廓）、定相机、定特征接线（sim.W.effects）。 */
(function (UI, S, D) {
  const WD = D.world;
  const U = UI.state;
  const CRE = { form: null, climate: null, traits: [], name: '', seed: [], rolls: 0, log: [] };

  UI.creator = CRE;

  const seg = () => Math.random().toString(32).slice(2, 6).toUpperCase();

  /* 坐标 = 种子。形制由坐标第二段决定，其余全部由坐标派生——
     所以「复制坐标 → 重进 = 同一个世界」在剖面图和结算层都成立（REVIEW-2026-11-03 2.1）。
     只有"摇一个新坐标"这一步用 Math.random；一旦有了坐标，后续一律走 S.rand。 */
  const parseCoord = s => {
    const p = String(s || '').toUpperCase().split('-');
    if (p.length !== 4 || p[0] !== 'SOL') return null;
    const form = WD.forms.find(f => f.code === p[1]);
    return form ? { form: form, coord: p } : null;
  };

  UI.rollWorld = function (coordStr) {
    let hit = parseCoord(coordStr);
    if (!hit) {
      const f = WD.forms[(Math.random() * WD.forms.length) | 0];
      hit = parseCoord(['SOL', f.code, seg(), seg()].join('-'));
    }
    const form = hit.form;
    CRE.form = form;
    CRE.seed = hit.coord;
    CRE.coord = hit.coord.join('-');
    CRE.rolls++;

    S.setSeed(CRE.coord);          // 从这一行起全部用 S.rand：同一坐标必然同一结果

    CRE.climate = S.pick(WD.climates);
    CRE.name = S.pick(WD.syl1) + S.pick(WD.syl2) + form.suffix;

    /* Fisher–Yates：均匀且确定。旧写法 sort(() => rand()-0.5) 既偏斜又依赖引擎排序实现 */
    const order = WD.traits.map((_, i) => i);
    for (let i = order.length - 1; i > 0; i--) {
      const j = S.randInt(i + 1);
      const t = order[i]; order[i] = order[j]; order[j] = t;
    }
    CRE.traits = order.slice(0, 3 + S.randInt(3)).sort((a, b) => a - b);

    CRE.params = {
      water: Math.round((0.3 + S.rand() * 0.5) * 100) + ' %',
      ranges: 1 + S.randInt(4),
      species: 5 + S.randInt(5),
      relief: (0.35 + S.rand() * 0.65).toFixed(2)
    };
    CRE.log = [
      ['陆块拓扑', form.blocks, '形制'],
      ['陆块形状', form.topo, '形制'],
      ['界缘交代', S.pick(['外海', '雾墙', '云海', '深渊', '冰盖']), '形制'],
      ['水覆盖', CRE.params.water, '水'],
      ['陆海比', (0.2 + S.rand() * 0.65).toFixed(2), '水'],
      ['起伏 relief', CRE.params.relief, '地貌'],
      ['山系数', CRE.params.ranges, '地貌'],
      ['山系走向', S.pick(['NNE', 'ESE', 'SSW', 'WNW']) + ' ' + Math.round(S.rand() * 180) + '°', '地貌'],
      ['气候带', CRE.climate, '气候'],
      ['风向', S.pick(['NE', 'SE', 'SW', 'NW']) + ' ' + Math.round(S.rand() * 360) + '°', '气候'],
      ['植被密度', (0.3 + S.rand() * 0.7).toFixed(2), '生命'],
      ['物种池', CRE.params.species + ' 种', '生命'],
      ['界名', CRE.name, '命名'],
      ['判据', WD.judge.length + ' / ' + WD.judge.length + ' PASS', '校验']
    ];
    return CRE;
  };

  UI.openCreator = function () {
    if (!CRE.form) {
      /* ?seed=SOL-XXX-XXXX-XXXX 直接复现一个界；没有就用新掷的坐标 */
      const q = (typeof location !== 'undefined' && location.search)
        ? new URLSearchParams(location.search).get('seed') : null;
      UI.rollWorld(q || null);
    }
    U.panel = null;
    UI.renderOverlay();
    const el = document.getElementById('creator');
    el.classList.add('on');
    UI.renderCreator();
  };

  UI.closeCreator = function () {
    document.getElementById('creator').classList.remove('on');
  };

  const GRP_ICON = { 形制: '◇', 水: '◐', 地貌: '▲', 气候: '☀', 生命: '✿', 命名: '✎', 校验: '✓' };
  const GRP_ORDER = ['形制', '水', '地貌', '气候', '生命', '命名', '校验'];

  function rollStrip() {
    const groups = {};
    CRE.log.forEach(([k, v, g]) => (groups[g] = groups[g] || []).push([k, v]));
    return GRP_ORDER.filter(g => groups[g]).map((g, i) => {
      const items = groups[g];
      const short = v => String(v).replace(/（[^）]*）/g, '');
      const vals = items.map(([, v]) => `<span>${short(v)}</span>`).join('<i>·</i>');
      const detail = items.map(([k, v]) => k + ' ' + v).join(' / ');
      return `<div class="stp" style="animation-delay:${i * 45}ms" title="${g}｜${detail}">
        <span class="sic">${GRP_ICON[g] || '•'}</span><span class="sl">${g}</span>
        <span class="sv">${vals}</span></div>`;
    }).join('');
  }

  /* 顶视缩略：形制从一句话变成一张图 */
  function miniMap(form) {
    const grid = [];
    for (let x = 26; x < 260; x += 26) grid.push(`<path d="M${x},0 L${x},200" stroke="#16222c"/>`);
    for (let y = 25; y < 200; y += 25) grid.push(`<path d="M0,${y} L260,${y}" stroke="#16222c"/>`);
    const isles = (form.isles || []).map(([cx, cy, rx, ry], i) => {
      if (form.name === '环礁界' && i === 1) return '';
      return `<ellipse cx="${130 + cx * 11}" cy="${100 + cy * 15}" rx="${rx * 11}" ry="${ry * 15}"/>`;
    }).join('');
    return `<svg viewBox="0 0 260 200" style="width:100%;height:auto;display:block;border:1px solid #1b222b;background:#08121a">
      ${grid.join('')}
      <rect x="0.5" y="0.5" width="259" height="199" fill="none" stroke="#1b2a35"/>
      <g fill="#33513c" stroke="#4a6b52" stroke-width="1.3">${isles}</g>
      <circle cx="130" cy="100" r="2" fill="#e0a33e" opacity=".8"/>
      <path d="M244,12 L244,26 M238,18 L250,18" stroke="#5d6b7a"/>
    </svg>`;
  }

  /* 剖面：随形制变地表、随特征加奇观（静态图，不参与结算） */
  function cutaway(form, traits) {
    const has = n => traits.includes(n);
    const LAND = {
      '单岛界': 'M60,300 C180,196 820,196 940,300 L940,520 L60,520 Z',
      '大陆界': 'M-30,282 C160,232 840,232 1030,282 L1030,520 L-30,520 Z',
      '半岛界': 'M-30,292 C120,210 520,206 700,262 L700,520 L-30,520 Z',
      '群岛界': 'M30,330 C80,294 180,294 230,330 L230,520 L30,520 Z M370,314 C430,274 560,274 620,314 L620,520 L370,520 Z M760,336 C810,300 910,300 960,336 L960,520 L760,520 Z',
      '环礁界': 'M50,352 C110,322 210,320 270,344 L270,520 L50,520 Z M730,344 C790,320 890,322 950,352 L950,520 L730,520 Z',
      '全海界': '',
      '全陆界': 'M-30,268 C160,238 840,238 1030,268 L1030,520 L-30,520 Z'
    };
    const land = form.name in LAND ? LAND[form.name] : LAND['单岛界'];
    const surf = { '大陆界': 282, '全陆界': 268, '半岛界': 292, '群岛界': 322, '环礁界': 348 }[form.name] || 300;
    const wet = !['全陆界', '内陆湖国'].includes(form.name);

    const trees = [];
    const spans = form.name === '群岛界' ? [[60, 200], [370, 610], [760, 950]] : [[120, 860]];
    for (const [x0, x1] of spans) {
      for (let x = x0; x < x1; x += 30) {
        const rx = form.name === '群岛界' ? 0.5 : (x - 60) / 880;
        const y = surf - (form.name === '群岛界' ? 18 : 96 * Math.sin(Math.PI * Math.min(1, Math.max(0, rx))));
        trees.push(`<path d="M${x},${y - 10} L${x + 4},${y} L${x - 4},${y} Z" fill="#3c7048" opacity=".92"/>`);
      }
    }

    const oreN = has('金属富集') ? 14 : 6;
    const ore = `<g class="cvt" data-x="金属富集">${Array.from({ length: oreN }, (_, i) => {
      const x = 140 + ((i * 137) % 700), y = 396 + ((i * 53) % 70);
      return `<circle cx="${x}" cy="${y}" r="2.6" fill="#c79a54"/>`;
    }).join('')}</g>`;

    const volcano = `<g class="cvt" data-x="地热活跃">${has('地热活跃') ? `
      <path d="M560,${surf} L610,${surf - 86} L640,${surf - 86} L690,${surf} Z" fill="#4a3b33"/>
      <path d="M628,${surf - 86} L640,${surf - 86} L648,${surf - 70} L620,${surf - 70} Z" fill="#b4553f" opacity=".9"/>
      <path d="M636,${surf - 70} C636,${surf - 40} 630,${surf - 20} 626,${surf}" stroke="#c4643f" stroke-width="3" fill="none" opacity=".8"/>` : ''}</g>`;

    const geyser = `<g class="cvt" data-x="地热活跃">${has('地热活跃') ?
      [0, 1, 2].map(k => { const x = 180 + k * 60;
        return `<path d="M${x},${surf} C${x - 2},${surf - 26} ${x + 3},${surf - 44} ${x},${surf - 62}" stroke="#cfe0e6" stroke-width="2" fill="none" opacity=".35"/>`; }).join('') : ''}</g>`;

    const giant = `<g class="cvt" data-x="古树群">${has('古树群') ? `
      <path d="M300,${surf + 4} L300,${surf - 64}" stroke="#4a3a2a" stroke-width="7"/>
      <path d="M300,${surf - 118} L330,${surf - 58} L270,${surf - 58} Z" fill="#3f7549"/>
      <path d="M300,${surf - 140} L324,${surf - 92} L276,${surf - 92} Z" fill="#4a8554"/>` : ''}</g>`;

    const floating = `<g class="cvt" data-x="浮空岛">${has('浮空岛') ? `
      <path d="M700,150 C730,132 786,132 816,150 C800,176 716,176 700,150 Z" fill="#33453a" stroke="#4a6b52"/>
      <path d="M712,158 C730,186 786,186 806,158" fill="none" stroke="#2a3a30" stroke-dasharray="3 3"/>` : ''}</g>`;

    const moons = `<g class="cvt" data-x="双月">${has('双月') ? `
      <circle cx="150" cy="86" r="16" fill="#dfe8ee" opacity=".85"/>
      <circle cx="196" cy="112" r="10" fill="#cdd8e0" opacity=".7"/>`
      : `<circle cx="822" cy="92" r="26" fill="#f6e2b4" opacity=".75"/>
         <circle cx="822" cy="92" r="44" fill="#f6e2b4" opacity=".12"/>`}</g>`;

    const storm = `<g class="cvt" data-x="永久雷暴">${has('永久雷暴') ? `
      <path d="M560,74 q30,-20 64,-4 q26,-14 44,8 q18,0 18,12 l-126,0 z" fill="#3a4550" opacity=".9"/>
      <path d="M640,92 L626,124 L642,124 L620,156" stroke="#cfd9e2" stroke-width="2" fill="none" opacity=".9"/>` : ''}</g>`;

    const cliff = `<g class="cvt" data-x="深海崖岸">${has('深海崖岸') && wet ? `
      <path d="M940,${surf} L940,520" stroke="#18222b" stroke-width="10"/>` : ''}</g>`;
    const tide = `<g class="cvt" data-x="强潮汐">${has('强潮汐') && wet ? `
      <path d="M0,${surf - 6} L1000,${surf - 6}" stroke="#3f7fa8" stroke-dasharray="8 6" opacity=".5"/>
      <path d="M0,${surf + 4} L1000,${surf + 4}" stroke="#3f7fa8" stroke-dasharray="4 8" opacity=".35"/>` : ''}</g>`;
    const islets = `<g class="cvt" data-x="物种隔离">${has('物种隔离') && wet ? `
      <ellipse cx="150" cy="${surf + 28}" rx="26" ry="9" fill="#2c4632"/>
      <ellipse cx="860" cy="${surf + 34}" rx="20" ry="7" fill="#2c4632"/>` : ''}</g>`;

    const fjord = form.name === '峡湾界' ? `<path d="M600,${surf} L620,${surf + 30} L640,${surf + 96} L660,${surf + 30} L680,${surf} Z" fill="#1d4a5c" opacity=".95"/>` : '';
    const lake = form.name === '内陆湖国' ? `<ellipse cx="500" cy="${surf + 8}" rx="180" ry="26" fill="#1d4a5c"/>
      <ellipse cx="500" cy="${surf + 8}" rx="180" ry="26" fill="none" stroke="#3f7fa8" opacity=".7"/>` : '';
    const rift = form.name === '裂谷界' ? `<path d="M470,${surf} L500,${surf + 56} L470,${surf + 56} L430,520 L470,520 Z" fill="#0c1117"/>
      <path d="M520,${surf} L560,${surf + 60} L520,${surf + 60} L500,520 L540,520 Z" fill="#12181f"/>` : '';
    const lagoon = form.name === '环礁界' || form.name === '全海界' ? `<ellipse cx="500" cy="${surf + 8}" rx="200" ry="26" fill="#1d4a5c" opacity=".9"/>` : '';

    return `<svg viewBox="0 0 1000 520" preserveAspectRatio="xMidYMid meet">
      <defs>
        <linearGradient id="sky4" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0" stop-color="#33506a"/><stop offset=".55" stop-color="#22323f"/><stop offset="1" stop-color="#141b23"/>
        </linearGradient>
        <linearGradient id="lay4" x1="0" y1="250" x2="0" y2="520" gradientUnits="userSpaceOnUse">
          <stop offset="0" stop-color="#4a6b3f"/><stop offset="${has('贫瘠土壤') ? 0.12 : 0.24}" stop-color="#415d38"/>
          <stop offset="${has('贫瘠土壤') ? 0.12 : 0.24}" stop-color="#4a3d2b"/><stop offset=".44" stop-color="#3b3226"/>
          <stop offset=".44" stop-color="#39424e"/><stop offset=".74" stop-color="#2c333c"/>
          <stop offset=".74" stop-color="#232a33"/><stop offset="1" stop-color="#171c24"/>
        </linearGradient>
        <pattern id="hatch4" width="9" height="9" patternTransform="rotate(38)" patternUnits="userSpaceOnUse">
          <line x1="0" y1="0" x2="0" y2="9" stroke="#000" stroke-opacity=".16"/>
        </pattern>
        <clipPath id="clip4"><path d="${land}"/></clipPath>
      </defs>
      <rect width="1000" height="520" fill="url(#sky4)"/>
      ${moons}${storm}
      <path d="M120,120 q26,-22 56,-6 q22,-16 46,4 q20,-4 22,14 l-124,0 z" fill="#e8eef2" opacity=".16"/>
      <path d="M0,${surf} L120,${surf - 64} L210,${surf - 18} L300,${surf - 56} L420,${surf} Z" fill="#1d2a33" opacity=".8"/>
      <path d="M540,${surf} L660,${surf - 52} L760,${surf - 14} L860,${surf - 48} L1000,${surf} Z" fill="#1b2730" opacity=".8"/>
      ${wet ? `<rect x="0" y="${surf}" width="1000" height="${520 - surf}" fill="#16323f" opacity=".6"/>` : ''}
      ${tide}
      ${land ? `<path d="${land}" fill="url(#lay4)"/><path d="${land}" fill="url(#hatch4)" opacity=".55"/>` : ''}
      ${geyser}
      <g clip-path="url(#clip4)">
        ${['单岛界', '大陆界', '半岛界', '峡湾界', '内陆湖国', '裂谷界', '全陆界'].includes(form.name) ? `
          <path d="M255,${surf} L338,${surf - 80} L425,${surf} Z" fill="#5b6b78"/>
          <path d="M320,${surf - 62} L338,${surf - 80} L358,${surf - 60} L340,${surf - 52} Z" fill="#e8f0f3" opacity=".85"/>
          <path d="M545,${surf} L640,${surf - 64} L735,${surf} Z" fill="#4e5d6a"/>
          <path d="M620,${surf - 48} L640,${surf - 64} L662,${surf - 46} L642,${surf - 38} Z" fill="#e8f0f3" opacity=".8"/>` : ''}
        ${volcano}${giant}${fjord}${lake}${rift}${lagoon}
        ${trees.join('')}
        <path d="M90,${surf + 108} C220,${surf + 92} 420,${surf + 100} 560,${surf + 110} C700,${surf + 120} 830,${surf + 110} 930,${surf + 102}"
          fill="none" stroke="#4f9fc0" stroke-width="1.6" stroke-dasharray="5 4" opacity=".75"/>
        <ellipse cx="300" cy="${surf + 130}" rx="86" ry="22" fill="#1d4a5c" opacity=".85"/>
        <ellipse cx="560" cy="${surf + 166}" rx="52" ry="17" fill="#0a0e13"/>
        ${ore}
      </g>
      ${floating}${cliff}${islets}
      ${land ? `<path d="${land}" fill="none" stroke="#39493f"/>` : ''}
      <g font-family="ui-monospace,Consolas,monospace" font-size="10" fill="#7e8a9a">
        <text x="24" y="52">天空</text>
        <text x="24" y="${surf + 10}">表层土</text>
        <text x="24" y="${surf + 56}">土壤</text>
        <text x="24" y="${surf + 118}">岩层</text>
        <text x="24" y="${surf + 196}">基岩</text>
        <text x="196" y="${surf + 158}" fill="#5f9fc0">地下水</text>
        <text x="212" y="${surf + 92}" fill="#c79a54">矿脉</text>
        <text x="286" y="${surf - 6}" fill="#e0a33e">${form.name} · ${CRE.climate}</text>
      </g>
    </svg>`;
  }

  UI.renderCreator = function () {
    const el = document.getElementById('creator');
    if (!el) return;
    const form = CRE.form;
    const traits = CRE.traits.map(i => WD.traits[i]);

    el.innerHTML = `
      <div id="cTop">
        <div class="col" style="line-height:1.25">
          <span class="mono" style="font-size:10px;color:var(--ink3);letter-spacing:.18em">新的一界</span>
          <span class="mono" style="font-size:22px;color:#fff">${CRE.name}</span>
        </div>
        <div class="col gap4" style="margin-left:8px">
          <div class="row gap4"><span class="tag a">${form.name}</span>
            <span class="tag b">${CRE.climate}</span><span class="tag">${D.total} 张卡</span></div>
          <span class="xs">${form.note}</span>
        </div>
        <span class="xs" style="margin-left:auto">已重掷 ${CRE.rolls} 次</span>
        <div class="row gap6" style="border:1px solid var(--line2);padding:5px 9px;border-radius:3px">
          <span class="xs">坐标</span>
          <span class="mono" style="font-size:12.5px;color:var(--acc)">${CRE.seed.join(' - ')}</span>
          <button class="btn ghost" data-c="copy" style="height:22px">复制</button>
        </div>
        <button class="btn ghost" data-c="reroll">重掷</button>
        <button class="btn on" data-c="enter" style="height:36px;padding:0 18px">进入 ▸</button>
      </div>

      <div id="cMain">
        <div id="cLeft">
          <div class="row" style="justify-content:space-between;align-items:baseline;margin-bottom:6px">
            <span class="xs">掷骰 · 7 组</span><span class="xs" style="color:#3f4b58">不可选 · 悬停看细节</span>
          </div>
          ${rollStrip()}
          <div class="row" style="justify-content:space-between;align-items:baseline;margin:12px 0 6px">
            <span class="xs">顶视</span><span class="mono xs" style="color:#3f4b58">1 : 1</span>
          </div>
          ${miniMap(form)}
        </div>

        <div id="cCenter">
          <div id="cutaway">${cutaway(form, traits.map(t => t.name))}<div id="cvHint"></div></div>
          <div class="row gap6" style="align-items:baseline">
            <span class="mono" style="font-size:17px;color:#fff">${CRE.name}</span>
            <span class="tag a">${form.name}</span><span class="tag b">${CRE.climate}</span>
            <span class="tag ok">判据 ${WD.judge.length} / ${WD.judge.length}</span>
            <span class="sm" style="margin-left:auto;color:#5d6b7a">世界仍然从虚无开始——形制只是蓝图</span>
          </div>
          <div class="legend">
            ${[['#4a6b3f', '表层土'], ['#3b3226', '土壤'], ['#39424e', '岩层'], ['#232a33', '基岩'], ['#1d4a5c', '地下水'], ['#c79a54', '矿脉']]
              .map(([c, n]) => `<span class="xs"><i style="background:${c}"></i>${n}</span>`).join('')}
          </div>
        </div>

        <div id="cRight">
          <div class="row" style="justify-content:space-between;align-items:baseline;margin-bottom:6px">
            <span class="xs">本界特征 · ${traits.length} 条</span>
            <span class="xs" style="color:#3f4b58">悬停：亮图上 / 出说明</span>
          </div>
          <div class="tgrid">
            ${traits.map(t => `<div class="tchip on ${t.wired ? 'live' : 'ghost'}"
              title="${t.name}｜${t.note}（${t.cat}）${t.wired ? '' : ' · 仅剖面，本界不结算'}" data-cv="${t.name}">
              <span class="ti">${t.ico}</span><span class="tn">${t.name}</span>${t.wired ? '<em class="tw">生效</em>' : ''}</div>`).join('')}
          </div>

          <div class="row" style="justify-content:space-between;align-items:baseline;margin:14px 0 6px">
            <span class="xs">成立性判据</span>
            <span class="mono xs" style="color:var(--ok)">${WD.judge.length} / ${WD.judge.length}</span>
          </div>
          <div class="chk">${WD.judge.map(t => `<i class="on" title="${t} · PASS"></i>`).join('')}</div>

          <div class="xs" style="margin:16px 0 6px">掷骰结果 → 世界读数</div>
          <div style="line-height:1.9">
            <div class="rowline"><span class="xs">水覆盖</span><span class="mono xs">${CRE.params.water}</span></div>
            <div class="rowline"><span class="xs">山系数</span><span class="mono xs">${CRE.params.ranges}</span></div>
            <div class="rowline"><span class="xs">起伏</span><span class="mono xs">${CRE.params.relief}</span></div>
            <div class="rowline"><span class="xs">物种池</span><span class="mono xs">${CRE.params.species} 种</span></div>
          </div>
          <div class="xs" style="margin-top:10px;line-height:1.7">
            标「生效」的四项真进结算：地热（全界 +0.5 °C）、贫瘠（生长 ×0.7）、
            隔离（繁殖 ×0.8）、金属富集（新铺的地 35 % 含矿）。
            没标的那几条只在剖面图里出现——这一界里它不结算，只是地貌传闻。
          </div>
        </div>
      </div>

      <div id="cFoot">
        <span class="xs">你只能重掷——形制、气候、特征都不由你挑；同一坐标 = 同一个世界</span>
        <button class="btn ghost" data-c="reroll" style="margin-left:auto">重掷一颗骰子 ⟳</button>
        <button class="btn on" data-c="enter" style="height:34px;padding:0 22px">进入这一界 ▸</button>
      </div>`;

    el.onclick = ev => {
      const b = ev.target.closest('[data-c]');
      if (!b) return;
      if (b.dataset.c === 'reroll') return UI.rerollCreator();
      if (b.dataset.c === 'enter') return UI.enterWorld();
      if (b.dataset.c === 'copy') {
        const t = CRE.seed.join('-');
        if (navigator.clipboard) navigator.clipboard.writeText(t);
        UI.toast('坐标已复制', t + ' — 同一串就是同一界');
      }
    };
    el.onmouseover = ev => {
      const t = ev.target.closest('[data-cv]');
      UI.cvHi(t ? t.dataset.cv : null);
    };
    el.onmouseout = ev => { if (ev.target.closest('[data-cv]')) UI.cvHi(null); };
  };

  UI.cvHi = function (name) {
    const box = document.getElementById('cutaway');
    if (!box) return;
    let hit = false;
    box.querySelectorAll('.cvt').forEach(g => {
      const match = name && g.getAttribute('data-x') === name;
      g.classList.toggle('hl', !!match);
      if (match) hit = true;
    });
    box.classList.toggle('dim', !!name && hit);
    const hint = document.getElementById('cvHint');
    if (hint) {
      hint.textContent = name && hit ? name + ' · 图上已高亮' : '';
      hint.classList.toggle('on', !!(name && hit));
    }
  };

  UI.rerollCreator = function () {
    UI.rollWorld();
    UI.renderCreator();
    UI.log('重掷：' + CRE.name + ' · ' + CRE.form.name + ' · ' + CRE.climate, true);
  };

  UI.enterWorld = function () {
    const form = CRE.form;
    const world = {
      name: CRE.name, form: form.name, climate: CRE.climate,
      seed: CRE.seed, traits: CRE.traits.map(i => WD.traits[i].name)
    };
    S.startWorld(world);
    S.resetClock();
    UI.closeCreator();
    S.dealHand();
    UI.renderHand();
    UI.updateHud();
    UI.renderRight();

    /* 3D 侧：清空、换蓝图、相机回原点 */
    SOL.render.clear();
    SOL.render.trace.reset();
    SOL.render.setBlueprint(form);
    SOL.render.cam.target.set(0, 0.22, 0);
    SOL.render.cam.setPitch(1);
    SOL.render.cam.setZoom(2);
    SOL.render.cam.place();

    UI.toast('进入这一界', world.name + ' · 天是空的，先打一张「土地」');
    S.setGear(2);          // 1×
    UI.syncGears();
  };

})(SOL.ui, SOL.sim, SOL.data);
