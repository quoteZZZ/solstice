---
id: FILL
title: 填充流程
status: active
---

# 填充流程

> 这份文档只讲**怎么做**：改哪个文件、跑哪条命令、怎么算做对了。
> 讲**是什么**的在各域文档（[SETTING.md](SETTING.md)、[CARD-SCHEMA.md](CARD-SCHEMA.md)）；
> 讲**做到哪了**的在 [STATUS.md](STATUS.md)。
>
> 长期工作量集中在两件事上：把卡池从 55 张填到 540 张、把图鉴从 6 条填到 60 条。
> 所以这两条流程写得最细。

---

## 0 开工前：四条命令

在 `prototypes/solstice/` 下：

| 命令 | 管什么 | 期望 |
|---|---|---|
| `npm run cards` | 卡表契约（[CARD-SCHEMA.md](CARD-SCHEMA.md) §8「校验」） | `PASS · 9 项`，硬错误 0 |
| `npm run det` | 确定性：同种子两跑摘要一致 | `PASS · 6 项` |
| `npm run codex` | 图鉴每条判定能不能真被玩出来 | `PASS · 14 项` |
| `npm test` | 浏览器冒烟（要本机 Edge） | `"errors": []` |

在仓库根：

| 命令 | 管什么 | 期望 |
|---|---|---|
| `python tools/docs-check.py` | 断链 / 章节号 / 引用是否带标题片段 | `错误 0 条` |

**改数据只跑 `cards` 就够；改 sim 必须再跑 `det`；改 UI 必须再跑 `npm test`。**
CI（`.github/workflows/check.yml`）跑前三项 + `docs-check`，不跑浏览器那项。

---

## 1 加一张卡

### 1.1 改哪

1. **定 id**：`层.元素.环.类.slug`（[CARD-SCHEMA.md](CARD-SCHEMA.md) §1「身份 `id`」）。
   例：`flora.wood.2.matter.fern`。层 ∈ terrain / mineral / flora / fauna / sky / rite / law；类 ∈ matter / rite / law。
2. **写进对应文件**：物 → `data/cards.matter.js`，术 → `data/cards.rite.js`，律 → `data/cards.law.js`。
   插在同类区块里，别插到别的元素段中间（不影响功能，但下一个读的人会找不到）。
3. **字段一次写全**（共 18 个，缺哪个渲染就少一块）：
   `id / name{zh,en} / title / slug / elements / tier / class / line{fx,cond,num,hl} / radius_w / value /
   stats[] / lore[] / effect[] / rel[] / usage / tips[] / flavor{zh} / art{sigil,palette} / runtime{tags} / sim{op}`
4. **`sim.op` 决定它在世界里做什么**——这是唯一会被结算读到的字段。写了 `sim` 却没有结算读点，等于只做了一张好看的图。

### 1.2 跑哪

```bash
cd prototypes/solstice && npm run cards
```

### 1.3 怎么算做对了

- **硬错误 0**。四条最容易踩的：
  - `line.fx` > 40 字 → 卡面被裁。
  - `line.hl` 里写了 `fx` 里没有的词 → 高亮静默消失（**这条最阴，不报错就看不见**）。
  - `line.num` 超过 4 条 → 版式只有 4 格。
  - `stats[].bind` 写了不在 `BINDS` 里的名字。
- 警告可以留，但要知道自己留了什么（`npm run cards` 会打印）。
- 浏览器里开一次：卡在卡库出现、能抽到、能落下、卡面四块都在。

### 1.4 常见坑

- **不要手写 `number` / `atk` / `def` / `reclaim`**：全部由 `data/index.js` 派生，写进去会被覆盖，纯属噪音。
- **`rel[].to` 指向还没写的卡**：允许，会进警告。但填完那一批要回头清。
- **改了 `data/` 就顺手看 `CARD-INDEX.md`**：那份是 540 张的目标清单，新卡的 id/环要与它一致，
  不一致要么改卡、要么改清单——**不许两边各说一套**（[STATUS.md](STATUS.md) §3「已知缺口」第 2 条记的就是这件事）。

---

## 2 加一条图鉴成就

### 2.1 改哪

1. `sim/codex.js` 的 `S.CODEX` 数组加一条：
   ```js
   { id: 'eco.xxx', grp: '生态', name: '成就名', hint: '给玩家看的一句话判据',
     need: 24,                       // 需要「连续 N 月」才填；瞬时成立的不要填
     test: f => f.plantCells >= 3 }  // f 是 scan() 的产出，只读世界
   ```
2. `test` 只能读 `scan()` 给的 `f` 或 `W`，**不许开随机流、不许改世界**（图鉴是纯函数，才敢说不影响确定性）。
3. `grp` 用现有的六组之一（天文 / 水文 / 生态 / …），新组要同次在 `S.CODEX_GROUPS` 登记。

### 2.2 跑哪

```bash
npm run codex
```

### 2.3 怎么算做对了

`tests/codex.js` 的规矩是**每条判定都要有一个能点亮它的剧本**，所以新条目必须同时在
`tests/codex.js` 里加一段「怎么玩才能点亮」的断言，否则这条成就会永远点不亮而没人发现。
反向也要一条：**条件不满足时不许点亮**（否则是白送）。

- 带 `need` 的条目只在 `tick` 路径累加，`cast` 路径只判瞬时——否则连打两张卡就能把「连续 12 月」灌满。
- 换界时：点亮记录保留（账号级），**连续月计数归零**。

---

## 3 加一个世界事件

1. `sim/events.js` 的 `EVENTS` 数组加一条：`k`（禁物 / 灾变 / 文明）、`t` 标题、`a` 图标 key、`b` 描述、`tags[]`、
   `apply()`（承受的后果）、`rewrite()`（改写的后果）。
2. **效果写在 sim 里**，不许写到 ui——见 [TECH.md](TECH.md) §3.1「五条硬约束」第 3 条。
3. 取格子用 `anyCell()` / `someCells(n)`（内部走 `S.randInt`）。
   **不许用 `Math.random()`**：同一事件在不同帧点开必须得到同一后果。
4. 新图标要在 `ui/events.js` 的 `evArt()` 里加一条 SVG，否则显示空白。

跑 `npm test`，`panels.event` 必须是 `true`。

---

## 4 加一个创世特征

1. `data/world.js` 的 `traits` 加一条。
2. 要它真的影响世界，就在 `sim/time.js` 的 `S.startWorld` 里给 `W.effects` 赋值，并在
   `data/world.js` 对应条目上标 `wired: true`——没标的会在创世界面标成「仅剖面」。
3. `STATUS.md` §3「已知缺口」第 6 条记着现在只有 4 / 10 条 `wired`，加一条就改那个数。

跑 `npm run det`（改的是开局结算，必须验确定性）。

---

## 5 提交前

1. 四条命令全绿（§0）。
2. 引用别档章节要带标题片段（[README.md](README.md) §3「改文档的规矩」第 7 条），`docs-check` 会拦。
3. 改了什么就同步改哪里：实装变化 → `STATUS.md`；规格变化 → 对应域文档；新增/作废文档 → `README` §2 的表。
4. 提交信息：英文简短祈使句；文档与代码同次提交（[README.md](README.md) §3「改文档的规矩」第 5 条）。
