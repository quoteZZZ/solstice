/* Solstice · 图鉴：世界状态成就（SETTING.md §5.1）
   收的不是卡，是「世界状态」。判定一律从 W 现场读，不写死任何卡名——
   一条成就只要读得到状态，就一定能被截图证明。

   图鉴是账号级的：S.resetWorld 不清空 A.on（换界不丢页），只清连续月计数。
   本轮只实装 3 组 6 条（v1 下限），未实装的组不列出来，也不留灰格。 */
(function (S) {
  const W = S.W;

  const A = S.codex = {
    on: new Set(),      // 已点亮的 id
    at: {},             // id → { month, world }：在哪个世界的第几月点亮
    st: {}              // id → 连续月计数（只给带 need 的条目用）
  };

  /* 一次遍历同时喂 6 条判定。
     每条各扫一遍世界不会有第二种说法，但会慢 6 倍；这里只有一个 scan()。 */
  function scan() {
    const f = { cells: 0, plantCells: 0, minW: 9, maxW: 0, dark: true, springWet: false, herb: false, carn: false };
    for (const c of W.cells.values()) {
      f.cells++;
      if (c.W < f.minW) f.minW = c.W;
      if (c.W > f.maxW) f.maxW = c.W;
      if (c.L >= 0.01) f.dark = false;
      let plant = false;
      for (const e of c.ents) {
        if (e.tags.indexOf('watersource') >= 0 && c.W > 0) f.springWet = true;
        if (e.ground) continue;
        if (e.tags.indexOf('grower') >= 0) plant = true;
        else if (e.tags.indexOf('mobile') >= 0) {
          if (e.diet === '草') f.herb = true;
          else if (e.diet) f.carn = true;
        }
      }
      if (plant) f.plantCells++;
    }
    return f;
  }

  /* need = 必须连续满足的月数；没有 need 的判「这一月成立」就算。
     阈值是 v1 首版，等数值拍板（SETTING.md §5.1 只给了示例判据）。 */
  S.CODEX = [
    { id: 'sky.three', grp: '天文', name: '三阳', hint: '同月太阳源 ≥ 3',
      test: f => W.sun >= 3 },
    { id: 'sky.dark', grp: '天文', name: '长夜', hint: '有植物，且连续 12 月全格无光', need: 12,
      test: f => f.cells >= 6 && f.plantCells > 0 && f.dark },

    { id: 'aqua.hold', grp: '水文', name: '长存', hint: '水源格连续 24 月有水', need: 24,
      test: f => f.springWet },
    { id: 'aqua.split', grp: '水文', name: '旱涝同界', hint: '同月里一格含水 ≥ 2、另一格 ≤ 0.05',
      test: f => f.cells >= 4 && f.maxW >= 2 && f.minW <= 0.05 },

    { id: 'eco.forest', grp: '生态', name: '成林', hint: '植物格 ≥ 世界格数的一半',
      test: f => f.cells >= 6 && f.plantCells * 2 >= f.cells },
    { id: 'eco.chain', grp: '生态', name: '三级链', hint: '植物 · 食草 · 食肉 同月共存',
      test: f => f.plantCells > 0 && f.herb && f.carn }
  ];

  S.CODEX_GROUPS = ['天文', '水文', '生态'];

  S.codexCount = () => A.on.size;
  S.codexOf = grp => S.CODEX.filter(a => a.grp === grp);

  /* 换界：连续月计数归零，已点亮的页不撕 */
  S.codexReset = function () { A.st = {}; };

  /* fromTick：只有世界走了一月，才允许给「连续 N 月」计数。
     施卡也调这里（三阳要当刻亮），但施卡不替世界数月份，
     否则在 11 个月里连打两张卡就能凑满 12 月。 */
  S.checkCodex = function (fromTick) {
    const f = scan();
    const lit = [];
    for (const a of S.CODEX) {
      if (A.on.has(a.id)) continue;
      const ok = a.test(f);
      if (a.need) {
        if (!fromTick) continue;
        A.st[a.id] = ok ? (A.st[a.id] || 0) + 1 : 0;
        if (A.st[a.id] < a.need) continue;
      } else if (!ok) continue;

      A.on.add(a.id);
      A.at[a.id] = { month: W.month, world: (W.world && W.world.name) || '—' };
      S.log('图鉴点亮 · ' + a.name, true);
      S.addChron('图鉴', '「' + a.name + '」点亮（' + a.grp + '）');
      lit.push(a);
    }
    if (lit.length) S.emit('codex', lit);
    return lit.length ? lit : null;
  };

})(SOL.sim);
