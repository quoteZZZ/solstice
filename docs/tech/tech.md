
# 技术与工程（ARCH + tech.md）

## 0 本文件管什么 / 不管什么

- **管**：**两件事**——① 引擎与模块划分、依赖方向、硬约束、**性能预算**（§1–§2）；② **仓库目录与命名**（§3–§14，原 `tech.md`，**"新文件放哪、叫什么"的唯一真源**）。
- **不管**：玩法与内容设定（真源见 [`../charter/CHARTER.md`](../charter/CHARTER.md)）、里程碑与计划（见 [`../plan/ROADMAP.md`](../plan/ROADMAP.md)）。
- **来源**：原 `CHARTER.md` §1（性能）+ §17（技术架构与硬约束）拆出；**内容未改，仅重编号**。
- 文中的裸 `§x` 引用按语境指 [`../art/art.md`](../art/art.md)（风格，含原 `STYLE.md` 设定层 §0）/ [`../setting/WORLD.md`](../setting/WORLD.md)（世界）/ [`../setting/cards.md`](../setting/cards.md)（卡片）。

---

## 1 性能与优化（★一等公民）

### 1.1 目标预算（1080p / RTX 5060 级 8GB）

| 项 | 目标 |
|---|---|
| 帧率 | 观测态 30fps 上限；推演态 60fps |
| 逻辑 | 观测态 ≤ 2ms/帧；推演态 20x 倍率下 ≤ 8ms/帧 |
| GPU | 观测态占用 **< 5%**；推演态 < 60% |
| 内存 | **512² 默认档 < 3GB**；128² < 1.5GB；256² < 2GB；>512² 另议（需流式卸载） |
| 加载 | **512² 首载 < 5 秒**；128² < 3 秒；切界 < 1 秒（惰性分块） |

### 1.2 空间侧

1. **分块（64×64）**：只加载视野内与邻接分块；远离则卸载（保留统计摘要）。512² = **8 × 8 = 64 块**（128² 是 2×2，256² 是 4×4）。
1.1 **地形网格按块一个 mesh**（`ASolsticeRealm::RebuildDirtyChunks`）：512² 下每块 64×64 格，**全量脏重建必须分帧预算**（`MarkAllDirtyEveryWorldMinutes = 360` 会触发全量），否则水位一变就掉帧。
2. **地形网格脏标记重建**：只重建被改动分块，不每帧重建。
3. **LOD 三级**：近（细节几何）/ 中（合并面）/ 远（单色块 + 轮廓）。
4. **视锥 + 遮挡剔除**；大界启用距离剔除。

### 1.3 模拟侧

1. **模拟 LOD（SimLOD）**：
   - 近（相机附近）：逐个体状态机；
   - 中：分组聚合（群组按代表个体演化）；
   - 远 / 不可见：**统计式演化**（种群差分方程，不推个体）。
2. **只推演激活界**；非激活界零开销。
3. **预算分摊**：模拟按帧固定预算切片执行，不阻塞渲染。
4. `Sim` 零渲染依赖 → 可 headless 跑基准（见 [`../plan/ROADMAP.md`](../plan/ROADMAP.md) §2 验收 2）。

### 1.4 渲染侧

1. **ISM / HISM 实例化**承载岩石、植被、装饰（单次绘制调用渲染数百实例）；用 **per-instance Custom Data** 做颜色 / 风动相位差异（实例共享材质实例）。
2. 动态个体用轻量代理（低面数 + 程序化动画），关键个体才用完整部件集。
3. **Niagara 粒子预算**：按档位缩放粒子数与生成率；雨雪用屏幕空间近似。
4. **赛璐璐风格本身即优化**：无 Lumen / Nanite / 全 PBR；硬阴影；无体积云 / 体积雾（平面分层）；少量材质参数。
5. 材质参数集中管理，避免材质实例爆炸。

### 1.5 系统侧

对象池（个体、装饰、粒子、UI 元素）、避免每帧分配、分块序列化 + 惰性加载、异步加载与流式。
**度量手段**：headless 基准脚本 + `stat unit / stat gpu` + 自动化 profile 报告。

---

### 1.6 参数与常量的分层真源（★不得写死）

调参不能靠在代码里翻数字。**四层**真源，各管各的（第 54 轮补"世界骰参数"一层）：

| 层 | 放哪 | 放什么 | 谁读 |
|---|---|---|---|
| **设计参数** | `Sim/SolsticeTypes.h` 的 `namespace Tuning`（定义在 `SolsticeTypes.cpp`） | 跨模块可见、且会影响世界演化的数值：地形塑形 / 生物群系阈值 / 天气与生态节奏 / 太阳几何 / 漂移与燃烧速率 | 任意模块（Sim 层优先） |
| **实现常量** | 各 `.cpp` 顶部匿名 namespace | 只对某个算法有意义的内部常数：噪声频率与八度、FBM 权重、色温换算系数 | 仅该文件 |
| **世界骰参数** | `game/Config/Solstice/Data/world.json`（契约 `shared/schema/world.schema.json`） | 每局世界的六轴取值：形制 / 水体 / 气候 / 生态 / 天象 / 开局时刻（[`../setting/WORLD.md`](../setting/WORLD.md) §2.4） | 数据加载器 `Core/SolsticeDataLoader` → `FWorldConfig` |
| **表现配方** | [`../art/art.md`](../art/art.md) §2 的实现：`Gen/SolsticeGenerators.h` 的 `FPaperPalette`（色板）与 `FStandeeRecipe`（立牌几何），以及各表现文件顶部的 scene rig 常量 | 配色、立牌卡片几何、灯光与雾 | 仅表现层 |

四条规则：

1. **每个常量都配一句"为什么"**，并标注文档出处（`docs/…` 的 § 号）——**出处必须真实存在**，指错了等于没有。
2. `Tuning` 是**只读**的：改数值就是改 `SolsticeTypes.cpp` 里的一处，不在调用点就地写字面量。
3. **表现层不得影响 Sim**：调色板与立牌几何改一百遍，世界演化也不该动一个字节。
4. 一次"去魔法值"必须是**等价变换**：值一个不变，改完 `bench` 的 digest 必须与改前**完全一致**（这是唯一可接受的验收方式）。

## 2 技术架构与硬约束

### 2.1 模块划分（依赖方向 `Render → World → Sim`）

```text
game/Source/Solstice/
├─ Core/    启动、存档与多界管理、现实锚（系统时钟 + 纬度 + 联网天气）、事件总线、数据加载
├─ Sim/     ★ 世界钟（双模式）+ 生态演化 + 模拟 LOD：纯逻辑、零渲染依赖
├─ World/   地形分块、水体、种群、生态域、脏标记网格重建
├─ God/     卡池与指令系统（余响 / 卡包 / 卡库 / 笔刷 → 指令队列；接管与释放）
├─ Gen/     程序化几何生成器（地形网格、岩石、植物、动物部件）
├─ UI/      HUD（默认 C++ 构建控件树，UMG 可视化打磨）
└─ Render/  赛璐璐渲染管线：Cel LUT、Inverted Hull 描边、硬阴影、移轴景深、实例化
```

### 2.2 硬约束

1. **零渲染依赖**：`Sim` 不依赖 Tick / 渲染 / 输入，可 headless 驱动。
2. **只推演激活界**。
3. **可复现**：固定逻辑 tick（1 世界分钟）+ 随机数分流域（生态 / 天气 / 事件 / 演化）+ 遍历按显式 ID 升序 + 避免平台差异函数。目的：同种子同世界、headless 回归。
4. **内容数据驱动**：物种、卡池、法则、预设全 JSON，运行时读取；AI 生成 + 官方脚本灌入 DataTable/DataAsset。
5. **游戏逻辑不使用 Python**；资产批处理优先官方编辑器 Python（ScriptPlugin），必要时 C++ Commandlet。
6. **逻辑在 C++，表现层在蓝图**；`Sim` 不得依赖蓝图。
7. 存档版本化：`solstice.save.v<n>`，**每界一份**。
8. UI **中英双语**。

---

---

# 目录与命名唯一真源（tech.md）

> **本文档是"新文件放哪、叫什么"的唯一真源。** 与 [`../charter/CHARTER.md`](../charter/CHARTER.md) 冲突时以 CHARTER.md 为准，并在同一次改动内同步修正本文。
> 旧稿（v1.0）的 `app/` `studio/` `shared/` 三分法已作废——`app/` 随旧形态删除。

---

## 3 分层原则与状态

| 层 | 定位 | 入 git | **状态** |
|---|---|---|---|
| `game/` | **进发行包**：UE 工程、源码、配置 | 源码 / 配置 **入**；二进制资产 **不入** | ✅ **现状**（`.uproject` + 2 个 Target + 主模块 `Solstice` + 编辑器模块 `SolsticeEditor`；M0 目标已编译通过并实测运行） |
| `studio/` | **产出素材**：ComfyUI 生图线、生成产物、风格锚与预设 | 工作流 / 任务 / 预设 / manifest **入**；产物 **不入** | ✅ **现状**（工作流 / jobs / 预设 / 驱动脚本已落地；`out/` 产物尚未生成，`manifest.jsonl` 仅表头） |
| `shared/` | **双方共读的契约**：JSON Schema | **入** | ✅ **现状**（`schema/` 4 个：`card` / `species` / `world` / `save`（信封骨架）+ `fixtures/` 1 个；`style` 待 M1） |
| `tools/` | **开发期脚本**：共享库、环境、工程生成、构建、内容生成、基准、提交前自检、提交钩子、AI 配置模板 | **入** | ✅ **现状**（按用途分 7 层：`lib/` 1 个共享库 · `env/` 6 · `build/` 3 · `run/` 2 · `checks/` 2 Python · `hooks/` 钩子 + 安装脚本 · `ai-config/` 2 模板 + 1 脚本 + README） |
| `docs/` | 设计与约定文档 | **入** | ✅ **现状**（结构见 [`../INDEX.md`](../INDEX.md)） |
| `.github/` | **CI**：GitHub Actions 跑 `tools/checks/` 两条自检（无需引擎） | **入** | ✅ **现状**（`workflows/checks.yml`） |
| `.vscode/` `.codebuddy/` `.workbuddy/` | IDE 与项目数据配置 | **入** | ✅ **现状** |

**核心不变量**：`game/Content/` 的二进制资产**不入 git**，因此必须**可由"种子 + 生成器 + 契约 + manifest"完整重建**。这是全部目录设计的出发点。

M0 的两个纸材质主材质（`/Game/Core/M_Paper`、`M_Ink`）是这条不变量的第一个真实样本：它们不存在于 git，由 `tools\build\gen-assets.ps1` 调 `SolsticeEditor` 的生成器按代码建图后写出。

---

## 4 仓库总览

```text
solstice/
├─ game/          ✅ 现状  —— 虚幻引擎工程（进发行包）
├─ studio/        ✅ 现状  —— 本地生图线 + 程序化资产产物
├─ shared/        ✅ 现状  —— 契约：JSON Schema（引擎与生图线共读）
├─ tools/         ✅ 现状  —— 开发期脚本，按用途分 7 层：
│   ├─ lib/          共享库（引擎 / 工程 / 可执行文件定位的唯一实现，其余脚本 dot-source）
│   ├─ env/          环境：探测引擎、检查环境、装 VS 生成工具 / Epic Launcher、写编辑器环境变量、装扩展
│   ├─ build/        构建：编译、生成工程文件、生成资产
│   ├─ run/          运行：起编辑器（含 Unreal MCP / 无头）、跑 bench
│   ├─ checks/       提交前自检（Python，标准库）
│   ├─ hooks/        git 钩子 + 安装脚本
│   └─ ai-config/    AI 工具链用户级配置的仓库模板 + 同步脚本
├─ docs/          ✅ 现状  —— 见 ../INDEX.md（charter / setting / gameplay / art / tech / plan / ops / archive）
├─ .github/       ✅ 现状  —— CI：GitHub Actions 跑 checks/ 两条自检（无需引擎）
├─ .vscode/       ✅ 现状  —— 构建任务 / 调试 / cpptools / 代码片段
├─ .codebuddy/    ✅ 现状  —— agents/ 与 rules/（项目数据，勿删）
├─ .workbuddy/    ✅ 现状  —— memory/：工作日志与长期项目笔记（项目数据，勿删）
├─ .clang-format  ✅ 现状
├─ .editorconfig  ✅ 现状
├─ .gitignore     ✅ 现状
└─ README.md      ✅ 现状  —— 入口页
```

> **规则**：凡本文出现 `🟡 计划中` 的目录，**不要按其存在去写代码或脚本**；建立目录时把状态列改为 `✅ 现状`。

---

## 5 `game/`（进发行包）✅ 现状

```text
game/
├─ Solstice.uproject
├─ Config/
│  ├─ DefaultEngine.ini  DefaultGame.ini  DefaultInput.ini
│  └─ Solstice/
│     ├─ Data/                  ★ 运行时契约 JSON（cards.earth / cards.fire / cards.metal / species / world）
│     └─ Localization/          ★ 中英双语文本（zh-Hans.json / en.json）
├─ Content/                     ⚠ 二进制资产不入 git；目录骨架（每子目录 .gitkeep）入 git
│  ├─ Core/                     M_Paper.uasset / M_Ink.uasset（由 gen-assets.ps1 生成）
│  └─ World/ Cards/ UI/ Data/ Levels/   目前均为占位；用途与生成方见 game/Content/README.md
├─ BLUEPRINT-GUIDE.md           🟡 计划中：编辑器人工步骤说明书（M1 首批蓝图工作开始时由
│                                 blueprint-guide-writer 子代理创建；入 git）
├─ Source/
│  ├─ Solstice.Target.cs            游戏目标（无编辑器依赖，产出 Solstice.exe）
│  ├─ SolsticeEditor.Target.cs      编辑器目标（产出 UnrealEditor-SolsticeEditor.dll）
│  ├─ Solstice/                 ★ 主模块
│  │  ├─ Core/   启动（GameInstance）、现实锚、数据加载、本地化
│  │  ├─ Sim/    世界钟（双模式）+ 地形 / 生态域 / 天气 / 生态 / 卡指令 / 余响 / 编年史（纯逻辑，零渲染依赖）
│  │  ├─ Gen/    程序化几何生成器（地形网格 / 描边卡 / 轮廓带）
│  │  ├─ World/  分块网格构建与脏标记重建（`ASolsticeRealm`）
│  │  ├─ God/    观察者、玩家控制器、游戏模式
│  │  ├─ Dev/    无人值守抓帧（`SolsticeShot`）：-SolsticeShot=<png> 时抓一帧就退，非调试启动完全不执行
│  │  ├─ UI/     HUD（纯 Slate，无 UMG 资产）
│  │  └─ Render/ 赛璐璐渲染：主材质解析、Cel 分带、天空调色
│  └─ SolsticeEditor/           编辑器模块：Commandlet（资产生成 / 无头基准）
└─ (无 Plugins/ —— 尚未需要；ProceduralMeshComponent 直接启用引擎自带插件)
```

> **`.Target.cs` 放在 `Source/` 下，不是工程根**：UE 5.8 的 `ProjectRulesAssembly` 只 glob `Source/*.Target.cs`，放根目录会报 `Couldn't find target rules file`。
> `Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/` 由引擎生成，已在 `.gitignore` 忽略。

---

## 6 `Source/` 模块约定

| 模块 | 依赖 | 说明 |
|---|---|---|
| `Solstice` | 公共 `Core` `CoreUObject` `Engine` `InputCore` `Json` `ProceduralMeshComponent`；私有 `Slate` `SlateCore` `HTTP`（**无** UMG / JsonUtilities / RenderCore / RHI / Niagara / Lumen / Nanite） | 主模块。`Sim/` 子目录**禁止** include 渲染 / 输入 / UObject 头 |
| `SolsticeEditor` | `Solstice` + `UnrealEd` `MaterialEditor` `AssetTools` `AssetRegistry` `RenderCore` `RHI` | 类型 `Editor`，`LoadingPhase: Default`。两个 Commandlet：`SolsticeGenAssets`（生成主材质）、`SolsticeBench`（无头确定性回归） |

**依赖表是白名单**：只列**真正被 include** 的模块。`JsonUtilities` 曾在两个 `Build.cs` 里空挂（源码只用 `Json` 的 `Dom/JsonObject` 与 `Serialization/JsonSerializer`），2026-10-17 已移除并在本机重编验证。增删依赖必须同步本节表格。

**主材质是靠代码建图生成的**：`UnrealEd` 只在编辑器模块里，而运行模块不能链接它，所以运行模块把"怎么生成"这件事抽象成一个 `TFunction`（`Solstice::GBuildPaperMaterials`），编辑器模块在 `StartupModule` 里把它的实现塞进去。运行模块在 `/Game/Core/M_Paper` 缺失时才调它一次——这就是新克隆自愈的机制。

**`LoadingPhase` 必须是 `Default`，不能是 `PostEngineInit`**：`-run=XxxCommandlet` 的类解析发生在 `LaunchEngineLoop.cpp:3931`，而 `PostEngineInit` 阶段的项目模块加载在同文件 `4861` ——命令行解析时模块还没加载，会直接报 `looked like a commandlet, but we could not find the class`（实测踩过）。`Default` 阶段加载在 `3896`，早于解析。`-game` 下 `GIsEditor` 为假，`EHostType::Editor` 模块仍然不会加载（`ModuleDescriptor.cpp:730`），所以这条改动不影响发行路径。

**目标与模块同名不同物**：`.Target.cs` 是构建目标，`Source/Solstice/` 是模块。新增能力优先落在主模块的对应子目录，**不要新建模块**。M0 曾计划独立 `SolsticeSim` 模块 / 目标，实测下来 `Solstice` 目标本身就是无编辑器依赖的可执行文件，再加一个完全相同的 Game 目标只会让人困惑，故删除；无头回归改为 `SolsticeEditor` 的 `SolsticeBench` Commandlet（同样不碰 RHI / World / GameMode，足以证明 `Sim/` 可无头运行）。真正零引擎依赖的独立 Sim 模块留到 M1 存档格式需要它时再做。

---

## 7 `studio/`（产出素材）✅ 现状

```text
studio/
├─ README.md
├─ workflows/      ComfyUI 工作流 JSON（当前：paper-texture）
├─ jobs/           批量任务清单（jsonl）：模型、prompt、seed、尺寸、输出名（当前：paper-textures.jsonl）
├─ presets/        ★ 风格主题参数（入 git，当前：paper-diorama.json）
├─ tools/          ★ 驱动脚本：调 ComfyUI HTTP API 跑 jobs、写 manifest（当前：comfy_batch.py）
├─ scripts/        本地服务的启停脚本（PowerShell）：start-comfy.ps1（隐藏窗口起 ComfyUI，不留 cmd；-Stop 停）· serve-previews.ps1（起 studio/previews 的静态服务，默认 8099）
├─ previews/        ★ 交互式设计演示（纯 HTML，无需引擎）：动效 / 布局 / 运镜在改代码前先在这里看
│  └─ camera-move.html   进场 / 出场运镜（样条 + 单次缓动 + 分层展开 + 落位沉降）
├─ out/            ⚠ 生成产物（不入 git，manifest 入 git）
│  ├─ cards/        卡面（3:4）
│  ├─ cutouts/      立牌 LOD 贴图（纯色底，供抠图）
│  ├─ textures/     地表色带 / 水纹 / 苔藓（**无纸纹**）
│  ├─ world/        界形 / 风格对照图（数量少、体积小，是定标依据）
│  └─ manifest.jsonl  ★ 逐张留档：模型 / prompt / seed / 日期（Steam AI 披露用）
└─ work/           ⚠ 临时试验（不入 git）
```

> **与 v2.4 的差异**：`scripts/` 已建立（`start-comfy.ps1`，2026-10-17 登记）；驱动脚本仍是 `studio/tools/comfy_batch.py`；原定 `refs/`（风格锚图）尚未建立，待第一批风格锚图产出时再建（留档要求见 [`../art/art.md`](../art/art.md)）。
>
> **`work/` 的边界（硬规则）**：凡被文档或 `manifest.jsonl` 引用的文件，**一律不得放在 `studio/work/`**。那里只放一次性挑图与脚本试验；`work/` 不进 git，引用它等于引用一个别人取不到的文件。`comfy_batch.py` 的 `require_archived()` 已把这条做成脚本级校验（落在 `work/` / `out/` 直接拒绝运行）。
>
> **`out/` 子目录轴**：按"哪类素材"分（`cards` / `cutouts` / `textures` / `world`），与 `game/Content/` 的"功能域"轴不是一套——两者的对应关系由 `tools/build/import-assets.ps1` 的 `kind → 目录` 映射承担（card→Cards、ui→UI、texture/cutout/fx/bg→World），不靠人工记。

**脚本语言**：`studio/tools/` 与 `tools/` 调外部 HTTP 服务用 **PowerShell 或 Python**均可（当前 `comfy_batch.py` 是 Python，因为要复用 ComfyUI 的 JSON 工作流结构）；**编辑器内的资产批处理**用 `SolsticeEditor` 的 Commandlet。**游戏逻辑不使用 Python**。
**登记规范**：manifest 字段、批次一致性、QC 与合规见 [`../art/art.md`](../art/art.md)。

---

## 8 `shared/`（契约）✅ 现状

```text
shared/
├─ README.md
├─ fixtures/
│  └─ save.sample.json     存档信封样例（validate-contracts 第 6 对契约的校验对象；M1 实装的真值参考）
└─ schema/
   ├─ species.schema.json   物种模板（[`../setting/SPECIES.md`](../later/SPECIES.md) §4）
   ├─ card.schema.json      干涉卡（元素 / 级 / 卡型 / 持续性 / 作用域 / 参数白名单）
   ├─ world.schema.json     界：尺寸 / 地形算法与种子 / 生态域 / 水体 / 大气 / 法则
   └─ save.schema.json      存档信封：版本 + 元数据 + 快照 / 卡池历史 / 年表容器（载荷 M1 收紧）
```

**待 M1**：`style.schema.json`（风格主题参数）；`save.schema.json` 已落**信封骨架**（版本 + 元数据 + 三个载荷容器），snapshot / card_history / chronicle 的内部结构 M1 收紧（规划：元数据 + 快照 + 卡池历史 + 年表）。

契约是 `game/Config/Solstice/Data/` 与 `studio/presets/` 的**共同校验源**：改契约必须同步改两边数据。

---

## 9 运行时文件位置（发行版）

| 内容 | 位置 | 备注 |
|---|---|---|
| 契约数据 | `game/Config/Solstice/Data/*.json` | 运行时读 `FPaths::ProjectConfigDir()`；⚠ **打包 stage 未配置**（UAT 不带 Config 下非 ini 文件，见 §13.7） |
| 本地化 | `game/Config/Solstice/Localization/` | 中英双语（文案规范见 [`../setting/LORE.md`](../setting/LORE.md) §5）；打包同 §13.7 |
| 存档（每界一份） | `%LOCALAPPDATA%\Solstice\Saved\SaveGames\<realmId>.sav` | 键前缀 `solstice.save.v<n>` |
| 导出的界 | `%USERPROFILE%\Documents\Solstice\Exports\<realmId>.solstice` | 导入 / 导出用 |
| 日志 / 崩溃 | `Saved/Logs`、`Saved/Crashes` | 不入 git |
| 着色器缓存 DDC | `D:\UE-DDC`（环境变量 `UE-LocalDataCachePath`） | **必须外置**，否则吃 C 盘 |

---

## 10 命名规范

### 10.1 C++

| 类型 | 前缀 | 示例 |
|---|---|---|
| UObject / UActorComponent | `U` | `USolsticeGameInstance` |
| AActor | `A` | `ASolsticeRealm` |
| 结构体 | `F` | `FTile`、`FCatalog` |
| 枚举 | `E` | `ETimeMode`、`EBiome` |
| 接口 | `I` | `ISolsticeTickable` |
| 纯逻辑类（`Sim/` 内） | `F` + 无 UObject | `FSimKernel`、`FRngBank` |

规则：**`Sim/` 内不出现 `UObject` 派生**（保持零引擎依赖可测）。
**跨 DLL 可见的公共类型必须标 `SOLSTICE_API`**：`UnrealEditor-Solstice.dll` 与 `UnrealEditor-SolsticeEditor.dll` 是两个 DLL，未标注的符号不会导出，编辑器模块会在链接期报 `LNK2019`（实测踩过两次：`FPaperMaterials`、`FSimKernel`/`FDataLoader`）。
**`Solstice` 命名空间内的类型不再重复 `Solstice` 前缀**——`namespace Solstice` 已经把工程身份说清楚了，`Solstice::FSolsticeRealityAnchor` 是口吃。`UObject` / `AActor` / `Slate` 类不能进命名空间，因此**保留**工程前缀（`USolsticeGameInstance`、`ASolsticeRealm`、`SSolsticeHUDView`）。两类写法并存是刻意的，不是不一致。

### 10.2 资产

| 类型 | 前缀 | 示例 |
|---|---|---|
| 静态网格 | `SM_` | `SM_PaperRock_A` |
| 材质 / 实例 | `M_` / `MI_` | `M_Paper` / `MI_Style_Woodcut` |
| 贴图 | `T_` | `T_Card_WeatherRain` |
| Niagara | `NS_` | `NS_Rain_Light` |
| 控件蓝图 | `WBP_` | `WBP_RealmHud` |
| 蓝图类 | `BP_` | `BP_RealmPawn` |
| DataAsset / DataTable | `DA_` / `DT_` | `DA_StyleTheme` / `DT_Cards` |
| 关卡 | `L_` | `L_Solstice` |

### 10.3 数据与目录

- JSON 文件：小写下划线，`species.json`、`cards.earth.json`、`styles.paper.json`
- **卡 id**：**唯一真源见 [`../setting/cards.md`](../setting/cards.md) §14**（示例 `card.water.3.rite.rain`、`card.fire+wood.4.rite.wildfire`；禁物 = `ban`，超环 = `over`）
- **物种 id**：`sp.<名>` → `sp.moss-grass`
- **界 id**：`realm.<slug>-<yyyymmdd>` → `realm.valley-20261016`
- **风格主题 id**：`style.<名>` → `style.paper-diorama`
- **预设 id**：`preset.<名>` → `preset.eternal-winter`
- **C++ 文件**：一律以模块前缀 `Solstice` 开头，stem = 主类型去掉类型前缀后的干名 —— 判定表见 **§10.4**
- **文档 id**：与 `docs/` 下的相对路径一致（`charter/CHARTER.md`、`tech/tech.md`…），见 [`../INDEX.md`](../INDEX.md)

> 术语与命名的**语言层**规范（中英对照、文案禁忌）见 [`../setting/LORE.md`](../setting/LORE.md) **§4 术语与命名规范** / **§5 文案与本地化规范**。

### 10.4 文件名 ↔ 主类型名（判定表）

三条判定法则：

1. **文件是模块级的，类名是类型级的**。文件名恒带 `Solstice` 前缀（`SolsticeRealm.h`），即便里面的类型按 §10.1 已经去掉了前缀（`namespace Solstice` 里的 `FRealityAnchor`）。前缀属于作品，不属于类。
2. **主类型决定文件名，其余是同域配角**。一个文件可以装多个类型 —— `SolsticeTypes.h` 装了全部枚举与共享 POD，`SolsticeRealityAnchor.h` 装了 `FRealityCity` + `FRealityAnchor`。配角不参与命名。
3. **引擎对拼词不进文件名**。`Realm` 已经是 `AActor`，再叫 `SolsticeRealmActor.h` 是把「它是个 Actor」这件引擎事实写进作品词汇，属于重复；同理 `SolsticeHUDActor.h`。

| 文件 | 主类型 | 判定 |
|---|---|---|
| `SolsticeRealm.h` | `ASolsticeRealm` | ✅ 单类型，去 `A` = 文件名 |
| `SolsticeHUD.h` | `ASolsticeHUD` | ✅ 单类型（演员那一半） |
| `SolsticeHUDView.h` | `SSolsticeHUDView` | ✅ 单类型（Slate 控件那一半）—— 见下 |
| `SolsticeGameMode.h` | `ASolsticeGameMode` | ✅ 单类型 |
| `SolsticeObserver.h` | `ASolsticeObserver` | ✅ 单类型 |
| `SolsticePlayerController.h` | `ASolsticePlayerController` | ✅ 单类型 |
| `SolsticeGameInstance.h` | `USolsticeGameInstance` | ✅ 单类型 |
| `SolsticeKernel.h` | `FSimKernel`（+ `FRealitySample`） | ✅ 单类型，文件名取**域名词干** `Kernel`，不照抄类干 `SimKernel` |
| `SolsticeMeshBuilder.h` | `FMeshBuilder` | ✅ 同上（域词干 = `MeshBuilder`，恰好与类干同形） |
| `SolsticeDataLoader.h` | `FDataLoader`（+ `FLoadedContent`） | ✅ 单类型 + 配角 |
| `SolsticePaperMaterial.h` | `FPaperMaterials`（+ `FPaperMaterialSet`） | ✅ 单类型 + 配角；文件名**用单数**，因为它是「纸素材」这个域，不是一个实例 |
| `SolsticeRng.h` | `FRngBank`（+ `FRng`） | ✅ 聚合，域词干 |
| `SolsticeTypes.h` | 全部 `E*` 枚举 + 共享 POD | ✅ 聚合，域词干 |
| `SolsticeLocalization.h` | `FLocalization` | ✅ 单类型 |
| `SolsticeGenerators.h` | `FPaperPalette` / `EStandeePlane` / `FStandeeCard` / `FStandeeRecipe` | ✅ 聚合；**没有主类型**，用域词干 `Generators` |
| `SolsticeEditor.h` | `FSolsticeEditorModule` 等 4 个 | ✅ 模块头，域词干；实现拆在 `Private/` 各自 `.cpp` |
| ~~`SolsticeRealmActor.h`~~ | ~~`ASolsticeRealm`~~ | ❌ 引擎对拼词重复；已改名 |
| ~~`SolsticeHUDActor.h`~~ | ~~`ASolsticeHUD`~~ | ❌ 同上；已改名 |

**为什么 HUD 要分两个文件**：`ASolsticeHUD`（`AHUD` 派生，进不了命名空间）与 `SSolsticeHUDView`（`SCompoundWidget`，进不了命名空间）是两个不能进命名空间的类型，都保留 `Solstice` 前缀；同名前缀下必须靠词干区分身份，所以一个是「HUD」（演员），一个是「HUDView」（控件）。`SolsticeHUDActor.h` / `SolsticeHUD.h` 这套旧命名把「演员」写成了 `Actor` 后缀 —— 换成「裸名给演员、`View` 给控件」，与 UE 自身 `AHUD` / Slate 的分工一致。

**`.generated.h` 随文件走**：UHT 由文件名派生反射头（`SolsticeRealm.h` → `SolsticeRealm.generated.h`）。改文件名会让 UHT 重新生成，`Intermediate/` 里旧名的 `.obj` / `.hpp` 只是残渣、不参与链接；但**改完必须重编**，否则 `IMPLEMENT_CLASS` 宏里的旧字符串会与新 `.generated.h` 对不上。

---

## 11 归位决策表（新增东西放哪）

| 我要新增 | 放哪 | 入 git | 状态 |
|---|---|---|---|
| 一段游戏逻辑 | `game/Source/Solstice/<对应子目录>/` | ✅ | ✅ |
| 新干涉卡 | 数据 `game/Config/Solstice/Data/cards.*.json` + 卡面走 `studio/jobs/` | ✅ / 产物 ❌ | ✅ |
| 新物种 | `game/Config/Solstice/Data/species.json` + 几何生成器 `Source/Solstice/Gen/` | ✅ | ✅ |
| 新风格主题 | `shared/schema/style.schema.json` 参数（待 M1）+ `studio/presets/style.*.json` + `Content/Styles/<名>/` | 参数 ✅ / 资产 ❌ | 🟡 |
| 美术素材（贴图 / 立牌 / 卡面） | 源与参数 `studio/`，产物 `Content/` 对应子目录 | 源 ✅ / 产物 ❌ | 🟡 |
| 新控件 | C++ 优先（`Source/Solstice/UI/` Slate）；可视化打磨放 `Content/UI/` | ✅ / ❌ | ✅ |
| 新的无头检查 / 批处理 | `Source/SolsticeEditor/` 增 Commandlet，并在 `tools/run/` 或 `tools/build/` 加同名薄包装脚本 | ✅ | ✅ |
| 环境 / 构建 / 运行脚本 | `tools/env/` · `tools/build/` · `tools/run/`（放哪先看 [`../../tools/README.md`](../../tools/README.md) 的分层表） | ✅ | ✅ |
| 脚本共用的定位逻辑（引擎 / 工程 / 可执行文件） | `tools/lib/Solstice.Common.ps1`（**唯一实现**，脚本 dot-source 复用，禁止再写第二份） | ✅ | ✅ |
| AI 工具链的用户级配置模板 | `tools/ai-config/`（只存**不含密钥**的模板 + 同步脚本；真配置在 `~/.codebuddy/`，不入库） | ✅ | ✅ |
| 文档 / 契约自检脚本 | `tools/checks/`（Python，标准库） | ✅ | ✅ |
| 提交钩子 / CI 工作流 | `tools/hooks/` + `.github/workflows/`（只包 checks/ 现有两条，不新增逻辑） | ✅ | ✅ |
| 设计文档 | `docs/`（先读 [`../INDEX.md`](../INDEX.md) 的事务域表） | ✅ | ✅ |
| 存档 / 日志 / 缓存 | `Saved/`、`D:\UE-DDC` | ❌ | ✅ |
| 临时试验 | `studio/work/`（已忽略）或仓库根 `tmp/`（已忽略） | ❌ | ✅ |

---

## 12 git 与 Content 重建

**入 git**：`game/Source`、`game/Config`、`game/Solstice.uproject`、`game/Content/README.md` 与 `.gitkeep`、`game/BLUEPRINT-GUIDE.md`（落地后）、`shared/`、`studio/{workflows,jobs,presets,tools,README.md}`、`studio/out/manifest.jsonl`、`tools/`、`docs/`、`.vscode/`、`.github/`、`.codebuddy/`、`.workbuddy/`、`.gitattributes`。

**不入 git**：`game/{Binaries,Intermediate,Saved,DerivedDataCache}`、`game/Content/**` 的二进制（含生成的 `M_Paper.uasset` / `M_Ink.uasset`）、`studio/out/**`（除 manifest）、`studio/work/**`、`.vs/`、`*.sln`、`*.slnx`、`.vsconfig`、`tmp/`。

**可重建性的保证**（对单机开发、重装系统、更换机器同样成立）：Content 不入 git ⇒ **同一套"种子 + 生成器 + 契约 + manifest"**可完整重建产物。因此：
1. 生成器与契约的改动必须与产物**同一次提交**；
2. manifest 是产物的"身份证明"，缺 manifest 的素材视为**不可复现**，不得进游戏；
3. 卡面 / 立绘类素材**单独提交**，先过用户裁决。

> **操作细节**：提交规范见 [`../ops/START.md`](../START.md) §6 / §8。本节只定义**哪些目录入 git**。旧的双机同步约定（`SYNC` v1.1）已随双机协作废止而归档。

---

## 13 由本文带来的待办

1. ~~重建 `studio/`~~ ✅ 已落地（工作流 / jobs / 预设 / 驱动脚本）。
2. ~~重建 `shared/schema/`~~ ✅ 已落地 4 个（`save` 为信封骨架）；`style` 待 M1。
3. ~~`game/` 工程骨架~~ ✅ 已落地（`.uproject` + 2 个 Target + 2 个模块，编译并运行通过）。
4. ~~`tools/` 新增 headless 基准脚本入口~~ ✅ `tools/run/bench.ps1`；另加 `tools/build/gen-assets.ps1`。
5. ~~落地后回来改状态列~~ ✅ 本文 v3.0 已改。
6. **待 M1**：`studio/refs/`（风格锚图）、`shared/schema/style.schema.json`、`save.schema.json` 载荷收紧（信封已落地）、独立零依赖 Sim 模块。
7. **待 P5**：**运行时 JSON 打包 stage**——`game/Config/Solstice/{Data,Localization}/*.json` 走 `FPaths::ProjectConfigDir()`，UAT 默认只 stage `Default*.ini`，打包版读不到。方案：迁 `game/Content/Solstice/` + `DefaultGame.ini` 加 `+DirectoriesToAlwaysStageAsUFS`，同步改 `FDataLoader` 路径与本文 §5/§9，并实测一次打包。

---

## 14 更新触发

| 改动类型 | 必须同次更新 |
|---|---|
| 新增 / 移动目录、改命名 | 本文件对应章节（含 §11 归位表）+ `.gitignore` 注释 + [`../INDEX.md`](../INDEX.md) |
| 目录从"计划"变为"现状" | 本文件所有 🟡 状态列 + 相关文档的存在性描述 |
| 入 git 策略变化 | 本文件 §12 + [`../ops/START.md`](../START.md) §8 |
| 模块 / Target / Commandlet 增删 | 本文件 §5 / §6 + [`../ops/ENVIRONMENT.md`](../../tools/docs/ENVIRONMENT.md) §5.6 |
| 命名规范变化 | 本文件 §10 + [`../setting/LORE.md`](../setting/LORE.md) §4.2（如涉及文案用词） |

---
