# Solstice 项目长期记忆

> 只记「不看代码就会踩坑」的东西；规格一律以 `docs/` 为准（入口 `docs/INDEX.md`）。

## 硬性纪律
- **任何文档增删改（含版本号、状态列）必须同次更新 `docs/INDEX.md`**，否则视为缺陷。
- 提交前必跑两条自检（`tools/hooks/pre-commit` 与 CI 也会代跑）：
  - `python tools/checks/check-links.py` → 期望 `broken links : 0`
  - `python tools/checks/validate-contracts.py` → 期望 `errors : 0`
- `docs/archive/**` 无约束力；不要把已作废形态（动态壁纸 / 场景引擎 / 卡牌收集 / 自走棋 / 2.5D / Tauri / WebGL）带回方案或文档。

## 本机构建（三条必踩坑，详见 `docs/ops/ENV.md` §2.4.1）
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
- 分层 `Render → World → Sim`；新增能力落主模块子目录，**不新建模块**（`SolsticeSim` 模块已被实测否决）。
- 跨 DLL 公共类型必须标 `SOLSTICE_API`（漏标 = `LNK2019`，已踩过 `FPaperMaterials` / `FSimKernel` / `FDataLoader`）。
- `Build.cs` 依赖是**白名单**：只列真正被 include 的模块（`JsonUtilities` 曾空挂，2026-10-17 移除）。
- 数据全 JSON 驱动，改 `shared/schema/` 必须同步 `game/Config/Solstice/Data/` 与 `studio/presets/`。
- 卡 id `card.<元素>.<级>.<卡类>.<名>`；元素 `metal|wood|water|fire|earth|light|dark`；卡类 `matter|rite|law`。
- 本地化走 `Solstice::FLocalization` 单例 + `Loc()` / `Loc2(zh,en)`；`Localization/{zh-hans,en}.json` 两份 key 必须对齐。

## 工具配置位置（不在仓库里）
- CodeBuddy 全局设置 `~/.codebuddy/settings.json`（插件集 = `enabledPlugins`，当前 11 个）；
  `CODEBUDDY_API_KEY` 已改为**用户级环境变量**，不写在文件里；改完需**重启**才生效。
- 插件取舍理由记在 `docs/ops/AI-TOOLING.md` §3.1；VSCode 扩展真源是 `.vscode/extensions.json`。
- CodeBuddy 侧已配置 `unreal-mcp`（`http://127.0.0.1:8000/mcp`，需用户在编辑器启用插件后才通）。

## 协作
- 用户 ≈ 15%：安装、GUI 操作、审美裁决、立项与数值拍板。系统/环境类改动要求「先扫现状 → 分类方案 → 拍板 → 执行」且可回滚。
