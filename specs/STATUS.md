---
id: STATUS
title: 实装现状
status: active
---

# 实装现状（2026-10-27）

> 只记"现在能跑到哪"，不写规格。规格在各域文档里。

---

## §1 原型

| 原型 | 入口 | 状态 | 实装内容 |
|---|---|---|---|
| 至日 · 3D 世界版 | `prototypes/world-3d/index.html` | 可玩 | 21 张卡 / 11 个物种 / 格地形 + 立牌 / 生态 tick / 手牌 · 开包 · 回收 / 拖卡落点 / 相机格点 |
| 至日 · 界面全流程版 | `prototypes/ui-flow/index.html` | 可点 | 创世（四组参数滚轴）→ 世界（顶栏 + 左功能栏 + 右信息栏 + 手牌）→ 卡库 / 年表 / 事件 / 余痕 / 生态环 / 拖卡 |
| `alt/*` 五个 | `prototypes/alt/<代号>/index.html` | **冻结** | 见各自 README；不再修改 |

两个至日原型是**两套独立实现**：3D 版是 Three.js + DOM 卡面，界面版是纯 SVG 平面图 + 面板。二者卡表与卡面写法不同，没有共享代码。

---

## §2 数字口径（说话前先确认层级）

| 层 | 数字 | 出处 |
|---|---|---|
| 目标卡池 | **540** | [CARD-INDEX.md](CARD-INDEX.md) |
| 组件映射口径 | **710** | [CARD-COMPONENT.md](CARD-COMPONENT.md) |
| 已填合成链 | **75** | [CARD-TREE.md](CARD-TREE.md) |
| 原型实装 | **21** | `prototypes/world-3d/index.html` 的 `CARDS` |

**任何"卡有多少张"都必须带层级**，裸说一个数字就是错。

物种同口径：3D 版实装 11 个（`SPECIES`），设定目标见 [SETTING.md](SETTING.md)。

---

## §3 已知缺口（按影响排序）

1. **单文件无边界**：`world-3d/index.html` 1283 行里混着卡表、生态、Three.js 渲染、卡面 CSS。没有任何模块接缝，转正必须重切。
2. **两套实现互不对账**：3D 版 21 张卡、界面版走另一套卡面表（`ELC` / `FX` / `SIGIL` / `ART`），同一张卡在两边的字段与文案可能已经分叉。
3. **卡池几乎没实装**：540 张目标里 21 张有数据（3.9 %），且这 21 张未按 CARD-SCHEMA 的字段表完整填写。
4. **无存档、无音频、无确定性回归**：3D 版没有 localStorage；两个至日原型都没有可复现的状态摘要（alt 里只有 `strand` 有存档）。
5. **卡面尺寸硬编码**：3D 版写死 302×440 与手牌 260×380，与 [specs/ui/CARD-LAYOUTS.html](ui/CARD-LAYOUTS.html) 的三档实测尚未对账。

---

## §4 下一步（未排期，仅记录）

- [ ] 定转正形态：3D 世界 + 面板信息架构合并，还是先只做其中一半
- [ ] 按 [TECH.md](TECH.md) §3 切 `src/sim` 与 `src/render`，先把卡结算与生态抽成可 node 跑的纯逻辑
- [ ] 把 21 张卡导出成 `src/data/cards.json`，字段按 [CARD-SCHEMA.md](CARD-SCHEMA.md) 补齐
- [ ] 给 `sim` 补确定性回归（固定种子跑两次，状态摘要一致）
