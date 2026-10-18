# tools/ — 开发环境脚本

Solstice 的环境检测、安装、工程生成与构建脚本；PowerShell 部分兼容 Windows PowerShell 5.1 与 PowerShell 7+。

目录按职责分组：

| 子目录 | 装什么 |
|---|---|
| `lib/` | **共享库**（`Solstice.Common.ps1`）——引擎 / 工程 / 可执行文件定位与临时日志路径的**唯一实现**，其余脚本一律 dot-source 它，不再各写一份 |
| `env/` | 环境体检、安装、IDE（VS Code）与扩展配置 |
| `build/` | 工程文件生成、编译、内容生成 |
| `run/` | 启动编辑器、无头确定性回归 |
| `checks/` | 提交前自检（Python，标准库，跨机一致），由 `hooks/` 与 GitHub Actions 自动执行 |
| `hooks/` | pre-commit 钩子及其安装脚本 |
| `ai-config/` | CodeBuddy 用户级配置的**仓库模板**与跨主机同步脚本 |
| `docs/` | **环境手册**（`ENVIRONMENT.md`：装机 / 构建 / DDC / MCP / 排错）——**不属于游戏设计文档**，2026-10-17 从 `docs/ops/` 迁出 |

> **保存编码**：脚本含中文注释/输出，PS 5.1 下必须以 **UTF-8 with BOM** 保存。
> 新建 `.ps1` 最容易漏 BOM —— 漏了会被 PS 5.1 按 ANSI 解析，中文注释变乱码并**引发语法错误**
> （2026-10-17 实际踩过：`lib/Solstice.Common.ps1` 缺 BOM，导致 `env/configure-vscode.ps1` 报「意外的标记 }」）。

## 脚本清单

| 脚本 | 用途 | 前置 | 示例 |
|---|---|---|---|
| `lib/Solstice.Common.ps1` | 共享库：`Get-SolsticeRepoRoot` / `Find-UnrealEngine` / `Resolve-SolsticeEngineRoot` / `Resolve-SolsticeProject` / `Resolve-SolsticeExe` / `New-SolsticeLogPath`。**只定义函数、不改文件**，被 dot-source 时不执行 main | 无 | `. (Join-Path $PSScriptRoot '..\lib\Solstice.Common.ps1')` |
| `env/check-env.ps1` | 只读体检：winget / Git / Python / GPU / 磁盘 / ComfyUI / Epic Launcher / UE / VS2022 | 无 | `powershell -ExecutionPolicy Bypass -File tools\env\check-env.ps1` |
| `env/find-ue.ps1` | 命令行探测引擎路径与版本（薄封装，实现见 `lib/`） | 无 | `... -File tools\env\find-ue.ps1 -All` |
| `env/install-epic-launcher.ps1` | winget 安装 Epic Games Launcher | winget；**用户主动执行** | `... -File tools\env\install-epic-launcher.ps1 -DryRun` |
| `env/install-vs-buildtools.ps1` | winget 安装 VS2022 生成工具 + C++ 负载（约 15–20GB） | winget；**用户主动执行** | `... -File tools\env\install-vs-buildtools.ps1 -DryRun` |
| `env/configure-vscode.ps1` | 探测引擎并写入**用户级环境变量** `SOLSTICE_UE_ENGINE_ROOT`；`.vscode` 三个配置通过 `${env:...}` 引用它。**不再改写 `.vscode` 里的任何文件**（`-DryRun` 只体检） | 已装引擎（否则降级） | `... -File tools\env\configure-vscode.ps1` |
| `env/vscode-extensions.ps1` | VS Code 扩展体检 / 安装推荐 / 卸载无关（默认只读） | VS Code CLI | `... -File tools\env\vscode-extensions.ps1 -InstallRecommended` |
| `ai-config/sync-ai-config.ps1` | CodeBuddy 用户级配置（插件集 / 权限模式 / 记忆开关 / MCP）与本仓库模板同步。默认只读对比；`-Apply` 落地到本机、`-Export` 反向导出（密钥自动剥离） | 无 | `... -File tools\ai-config\sync-ai-config.ps1 -Apply` |
| `build/gen-projectfiles.ps1` | 生成 `Solstice.sln`（cpptools）或 `compile_commands.json`（clangd）；**自动改用引擎自带 dotnet 运行 `UnrealBuildTool.dll`**（直接跑 `UnrealBuildTool.exe` 会因本机只装 .NET 8/9 而失败，见 [`docs/ENVIRONMENT.md`](docs/ENVIRONMENT.md) §2.4.1） | 已装引擎 | `... -File tools\build\gen-projectfiles.ps1 -Mode Solution` |
| `build/build.ps1` | 调用 `Build.bat` 编译目标，透传退出码。**默认追加 `-NoUBA`**（本机 UBA 的 FS detour 失效、PCH 会 `C1083`，见 [`docs/ENVIRONMENT.md`](docs/ENVIRONMENT.md) §2.4.1）；`-UseUBA` 换回加速 | 引擎 + VS2022 生成工具 | `... -File tools\build\build.ps1 -Target SolsticeEditor` |
| `build/gen-assets.ps1` | **生成内容**：跑 `-run=SolsticeGenAssets` 写出 `/Game/Core/M_Paper` 与 `M_Ink`。幂等，已存在则跳过 | 已编译 `SolsticeEditor` | `... -File tools\build\gen-assets.ps1` |
| `run/bench.ps1` | **无头确定性回归**：跑 `-run=SolsticeBench`，同种子跑两遍比对世界摘要；不一致即退出码 1 | 已编译 `SolsticeEditor` | `... -File tools\run\bench.ps1 -Days 2` |
| `run/shot.ps1` | **无人值守抓帧 + 量化**：`-game -RenderOffscreen` 起一帧，等世界稳定、相机停到指定格点，写 PNG 后自行退出；从日志读引擎投影出的世界屏幕盒，自动调 `checks/measure-frame.py` 判 PASS/FAIL。`-Minute` 钉时钟、**`-Seed` 钉世界种子**（`world.json` 的 `seed:0` = 每次新世界，不钉种子两帧不可比）。无人值守会话的"眼睛" | 已编译 `SolsticeEditor` + 装了 Pillow 的 python（`SOLSTICE_PYTHON` 可指定） | `... -File tools\run\shot.ps1 -Preset 0 -Delay 5 -Minute 720` |
| `run/launch-editor.ps1` | 启动虚幻编辑器打开工程；`-Mcp` 额外开启引擎内置 MCP 服务器（`-McpPort` 改端口，默认 `8000`），`-Headless` 走无窗口（`-NullRHI -unattended -nopause`） | 已装引擎 | `... -File tools\run\launch-editor.ps1 -Mcp -Headless` |
| `hooks/install-hooks.ps1` | 安装 pre-commit 钩子：设 `core.hooksPath=tools/hooks`，提交时自动跑 `checks/` 两条自检；幂等；卸载 `git config --unset core.hooksPath` | git + Python | `... -File tools\hooks\install-hooks.ps1` |

## checks/ — 提交前自检（Python，标准库）

提交文档 / 数据前必须两条都跑通（约定见 [`docs/INDEX.md`](../docs/INDEX.md) §5）：

| 脚本 | 用途 | 示例 |
|---|---|---|
| `checks/check-links.py` | 扫描 `docs/` 与各 README 的相对链接，0 断链才提交；有断链退出码 1 | `python tools/checks/check-links.py` |
| `checks/check-materials.py` | **材质防漂移**：每份 master material 必须带 `RecipeVersion` 戳、且都被 `BuildAll` 调用。防的是「改了材质图但资产没重建」——这个坑已经踩过一次（M_Sky 重写 200 行，资产体积一位没变，画面直接纯白） | `python tools/checks/check-materials.py` |
| `checks/check-layers.py` | **层次防线**：①`Sim/` 只许 include 纯数学/容器头（禁 UObject/Tick/渲染/输入），否则内核就跑不了无头回归；②依赖方向 `Sim→Gen→Render→World→God/UI`，不许反向。已知例外写在脚本 `ALLOWED` 里，每条都注明谁来消除 | `python tools/checks/check-layers.py` |
| `checks/grade-preview.py` | **色板验证**：拿当前帧按 `shared/palettes/*.json` 的精确 HEX 重绘，输出到 `tmp/review/palette/`。证明的是颜色结构（三档海 / 三段天 / 三层地），不是画质 | `python tools/checks/grade-preview.py` |
| `checks/validate-contracts.py` | `shared/schema/` ↔ `game/Config/Solstice/Data/` 逐对校验 + 卡 / 物种 id 查重；并校验 `Localization/` 双语言 key 对齐（缺失 / 多余 key、占位符不一致、空值）；内置迷你 JSON Schema 子集，无需安装依赖 | `python tools/checks/validate-contracts.py` |

**验收用的量具（不在上面两条链路里，需要 Pillow）**：

| 脚本 | 用途 | 示例 |
|---|---|---|
| `checks/analyze-frame.py` | **画面诊断**（不是判据）：分区给亮度/饱和/拉普拉斯细节/色相占比/明度直方图，回答「gate 过了但画面还是平，缺的是哪一项」。判据是 `measure-frame.py`，这个是**读数**——明度三段（暗/中/高）、暖绿冷占比、`island/sea` 细节比都在这里，阈值写在文件头的 docstring | `python tools/checks/analyze-frame.py tmp/review/a.png [b.png]` |
| `checks/measure-frame.py` | 把截图按 `art/art.md` §2.1 的 Cel 判据量化：平均饱和度、亮度中位数、`≥0.90` 高光占比，三项各给 PASS / FAIL，退出码 1 = 不达标。**取代"看图数明度带"这类主观验收**；世界区域的 `--box` 别自己估，用 `run/shot.ps1` 日志里的 `[shot] box=` | `python tools/checks/measure-frame.py tmp/review/r2_01.png --box 330,150,760,430` |

## hooks/ 与 CI — 自检自动执行

两条自检无需手动跑（手动跑仍可用于排障）：

- **本地**：`tools/hooks/pre-commit` + `tools/hooks/install-hooks.ps1` —— 每个克隆执行一次安装（设 `git config core.hooksPath tools/hooks`），之后每次 `git commit` 自动跑；卸载：`git config --unset core.hooksPath`。
- **远端**：`.github/workflows/checks.yml` —— push / PR 在 GitHub Actions 跑同两条脚本（标准库、无需引擎；引擎构建仍在开发机，见 [`docs/ENVIRONMENT.md`](docs/ENVIRONMENT.md)）。
- `tools/hooks/pre-commit` 必须保持 **LF** 行尾（sh 解释 shebang；`.gitattributes` 已锁定）。

## 推荐顺序

**首次搭环境**（本机已完成，换机才需要重跑）：

```
env/check-env.ps1
  → env/install-epic-launcher.ps1（用户执行）
  → 【人工登录 Epic】→ Launcher 内安装 UE
  → env/install-vs-buildtools.ps1（用户执行）
  → 补 .NET Framework 4.6.2 SDK（见 docs/ops/ENV.md §1）
  → env/check-env.ps1（复检全绿）
  → env/configure-vscode.ps1        # 写 SOLSTICE_UE_ENGINE_ROOT，之后重启 VS Code
  → build/gen-projectfiles.ps1 -Mode Solution
  → build/build.ps1
  → run/launch-editor.ps1
```

**日常 / 新克隆的最小闭环**（前三步缺一不可）：

```
env/find-ue.ps1            # 确认引擎在（找不到就什么都做不了）
  → build/gen-projectfiles.ps1 -Mode Solution
  → build/build.ps1
  → build/gen-assets.ps1   # 生成两个纸材质主材质，否则界面退化到引擎调试材质
  → run/bench.ps1 -Days 2   # 确认模拟仍然确定
```

> 内容生成与基准都走 `UnrealEditor-Cmd.exe`（无窗口）。它们依赖 `SolsticeEditor` 模块被加载，
> 因此该模块在 `.uproject` 里的 `LoadingPhase` 必须是 `Default` —— 原因见 `docs/ops/ENV.md` §2 第 9 条。

## 换机 / 跨主机同步

换一台主机后，让「开发环境 + 编辑器配置 + AI 助手配置」全部到位，三步走：

1. `git clone` 本仓库 → 跑上面的**首次搭环境**序列（引擎、VS2022、编译链）。
2. **VS Code**：登录并开启内置设置同步（左下角账户图标 → Turn on Settings Sync…）→ 用户设置 / 快捷键 / 片段 / 扩展清单自动到位；打开本仓库时 `.vscode/extensions.json` 还会再提示安装推荐扩展。核对与增删：`tools\env\vscode-extensions.ps1`。
3. **CodeBuddy**（IDE 内 AI 助手）：`tools\ai-config\sync-ai-config.ps1 -Apply` → 插件集 / 权限模式 / 记忆开关 / MCP 到位。密钥类（`gateway.password`、`CODEBUDDY_API_KEY`）刻意不入库，脚本会把它们列出来让你手工补。详见 [`docs/ENVIRONMENT.md`](docs/ENVIRONMENT.md) §4。

## 约定

- 全部脚本**幂等**，可反复运行。
- **不强制要求管理员**；仅在必要时提示。
- **安装类脚本不会被其它脚本自动调用**，只能由用户主动发起。
- **盘符 / 机器相关路径一律不写死**，优先级：显式参数 > 环境变量 > 自动探测 > 内置默认。单一真源如下：

| 要定位的东西 | 单一真源 | 环境变量覆盖 | 说明 |
|---|---|---|---|
| 虚幻引擎安装根 | `lib/Solstice.Common.ps1` 的 `Find-UnrealEngine` / `Get-DefaultUeSearchRoots` | 钉死 `SOLSTICE_UE_ENGINE_ROOT`；搜根 `SOLSTICE_UE_SEARCH_ROOTS`（分号分隔） | 源是 Epic 默认位置 + **本机现存盘符** × `Epic Games` / `UE` / `UnrealEngine`。`SOLSTICE_UE_ENGINE_ROOT` 有效时**直接返回**，不再扫描 |
| 引擎路径给 VS Code 用 | `env/configure-vscode.ps1` 写入用户级 `SOLSTICE_UE_ENGINE_ROOT` | — | `.vscode/{c_cpp_properties,launch,settings}.json` 引用 `${env:SOLSTICE_UE_ENGINE_ROOT}`，**这三个文件因此与主机无关、可安全入库**；变量改了要重启 VS Code |
| ComfyUI 根目录 | `env/check-env.ps1` 的 `-ComfyUIRoot` | `SOLSTICE_COMFYUI_ROOT` | 模型不写死文件名：扫到任一 `models/**/diffusion_models/*.safetensors` 即算就绪 |
| VS Code CLI | `env/vscode-extensions.ps1` 的 `Resolve-CodeCli` | `SOLSTICE_VSCODE_CLI` | 其次查 PATH 与标准安装位置 |
| `.codebuddy` 目录 | `ai-config/sync-ai-config.ps1` 的 `-CodeBuddyHome` | — | 默认 `%USERPROFILE%\.codebuddy` |
