---
id: TECH
title: 技术与工程
status: active
---

# 技术与工程（TECH）

> 管三件事：**代码放哪、依赖朝哪、性能与管线怎么定**。玩法见 [WORLD.md](WORLD.md) / [CARDS.md](CARDS.md)，画面见 [ART.md](ART.md)。

---

## 1 分层与目录（现状）

```text
game/Source/
├─ Solstice/                     唯一 Runtime 模块
│  ├─ Core/      GameInstance、现实锚（时钟/纬度/联网天气）、数据加载、本地化
│  ├─ Sim/       世界钟 + 世界生成 + 生态 + 天气 + 卡指令 + 余响 + 年表（纯逻辑）
│  ├─ Gen/       程序化几何（地形网格 / 天空球 / 海 / 立牌 / 道具）
│  ├─ Render/    材质解析、分带参数、天空调色
│  ├─ World/     ASolsticeRealm：分块网格、道具、光照、雾、后处理
│  ├─ God/       观察者相机、玩家控制器、GameMode
│  ├─ UI/        Slate HUD（纯 C++，无 UMG 资产）
│  └─ Dev/       无人值守抓帧（-SolsticeShot=<png>）
└─ SolsticeEditor/               编辑器模块：材质生成 + 无头 bench Commandlet
```

**依赖方向**：`Sim → Gen → Render → World → God/UI`；`Core` 与 `Dev` 对所有层开放。
`Sim/` **禁止** include UObject / Tick / 渲染 / 输入头。由 `tools/checks/check-layers.py` 守（pre-commit + 应进 CI）。

**已知架构债**（不是目标，是可改的地方）：

| 债 | 现状 | 影响 |
|---|---|---|
| `ASolsticeRealm` 上帝 Actor | 1709 行；`BuildSkyMesh` 单函数 431 行 | 天空 / 海 / 光照 / 地形互相牵连，改 A 破 B |
| 材质由 C++ 命令式建图 | `SolsticeCelMaterialBuilder.cpp` 1980 行 | 美术迭代一轮 5–10 分钟，节点错误只能靠画面发现 |
| 无 LOD / 无 ISM | 每块地形一个 ProceduralMeshComponent，道具合并为一个 | 512² 未实测，`MarkAllDirty` 定时全量重建是尖峰风险 |
| UI 无 DPI 概念 | 字号 / 内边距全字面量 | 窄窗口下卡名 7–9 px（`PLAN` 缺陷 G-05） |
| 无存档 | `save.schema.json` 只有信封 | "永续世界"缺持久化，越晚补越贵 |

---

## 2 四条硬约束

1. **`Sim/` 零渲染依赖**：可 headless 跑；`bench` 是唯一证明。
2. **不新建模块**：能力落主模块子目录。独立 `SolsticeKernel` 静态库排 M1（届时 `.Build.cs` 只依赖 `Core`，用链接期强制纯度）。
3. **跨 DLL 公共类型必须标 `SOLSTICE_API`**：漏标 = `LNK2019`（已踩过两次）。
4. **`Build.cs` 是白名单**：只列真正被 `#include` 的模块。当前 Runtime：`Core CoreUObject Engine InputCore Json ProceduralMeshComponent` + 私有 `Slate SlateCore HTTP ImageWrapper`。新增依赖必须同步本表。

---

## 3 确定性（bench 能证明的唯一不变量）

| 要求 | 做法 |
|---|---|
| 固定逻辑 tick | 1 世界分钟；渲染帧只把真实秒换算成整分钟，不插值演化 |
| 随机数分流域 | 生态 / 天气 / 事件 / 演化 / 生成各一条流，用种子派生 |
| 遍历顺序 | 按显式 ID 升序，不用容器迭代顺序 |
| **不读真实时钟** | `Sim` 里任何 `FDateTime::Now()` 都必须有"可钉"入口：`Seed / StartingDayOfYear / StartingMinuteOfDay` |
| 同种子同世界 | `seed + generator_version + size` 三者相同 |

**bench 基线**：`seed=20261016 / dayOfYear=80 / minuteOfDay=06:30`。新增真实时钟读取前先问"bench 怎么钉住它"。

---

## 4 数据与契约

| 文件 | 内容 |
|---|---|
| `game/Config/Solstice/Data/world.json` | 世界配方（六轴 + 预设） |
| `.../Data/cards.{earth,fire,metal}.json` | 各元素 12 张卡（M0 共 36） |
| `.../Data/species.json` | 五个物种模板 |
| `.../Localization/{zh-Hans,en}.json` | 中英双语，**key 必须一一对齐**（当前 169 : 169） |
| `shared/schema/*.json` | `card` / `species` / `world` / `save` 四份契约 |
| `shared/fixtures/save.sample.json` | 存档信封样例 |

**规则**：改 schema 必须同次改 `game/Config/` 与 `studio/presets/`；`validate-contracts.py` 校验 6 对契约 + 41 个 id + 双语对齐。

**已知缺口**：`world.json` 里六轴字段有 20+ 个，`FWorldConfig` 只消费 10 个左右（`relief / ranges / bearing / rivers / lakes / aridity` 等尚未接）。**loader 应打印未消费字段清单**，别让"文档以为接通了"再发生一次。

---

## 5 性能预算（**未实测，数字是目标不是结论**）

| 项 | 目标 |
|---|---|
| 帧率 | 观测态 60 fps / 推演态 60 fps @1080p |
| 逻辑 | 观测 ≤2 ms/帧；20x 倍率 ≤8 ms/帧 |
| GPU | 观测 <5% |
| 内存 | 512² <3 GB；256² <2 GB；128² <1.5 GB |
| 首载 | 512² <5 s；128² <3 s |
| 快进 | 512² ≥1 世界日 / 10 s；128² ≥30 世界日 / 10 s（算法回归口径） |

**已有实测**（2026-10-19）：
- 逻辑：128² / 2880 世界分钟 **0.34 s**（约 9580 世界分钟/秒）。
- 生命：开局 **306** 只 → 两世界日后 **1046**（灌木 39 / 走兽 805 / 掠禽 22 / 鱼 180），`digest` 两次独立进程一致 `2C1A3AC0CDE6C6B5`。
- 渲染：128² / 1280×720 / 35° 机位 **16.1 ms（62 fps）**。
**缺**：512² 的任何数字（帧率 / 内存 / 首载 / 分块重建尖峰）。**在拿到 512² 数字之前不要按 512² 做内容决策。**

---

## 6 渲染管线（当前真相）

| 项 | 现状 | 为什么 |
|---|---|---|
| 着色模型 | `MSM_DefaultLit` + 材质内三段量化，**分带切在 Emissive** | 实测方向光与天光对地表贡献≈0，`BaseColor` 乘零 |
| 材质 | 5 个母材质：`M_Cel` / `M_Ink` / `M_InkHull` / `M_Sky` / `M_Sea`，**由编辑器模块按代码生成** | 二进制不入 git，必须可重建 |
| AA | **TSR**（`r.AntiAliasingMethod=4`） | 边缘稳定性直接决定"高清感" |
| GI | **SSGI**（`DynamicGlobalIlluminationMethod=2`） | **Lumen 在此工程无效**：ProceduralMesh 没有 Surface Cache / 距离场，Lumen 里没有场景表示，帧时间一位不涨 |
| 阴影 | CSM（`r.Shadow.Virtual.Enable=0`） | VSM 同样依赖 Nanite / 距离场，实测零参与 |
| 反射 | SSR | 海面另有自定义菲涅尔 |
| 天空 | 相机子挂单位球 + `M_Sky` 逐像素 | 平面卡片有边；`SkyAtmosphere` 组件已移除（permutation 未编译时渲染纯黑，且画不出手绘云带） |
| 雾 | 指数雾（过渡方案） | 目标形态是三层平面；上限压住（能见度 ≥0.6 R） |
| 禁用 | Motion Blur / Lens Flare / Lumen / Nanite / 体积云 / 体积雾 | 卡通风杀手或依赖不存在的场景表示（`r.Nanite.ProjectEnabled=False`） |
| 颗粒 | **FilmGrain 0.03**（唯一保留的噪点） | 海与天都是极慢的渐变，8 bit 下会出等高线；3% 是看不见的抖动 |

> **给下一个接手的人**：改 ini 里的 `r.SupportSkyAtmosphere` 之类**shader permutation** 开关不会立刻生效，必须先重编 permutation，否则天空会渲染成纯黑（已踩过一次并回退）。
> **Nanite.ProjectEnabled=True 只是为"烘焙成 StaticMesh 后再说"留的开关**，当前没有 Nanite 网格。

---

## 7 归位与命名

| 要新增 | 放哪 |
|---|---|
| 一段游戏逻辑 | `Solstice/<对应子目录>/` |
| 新卡 / 新物种 | `Config/Solstice/Data/*.json`（+ 必要时 Gen 里加几何） |
| 新材质参数 | `Render/SolsticeCelMaterial.h` 的 `FCelMaterialSet` + builder 配方 |
| 新命令 / 无头检查 | `SolsticeEditor` 加 Commandlet + `tools/run` 或 `tools/build` 薄包装 |
| 新脚本 | `tools/{env,build,run,checks}`（分层见 `tools/README.md`） |
| 新文档 | 对应域文档；**不新建索引页** |

**命名**

- C++：`U` / `A` / `F` / `E` / `I` 前缀；`Sim/` 内不出现 UObject 派生。
- `namespace Solstice` 内的普通类型**不重复** `Solstice` 前缀；UObject / AActor / Slate 类保留（`USolsticeGameInstance`、`ASolsticeRealm`、`SSolsticeHUDView`）。
- 文件：`Solstice<域>.h/.cpp`，主类型去前缀即文件名（`SolsticeRealm.h` → `ASolsticeRealm`）；`.generated.h` 随文件走，改名必须重编。
- 资产：`SM_` / `M_` / `MI_` / `T_` / `NS_` / `WBP_` / `DA_` / `L_`。
- 卡 id：`card.<元素>.<级>.<类>.<名>`；物种 `sp.<名>`；界 `realm.<slug>-<yyyymmdd>`。

---

## 8 提交与重建

**入 git**：`game/Source`、`game/Config`、`*.uproject`、`shared/`、`studio/{workflows,jobs,presets,tools}`、`studio/out/manifest.jsonl`、`tools/`、`docs/`、`.vscode/`、`.github/`、`.codebuddy/`、`.workbuddy/`。
**不入 git**：`game/{Binaries,Intermediate,Saved,DerivedDataCache}`、`game/Content/**` 二进制、`studio/out/**`（除 manifest）、`studio/work/**`、`tmp/`。

**核心不变量**：`Content/` 不入库 ⇒ 必须能由「种子 + 生成器 + 契约 + manifest」完整重建。所以改生成器与契约必须同次提交。

**打包欠账（P5）**：运行时 JSON 走 `FPaths::ProjectConfigDir()`，UAT 默认只 stage `Default*.ini`——**打包版读不到数据**。方案：迁 `Content/Solstice/` + `+DirectoriesToAlwaysStageAsUFS`，并实测一次打包。

---

## 9 工具链（本机三坑）

1. **必须关 UBA**：`tools/build/build.ps1` 已默认 `-NoUBA`（否则 PCH 报 `C1083 拒绝访问`）。
2. **不要直接跑 `UnrealBuildTool.exe`**：本机只有 .NET 8/9（`EXIT=150`）；用引擎自带 `Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe`（`gen-projectfiles.ps1` 已内置）。
3. **承载 Commandlet 的模块 `LoadingPhase` 必须是 `Default`**，否则 `-run=` 解析时模块还没加载。
4. 新建 `.ps1` **必须 UTF-8 带 BOM**（PS 5.1 无 BOM 按 ANSI 解析）。

详细装机、VSCode、MCP、排错见 [`tools/docs/ENVIRONMENT.md`](../tools/docs/ENVIRONMENT.md)。
