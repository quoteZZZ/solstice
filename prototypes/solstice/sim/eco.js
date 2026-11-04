/* Solstice · 生态结算：水土光温四场 + 逐实体 tick
   所有数字都从实体 tags 与格的 mods/flags 推出来，不看卡名。 */
(function (S) {
  const W = S.W;

  const has = (e, t) => e.tags.indexOf(t) >= 0;

  /* ───────────────── 环境四场 ───────────────── */
  S.computeEnv = function () {
    const springs = [];
    for (const c of W.cells.values())
      for (const e of c.ents) if (has(e, 'watersource')) springs.push(c);

    /* 气候通道只跟世界有关，跟格无关——放在循环外取一次，别在 1200 格里反复取属性 */
    const fx = W.effects || {};
    const fxTemp = (fx.temp || 0) + (W.sunTemp || 0);
    const fxLight = fx.light || 0;
    const fxRain = fx.rain == null ? 1 : fx.rain;
    const fxEvap = fx.evap == null ? 1 : fx.evap;
    /* 每月的基础降水。这个数字不是随手写的：蒸发是 0.34 × (W/3) × (T−10)/10，
       温带（T≈27）下等于 0.19×W，所以降水必须大于 0.19 才可能把水撑到 1 以上。
       原来是 0.12 —— 平衡点落在 0.63，而植物要 W ≥ 1 才长：
       于是「离水源两格以外的地方永远是干的」，整界只有 13 格能活，
       世界在第 12 个月就完全静止（120 月实测：植物 1 种、兽 0 种，之后再无变化）。
       0.24 把温带的平衡点抬到 1.2 左右，沙漠（T≈35、rain ×0.25）仍然停在 0.5 以下。 */
    const RAIN = 0.24;

    for (const c of W.cells.values()) {
      let L = Math.max(0, W.sun - W.law.lightOff + fxLight);
      let heat = 0, shade = 0, keep = 1;

      for (const e of c.ents) {
        if (has(e, 'lightsource')) L += 1;
        if (has(e, 'heating')) heat += 0.5;
        if (has(e, 'shade')) shade += 0.6;
        if (has(e, 'waterkeep')) keep *= 0.55;
      }
      for (const n of S.nb(c)) for (const e of n.ents)
        if (has(e, 'shade')) shade += 0.4;

      if (c.flags.clear) shade = 0;          // 「透」：冠层在本格不算数
      L = Math.max(0, L + c.mods.l - shade * 0.35);
      const T = 20 + heat - shade * 0.5 + c.mods.t + fxTemp;

      if (c.flags.rooted) keep *= 0.6;       // 「扎根」
      if (c.flags.seep) keep *= 0.5;         // 「渗」
      if (c.flags.loose) keep *= 1.3;        // 「松」
      if (c.flags.wetland) keep *= 1.2;

      let wi = 0;
      for (const s of springs) {
        const d = Math.abs(s.x - c.x) + Math.abs(s.y - c.y);
        if (d === 0) wi += 1.00;
        else if (d === 1) wi += 0.62;
        else if (d === 2) wi += 0.26;
      }
      if (c.flags.wetland) wi += 0.15;

      const evap = 0.34 * (c.W / 3) * Math.max(0, T - 10) / 10 * keep * W.law.evapX * c.mods.evap * fxEvap;
      c.W = Math.min(3, Math.max(0, c.W + wi + RAIN * fxRain - evap));
      c.T = T; c.L = L;
      c.kind = S.classify(c);      // 地形归 sim 判：render 只读，digest 不依赖渲染帧（X-6）
    }
  };

  /* 全界的水 / 光 / 温均值。右栏那三条横条读它。
     只读，不改世界。「宜居带」的阈值不在这里定——它由 eco 真正用的判据决定
     （植物生长要 `W >= 1 && L >= 1`，见下面的 entityTick），UI 照那个写，不另编一套。 */
  S.fieldStats = function () {
    let n = 0, sw = 0, sl = 0, st = 0;
    for (const c of W.cells.values()) { n++; sw += c.W; sl += c.L; st += c.T; }
    if (!n) return null;
    return { n: n, w: sw / n, l: sl / n, t: st / n };
  };

  /* ───────────────── 觅食 ───────────────── */
  S.findFood = function (cell, pred) {
    const wantPlant = pred.diet === '草';
    const R = pred.speed >= 3 ? 2 : 1;
    const ring = [cell].concat(S.ring(cell, R));
    for (const c of ring) for (const e of c.ents) {
      if (e === pred || e.ground) continue;
      if (wantPlant) {
        if (has(e, 'edible') && has(e, 'grower')) return { cell: c, ent: e };
      } else if (e.diet === '草' && pred.size >= e.size) {
        return { cell: c, ent: e };
      }
    }
    return null;
  };

  /* ───────────────── 逐实体 ───────────────── */
  S.entityTick = function () {
    const dead = [];
    const before = S.lifeStats();

    for (const c of W.cells.values()) {
      for (const e of c.ents.slice()) {
        if (e.ground) continue;
        if (e.stun) { e.stun--; continue; }        // 「擦痕」：本月不生长

        if (e.grow) {
          const bonus = (1 + (c.fertile || 0)) * ((W.effects && W.effects.grow) || 1);
          if (c.W >= 1 && c.L >= 1) e.age += e.grow * bonus;
          if (c.W <= 0 && !c.flags.noDecay) e.hp -= 1;
        } else {
          e.age += 1;
        }

        if (e.age > e.lifespan * W.law.lifeX) { dead.push([c, e, '老死']); continue; }
        if (e.grow && c.W <= 0 && e.age > 2) { dead.push([c, e, '旱死']); continue; }

        /* 夜行 / 昼行由整数月判定（S.night），不读表现钟——
           表现钟带浮点进度，同一世界会因帧率跑出不同生态（REVIEW-2026-11-03 3.1）。 */
        const nightOnly = e.tags.includes('nocturnal');
        const dayOnly = e.tags.includes('diurnal');
        const night = S.night();
        const active = (!nightOnly || night) && (!dayOnly || !night);

        if (e.diet && !e.grow && active) {
          if (e.fed > 0) e.fed--;
          else {
            const t = S.findFood(c, e);
            if (t) {
              const i = t.cell.ents.indexOf(t.ent);
              if (i >= 0) {
                t.cell.ents.splice(i, 1);
                e.hp = Math.min((e.card.species && e.card.species.hp) || 5, e.hp + 1);
                e.fed = 4;
              }
            } else if (c.flags.noDecay) {
              /* 「不锈」的卡面原话是「饥饿不再致死，只会停下」——
                 原来只挡了旱死（上面的 hp 自然损失），饿还是会死，承诺只兑现了一半。 */
            } else {
              e.hp -= 1;
              if (e.hp <= 0) { dead.push([c, e, '饿死']); continue; }
            }
          }
        }

        /* 繁殖：受丰饶（propX）影响；propag 越大越慢。
           水位门槛按物种走：芦苇卡面写的是「水 ≥1 即生长，水 ≥1.5 才繁殖」，
           只有生长线（1）是共用的，繁殖线由 species.minW 给。 */
        const propX = W.law.propX * ((W.effects && W.effects.propag) || 1);
        if (e.propag && c.W >= (e.minW || 1) && c.L >= 1 && S.rand() < (propX / e.propag)) {
          const free = S.nb(c).filter(n => n.ents.filter(x => !x.ground).length < 2);
          if (free.length) {
            const t = free[S.randInt(free.length)];
            S.spawn(e.card, t, { age: 0 });
          }
        }
      }
    }

    for (const [c, e, why] of dead) {
      const i = c.ents.indexOf(e);
      if (i >= 0) c.ents.splice(i, 1);
      if (!c.flags.hidden && (e.size >= 3 || e.lifespan >= 100))
        S.log(e.name + ' ' + why);
    }

    /* 生态事件：物种在这一月归零 → 写年表（比"日志刷屏"值钱） */
    const after = S.lifeStats();
    for (const id of before.floraSet) if (!after.floraSet.has(id) && !before.faunaSet.has(id))
      S.addChron('生态', '「' + cardName(id) + '」从这一界消失');
    for (const id of before.faunaSet) if (!after.faunaSet.has(id))
      S.addChron('生态', '「' + cardName(id) + '」从这一界消失');
  };

  const cardName = id => (SOL.data.byId.get(id) || { name: { zh: '?' } }).name.zh;

})(SOL.sim);
