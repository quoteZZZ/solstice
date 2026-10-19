# Solstice 项目长期记忆

> 只记「不看代码就会踩坑」的东西；规格一律以 `docs/` 为准（入口 `docs/README.md`）。

## 硬性纪律
- **单工作树**：`D:/GitCode/solstice` 是唯一检出，停在 **main**。原先 WorkBuddy 在
  `C:/Users/.../Worktrees/solstice/main-2296c354` 的第二工作树（分支 `workbuddy/main-2296c354`，领先 14 个提交）
  已于 2026-10-19 快进合并进 `main` 并删除，分支名仅留作标记。不要再按「提交了 ≠ main 上有」的前提做事；
  推送用 `git push origin HEAD:main`。
- **文档只有 9 份**（`docs/README` + CHARTER/WORLD/CARDS/ART/TECH/PLAN/OPS/LATER）。改内容只改正文 +
  `docs/CHANGELOG.md` 一行；**新增/作废文档**才改 `docs/README.md` §2 的表。**不建索引页、不抄规格副本**。
- **改画面必须量，不许看图**：`& tools/run/shot.ps1 -Preset 0 -Delay 5` 无人值守抓一帧（写出
  `tmp/review/shot.png`），日志里的 `[shot] box=` 是世界圆盘的屏幕包围盒，脚本自动喂给
  `tools/checks/measure-frame.py` 判 PASS/FAIL。基线：修曝光前 0.095/0.910/0.597 三项全 FAIL，
  修后 0.497/0.563/0.010 三项 PASS。**别自己 crop 全帧**（深蓝背景会把中位拉到 0.44 假通过）。
- 提交前必跑两条自检（`tools/hooks/pre-commit` 与 CI 也会代跑）：
  - `python tools/checks/check-links.py` → 期望 `broken links : 0`
  - `python tools/checks/validate-contracts.py` → 期望 `errors : 0`
- 改表现层后必跑 `& tools/run/bench.ps1 -Days 2` → 期望 `digest A == digest B`。
- `docs/archive/**` 无约束力；不要把已作废形态（动态壁纸 / 场景引擎 / 卡牌收集 / 自走棋 / 2.5D / Tauri / WebGL）带回方案或文档。

## 本机构建（三条必踩坑，详见 `tools/docs/ENVIRONMENT.md` §2.4.1）
1. **必须关 UBA** → `tools/build/build.ps1` 已默认追加 `-NoUBA`（`-UseUBA` 可换回）。
2. **不要直接跑 `UnrealBuildTool.exe`**（本机只有 .NET 8/9 → `EXIT=150`）；用引擎自带
   `Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe` 挂 `UnrealBuildTool.dll`（`tools/build/gen-projectfiles.ps1` 已内置）。
3. 承载 Commandlet 的模块 `LoadingPhase` 必须是 **`Default`**，否则 `-run=` 解析时模块尚未加载。
4. 新建 `.ps1` **必须带 UTF-8 BOM**（PS 5.1 无 BOM 按 ANSI 解析 → 中文乱码 / 语法错）。

## 环境坐标
- 引擎 `D:\Epic Games\UE_5.8`（5.8.3，含源码）｜VS2022 生成工具 `D:\VS2022`
- 仓库 `D:\GitCode\solstice`｜DDC 外置 `UE-LocalDataCachePath=D:\UE-DDC`（别落到 C 盘）
- **`tools/` 已按用途分 7 层**：`lib`（共享库）/ `env` / `build` / `run` / `checks` / `hooks` / `ai-config`；清单一览 `tools/README.md`。
- 引擎路径单一真源 = `tools/lib/Solstice.Common.ps1` 的 `Find-UnrealEngine`（`SOLSTICE_UE_ENGINE_ROOT` 钉死 > `SOLSTICE_UE_SEARCH_ROOTS` 候选根 > 自动探测）。
- `.vscode` 里**不含**引擎绝对路径，只写 `${env:SOLSTICE_UE_ENGINE_ROOT}`；该用户级环境变量由 `tools/env/configure-vscode.ps1` 写入（改引擎后重跑 + **整进程重启 VS Code**）。

## 代码不变量
- **`Sim/` 零渲染依赖**：不得 include UObject / Tick / 渲染 / 输入头——这是 `SolsticeBench` 能无头跑的前提。
- **`Sim/` 里不许读真实时钟**：`FSimKernel::Initialize` 见 `Config.Seed == 0` 会摇新种子、开局
  `WorldMinute` 取 `FDateTime::Now()`。这两处曾让 `bench` "永远 NON-DETERMINISTIC"（它在比两个随机世界）。
  bench 侧必须钉 `Seed / StartingDayOfYear / StartingMinuteOfDay`（已钉：20261016 / 80 / 06:30；digest 以 bench 实测为准，2026-10-19 为 `ECCC4EAC4EFDBE58`）。新增任何 `FDateTime::Now()` 到 Sim 路径前，先想 bench 怎么钉住它。
- **目视取色的常量一律走 `CelSrgb()`**（`Gen/SolsticeGenerators.h`）。把 sRGB 数值直接塞进
  `FLinearColor` 等于 gamma 两次，草地 0.40 出屏 0.66——整界"发灰"的元凶就是它。
- **`tools/art/textures/*.png` 必须写 sRGB 编码后的字节**（`gen-textures.py::save()` 已处理）。
  运行时 `LoadPng` 一律 `Texture->SRGB = true`，写线性值等于多解一次 gamma：草地 0.58 → 0.30。
  法线图同理（它是数据，但走同一条 sRGB 取样路径）。
- **HUD 的度量只有一个源**：`SolsticeHUDView.cpp` 全按 1080p 书写，`GUiScale = clamp(视口高/1080, 0.85, 2.0)`
  统一缩放（字号 `Font()`、间距 `Px()`）。Slate 的 DPI 曲线对直接挂到 viewport 的 widget **无效**，
  缺这层就会"窗口越大字越小"。新增控件必须用 `Px()` 包住所有 padding / 宽高。
- **画面里必须有地平线**：垂直 FOV 29.4°，俯仰 >14.7° 时整帧都是海（顶边已在水平线下）。俯仰档是 **8/24/48/88**。
- **`shot.ps1` 必须带 `-ForceRes`**：不带则引擎用存档窗口尺寸，此前所有"4K"实际 888×500。
- **M_Cel 的可见亮度 = Emissive**（`albedo × 分带 × CelFill`），阴影贴图够不到它；要做暗部只能改**顶点色 RGB**
  （`SolsticeMeshBuilder::Add` 已把 Occlusion 乘进 RGB；顶点色 A 通道没人读，接 ComponentMask 进 Custom 会编译失败）。
- **海面反射不再是常数色**：`M_Sea` 的 `SeaSky` 在像素里重算 `M_Sky` 同段梯度 + 太阳镜面；改天空色板必须同步改 `SeaHazeColour`/雾色，否则天际线出硬线。
- **相机只有 32 格点**（8 方位 × 4 俯仰 14/35/62/88），`Orbit()` 已删，中键只推拉；`shot.ps1 -Preset` 指俯仰档（默认 1）、`-Azimuth 0–7`。
- **基线（2026-10-19 夜）**：128² 逻辑 ~9200-9600 世界分钟/秒（0.34-0.36 s / 2 日）；128²@720p 16.1 ms / 62 fps；
  digest `2C1A3AC0CDE6C6B5`（生态修复后）；开局 306 只 → 2 世界日 1046 只。**生态步长必须是 0.0667 小时，不许再乘 4**。
- **M_Cel 的三档着色切在 `Emissive` 里，不在 `BaseColor` 里**。这个世界只有一盏 100 单位方向光
  和天光捕获，在 `-1.3` 档手动曝光下它们对地面的贡献≈0（实测太阳 ×20、天光 ×2.6、关阴影，
  岛的中位线性亮度恒为 0.045，只有 `-SolsticeCel=` 的 albedo 增益能动它）。所以
  `BaseColor = albedo × 三档` 被乘零，三档必须在 `ShadowFill` 自发光里切。改 `BaseColor` 无效。
- 分层 `Render → World → Sim`；新增能力落主模块子目录，**不新建模块**（`SolsticeSim` 模块已被实测否决）。
- 跨 DLL 公共类型必须标 `SOLSTICE_API`（漏标 = `LNK2019`，已踩过 `FPaperMaterials` / `FSimKernel` / `FDataLoader`）。
- `Build.cs` 依赖是**白名单**：只列真正被 include 的模块（`JsonUtilities` 曾空挂，2026-10-17 移除）。
- 数据全 JSON 驱动，改 `shared/schema/` 必须同步 `game/Config/Solstice/Data/` 与 `studio/presets/`。
- 卡 id `card.<元素>.<级>.<卡类>.<名>`；元素 `metal|wood|water|fire|earth|light|dark`；卡类 `matter|rite|law`。
- 本地化走 `Solstice::FLocalization` 单例 + `Loc()` / `Loc2(zh,en)`；`Localization/{zh-hans,en}.json` 两份 key 必须对齐。

## 工具配置位置（不在仓库里）
- CodeBuddy 全局设置 `~/.codebuddy/settings.json`（插件集 = `enabledPlugins`，当前 11 个）；
  `CODEBUDDY_API_KEY` 已改为**用户级环境变量**，不写在文件里；改完需**重启**才生效。
- 插件取舍理由记在 `tools/docs/ENVIRONMENT.md` §3.1；VSCode 扩展真源是 `.vscode/extensions.json`。
- CodeBuddy 侧已配置 `unreal-mcp`（`http://127.0.0.1:8000/mcp`，需用户在编辑器启用插件后才通）。

## 协作
- 用户 ≈ 15%：安装、GUI 操作、审美裁决、立项与数值拍板。系统/环境类改动要求「先扫现状 → 分类方案 → 拍板 → 执行」且可回滚。
