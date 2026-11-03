/* 至日 · 命令层：UI → sim 的唯一写入口
   TECH.md §3.1 第三条「一个状态一个真源，ui 只读不写」——这一层就是它的实现。
   ui 可以随便读（S.W / S.at / S.lifeStats…），但**改**只能走这里：

     S.cmd.cast / playSlot / recycle / reroll / select / dealHand
     S.cmd.gear / nextGear / tick
     S.cmd.seed / newWorld
     S.cmd.drawPack / spawn / awaken / chron

   把这些包起来不是为了好看：一是有地方做参数兜底（施卡序号越界、落点缺失），
   二是把「多步才能完成的一件事」收成一个原子动作（施卡＝放 + 消耗 + 修选中），
   以后做命令日志 / 回放 / 第二个前端，只需要认这一个面。 */
(function (S, D) {
  const W = S.W;
  const cmd = S.cmd = S.cmd || {};   // 合并而非覆盖：sim/events.js 也往这一面挂命令

  /* ───────── 世界 ───────── */
  cmd.seed = function (s) { return S.setSeed(s); };

  /* 入界：开局 + 归零表现钟 + 发手牌。三件事必须一起发生，分开放就会漏。 */
  cmd.newWorld = function (world) {
    S.startWorld(world);
    S.resetClock();
    S.dealHand();
    return world;
  };

  /* ───────── 手牌 ───────── */
  cmd.cast = function (card, x, y) { return S.cast(card, x, y); };

  /* 施一张手牌：放 + 消耗 + 把选中夹回合法范围。旧 UI 三步分开写，漏掉任一步就出脏状态。 */
  cmd.playSlot = function (i, x, y) {
    const card = W.hand[i];
    if (!card) return { ok: false, msg: '没有这张牌' };
    const res = S.cast(card, x, y);
    if (!res.ok) return res;
    S.consume(i);
    cmd.select(W.sel);
    return res;
  };

  cmd.select = function (i) {
    const n = W.hand.length;
    W.sel = n ? Math.max(0, Math.min(n - 1, i | 0)) : -1;
    return W.sel;
  };

  cmd.recycle = function (i) { return S.recycle(i == null ? W.sel : i); };
  cmd.dealHand = function () { return S.dealHand(); };

  /* 重抽：先看价、再扣、再发。扣时素从前写在 ui 里，是 ui 唯一改世界钱包的地方。 */
  cmd.reroll = function () {
    const COST = 5;
    if (W.essence < COST) return { ok: false, msg: '时素不够（重抽要 ' + COST + '）' };
    W.essence -= COST;
    S.dealHand();
    return { ok: true, cost: COST };
  };

  /* ───────── 时间 ───────── */
  cmd.gear = function (i) { return S.setGear(i); };
  cmd.nextGear = function () { return S.nextGear(); };
  cmd.tick = function (n) { for (let i = 0; i < (n || 1); i++) S.tick(); return W.month; };

  /* 每帧推进：主循环在 ui/boot.js，但「推时间」这个动作照样走命令层，
     ui 里不出现 S.advance —— 换前端时只需实现一个主循环去调它。 */
  cmd.advance = function (dt) { return S.advance(dt); };

  /* ───────── 卡包 / 事件 ───────── */
  cmd.drawPack = function () { return S.drawPack(); };

  /* 按 id 落一张卡（事件用）：ui 不该拿着 D.byId 自己去拼 sim 的落点。 */
  cmd.spawn = function (id, cell, opts) {
    const card = D.byId.get(id);
    if (!card) return { ok: false, msg: '没有这张卡：' + id };
    if (!cell) return { ok: false, msg: '没有落点' };
    return { ok: true, ent: S.spawn(card, cell, opts) };
  };

  cmd.awaken = function () { W.sapient = Math.max(1, W.sapient); return W.sapient; };
  cmd.chron = function (kind, text) { S.addChron(kind, text); return W.chron.length; };

})(SOL.sim, SOL.data);
