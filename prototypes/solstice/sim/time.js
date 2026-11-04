/* Solstice · 时间：月 tick + 五档（TIME.md §1–§2）
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

    /* 肥力自然消退：两季左右回落到 0 */
    for (const c of W.cells.values()) if (c.fertile) c.fertile *= 0.97;

    /* mods 是「此刻的条件」，不是永久属性。卡与事件往里写的偏移按月往 0 回落。
       不回落的后果：同一张卡叠十次就等于永久改了这格地形，
       而一次「长夜」事件（全界 mods.t −1）会把世界永久压暗，没有任何办法扳回来——
       年表里那条记录还留着，世界却再也回不到事件之前。
       半衰期约 4 个月：比一张卡的一生效期长一点，比一个季节短一点。 */
    for (const c of W.cells.values()) {
      const m = c.mods;
      if (m.l) m.l = Math.abs(m.l) < 0.01 ? 0 : m.l * 0.84;
      if (m.t) m.t = Math.abs(m.t) < 0.01 ? 0 : m.t * 0.84;
      if (m.evap !== 1) m.evap = Math.abs(m.evap - 1) < 0.005 ? 1 : 1 + (m.evap - 1) * 0.84;
    }

    S.checkCodex(true);

    /* 纪年：每满一年记一笔当时的世界规模。
       没有这一条，年表只剩「某张卡落在某格」，读起来像操作日志而不是历史；
       而玩家真正想知道的是「第六年这一界长成了什么样」。 */
    if (W.month % 12 === 0 && W.cells.size) {
      const life = S.lifeStats();
      /* 原来这里是 '第 ' + (W.month / 12) + ' 年'：month=30 会印出「第 2.5 年」。
         而且行首本来就有时间列，年份等于写了两遍（UI 评审 U21 / P14 / E39）。
         时间交给 UI 的 UI.mon() 统一格式化，正文只留世界读数。 */
      S.addChron('纪年', W.cells.size + ' 格 · 植物 ' + life.flora + ' 种 · 兽 ' + life.fauna + ' 种');
    }

    S.monthChanged = true;
    S.emit('tick');
  };

  /* 时素按真实时间涨：每 10 秒 1 点（SETTING.md §3.1），**不随档位加速，暂停也照涨**。
     原来挂在 tick 里（每世界月 +0.1），4× 就变成 4 倍收入 —— 于是「停下来读一张卡」
     在数值上是纯亏，最优玩法是开着 4× 不看画面。时素买的是「等」，不是「快进」。
     dtMs 由主循环传入，sim 里不读时钟（TECH.md §4）。 */
  S.ESS_MS = 10000;
  S.essenceTick = function (dtMs) {
    if (!(dtMs > 0)) return 0;
    W.essAcc += dtMs / S.ESS_MS;
    let n = 0;
    while (W.essAcc >= 1) { W.essAcc -= 1; W.essence++; n++; }
    return n;
  };

  /* 一个月多少毫秒。原来是 1000（1 秒 = 1 月），而时素是 10 秒 1 点——
     等于「买一包基础卡要 10 年世界时间」：玩家看着世界跑完一整个演替，
     手里还攒不到一包。两套节奏差 10 倍，世界必然在攒够卡之前就停滞。
     12 秒/月下，基础包 12 时素 ≈ 10 个月，进阶包 ≈ 20 个月，4× 档 3 秒一月。 */
  S.MONTH_MS = 12000;

  /* 每帧推进：dt 毫秒。1× = 12 秒 1 月（TIME.md §1）。
     昼夜不是第二个时钟，是日历的投影（TIME.md §7）：一年一昼夜，每月 2 小时，
     至日（12 月 / 6 月）是昼夜分界。这里只写表现值 W.hour；
     结算读的是整数月 S.sunHour()，所以帧率与档位都不影响生态结果。 */
  S.advance = function (dt, realtime) {
    if (W.paused) return 0;
    let ticks = 0;
    acc += dt * W.speed / S.MONTH_MS;
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
     月 0 与月 6 是至日，也是昼与夜的分界——英文名 Solstice 即由此来。
     结算只认整数月，所以同一世界的昼夜与机器、帧率、档位全都无关。 */
  S.sunHour = (m) => ((m == null ? W.month : m) % 12) * 2;
  S.night = (m) => { const h = S.sunHour(m); return h < 6 || h >= 18; };
  S.dayText = () => (S.night() ? '长夜' : '白昼');
  S.timeText = () => S.year() + ' 年 ' + (S.monthOfYear() + 1) + ' 月';

  /* ── 开局：形制只决定蓝图与相机，世界仍然是空的 ── */
  S.startWorld = function (world) {
    S.resetWorld(world);
    /* 气候给底子，特征在其上叠加。之前只有特征进结算，气候是一行纯文字——
       选「沙漠」和选「温带海洋」跑出来的世界完全一样（REVIEW-DEMO §11）。 */
    const cf = (D.world.climateFx && D.world.climateFx[world.climate]) || {};
    const ef = {
      temp: cf.temp || 0,
      grow: cf.grow == null ? 1 : cf.grow,
      propag: cf.propag == null ? 1 : cf.propag,
      rain: cf.rain == null ? 1 : cf.rain,
      evap: cf.evap == null ? 1 : cf.evap,
      light: cf.light || 0,
      metal: 0
    };
    for (const t of world.traits || []) {
      if (t === '地热活跃') ef.temp += 0.5;
      if (t === '贫瘠土壤') ef.grow *= 0.7;
      if (t === '物种隔离') ef.propag *= 0.8;
      if (t === '金属富集') ef.metal += 1;
    }
    W.effects = ef;
    S.resetClock();
    S.log('新的一界：' + world.name + ' · ' + world.form + ' · ' + world.climate, true);
    S.addChron('气候', '「' + world.name + '」成形 · ' + world.climate);
  };

})(SOL.sim, SOL.data);
