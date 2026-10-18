---
id: INDEX
title: 文档清单
status: active
truth: index
owner: both
---

# 文档清单（INDEX）

> **本页只回答一个问题：有哪些文档、各自管什么、什么时候读。**
> **开工看 [`START.md`](START.md)**；**改数字先查 [`CONSTANTS.md`](CONSTANTS.md)**。
> **只在新增 / 作废 / 迁移文档时改本页**——改内容不必动它。

## 1 现行（L0–L2 + ops）

| 路径 | 层 | 一句话 |
|---|---|---|
| [`START.md`](START.md) | L0 | **唯一入口**：现在做什么、只读哪份、红线、卡点 |
| [`CONSTANTS.md`](CONSTANTS.md) | L0 | **跨文档数字的唯一索引**：值 / 真源 / 还有谁在用 |
| [`setting/WORLD.md`](setting/WORLD.md) | L1 | **世界形态与生成**：界形态判据、尺度、生态域、世界骰六轴与七条判据、地块与水体模型、环境曲线与天气、生成流程、年表结构 |
| [`setting/LORE.md`](setting/LORE.md) | L1 | **世界怎么说**：术语表与命名 id、文案与本地化规范、叙事原则、留档设定（相合 / 异质）。**施工不读，写文案时读** |
| [`art/art.md`](art/art.md) | L1 | 动画赛璐璐配方（着色 / 天空 / 地形水植被 / 昼夜）、**取景与机位**（§2.6）、资产生产与登记 |
| [`setting/cards.md`](setting/cards.md) | L1 | 卡的三轴与生克、卡种字段、M0 36 格清单、价值表、240 功能位 |
| [`gameplay/INTERVENTION.md`](gameplay/INTERVENTION.md) | L1 | 玩家怎么用卡：手牌、实体池、余响规则、释放手感 |
| [`tech/tech.md`](tech/tech.md) | L1 | 性能预算、模块依赖、仓库目录与命名、归位决策 |
| [`plan/work.md`](plan/work.md) | L1 | **施工派单板 + 工单池 + 待裁决**（分析→开发的唯一交接口） |
| [`plan/REBOOT.md`](plan/REBOOT.md) | L1 | **架构与画面重启方案 v2（draft，待确认）**：目录树与命名、依赖矩阵与扩展点、ToonRamp 参数化管线、性能分项预算、三套色板、文档重整清单 |
| [`art/PALETTE.md`](art/PALETTE.md) | L1 | **色板唯一真源**：三条色彩法则（明度阶梯 / 冷暖对位 / 60-30-10）、三套成品 HEX、来源与"廉价感"自检清单 |
| [`plan/ROADMAP.md`](plan/ROADMAP.md) | L2 | 三阶段切分与验收口径 |
| [`charter/CHARTER.md`](charter/CHARTER.md) | L2 | **宪章**：定位 / 支柱 / 非目标 §1 · **已定口径表 58 轮** §5 · **待裁决与已否决** §6 |
| [`CHANGELOG.md`](CHANGELOG.md) | L2 | **全仓唯一变更记录**：一行一条，倒序 |
| [`tools/docs/ENVIRONMENT.md`](../tools/docs/ENVIRONMENT.md) | ops | 装机、VSCode、构建与 DDC、MCP、排错（**不在 docs/ 内**） |
| [`ops/PLAYTEST.md`](ops/PLAYTEST.md) | ops | 怎么试玩：启动、种子、冒烟流程、记录模板 |


## 2 冻结（M1 及以后，**M0 不读不改**）

| 路径 | 内容 |
|---|---|
| [`later/ECONOMY.md`](later/ECONOMY.md) | 余响经济完整数值模型（三源、五道闸、长期验算） |
| [`later/TRIGGERS.md`](later/TRIGGERS.md) | 律卡的 `WHEN-THEN` 规则 DSL |
| [`later/TIME.md`](later/TIME.md) | 现实时间耦合、昼夜相位、离线行为 |
| [`later/SCREENS.md`](later/SCREENS.md) | 全屏清单与线框（S0–S9） |
| [`later/CUSTOMIZE.md`](later/CUSTOMIZE.md) | 高级面板（锁轴）与创世面板全参数 |
| [`later/UI-UX.md`](later/UI-UX.md) | M1 的全屏设想与 HUD 版式（**相机已迁出**：现真源 `art.md` §2.6；键位与 HUD 缺真源一事挂在 `plan/work.md` `A-09`） |
| [`later/PHYSICS.md`](later/PHYSICS.md) | 材质属性与物理交互 |
| [`later/SPECIES.md`](later/SPECIES.md) | 物种模板、种群动力、因果可读性 |

> 冻结件**不参与施工、评审与契约校验**；M1 立项时再解冻并对齐现状。引用时注明「M1 设想」。

## 3 归档

[`archive/`](archive/)：已作废形态与旧稿（动态壁纸 / 场景引擎 / 卡牌收集 / 自走棋 / 2.5D / Tauri / WebGL / 纸雕时期旧稿）。**无约束力，不作依据。**

## 4 三条维护规则

1. **改内容**：只改正文 + [`CHANGELOG.md`](CHANGELOG.md) 加一行。**不写「沿革」表，不 bump 版本号。**
2. **禁止复制规格段**：一段规格只在一份文档里写成，别处只写指针。新出现的数字先进 [`CONSTANTS.md`](CONSTANTS.md)。
3. **新增 / 作废 / 迁移**：才改本页；文档头只需 `id / title / status / truth / owner` 五字段。
