---
id: START
title: 入口（从这里开始）
status: active
truth: index
owner: both
---

# START · 从这一页开始

> **Solstice**：一只捧在手里、会自己活起来的微缩世界。玩家只有一种交互——**卡**。
> 当前阶段 **M0**：单界、512² 默认档、36 张卡、动画赛璐璐画风。
>
> **这一页是唯一入口。** 读完它就能开工，不需要先读别的。

---

## 1 现在做什么

**Batch 1「先让世界看得见」**——五张单，**按序做**，明细在 [`plan/work.md`](plan/work.md) §1：

| 单 | 做什么 | 为什么排在这 |
|---|---|---|
| `D-01` | `Paper*` → `Cel*` 改名（纯改名，不改画面） | 可回退，先把地基换掉 |
| `D-02` | Cel 3 段硬切 + 描边随深度 + 暗部抬亮 | 画面像不像动画，一半在这 |
| `D-04` | 天空：三层渐层 + 两层云带 + 天体圆盘 | **天空占画面一半**，现在是空的 |
| `D-03` | 几何：去纸层叠 / 撕裂边，地形改平涂色带 | 前两步做完才看得出对错 |
| `D-05` | 文案与本地化：把"纸"从玩家能读到的地方清掉 | 最后收尾 |

**做完这五张，出三张图（正午 / 黄昏 / 雨）给用户裁决**——这就是**闸门**。
闸门不过，`D-06`–`D-09`（尺度真化 / 去混沌 / 世界骰 / 512² 实测）**不开工**。

## 2 只读这几份（别的先别翻）

| 你要做的事 | 只读这一份 | 章节 |
|---|---|---|
| 画风 / 材质 / 天空 / 描边 | [`art/art.md`](art/art.md) | §2（渲染配方）、§7（资产） |
| 世界形态 / 尺度 / 世界骰 | [`setting/WORLD.md`](setting/WORLD.md) | §1.1（形态）、§2.4（骰子） |
| 卡的数据与清单 | [`setting/cards.md`](setting/cards.md) | §7（字段）、§13（清单） |
| 玩家怎么用卡 | [`gameplay/INTERVENTION.md`](gameplay/INTERVENTION.md) | §1（手牌）、§2（余响） |
| 工程结构 / 性能预算 | [`tech/tech.md`](tech/tech.md) | §1（预算）、§4（目录） |
| **任何一个数字** | [`CONSTANTS.md`](CONSTANTS.md) | 全页 |

## 3 十条硬红线（全文在 [`plan/work.md`](plan/work.md) §0.2）

1. `Sim/` **零渲染依赖**——不 include UObject / Tick / 渲染 / 输入头，`SolsticeBench` 要能无头跑。
2. **digest 不变**：改表现层后跑 `bench`，模拟摘要必须一致。
3. **不新建模块**（`SolsticeSim` 已被实测否决），能力落主模块子目录。
4. 跨 DLL 公共类型必须标 `SOLSTICE_API`（漏标 = `LNK2019`）。
5. **无 Lumen / Nanite / 全 PBR / 体积云 / 体积雾**。
6. **不出现纸质感**（纸纹、纸厚、纸纤维、撕裂边）。
7. **不许自行改规格数字**——数字只在 [`CONSTANTS.md`](CONSTANTS.md) 与各真源里。
8. **一次只做一张单**，做完再领下一张。
9. 提交前两条自检必须 0 错。
10. **相机只许停在 32 个格点**（方位 8 × 俯仰 4，[`art/art.md`](art/art.md) §2.6）——不许给相机加任何连续旋转入口。

## 4 现在卡在哪

| 卡点 | 说明 |
|---|---|
| **闸门：三张定标图** | 用户没点头前不开 Sim 侧的单 |
| `R-33` 纸雕残留 | 命名 / 材质 / 几何 / 天空 / 文案五处，就是 `D-01`–`D-05` |
| `A-07` 资源根 | 待用户拍板（三个选项在 [`plan/work.md`](plan/work.md) §7） |
| M1 规格 | 已冻结在 [`later/`](later/)，**M0 期间不读、不改、不评审** |

## 5 提交前必跑

```bash
python tools/checks/check-links.py          # 期望 broken links : 0
python tools/checks/validate-contracts.py   # 期望 errors : 0
```

改了**文档**：在 [`CHANGELOG.md`](CHANGELOG.md) 加**一行**。**不要**去改别的文档的版本号，也不要写「沿革」表。

## 6 协作与提交

**分工**：AI 负责代码、脚本、数据、文档、生图调度；用户负责安装登录、编辑器里的 GUI 选择、**审美裁决**与**立项 / 数值拍板**。

**只有这四处要等用户点头**，其余自己查证自己定：

| 闸门 | 判据 |
|---|---|
| 风格定标 | 出**正午 / 黄昏 / 雨**三张对照图，用户裁决 |
| 素材挑图 | 过 QC 清单 + 用户挑图 |
| 里程碑验收 | [`plan/ROADMAP.md`](plan/ROADMAP.md) §2 的人工项 |
| 立项 / 规格变更 | 定位、支柱、非目标；以及本文 §4 的待裁决 |

**提交**：英文短句祈使句；一次一件事；文档与代码**同一次提交**；素材单独提交；`main` 禁止 force push；**未提交 = 未落地**。

**禁止（都是踩过的坑）**：

- 把作废形态带回方案：**Tauri / WebGL / Vite / TypeScript / 2.5D / 自走棋 / 卡牌收集 / 纸雕**
- 引用 [`archive/`](archive/) 当依据（无约束力）
- 凭印象写数字（数字只在 [`CONSTANTS.md`](CONSTANTS.md) 与各真源里）
- 让 `Sim/` 依赖渲染 / 蓝图

## 7 文档地图

| 层 | 文件 | 什么时候读 |
|---|---|---|
| **入口** | 本页 · [`CONSTANTS.md`](CONSTANTS.md) · [`INDEX.md`](INDEX.md) | 总是 |
| **L1 现行规格** | [`setting/WORLD.md`](setting/WORLD.md)（形态与生成） · [`art/art.md`](art/art.md) · [`setting/cards.md`](setting/cards.md) · [`gameplay/INTERVENTION.md`](gameplay/INTERVENTION.md) · [`tech/tech.md`](tech/tech.md) | 施工时 |
| **语言** | [`setting/LORE.md`](setting/LORE.md)（术语 id / 文案规范 / 叙事原则） | **只写文案或定新名时** |
| **工作** | [`plan/work.md`](plan/work.md)（派单 + 工单池 + 待裁决）· [`plan/ROADMAP.md`](plan/ROADMAP.md) | 领单 / 排期 |
| **参考** | [`charter/CHARTER.md`](charter/CHARTER.md) · [`CHANGELOG.md`](CHANGELOG.md) | 查"为什么这么定" |
| **运维** | [`tools/docs/ENVIRONMENT.md`](../tools/docs/ENVIRONMENT.md) · [`ops/PLAYTEST.md`](ops/PLAYTEST.md) | 装机 / 试玩 |
| **冻结（M1）** | [`later/`](later/) 共 8 份 | **M0 别读** |
