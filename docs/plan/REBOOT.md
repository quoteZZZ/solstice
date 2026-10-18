---
id: REBOOT
title: 架构与画面重启方案 v2（待确认）
status: draft
truth: spec
owner: both
---

# REBOOT v2 · 架构、管线、色板

> 用户 2026-10-18 的五条：
> ①目录树要按目标需求重做、命名要改 ②PBR 与风格化 Lit 不确定，要高画质但不确定性能
> ③架构要分析完善、避免重调 ④色彩很差，重新分析 ⑤**没有红线了，所有文件都可改**
>
> 本文逐条回答。**确认后才动代码。**

---

## 0 先说结论

| 问题 | 结论 |
|---|---|
| 风格 vs 画质 | **不用二选一**。UE 里两者都是 Lit，差别只是「光照被量化成几档」。做成 `ToonRamp` 参数（1=纯卡通 / 0=连续 PBR），一套管线覆盖全部风格，随时可调 |
| 性能 | RTX 5060 @1440p 分项预算合计 **≈9 ms**（60fps 有 7ms 余量）；加 Lumen 软件 GI 再 +3–5 ms，进得去但要降云一档。**先不开 Lumen，B1 结束实测后决定** |
| 架构 | **只拆一个模块**：`SolsticeKernel`（纯 C++，禁 CoreUObject）。其余域是目录 + 脚本守依赖方向。DLL 边界只在"必须编译期强制"处值钱 |
| 色彩 | 换了方法论：不再用形容词，从 Wind Waker / 原神 / 吉卜力 / 新海诚 提取 HEX，按「明度阶梯 + 冷暖对位 + 60-30-10 饱和预算」组织。三套成品色板见 §5 |
| 目录树 | 按「内核 / 功能域 / 资源包」三层 + UE 官方惯例重写，见 §2 |

---

## 1 为什么之前丑（实测证据）

`tmp/review/shot.png`（seed 20261016 / 12:00）的实际读数 + `DefaultEngine.ini` 的实际配置：

| 现象 | 根因 |
|---|---|
| 海是一块纯色平板 | 海面是顶点色圆盘（`Gen::BuildOcean`），无波、无法线、无高光、无反射 |
| 岛上没有任何阴影 | 五个材质全是 `MSM_Unlit`（`SolsticeCelMaterialBuilder.cpp`），明暗写死在顶点色里 |
| 岛像贴纸 | `AmbientOcclusion=False`、`DynamicGlobalIlluminationMethod=0`，物体与地面之间没有暗部 |
| 天空是白条 + 蓝渐变 | 天空是画出来的卡片，`SupportSkyAtmosphere=False` |
| 边缘锯齿 | `AntiAliasing=2`（TAA），没有 TSR、没有锐化 |
| 地表一眼是色块 | 无贴图、无法线、颜色只有 4 档常量 |
| 树是绿球 | 树冠是 2 个六边形 `Cushion` |

**结论**：整条渲染管线被主动关闭，上限就是"干净的低多边形平涂"。这不是调色板能救的。

---

## 2 目录树（按目标需求，不按当前代码）

### 2.1 设计依据

1. **功能域优先，不按类型**（Allar UE5 Style Guide 的硬规则：`Meshes/` `Textures/` 会随规模腐化，类型已由 `SM_`/`T_` 前缀表达）。
2. **内核与引擎物理隔离**（`SolsticeKernel` 不链接 `CoreUObject`）——这是唯一值得付 DLL 代价的边界。
3. **资源包可整块启停**（`Content/Packs/Realm_*/` + `PrimaryAssetLabel`，每包一个 chunk）。
4. **多 agent 并行**：一域一目录一 owner；共享文件（`.uproject` / `*.Build.cs` / `DefaultEngine.ini` / 母材质）单写者。

### 2.2 完整树

```text
solstice/                             仓库根
├─ README.md
├─ docs/                              见 §6
├─ game/                              ← UE 工程（只放引擎要的东西）
│  ├─ Solstice.uproject
│  ├─ Config/
│  │  ├─ DefaultEngine.ini            渲染开关 · 默认地图 · GameMode
│  │  ├─ DefaultGame.ini              AssetManager 扫描 · 项目身份
│  │  ├─ DefaultInput.ini
│  │  └─ Solstice/
│  │     ├─ Data/                     ★ 运行期唯一真源（JSON，入 git）
│  │     │  ├─ manifest.json          域清单 + schema 版本 + 各文件 hash
│  │     │  ├─ world/                 terrain / water / sky / climate / biome
│  │     │  ├─ realm/                 世界预设（一个 pack 一个文件）
│  │     │  ├─ species/               一物种一文件
│  │     │  ├─ cards/                 一元素一文件
│  │     │  └─ render/                palette / toon / postfx / camera
│  │     ├─ Tags/                     GameplayTag，按域拆文件（避免争用）
│  │     └─ Localization/             zh-Hans.json / en.json
│  ├─ Content/
│  │  ├─ Solstice/                    ★ 项目资产根（不建 Meshes/Textures 类型目录）
│  │  │  ├─ Core/                     母材质 M_Cel / M_Sea / M_Sky / M_Ink / M_Foliage、MPC、LUT、材质函数
│  │  │  ├─ World/
│  │  │  │  ├─ Terrain/               SM_ 地形件、地形材质层、坡面遮罩
│  │  │  │  ├─ Water/                 水面网格、水材质、NS_ 浪花
│  │  │  │  ├─ Sky/                   天穹、云、天体、气候天空
│  │  │  │  ├─ Foliage/               SM_ 植被、FoliageType、风场
│  │  │  │  ├─ Fauna/                 SK_/ABP_ 生物、代理网格
│  │  │  │  └─ Props/                 石头、场景点缀、卡牌实体
│  │  │  ├─ Cards/                    DA_Card_*、卡面、投放 VFX
│  │  │  ├─ UI/                       WBP_ / Style_ / Font_，按界面分子目录
│  │  │  ├─ Audio/                    MetaSound、SoundClass、环境床
│  │  │  ├─ VFX/                      通用 NS_
│  │  │  ├─ Data/                     烘焙产物（JSON → DA_/DT_，禁手改）
│  │  │  └─ Maps/                     L_Entry 等入口关卡
│  │  ├─ Packs/                       ★ 资源包（可启停，每包一个 PrimaryAssetLabel）
│  │  │  ├─ Realm_Temperate/          温带（默认）
│  │  │  ├─ Realm_Arctic/             寒带
│  │  │  └─ Realm_Volcanic/           火山
│  │  └─ Developers/<name>/           个人沙盒，不进包
│  ├─ Plugins/
│  │  └─ ThirdParty/                  Fab 资源，原样不改
│  └─ Source/
│     ├─ Solstice.Target.cs
│     ├─ SolsticeEditor.Target.cs
│     ├─ SolsticeKernel/              ★ 纯 C++ 静态库：零 UObject / 零 Tick / 零渲染
│     │  ├─ SolsticeKernel.Build.cs   只依赖 Core，显式禁 CoreUObject
│     │  ├─ Public/Kernel/
│     │  │  ├─ World/                 种子 → 高度场 / 水系 / 气候场 / 生态域
│     │  │  ├─ Time/                  世界钟、季节相位、天气状态机
│     │  │  ├─ Ecology/               物种、种群、演替
│     │  │  ├─ Event/                 事件与年表
│     │  │  ├─ Rng/                   分域种子派生、位置哈希
│     │  │  └─ Contract/              内核与引擎共用的 POD 结构与枚举
│     │  ├─ Private/
│     │  └─ Tests/                    无头单测，不启引擎
│     ├─ Solstice/                    ★ 唯一 Runtime 模块（功能域 = 子目录）
│     │  ├─ Solstice.Build.cs
│     │  └─ Public|Private/
│     │     ├─ Core/                  跨域契约、DataLoader、快照、事件总线
│     │     ├─ World/                 分块、Actor 生命周期、内核→场景投影
│     │     ├─ Meshing/               三角化、LOD、StaticMesh 烘焙
│     │     ├─ Water/                 海面网格、Gerstner、泡沫、岸线
│     │     ├─ Sky/                   大气、云、天体、昼夜
│     │     ├─ Foliage/               HISM 分布、风动、季节换色
│     │     ├─ Fauna/                 代理池、群体表现
│     │     ├─ Render/                材质解析、Cel 分带、描边、PostFX
│     │     ├─ Camera/                32 格点机位、移轴、遮挡回避
│     │     ├─ Player/                输入、观察者、卡牌交互
│     │     ├─ Save/                  存档、多世界管理
│     │     ├─ Audio/                 环境层、事件映射、混响区
│     │     ├─ UI/                    Slate / UMG
│     │     └─ Diagnostics/           抓帧、bench hook、stat 组、debug draw
│     └─ SolsticeEditor/              编辑器：烘焙、校验、Bench
│        ├─ Commandlet/               GenAssets / VerifyAssets / Bench
│        ├─ Builders/                 材质图、DataAsset 烘焙
│        └─ Tests/
├─ shared/                            跨工程契约
│  ├─ schema/                         JSON Schema（内核数据的唯一契约）
│  └─ palettes/                       色板导出（json + png 色卡）
├─ studio/
│  ├─ previews/                       原 HTML 原型（算法出处）
│  └─ presets/                        世界预设
└─ tools/
   ├─ lib/                            共享 PS 库（找引擎等）
   ├─ env/                            环境配置
   ├─ build/                          编译、资产生成
   ├─ run/                            bench、抓帧、试玩
   ├─ checks/                         链接、契约、层次、画面量具
   └─ ai-config/                      MCP / 编辑器集成
```

### 2.3 命名调整（相对现状）

| 现状 | 改为 | 为什么 |
|---|---|---|
| `Gen/` | `Meshing/`（引擎侧）+ `Kernel/World/`（纯算法） | 现在一个目录同时装高度场和三角形，两类东西生命周期完全不同 |
| `God/` | `Player/` | `God` 是玩法的说法，不是代码职责；`Player` 装的是输入、控制器、交互 |
| `Dev/` | `Diagnostics/` | `Dev` 太模糊，无法判断什么该进来 |
| `Sim/`（混装） | 拆进 `SolsticeKernel/{World,Time,Ecology,Event,Rng}` | 一个 `FSimKernel` 承担四种职责，无法单独演进 |
| `Config/Solstice/Data/world.json`（单文件 6.7 KB） | 拆到 `Data/world/*.json` | 单文件是并行开发的头号冲突源 |
| `world.json` 的 `algo` 字段 | 删除，改 `realm/*.json` 的 `archetype` | `algo` 已失效（形制来自骰子），留着只会误导 |
| `M_Paper`（历史名） | `M_Cel` | 纸的时代已过 |

---

## 3 架构规则（可被脚本强制）

### 3.1 依赖矩阵

| 从 ↓ 依赖 → | Kernel | Core | 功能域 | Render | Editor |
|---|---|---|---|---|---|
| `SolsticeKernel` | — | ✗ | ✗ | ✗ | ✗ |
| `Solstice/Core` | ✓ | — | ✗ | ✗ | ✗ |
| `Solstice/<域>` | ✓ | ✓ | ✗ | ✓(仅 PostFX 接口) | ✗ |
| `Solstice/Render` | ✓ | ✓ | ✗ | — | ✗ |
| `SolsticeEditor` | ✓ | ✓ | ✓ | ✓ | — |

**唯一的硬规则**：`SolsticeKernel` 的 `.Build.cs` 里 `PublicDependencyModuleNames` 只有 `Core`，**不允许**出现 `CoreUObject` / `Engine`。违反时链接期就会失败，不需要脚本。

其余规则由 `tools/checks/check-layers.py` 守（`#include` 白名单 + 目录方向）。

### 3.2 扩展点（加东西要改哪里）

| 我要加… | 要动的文件 | 不用动 |
|---|---|---|
| 一个新世界主题 | `Content/Packs/Realm_X/` + `Config/Solstice/Data/realm/x.json` + manifest 一行 | 任何 C++ |
| 一个新功能域（如天气） | `Solstice/Weather/` 一个目录 + Build.cs 一列 | Kernel、其他域 |
| 一个新物种 | `Config/Solstice/Data/species/x.json` + 可选 `Content/World/Fauna/` 网格 | 代码 |
| 一个新内核规则 | `SolsticeKernel/Ecology/` 一个文件 + 提升 `SaveVersion` | 任何表现层 |
| 一个新卡 | `Config/Solstice/Data/cards/<元素>.json` 追加一条 | 代码 |
| 一个新机位 | `Config/Solstice/Data/render/camera.json` 追加一条 | 代码 |

### 3.3 并行开发规则

1. **一域一目录一 owner**：agent 只写自己域 + `Data/<域>/` + `Content/Solstice/<域>/`。
2. **共享单写者**：`.uproject`、`*.Build.cs`、`DefaultEngine.ini`、`Data/manifest.json`、母材质 —— 只有集成者改。
3. **只增不改**：内核结构体加字段必须带默认值并升 `SaveVersion`；枚举按域拆文件。
4. **资产不并行重建**：`Content/**` 由单一 job 生成，agent 不碰 `.uasset`。
5. **合并序固定**：`Kernel → Core → 域 → Content`。

---

## 4 渲染：把风格做成参数

### 4.1 关键认识

"卡通"和"PBR"不是两条管线，**是同一个 Lit 光照被量化成几档**：

```
N·L（连续光照） → [ToonRamp 量化] → 出屏
                 ToonRamp=1  → 3 档硬切（赛璐璐）
                 ToonRamp=0.5 → 3 档 + 段内渐变
                 ToonRamp=0  → 连续（半写实 PBR）
```

所以：**先按 Lit 建管线，风格留一个参数**。任何时候都能在编辑器里从"原神"滑到"半写实"，不需要重做资产。

### 4.2 管线清单（一次定死）

| 项 | 决策 | 关键参数 |
|---|---|---|
| 着色模型 | `MSM_DefaultLit` + 材质内 Ramp 量化 | `ToonRamp` 1.0 / 分段点 0.30 · 0.62 |
| 主光 | Directional + CSM 3 级 | 2048 / 60 m / 接触阴影 0.15 |
| 环境光 | SkyLight（实时捕获）+ SSAO | SSAO 半径 0.6 m / 强度 0.8 |
| GI | **先 SSAO，B1 后实测再决定是否上 Lumen** | Lumen 软件追踪，Diffuse only |
| 反射 | SSR + 水面平面反射 | SSR Quality 2 |
| 水 | **Single Layer Water** + Gerstner 6 层 | λ 3–10 m / A=λ/20 / ΣQ≤1 / 泡沫 J<0.3 |
| 天空 | SkyAtmosphere + 体积云 | 覆盖率 0.15–0.6 / 消散高度 3–8 km |
| 地形材质 | 顶点色作 macro tint + Triplanar 细节法线 | tiling 1.5 m / 法线强度 0.6 |
| 植被 | HISM + 500–3000 tri/丛 | 剔除 60 m / 风动 WPO 0.03 |
| AA | **TSR** | `r.AntiAliasingMethod=4`、History 200%、Sharpen 0.4 |
| 曝光 | **固定 EV100** | 关自动曝光 |
| 后处理 | Bloom 0.4 / 阈值 1.2 + 32³ LUT + 移轴 DOF + 暗角 0.25 | — |
| 禁用 | Motion Blur / Chromatic Aberration / Film Grain | 全部 0 |

### 4.3 性能预算（RTX 5060 · 1440p · 目标 16.6 ms）

| 项 | ms |
|---|---|
| 几何 + 材质 | 3.0 |
| 阴影 CSM 3×2048 | 0.9 |
| SSAO + SSR + 接触阴影 | 1.1 |
| 体积云 + 大气 + 水面 | 2.2 |
| TSR + Bloom + 调色 | 1.8 |
| **合计** | **≈9.0** |
| Lumen 软件 GI（可选） | +3.0 ~ 5.0 |

**结论**：不开 Lumen 有 7 ms 余量（可上 4K 或更密的植被）；开 Lumen 仍在 16.6 内，但要把体积云降到 2 层、云分辨率减半。**先不开，B1 结束时用真实数字决定。**

分档：

| 档 | 分辨率/帧率 | 差异 |
|---|---|---|
| 壁纸档（默认） | 1440p / 60 | 全开 |
| 低配档 | 1080p / 30 | TSR→SMAA、云换 2 层卡片、SSR 关 |

### 4.4 要改的 ini（一次列清）

```ini
[/Script/Engine.RendererSettings]
r.AntiAliasingMethod=4                 ; TSR
r.DefaultFeature.AutoExposure=False
r.DefaultFeature.AmbientOcclusion=True
r.DefaultFeature.AmbientOcclusionStaticFraction=1.0
r.DefaultFeature.MotionBlur=False
r.DynamicGlobalIlluminationMethod=0    ; 先关；Lumen 待实测
r.ReflectionMethod=1                   ; SSR
r.Shadow.Virtual.Enable=0              ; 卡通风用 CSM，VSM 会糊掉色阶
r.SupportSkyAtmosphere=True
r.SupportVolumetricFog=False           ; 近景雾用平面层
r.GenerateMeshDistanceFields=True      ; SSR / 接触阴影需要
r.Nanite.ProjectEnabled=True           ; 烘焙后的 StaticMesh 才用
```

---

## 5 色板（三套，含来源）

### 5.1 方法论（这次为什么不一样）

之前是"用形容词让 AI 生成"，出来的是平均化的糖水色。这次改成三条可执行的法则：

1. **明度阶梯**：卡通着色 = 把连续光照量化成 band。要 4 档（高光 L\*85–95 / 亮 65–80 / 中 40–55 / 暗 20–35）。相邻档 ΔL\* < 12 会"脏"，> 35 会"贴纸"。
2. **冷暖对位**：日间与黄金时刻用**暖光冷影**（阴影必须换色相偏蓝紫，不能只压暗）；阴天/黎明/月光反过来。
3. **饱和度预算 60-30-10**：60% 大面积 S 20–45%、30% 次级 S 40–60%、10% 焦点 S 70–90%。**高饱和超过 15% 面积立刻变糖水。**

### 5.2 三套成品（完整表见 [`../art/PALETTE.md`](../art/PALETTE.md)）

| | A 地中海正午 | B 北海道初夏 | C 黄金时刻 |
|---|---|---|---|
| 参考 | 风之杖 × 蒙德 | 吉卜力 × A Short Hike | 你的名字 × 夕照海拉鲁 |
| 意图 | 青绿海、近白地平线、通透天真 | 低饱和柔光、奶油光青灰影 | 冷紫天顶 + 橙金地平线 |
| 适用 | **默认白天** | 阴天 / 清晨 / 秋季 | 日循环高潮 15 分钟 |

三套都满足：海三档 ΔL\* ≥ 22、天空三段色相递变、阴影带冷偏移。

### 5.3 之前为什么"色彩很差"（诊断）

1. 饱和度全拉满，没有 60-30-10 的焦点层级；
2. 暗部只压明度不换色相 → 灰脏；
3. 天空只有一档蓝，空间被压成背景板；
4. 明度阶梯过密（草/土/岩全挤在 L\*55–70），剪影糊成一片。

### 5.4 验证工具

`tools/checks/grade-preview.py` —— 用精确 HEX 重绘当前帧，输出 `tmp/review/palette/`。这是**色板验证**，不是渲染结果；它只证明颜色结构，不证明光照质量。

---

## 6 文档重整

| 动作 | 文件 | 说明 |
|---|---|---|
| 重写 | `plan/REBOOT.md`（本文） | 架构 + 管线真源 |
| 新建 | `tech/LAYERS.md` | 三层定义、依赖矩阵、扩展点、检查脚本 |
| 新建 | `art/PALETTE.md` | 三套色板 HEX + 来源 + 色彩法则 |
| 新建 | `tech/RENDER.md` | 管线真源（材质、光照、后处理、性能预算） |
| 重写 | `plan/work.md` | 按 §7 批次重排，旧单全部作废 |
| 降级 | `setting/WORLD.md` §3.3 | 生成算法已被代码超越，改为一页指针 |
| 合并归档 | `later/` 8 份 → `archive/M1-SPEC.md` | 冻结件本来就"M1 再读" |
| 保留 | `START.md` `INDEX.md` `CONSTANTS.md` `CHANGELOG.md` `charter/CHARTER.md` `art/art.md`(审美部分) | 结构是对的 |
| 更新 | `CHARTER.md` §6 | 记明推翻的三条红线及理由（备查，不做约束） |

---

## 7 执行批次

| 批 | 内容 | 验收 |
|---|---|---|
| **B0 地基** | ① `check-layers.py` ② 目录迁移（`Gen`→`Meshing`、`God`→`Player`、`Dev`→`Diagnostics`、`Sim`→`Kernel`）③ `SolsticeKernel` 模块建起来 ④ ini 按 §4.4 改 | `bench` digest 一位不变；编译过；两条自检 0 错 |
| **B1 海面** | `Water/` + `M_Sea`(SLW) + Gerstner + 泡沫；机位降到海平面 | 俯视数得出 3 层水色；浪尖有泡沫；岸线白边；无硬缝 |
| **B2 天空** | `Sky/` + SkyAtmosphere + 体积云 + 天体 | 三段可辨；云有体积；太阳不是甜甜圈 |
| **B3 地形材质** | Triplanar + 曲率 AO + macro tint | 近景有纹理；坡度有明暗切分 |
| **B4 植被/岩石** | 真实网格 + HISM | 剪影不是圆球 |
| **B5 内核拆分 + 世界骰** | K1–K4 归位 + 参数表 + 分享码 | digest 不变；参数可驱动 |
| **B6 文档** | §6 全部 | `check-links` 0 错 |

**B1/B2 可并行**（都只依赖 B0）。B3/B4 依赖 B1。B5 独立。B6 最后。
