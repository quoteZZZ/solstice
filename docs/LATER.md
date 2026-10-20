---
id: LATER
title: M1 及以后（一页指针）
status: frozen
---

# M1 及以后（LATER）

> **M0 期间不读、不改、不评审。** 这里只记"有哪些东西排在后面"，细则在 `docs/archive/m1-specs/`（已冻结，仅供参考，不作施工依据）。

| 主题 | 一句话 | 细则 |
|---|---|---|
| 经济 | 三源产出、五道闸、100 小时验算——**在核心乐趣被验证前不展开** | `archive/m1-specs/ECONOMY.md` |
| 时间 | 昼夜相位细化、离线行为、年表回放 | `archive/m1-specs/TIME.md` |
| 物种 | 营养级扩表、种群动力、因果可读性 | `archive/m1-specs/SPECIES.md` |
| 生态全量 | 物种模板库 ≥120 条、12 个生态域全开、多带气候渐变 | [WORLD.md](WORLD.md) §9 |
| 形制扩展 | `vertical`（双层 / 浮空多层）与 `sky`（云顶 / 洞顶）两条轴，以及洞穴生态 | [WORLD.md](WORLD.md) §2.1 ④⑤ |
| 正交投影 | 完全等距的"插画感"；**前置**：`M_Sea` 天空反射与指数雾改成与投影无关 | [ART.md](ART.md) §7.2 |
| 物理 | 15 材质 × 13 参数、相变、三类基础能力 | `archive/m1-specs/PHYSICS.md` |
| 触发 | 律卡的 WHEN-THEN 词表（12 条上限、后写覆盖） | `archive/m1-specs/TRIGGERS.md` |
| 界面 | S0–S9 全屏清单与线框（M0 只做常驻 HUD + 手牌 + 三个抽屉，版式见 `ART.md §9`） | `archive/m1-specs/UI-UX.md` · `archive/m1-specs/SCREENS.md` |
| 创世面板 | 9 组参数、锁轴、世界配方库全量 | `archive/m1-specs/CUSTOMIZE.md` |
| 存档 | 双槽轮换 + 写前备份 + 崩溃恢复；**M0 需要的只是"种子 + 指令流"的确定性重放** | `shared/schema/save.schema.json`（信封已就位） |
| 音频 | 四条总线（Master / Environment / UI / SFX）+ 环境床 + 纸声仪式感 | — |
| 内核拆分 | 独立 `SolsticeKernel` 静态库（`.Build.cs` 只依赖 `Core`）+ 单测 | — |

**解冻条件**：M0 验收第 13 项（"摆 10 分钟不想关"）通过，且画面三张图定标。解冻时先对齐现状，再决定哪几条仍然成立。
