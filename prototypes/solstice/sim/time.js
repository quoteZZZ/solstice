/* 至日 · 时间：月 tick + 五档（TIME.md §1–§2）
   时间只由 UI 控制，卡不涉及时间——本文件是唯一推进世界的地方。 */
(function (S, D) {
  const W = S.W;

  /* TIME.md §2：暂停 / 0.5 / 1 / 2 / 4，没有别的档位 */
  S.GEARS = [
    { label: '暂停', speed: 0 },
    { label: '0.5×', speed: 0.5 },
    { label: '1×', speed: 1 },
    { label: '2×', speed: 2 },
    { label: '4×', speed: 4 }
  ];

  let acc = 0;

  S.setGear = function (i) {
    const g = S.GEARS[Math.max(0, Math.min(S.GEARS.length - 1, i))];
    W.paused = g.speed === 0;
    W.speed = g.speed || 1;
    S.gear = i;
    return g;
  };
  S.gear = 0;
  S.setGear(0);

  S.nextGear = function () { S.setGear((S.gear + 1) % S.GEARS.length); return S.GEARS[S.gear]; };

  S.tick = function () {
    W.month++;
    S.computeEnv();
    S.entityTick();
    S.expireTraces();

    W.essAcc += 0.1;
    if (W.essAcc >= 1) { W.essAcc -= 1; W.essence++; }

    /* 肥力自然消退：两季左右回落到 0 */
    for (const c of W.cells.values()) if (c.fertile) c.fertile *= 0.97;

    S.checkCodex(true);

    S.monthChanged = true;
    S.emit('tick');
  };

  /* 每帧推进：dt 毫秒。1× = 1 秒 1 月（TIME.md §1）。
     昼夜不是第二个时钟，是日历的投影（TIME.md §7）：一年一昼夜，每月 2 小时，
     至日（12 月 / 6 月）是昼夜分界。这里只写表现值 W.hour；
     结算读的是整数月 S.sunHour()，所以帧率与档位都不影响生态结果。 */
  S.advance = function (dt, realtime) {
    if (W.paused) return 0;
    let ticks = 0;
    acc += dt * W.speed / 1000;
    while (acc >= 1 && ticks < 8) { acc -= 1; S.tick(); ticks++; }
    if (ticks === 8) acc = 0;

    W.hour = S.sunHour() + Math.min(0.999, acc) * 2;
    return ticks;
  };

  S.resetClock = function () { acc = 0; W.hour = S.sunHour(); };

  /* 季节 / 年 都由 month 派生 */
  S.year = () => Math.floor(W.month / 12);
  S.monthOfYear = () => W.month % 12;
  S.season = () => ['春', '春', '春', '夏', '夏', '夏', '秋', '秋', '秋', '冬', '冬', '冬'][W.month % 12];
  /* 至日日历：1 月 = 2 小时，一年走完一昼夜。
     月 0 与月 6 是至日，也是昼与夜的分界——「至日」就是这一作的名字。
     结算只认整数月，所以同一世界的昼夜与机器、帧率、档位全都无关。 */
  S.sunHour = (m) => ((m == null ? W.month : m) % 12) * 2;
  S.night = (m) => { const h = S.sunHour(m); return h < 6 || h >= 18; };
  S.dayText = () => (S.night() ? '长夜' : '白昼');
  S.timeText = () => S.year() + ' 年 ' + (S.monthOfYear() + 1) + ' 月';

  /* ── 开局：形制只决定蓝图与相机，世界仍然是空的 ── */
  S.startWorld = function (world) {
    S.resetWorld(world);
    const ef = { temp: 0, grow: 1, propag: 1, metal: 0 };
    for (const t of world.traits || []) {
      if (t === '地热活跃') ef.temp += 0.5;
      if (t === '贫瘠土壤') ef.grow *= 0.7;
      if (t === '物种隔离') ef.propag *= 0.8;
      if (t === '金属富集') ef.metal += 1;
    }
    W.effects = ef;
    S.resetClock();
    S.log('新的一界：' + world.name + ' · ' + world.form + ' · ' + world.climate, true);
    S.addChron('气候', '「' + world.name + '」成形');
  };

})(SOL.sim, SOL.data);
