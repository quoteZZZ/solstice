---
id: README
title: 文档入口
status: active
---

# Solstice · 文档入口

> 卡片驱动的造物沙盒：从虚无起手，抽卡造世界，然后看它自己运转。
> 主线是《至日》；`prototypes/alt/` 下五个机制试吃不进主线。

---

## 1 分工：真源 / 投影 / 原型

| 类 | 是什么 | 能不能当依据 |
|---|---|---|
| **规格** `specs/*.md` | 设定、卡池、字段、时间、机制借鉴、技术约定 | 是 |
| **投影** `specs/ui/*.html` | 线框、卡面版式、单卡稿 | 否，只是规格的可读版本 |
| **原型** `prototypes/**` | 可跑的前端验证物 | 验证行为，不定义规格 |

冲突时按 **规格 md > 投影 html > 原型** 取，并把差异记进对应规格文档或 [`STATUS.md`](STATUS.md)，不要私自改一边。

---

## 2 文档地图

### 2.1 立项与现状

| 文档 | 管什么 |
|---|---|
| **[CHARTER.md](CHARTER.md)** | 定位、三条支柱、非目标、已否决（含五个试吃原型为什么不进主线） |
| **[STATUS.md](STATUS.md)** | 实装现状、四层数字口径、已知缺口、下一步 |
| **[REVIEW-2026-11-03.md](REVIEW-2026-11-03.md)** | 六角色评审与裁决记录：证据、分歧、优先级。结论已落进各域文档，本文只留过程 |
| **[REVIEW-AUDIT-2026-11-03.md](REVIEW-AUDIT-2026-11-03.md)** | **评审机制的复盘**（不是评 Solstice）：运行结果、各角色表现、11 条问题与 v2 补丁。评项目看上一行，改机制看这一行 |

### 2.2 世界与时间

| 文档 | 管什么 |
|---|---|
| **[SETTING.md](SETTING.md)** | 设定集（单一真源）：虚无开局、世界的大小由卡决定、卡的六层、判据与组件 |
| **[TIME.md](TIME.md)** | 时间只由 UI 控制（暂停 / 五档加速），卡不涉及时间 |

### 2.3 卡

| 文档 | 管什么 | 层级 |
|---|---|---|
| **[CARD-TREE.md](CARD-TREE.md)** | 合成树：环级 = 合成深度，图鉴兼工作台 | 已填 75 张 |
| **[CARD-INDEX.md](CARD-INDEX.md)** | 卡总目录，按类分组 | 目标 540 张 |
| **[CARD-COMPONENT.md](CARD-COMPONENT.md)** | 卡 → 组件映射（25 个组件、点数消耗） | 目标 710 张 |
| **[CARD-SCHEMA.md](CARD-SCHEMA.md)** | 字段表：卡面 / 悬浮 / 图鉴的字段来源 | 唯一真源 |

### 2.4 技术与借鉴

| 文档 | 管什么 |
|---|---|
| **[TECH.md](TECH.md)** | 原型约定（离线、单文件、互不引用）+ 转正后 `game/` 的分层与五条硬约束 |
| **[FILL.md](FILL.md)** | **填充流程**：加一张卡 / 一条成就 / 一个事件 / 一个特征，改哪、跑哪、怎么验 |
| **[GAME-SCAN.md](GAME-SCAN.md)** | 32 部卡牌游戏的机制扫描：抄什么 / 不抄什么 |

### 2.5 投影（`specs/ui/`）

| 文件 | 是什么 |
|---|---|
| **[WIREFRAMES.html](ui/WIREFRAMES.html)** | 静态线框板：抽屉 / 卡比例 / 设置 / 键位 / 视角 / 缩放 / 形制 |
| **[CARD-LAYOUTS.html](ui/CARD-LAYOUTS.html)** | 卡面版式四套方案 + 三档尺寸实测 |
| **[CARD-NIGHTBIRD.html](ui/CARD-NIGHTBIRD.html)** | 「夜沼鸟」单张卡的完整设计稿（CARD-SCHEMA 的实例） |

原型入口见 [`../prototypes/README.md`](../prototypes/README.md)。

---

## 3 改文档的规矩

1. **数字只写一处**：规格数字写进对应域文档，别在第二份文档里抄一遍；引用时带层级（目标 / 已填 / 实装）。
2. **markdown 是依据，HTML 是投影**：改规格先改 md，再回头改投影。
3. **实装变化写 STATUS**：代码跑到哪、缺口新增或修掉，只改 `STATUS.md`，不往规格里混。
4. **评审不动规格**：过程进 `REVIEW-*.md`，裁决按域落到对应文档（设定落 SETTING、缺口落 STATUS），不在评审文里改规格。
5. **提交**：英文简短祈使句；文档与代码同次提交。
6. **新增 / 作废文档**：同次改本文 §2 的表。
7. **引用别档章节，必须带标题片段**：写成 [TIME.md](TIME.md) §3「为什么时间不能做成卡」，不许只写 `§3`。
   因为「号存在」和「指对了」是两件事——A7 曾把「特征改乘数」指向 SETTING §2.8，号在、内容是「卡数的分配原则」，
   只查存在性永远抓不到。裸引用会进 `docs-check` 的警告清单，逐步改成带片段的写法。
8. **卡表改动必须过闸**：改 `data/cards.*.js` 后跑 `npm run cards`。
   [CARD-SCHEMA.md](CARD-SCHEMA.md) §8「校验」里的硬错误会让 `data/index.js` 直接抛错、装配不下去——
   这是有意的：坏数据进内存比当场报错贵得多。
9. **提交前自检**（CI 与本地同一条）：
   `python tools/docs-check.py`（断链 / 章节号 / 引用片段）、
   `cd prototypes/solstice && npm run cards && npm run det && npm run codex`。
