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
│  ├─ Core/      GameInstance、世界时钟（虚拟）、数据加载、本地化
│  ├─ Sim/       世界钟 + 卡指令 + 余响 + 年表（纯逻辑，门面 SolsticeKernel）
│  │  ├─ State/    ✅ 类型、世界状态、RNG、编年史
│  │  └─ Gen/      ✅ 世界生成（形制参数 + 高度场）
│  │     …        🟡 Rules/ 物理法则 · Entities/ 物种与聚落 · Cards/ 卡与原语
│  │                ——**计划中，目录尚未建**，不要按"已存在"去 include（PLAN G-20）
│  ├─ Gen/       程序化几何（地形网格 / 天空球 / 海 / 道具）
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
| `game/Config/Solstice/Data/world.json` | 世界配方（[WORLD.md](WORLD.md) §4 的 15 参数 + 预设） |
| `.../Data/cards.{earth,fire,metal}.json` | 各元素 12 张卡（M0 共 36） |
| `.../Data/species.json` | 五个物种模板 |
| `.../Localization/{zh-Hans,en}.json` | 中英双语，**key 必须一一对齐**（当前 169 : 169） |
| `shared/schema/*.json` | `card` / `species` / `world` / `save` 四份契约 |
| `shared/fixtures/save.sample.json` | 存档信封样例 |

**规则**：改 schema 必须同次改 `game/Config/` 与 `studio/presets/`；`validate-contracts.py` 校验 6 对契约 + 41 个 id + 双语对齐。

**已知缺口**：`world.json` 自己有 `_wiring` 自述——16 个字段（`relief / ranges / bearing / rivers / lakes / aridity / season_amplitude / weather_bias / wind_dir / vegetation_density / biome_pool / species_pool / start_season / start_hour / sky.*`）**"NOT read yet"**；`FWorldConfig` 实际只消费 12 个键，且 `algo` 读了不用。**loader 必须打印未消费字段清单**，别让"文档以为接通了"再发生一次。

**尺寸与高度**：`FWorldConfig::Size` 与 `world.json` 双默认 **128**；高度上限代码写死 **64 m**（对应 [WORLD.md](WORLD.md) §3.1 的 256² 档）。按 §3.1 换档时高度上限应随 `W` 走，这条随尺寸档一起改。

**参数 → 字段对照**（新增参数时照这张表落，别再出现"文档写了、代码没读"）：

| WORLD §4 参数 | 代码落点 | 状态 |
|---|---|---|
| `land_ratio` | `FWorldConfig::LandRatio`（二分海岸线） | ✅ 生效 |
| `water_level` | `FWorldConfig::BaseWaterLevel` | ✅ 生效（**代码默认 18 m 与数据 3 m 差 6 倍**，统一到数据侧） |
| `climate` | `FWorldConfig::Latitude`（当前用纬度代理） | 🟡 需要独立字段 |
| `wind_dir` | `FRoll::WindDir` | 🟡 内部抽了，配置不接 |
| `relief` | `FRoll::Relief` | 🟡 同上 |
| `topology / shape / rim / water_cover` | **无** | ❌ 见 PLAN P-08 |
| `ranges / bearing` | `FRoll::Orogeny*`（单条山带） | 🟡 只有 1 条 |
| `compactness / coast_rough` | `FRoll::Aspect` / `CoastWarp`（语义不同） | 🟡 需改名对齐 |
| `veg_density / species_pool_size` | **无**（物种是固定 5 条 JSON） | ❌ 见 PLAN P-10 |
| `anomaly` | **无** | ❌ M1 |

**水面网格**：**按 `water_cover` 与 `rim` 决定是否发射**——无水界不建海网格、不建泡沫环；水世界要把水面提到 `0.02 W` 内圈覆盖海床。这一条现在代码里是"永远建一个环形海"，属形制改造的一部分（P-08）。

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
- 渲染：128² / 1280×720 / 24° 机位 **16.1 ms（62 fps）**；同一场景 **3840×2160** 仍受 vsync 限幅，未单独计时。
- 抓帧：`shot.ps1` 现在**必须**带 `-ForceRes`（引擎默认忽略 `-ResX/-ResY` 用存档窗口尺寸，此前所谓"4K"实际是 888×500）。
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
| 反射 | SSR；**海面自己算天空**：`SeaSky` 在像素里重算 `M_Sky` 的同一段梯度，逐像素反射 + 太阳镜面路径 | 常数色反射不会随波面倾斜，也不会有太阳 |
| 天空 | 相机子挂单位球 + `M_Sky` 逐像素 | 平面卡片有边；`SkyAtmosphere` 组件已移除（permutation 未编译时渲染纯黑，且画不出手绘云带） |
| 雾 | 指数雾，**晴天色 = 天空地平色 `0xD8E9F0`** | 海面在天际线处是 100% 雾、天空穹顶几乎不吃雾；雾色差几个百分点就是一道硬线 |
| 阴影 | **无阴影贴图参与**：树的投影在生成期烘进地面顶点色 | 地表可见亮度几乎全部来自 Emissive，阴影贴图影响不到它（实测：关阴影零像素差） |
| 环境遮蔽 | **顶点色 A 通道烘进反照率** | 反照率乘进 Emissive，是唯一能被看见的"暗部"通道 |
| 禁用 | Motion Blur / Lens Flare / Lumen / Nanite / 体积云 / 体积雾 | 卡通风杀手或依赖不存在的场景表示（`r.Nanite.ProjectEnabled=False`） |
| 颗粒 | **FilmGrain 0.035** + 海面屏幕空间抖动 5.5% | 海与天都是极慢的渐变，8 bit 下会出等高线 |
| 投影 | **当前透视 + 长焦（≥85 mm 等效）**；正交是可选方案 | 长焦压缩已能读出 2.5D 感；**正交会让 `M_Sea` 的逐像素天空反射与指数雾整片错**（都按透视距离写死），要做必须一起重算（ART §7.2 · CHARTER D-5） |

> **给下一个接手的人**：改 ini 里的 `r.SupportSkyAtmosphere` 之类**shader permutation** 开关不会立刻生效，必须先重编 permutation，否则天空会渲染成纯黑（已踩过一次并回退）。
> **Nanite.ProjectEnabled=True 只是为"烘焙成 StaticMesh 后再说"留的开关**，当前没有 Nanite 网格。

---

### 6.1 HUD 的度量（唯一比例源）

`SolsticeHUDView.cpp` 里所有尺寸都按 **1080p 书写**，由 `GUiScale = clamp(视口高 / 1080, 0.85, 2.0)`
统一缩放（字号走 `Font()`，间距/宽度走 `Px()`）。Slate 自带的 DPI 曲线**不会**作用于直接挂到
viewport 的 widget，所以这一层必须自己做——缺了它，2560 宽的窗口会用 1280 的字号作画，
整块 HUD 显示成一条看不清的细带（这就是"UI 又小又简陋"的根因，不是配色）。

改字号只改 `Font()` 的入参，**不要在调用点写像素值**；新增控件请用 `Px()` 包住所有 padding / 宽高。

**三栏版式（2026-10-20 二改：抽屉作废，改固定栏框）**：

| 区 | 尺寸 | 放什么 | 不放什么 |
|---|---|---|---|
| 顶栏 | 高 **56** | 时钟 + 天气 + 扰动条 + 余响 + 六个入口图标 | 说明文字 |
| **左功能栏** | 宽 **56**，常驻 | **6 个页面入口**：抽卡 / 卡库 / 年表 / 创世 / 设置 / 帮助（40×40，键帽 + 汉字）+ 底部 1 个**模式开关**（摆件）。**只放动作，不放读数**；手牌不在这里（它本来就一直可见） | 任何读数、任何二级菜单 |
| 世界区 | 余下 | 3D 视口 + 落点 + 作用圈 + toast | 常驻面板 |
| **右信息栏** | 宽 **320**，常驻 | **三段，不分页签**（一屏读完）：① 这一界（界名 / 形制 / 气候 / 种子 / 余响）② 生态（5 条读数 + 12 生态域点亮）③ 刚刚发生（最近 5 条，末尾"全部 ▸"跳年表） | 操作按钮、需要滚动才能看全的内容 |
| 手牌区 | 高 **250** | **扇形悬浮、左右对称居中**：10 张 `140×204`（1:1.457）沿弧排布，角度按 `(i−中位)×3.3°`（最大 ±14.8°），边缘下沉 `d²×1.25 px`（最大约 25 px），`z` 由中间向两侧递减；悬停回正并抬升 34 px / 放大 1.05，选中抬升 46 px；**动作条在左下（占 250 宽），不压卡** | 卡的长文案 |

**为什么不做抽屉**：抽屉要么遮住世界（覆盖式），要么需要两次点击（开+关）；栏框把"功能"和"信息"各占一条固定窄边，世界区始终完整、点击成本为一。**左栏只放动作，右栏只放信息**，两边都不滚屏超过一屏。

**两条减法仍然成立**：① 说明性文字不进界面（规则进悬停/详情，帮助页只讲操作）；② 读数只进右栏，不在世界里飘。

> 版式基准 1920×1080；所有尺寸由 `GUiScale` 缩放（上行）。
> 可交互投影：[`docs/PROTOTYPE.html`](PROTOTYPE.html)（可点、可走流程）；静态板 [`docs/WIREFRAMES.html`](WIREFRAMES.html) 的 B1–B3 已被本次改版作废。

**世界内仪表（固定在视口，不随世界动）**：

| 仪表 | 位置 | 规则 |
|---|---|---|
| 比例尺 | 右下 18 px | 随 Z 级换档：Z0 `100 m` / Z1 `50` / Z2 `25` / Z3 `10` / Z4 `5`；线长恒 74 px |
| 指北针 | 右上 18 px | 针随方位（8 档 × 45°）；`N` 恒朝上，针指向世界北 |
| Z 徽章 | 左下 14 px | 三段：`Z2` / `俯仰 30°` / `侧视`（88° 时写"图纸（地图）"） |
| 地块悬停条 | 跟指针 | 显示 `生物群系 · 海拔 · 湿度 · 生物量`；**离开即消失**（Manor Lords 口径：点谁才出谁的信息） |

**逐屏两条细则**：

- **左栏悬停展开**：默认 56 px 只显示键帽，悬停展开到 104 px 并露出中文名（Blender / RimWorld 式）；展开是浮层，不推动世界区。
- **右栏时间线**：`刚刚发生` 用竖线 + 圆点（最多 5 条），不用表格行；生态条带趋势箭头（↑↓—）与状态词（贫瘠 / 起色 / 丰饶）。
- **水循环小图**：右栏生态段底部 `276 × 56`，五节点（海 → 蒸 → 云 → 雨 → 径）+ 虚线流动；**只做指向，不标数值**，数值在生态条里。
- **创世联动**：悬停特征卡 → 剖面里对应 `<g class="cvt" data-x="特征名">` 发光、其余压到 42 %，底部出一行"图上高亮：火山锥 + 间歇泉"；没有对应元素的特征（贫瘠土壤）不加高亮，避免骗人。
- **键位面板**：五个页签（全部 / 手牌 / 视角 / 时间 / 界面）+ 冲突检测（同键位重复 → 左红框 + 「冲突」签 + 顶部计数）。
- **世界事件卡**：全屏遮罩 `z = 80`，660 px 宽，3 px 暖色带，标题 26 / 正文 13，两个按钮「承受」「改写 · 80 余响」，`Esc` 或点遮罩关闭。**不许用 toast 代替**——"没有失败"必须有一个能被读到的瞬间。

### 6.1.1 手牌：没有栏（2026-10-20 定）

| 项 | 规则 |
|---|---|
| 栏 | **取消底部手牌栏**。十张卡直接浮在世界底边（`.cardslot{bottom:8px}`），世界区因此延伸到屏幕底（`#scene bottom:0`） |
| 居中 | 相对**世界区**居中（`#hand{left:56px;right:320px}`），不是相对整屏——否则会偏到右栏下面 |
| 卡面 | **只放三样**：元素纹样（占位图，之后换 manifest 插画）、卡名条、环级圆点（右上 10 个 3 px 点）。**不排效果文本、不排回收价、不排键位提示** |
| 悬浮 | 悬停才出详情卡（250 px）：卡名 / 元素·环级·类 / 效果一句话 / 回收价。指针抬起即消失 |
| 手势 | 向上拖 = 释放（不拖则随机落点）· 向下拖 = 回收 · 按住拖到地面 = 指定落点。**三种结果一套轴，不需要文字说明** |
| 动作条 | 右下四个圆钮（✦ 随机 / ↩ 回收 / ↻ 重抽 / 倍速），**只有图标 + tooltip**；说明统一放设置 → 键位 |
| 牌堆 | 左下两个小方块（池 / 手），只显示数字，不写"实体池""手牌" |

> **原则**：HUD 上不写"怎么操作"，只写"现在是什么"。操作说明只允许出现在**设置 → 键位**与**帮助**两处，别处重复即为噪音。

### 6.1.2 图案优先的四处落地

| 位置 | 规则 |
|---|---|
| 创世 · 掷骰带 | 15 条掷骰记录压成 **7 组**（形制 / 水 / 地貌 / 气候 / 生命 / 命名 / 校验），每组一行：`图标 + 2 字组名 + 值串`；完整键值只进 `title`。**不再逐条列"这一步掷了什么"** |
| 创世 · 顶视缩略 | 左列底部 `260 × 200` 俯视图：十种形制各一张几何形状（环礁是环、裂谷带裂缝、内陆湖国带湖），叠 26 px 网格 + 中心点 + 指北针。**形制从一句文字变成一张图** |
| 卡库 · 详情 | `1.26×` 大卡居中 + 三个图标读数（◔ 半径 / ◈ 价值 / ↩ 回收），编号与同元素卡收进「更多 ▾」 |
| 设置 · 示意图 | 每个分组顶部一张 `300 × 120` 示意图（画面=带移轴模糊带的小景 / 界面=三栏版式 / 世界=带轨道的岛 / 操作=鼠标上下拖 / 声音=波形 / 语言=A·文 / 辅助=眼睛）。`width:100%;max-width:480px;height:auto`，**不许拉伸变形** |
| 帮助 | 左：`620 × 370` 手势图（↑释放 / ↓回收 / ←拖到地面，含"地面"落点）；右：**4 张图标卡**（视角 / 界面 / 世界 / 事件），每张一行字；底部两个按钮「全部键位」「演练事件」。**不列 12 行快捷键** |
| 事件卡 | **9 条模板**：禁物 / 灾变 / 文明 各 3 条，每条带 `120 × 168` 缩略（锈核 / 钉 / 钟 / 火 / 陨石 / 冰 / 屋 / 墙 / 刻文）。`fireEvent(kind)` 可按类抽；`resolveEvent` 写年表时用事件类型 |
| 卡库 · 未获得格 | 灰稿 = **元素纹样 `30 × 30`、透明度 .17** + 虚线框，不再写"剪影"；整张矩阵因此读作"卡墙"而不是表格 |
| 摆件模式 | `#stage.ambient` 隐藏 `top / leftrail / rightpanel / actbar / deck / scalebar / compass / zchip`，手牌下移 `290 px`，底部一行「摆件模式 · Esc 退出」3.4 s 淡出。<br>**坑**：只写 `opacity:0` 不管用——`hudIn` 动画的 `fill-mode:both` 会把 opacity 顶回去，必须同时 `animation:none` + `visibility:hidden` |
| 创世 · 特征 | 两列小卡（`34 px` 高）：**图标 + 名**，描述与分类进 `title`；悬停仍触发剖面高亮。5 条特征从占满一列压到 3 行 |
| 右栏 · 生态 | 左 `84 × 84` **五段环**（林地 34 / 草原 26 / 水体 18 / 岩坡 14 / 裸地 8，中心显示植被 60 %），右侧五行图例；下方三个图标读数（✿ 多样性 / ◔ 肥力 / ◈ 生物量）。**不再用五根横条** |
| 年表图标 | 五类事件各一个字形 + 一个颜色：干涉 ✦ 金 / 气候 ☁ 青 / 生态 ✿ 绿 / 灾变 ⚠ 红 / 文明 ⌂ 黄；时间线圆点用 `--dot` 同步变色 |
| 创世 · 判据 | 7 行「PASS」压成**一条 7 点进度** + `7 / 7`，判据名进 `title` |
| 创世 · Z 三级 | 右栏下半原本空 480 px，补 **三帧缩略**（`120 × 44`）：Z0 全岛剪影 / Z2 树+房+灯 / Z3 人+鱼——同一块地的三种读法，顺手把 §1.2 的 Z 级讲清楚 |
| 入口图形 | 顶栏与左栏的 `C/B/L/G/O/H` 换成 16×16 字形（卷轴 / 牌堆 / 图鉴 / 骰子 / 齿轮 / 问号），**字母只出现在键位表**；tooltip 仍带键位 |
| 卡库 · 环级进度 | 矩阵上方 11 段条（0–9 环各 9 张、超环 3 张），每段宽度 = 该环获得比例，悬停显示 `n / 总` |
| 去重 | 删掉创世底部 6 张数字卡（与掷骰带重复：水覆盖 / 陆块 / 山系 / 物种池 / 聚落 / 奇观）与右栏「余响」（与顶栏 `◈` 重复）。**同一个数只许出现在一处** |

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
