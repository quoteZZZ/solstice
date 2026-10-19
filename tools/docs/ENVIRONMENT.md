---
id: tools/ENVIRONMENT
title: 环境与工具（安装 / VSCode / MCP / 排错）
status: active
truth: ops
owner: dev
---

# 环境与工具（ENVIRONMENT）

## 0 本文件管什么 / 不管什么

- **管**：从一台空机器跑到能进 PIE 的**全部环境事实**——安装顺序与体积、工具链路径、构建与 DDC、VSCode 扩展与设置、IntelliSense、生图环境、MCP 与插件、排错。
- **不管**：代码结构（→ [`../tech/tech.md`](../../docs/TECH.md)）；协作与文档纪律（→ [`../START.md`](../../docs/README.md)）；玩法与美术（→ L1 各规格）。
- **读法**：装机从 §1 顺着读；**日常开发只看 §4 与 §6**。

## 1 安装顺序与体积预期

| 步骤 | 操作 | 体积 | 耗时 | 谁来做 | 状态 |
|---|---|---|---|---|---|
| 1 | `tools\env\install-epic-launcher.ps1` | ~1GB | 几分钟 | 用户执行脚本 | ✅ **已完成，无需再跑** |
| 2 | 打开 Launcher **登录 Epic 账号** | — | 几分钟 | **必须人工**（不可脚本化） | ✅ 已登录 |
| 3 | Launcher 内安装**虚幻引擎**（实际落在 `D:\Epic Games\UE_5.8`） | 30–60GB | 30–90 分钟 | **必须 GUI** | ✅ **5.8.3 已装** |
| 4 | `tools\env\install-vs-buildtools.ps1`（VS2022 生成工具 + C++ 负载，实际在 `D:\VS2022`） | 15–20GB | 20–60 分钟 | **用户执行脚本** | ✅ **已装**（MSVC 14.44.35207 + SDK 10.0.22621.0） |
| 4b | **.NET Framework 4.6.2 SDK**（`SwarmInterface.Build.cs` 硬要求） | ~5MB | 秒级 | AI（见 §1.3） | ✅ **已补齐** |
| 5 | `tools\env\check-env.ps1` 复检 | — | 秒级 | AI / 用户 | ✅ |
| 6 | `tools\env\configure-vscode.ps1` 写入引擎环境变量（`SOLSTICE_UE_ENGINE_ROOT`） | — | 秒级 | AI / 用户 | ✅ |
| 7 | `tools\build\gen-projectfiles.ps1` 生成工程文件 | — | 1–3 分钟 | AI / 用户 | ✅ |
| 8 | `tools\build\build.ps1` 编译 | — | 1–10 分钟 | AI / 用户 | ✅ 两个目标均通过 |
| 9 | `tools\build\gen-assets.ps1` 生成内容（新克隆必跑） | — | 约 10 秒 | AI / 用户 | ✅ |
| 10 | `tools\run\bench.ps1` 无头确定性回归 | — | 约 10 秒 | AI / 用户 | ✅ |
| 11 | VSCode 打开 `.vscode\solstice.code-workspace` | — | — | 用户 | — |

### 本机实测（`tools\env\check-env.ps1`，2026-10-13 首测 / 2026-10-14 复核 / 2026-10-16 工具链复核）

| 项 | 状态 | 实测值 |
|---|---|---|
| winget | ✅ | v1.29.380 |
| Git | ✅ | 2.53.0.windows.1 |
| Python | ✅ | 3.12.10 |
| CPU / 内存 | ✅ | i5-12600KF（10 物理核 / 16 逻辑核）/ 31.84 GB |
| GPU（NVIDIA） | ✅ | RTX 5060，8151 MiB，驱动 610.74 |
| 磁盘 D: | ✅ | 剩余充足 —— **不需要清理 C 盘** |
| 磁盘 C: | ⚠ | 剩余 77.4 GB；实际需求约 10–15 GB（`check-env.ps1` 的 120 GB 是保守阈值） |
| ComfyUI | ✅ | `D:\ComfyUI` 就绪，FLUX.2 Klein 4B FP8 已装 |
| Epic Games Launcher | ✅ | 已安装 |
| 虚幻引擎 UE5 | ✅ | **5.8.3**（CL 58210709），`D:\Epic Games\UE_5.8`，`UnrealEditor.exe` 就位 |
| VS2022 生成工具（C++） | ✅ | `D:\VS2022`，MSVC 14.44.35207 + Windows SDK 10.0.22621.0 |
| .NET Framework 4.6.2 SDK | ✅ | 见 §1.3（原本缺失，是 UBT 的唯一硬阻塞） |
| 平台 SDK（引擎侧） | ⚠ | 仅 Win64 有效；`-Mode=ValidatePlatforms` 报 Mac / IOS / Android / Linux / TVOS / VisionOS 缺失——**M0 只发行 Windows，可忽略** |

**磁盘预算**：UE(60GB) + VS(20GB) + 工程 `Intermediate`/`DerivedDataCache`(20–40GB) ≈ **100–120GB**；D 盘充足。

### `.NET Framework 4.6.2 SDK`（**必须补，否则编不过**）

`SwarmInterface.Build.cs` 无条件要求 `NetFxSdkDir`，它会去找 `Include/um/metahost.h` 与 `Lib/um/x64/mscoree.lib`。
本机原本**三处都没有**（`Program Files (x86)`、`Program Files`、`D:\VS2022`），注册表 `HKLM\SOFTWARE\Microsoft\Microsoft SDKs\NETFXSDK` 也不存在 —— 于是 UBT 直接抛
`Unable to instantiate module 'SwarmInterface': Could not find NetFxSDK install dir`。**这和 C++ 工作负载装没装无关。**

补法（无需管理员、无需重装 VS）：

1. 从**本机已有的 VS 渠道清单**取下载地址：`C:\ProgramData\Microsoft\VisualStudio\Packages\_Instances\<id>\catalog.json`（一万九千余条，含每个包的 URL 与 SHA256）。
2. 取 `Microsoft.Net.Component.4.6.2.SDK` → 其 chained 包 `Microsoft.Net.4.6.2.SDK`（一个 `.msi` + 一个 `.cab`），**校验大小与 SHA256 与清单逐字节一致**。
3. `msiexec /a` 解包到 `D:\_toolchain\netfx\extract`，把 `NETFXSDK/4.6.2` 部署到 `D:\_toolchain\AutoSDK\HostWin64\Win64\Windows Kits\NETFXSDK\4.6.2`。
4. 注册 `KitsInstallationFolder` 指向它 —— 写 `HKLM:\SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\NETFXSDK\4.6.2`（另加 `HKCU` 一份更保险）。
   `MicrosoftPlatformSDK.TryReadDirRegistryKey`（`MicrosoftPlatformSDK.cs:477`）**只读字符串、不校验目录是否存在**，所以只要能读出路径即可。

> 也可让 `MicrosoftPlatformSDK` 走 `UE_SDKS_ROOT` 分支（`AutoSDK\Host<Platform>\Win64\Windows Kits\NETFXSDK\<ver>`）——两种方式本机都验证过，AutoSDK 那份即第 3 步落地的目录。
>
> `reg.exe` 若被安全策略拦截，用 PowerShell 的 `New-ItemProperty`（注册表）替代。

### AI 能做 / 用户必须做

- **AI 能做**：全部 C++ 与配置、所有脚本、文档、生图线、跑体检/工程生成/编译。
- **用户必须做**：① 批准并运行安装脚本；② **手动登录 Epic 账号**；③ 在 Launcher GUI 里选 UE 版本/路径/组件；④ 在 UE 编辑器里连蓝图（薄表现层）。职责边界见 [`START.md`](../../docs/README.md) §1。

---

## 2 把工程跑起来（构建 / 调试 / DDC）

### 为什么可行

UE 内置 **`VSCodeSourceCodeAccess`** 插件。启用后，在编辑器里双击 C++ 文件会直接用 VSCode 打开。
配置：**Editor Preferences → Source Code → Source Code Editor = Visual Studio Code**（下拉里没有就先在 Plugins 里启用 `VSCodeSourceCodeAccess`）。

### 调试（`cppvsdbg`）

`cppvsdbg` 是 VS 调试引擎，**要求 VS2022 生成工具已安装**。两种配置（见 `.vscode/launch.json`）：
- **attach**：先手动启动 `UnrealEditor.exe`，再 Attach。日常开发首选（编辑器常驻）。
- **launch**：由 VSCode 直接拉起 `UnrealEditor.exe <uproject> -game -log`。

### 构建：Live Coding vs 命令行全量重编

| 场景 | 方式 |
|---|---|
| 只改 `.cpp` 内部实现 | **Live Coding**（编辑器内 `Ctrl+Alt+F11`）热补丁 |
| 改头文件接口 / `UCLASS`·`USTRUCT`·`UENUM` 反射宏 / 增删类或成员 | **必须关编辑器**，走 `tools\build\build.ps1` 全量重编 |
| 新增模块 / 改 `.Build.cs` / 改 Target | 关编辑器 → 重跑 `gen-projectfiles.ps1` → `build.ps1` |
| 改 `.uproject` 的模块 `LoadingPhase` | 重跑 `build.ps1`（重建 target receipt），否则运行时仍按旧清单加载 |

#### UBA 已全局关闭（否则本机编不过）

**本机 UBA（Unreal Build Accelerator，UE5.6+ 默认开启）会失败**，症状是每个临时文件改名/删除都报：

```text
SetFileInformationByHandle (FileDispositionInfo) failed on <pid> ... (Access is denied.)
fatal error C1083: 无法打开预编译头文件 "...h.pch": 拒绝访问
```

**已在沙箱内外各复现一次**，说明不是 agent 策略；Windows Defender 实时防护开着，无第三方杀软，目录 ACL 也是 FullControl —— 就是 UBA 的文件系统 detour 在本机环境下的问题。

**2026-10-17 修正了对开关语义的理解**。UE 5.8 的 `bAllowUBAExecutor=false` **并不关闭 UBA 执行器** —— `ExecutorFactory.cs:85-93` 的闸门只是让配置回落到 `GetUBAExecutor()` 并把 `Config.bAllowDetour` 置 `false`。换句话说关掉的是 **UBA 的文件系统 detour**，而那恰好就是本机 access-denied / `C1083` 的根源。（本机日志里仍会出现 `Using Unreal Build Accelerator local executor` 与 `UbaSessionServer - Listening on 0.0.0.0:1345`，那是服务起监听，**与执行器是否可用无关**，别据此判断配置没生效——判断依据看编译动作前缀有没有 `[NoUba]`。）

**现在的做法：写用户级全局配置，而不是每次拼命令行参数。**

文件：`%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml`（本仓库**外**，故不入 git）

```xml
<?xml version="1.0" encoding="utf-8" ?>
<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
	<BuildConfiguration>
		<bAllowUBAExecutor>false</bAllowUBAExecutor>
	</BuildConfiguration>
</Configuration>
```

**段名不能省。** `<Configuration>` 只接受固定清单的直接子元素（`BuildConfiguration` / `UEBuildConfiguration` / `WindowsPlatform` / `TargetRules` / `UnrealBuildAccelerator` / `XGE` / …），段名 = 该属性所属的类名。把 `bAllowUBAExecutor` 直接挂在 `<Configuration>` 下，UBT 会报
`The element 'Configuration' ... has invalid child element 'bAllowUBAExecutor'` 并 `Result: Failed (OtherCompilationError)`、`EXIT=6`。

**作用域**：这份按 **UserProfile** 生效，**所有** UBT 调用都会读它 —— `Build.bat`、Live Coding、编辑器内构建、`gen-projectfiles.ps1`、手动直调，一视同仁。于是：

- `tools\build\build.ps1` 里那个 `-NoUBA`（默认追加，`-UseUBA` 可关）**现在成了冗余**，但**保留**：它是参数化的，换到没有这份用户级配置的主机上仍然需要；
- **Live Coding 从此也绕开了 UBA 的 detour**，不必再为了热补丁额外设环境变量。

代价是 UBA 的本地加速在本机拿不到（原本也没享受到，因为 detour 一开就失败）。**回滚**：删掉那份 `BuildConfiguration.xml`，或把值改回 `true`。

需要手动直调时用下面的方式（`-NoUBA` 现在可省，写上也无害）：

```bash
"<引擎>\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe" \
  "<引擎>\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" \
  SolsticeEditor Win64 Development -Project="<仓库>\game\Solstice.uproject" -NoUBA -WaitMutex
```

**注意不要直接跑 `UnrealBuildTool.exe`**：本机只装了 .NET 8/9，而 UBT 要 .NET 10，直接执行会以 `EXIT=150` 退出。用引擎自带的那份 `dotnet.exe`（上例）才正常。

> `tools\build\gen-projectfiles.ps1` 已内置该逻辑：自动探测 `Engine\Binaries\ThirdParty\DotNet\<ver>\win-x64\dotnet.exe`（版本目录倒序取最高）并用它运行 `UnrealBuildTool.dll`，工作目录设为 `Engine\Source`（与 `Build.bat` 一致）；探测不到才回退 `.exe` 直调。故**生成工程文件无需手动直调**（2026-10-17 实测：`-Mode Solution` 正常产出 `game\Solstice.sln`）。`tools\build\build.ps1` 走 `Build.bat`，后者内部已 `call GetDotnetPath.bat` 设置自带 dotnet，不受此问题影响。

> 网络受限时记得清掉代理变量（`HTTPS_PROXY` 等）——直调 dotnet 不走代理，但引擎的下载器会读。本机若用 `curl` 走 https 还需 `--ssl-no-revoke`（schannel 吊销检查在本机失败）。

### 怎么看世界：PIE 与播放设置

**刚打开工程时视口一片纯黑，这不是故障。** 世界由 `ASolsticeGameMode::StartPlay()` 运行时生成（`game/Source/Solstice/Private/God/SolsticeGameMode.cpp`），而 `StartPlay()` 只在**播放中的世界**执行——编辑器视口里那个世界从不 `BeginPlay`。空关卡里既没有地形，也没有灯（`SunLight` / `SkyLight` / `Fog` / 后处理全是 `ASolsticeRealm` 的组件），所以必然黑。这是 [`../tech/tech.md`](../../docs/TECH.md) §0「关卡不是数据的家」的直接后果。

**看世界的唯一方式就是 Play**：视口里按 **`Alt+P`**，或点工具栏的播放按钮。

默认 Play 落在哪里由本仓库钉死，配置文件 `game/Config/DefaultEditorPerProjectUserSettings.ini`：

| 键 | 值 | 含义 |
|---|---|---|
| `LastExecutedPlayModeType` | `PlayMode_InEditorFloating` | **新编辑器窗口**播放，编辑器布局不被占用、可边跑边改 |
| `NewWindowWidth` / `NewWindowHeight` | `1920` / `1080` | 固定窗口尺寸，便于跨会话比对画面与截图 |
| `CenterNewWindow` | `True` | 每次居中。**保持 `True`** —— 置 `False` 时 PIE 会把上次拖动的尺寸回写给 `NewWindowWidth`，上两行随之失效（`UnrealEd/Private/PlayLevel.cpp:655`） |

其他可选模式（改 `LastExecutedPlayModeType` 的值即可）：`PlayMode_InViewPort`（在选中视口内，**引擎默认**）、`PlayMode_InNewProcess`（独立进程，最接近打包后的环境，跑 bench 更有参考价值）、`PlayMode_Simulate`（模拟：跑 `BeginPlay` 但不接管玩家）。枚举名的写法与引擎自带的 `Engine/Config/BaseEditorPerProjectUserSettings.ini`（第 231 行起）一致。

> ⚠ **谁覆盖谁**：该 category 的读取链是 `Engine/Config/BaseEditorPerProjectUserSettings.ini` → `game/Config/DefaultEditorPerProjectUserSettings.ini`（本文所述，**已入库**）→ `game/Saved/Config/<Platform>/EditorPerProjectUserSettings.ini`（**用户级，优先级最高，且 `Saved/` 不入库**）。只要在「编辑器偏好设置」里动过同一项，UE 就会写第三份文件，从此覆盖仓库里的值。**Play 又跑回视口时，先查 `Saved/Config/…/EditorPerProjectUserSettings.ini`。**

---

---

## 3 本地生图环境（已就绪，无需额外下载）

| 项 | 值 |
|---|---|
| 根目录 | `D:\ComfyUI`（便携版，`python_embeded` + `run_nvidia_gpu.bat`） |
| 服务 | `http://127.0.0.1:8188`，健康探针 `/system_stats` |
| 扩散模型 | `models\diffusion_models\flux-2-klein-4b-fp8.safetensors`（3.79GB） |
| 文本编码器 | `models\text_encoders\qwen_3_4b.safetensors`（7.49GB，`CLIPLoader type=flux2`） |
| VAE | `models\vae\flux2-vae.safetensors` |
| 超分 | `models\upscale_models\RealESRGAN_x2plus.pth`（**x2**，非 x4） |

> **采样参数与调用方式不在这里**：实测基线（`steps=4` / `cfg=1.0` / `euler` / `Flux2Scheduler`）与 `CFG=1` 的行为说明、批次规范、manifest 登记见 [`../art/art.md`](../../docs/ART.md) §3。

---

## 4 VSCode（打开 / 扩展 / 设置 / IntelliSense）

### 打开方式

**推荐**：`文件 → 打开工作区 → .vscode\solstice.code-workspace`

或直接打开**本机仓库根文件夹**（本机为 `D:\GitCode\solstice`；`.vscode/settings.json` 同样生效，效果一致）。

首次打开后建议按顺序执行（`Ctrl+Shift+P → Tasks: Run Task`）：

1. `环境: 体检` —— 确认工具链状态
2. `Solstice: 配置 VS Code (写入引擎环境变量)` —— 写 `SOLSTICE_UE_ENGINE_ROOT`；**跑完要重启 VS Code**（环境变量只在进程启动时读取，Reload Window 不够）
3. `Solstice: 生成工程文件 (Solution / cpptools)` —— 产出 `game\Solstice.sln`
4. `Solstice: 构建 SolsticeEditor (Development)` —— `Ctrl+Shift+B` 亦可
5. `Solstice: 启动编辑器`（想让 AI 接进来就用 **`Solstice: 启动编辑器（含 MCP 服务器）`**，见 本文 § §2）

> UE 与 VS2022 未安装时，第 2–5 步会明确报错并给出补救提示（不会静默失败）。
> 契约校验（`shared/`）与生图（`studio/`）两组任务所依赖的目录**均已落地**（状态见 [`../tech/tech.md`](../../docs/TECH.md) §0），任务可直接运行；生图产物 `studio/out/` 尚未生成时，相关任务会按空清单跳过。

#### 任务集（16 个）

`label` 与 `tasks.json` 逐字一致，便于直接 `Run Task` 搜索。

| 分组 | `label` | 底层调用 |
|---|---|---|
| 构建 | `Solstice: 构建 SolsticeEditor (Development)` | `tools/build/build.ps1`，**默认构建**（`Ctrl+Shift+B`） |
| 构建 | `Solstice: 构建 SolsticeEditor (Shipping)` | 同上去 `Shipping`（M6 性能验收） |
| 构建 | `Solstice: 清理并全量重编` | 同上加 `-Clean`（UBT 行为异常时才用） |
| 工程 | `Solstice: 生成工程文件 (Solution / cpptools)` | `gen-projectfiles.ps1 -Mode Solution` → `game\Solstice.sln` |
| 工程 | `Solstice: 生成工程文件 (ClangDb / clangd)` | `-Mode ClangDb` → `game\compile_commands.json` |
| 工程 | `Solstice: 配置 VS Code (写入引擎环境变量)` | `tools/env/configure-vscode.ps1`，跑完要重启 VS Code |
| 运行 | `Solstice: 启动编辑器` | `tools/run/launch-editor.ps1` |
| 运行 | `Solstice: 启动编辑器（含 MCP 服务器）` | 同上加 `-Mcp`；**AI 接入用这个**，见 本文 § §2 |
| 环境 | `环境: 体检` | `tools/env/check-env.ps1`（只读） |
| 环境 | `环境: 探测虚幻引擎` | `tools/env/find-ue.ps1 -All` |
| 自检 | `自检: 契约校验（schema + 交叉引用 + 本地化 key）` | `python tools/checks/validate-contracts.py` |
| 自检 | `自检: 链接校验（文档相对链接）` | `python tools/checks/check-links.py` |
| 回归 | `回归: 无头确定性基准（SolsticeBench）` | `tools/run/bench.ps1 -Days 2`，不开编辑器 |
| 生图 | `生图: 体检 ComfyUI（探针 + 列出模型）` | `comfy_batch.py --check` |
| 生图 | `生图: 跑地表色带批次（studio\jobs\terrain-colour.jsonl）` | `comfy_batch.py --jobs …` |
| git | `Git: 提交全部改动` | `git commit -a -m ${input:commitMessage}` |

两处本轮的修正：

- **自检与生图原先各只挂一个"包装脚本"，把一个任务虚报成两个**（参数写在 `shared/*.ps1` / `studio/*.ps1` 里，任务本身没做区分）。现改为**每个任务对应一次真实调用**——`python tools/checks/*.py` 两条、`comfy_batch.py` 两条，各自独立可跑。
- **新增两条**：`回归: 无头确定性基准（SolsticeBench）`（M0 的自动化证明，走 `-run=SolsticeBench`，不碰 RHI / World / GameMode）与 `Solstice: 启动编辑器（含 MCP 服务器）`。

`launch.json` 同步做了一处**删错换对**：原有一个指向已不存在的 `SolsticeSim.exe` 的占位配置（`SolsticeSim` 独立目标在 M0 已取消，见 [`../tech/tech.md`](../../docs/TECH.md) §3），替换为可直接 F5 的 `Solstice: 无头回归 bench（-Days 2）`（`type: PowerShell`，走 `tools/run/bench.ps1`）。

---

### 扩展：现状与处置

### 唯一真源与「不重复声明」

扩展清单只写在 `.vscode/extensions.json`。
`solstice.code-workspace` 里**刻意不再写 `extensions` 段**——工作区文件中的该段会与 `extensions.json` 叠加，
容易出现两份清单不一致（改了一处忘另一处）。扩展的一切增删请只改 `extensions.json`。

### 本机现状（2026-10-17 复检）

- VSCode **1.119.0**；CLI 由 `tools\env\vscode-extensions.ps1` 自动探测（**盘符不写死**：环境变量 `SOLSTICE_VSCODE_CLI` → PATH 中的 `code` → 标准安装位置），本机命中 PATH
- 已装扩展：**61 → 26 → 39 → 38 个**（2026-10-13 卸 34 个无关；2026-10-14 补 `eamodio.gitlens`、`jeff-hykin.better-cpp-syntax`；2026-10-17 补 8 个：CI / Python / UE 编辑器脚本 / 数据审阅；**2026-10-17 下半程再卸 2 个、补 1 个**：卸 `johnstoncode.svn-scm`、`typescriptteam.native-preview`，补 `alefragnani.bookmarks`，明细见 §3.3 / §3.5）
- **废弃残留已清空**：`~/.vscode/extensions/` 下 52 个 `.obsolete` 旧版本文件夹（3.6GB，最大两份旧版 ChatGPT 扩展共 1.9GB）已全部移出扩展目录，`.obsolete` 归零。**注意：其中约 23 个是被永久删除的** —— 本机 `Microsoft.VisualBasic` 的"送回收站"调用在无桌面会话下退化成了直接删除，回收站里只有 4 个 PowerShell 临时文件、没有这些目录（事后核对过）。剩余 30 个改为移入可逆暂存目录后处理。它们都只是 VS Code 更新失败留下的历史版本，不参与运行。
- 清完后 `code --list-extensions` = **38 个**，其中 **29 个**来自 `.vscode/extensions.json` 的 `recommendations`（与本机清单逐项一致、无缺失）；余下 9 个按设计**不进任何清单**：§3.4 因与 cpptools 冲突而 Workspace 禁用的 clangd，以及 §3.6 列出的个人工具。

### 保留并使用（推荐清单 29 个，本机均已装）

| 类别 | 扩展 | 为什么需要 |
|---|---|---|
| C++ / 引擎 | `ms-vscode.cpptools` | IntelliSense 与 `cppvsdbg` 调试的唯一来源 |
| | `ms-vscode.cpptools-themes` | C++ 语义着色主题 |
| 脚本 / 工程 | `ms-vscode.powershell` | `tools/` 下全部脚本 |
| | `editorconfig.editorconfig` | 统一编码/缩进/换行 |
| | `timonwong.shellcheck` | `tools/hooks/pre-commit` 是 sh 脚本（2026-10-17 补装） |
| 界面语言 | `ms-ceintl.vscode-language-pack-zh-hans` | 代码、脚本、文档全中文，界面统一为简体中文 |
| 数据 / 文档 | `redhat.vscode-yaml` | YAML 校验与补全 |
| | `yzhang.markdown-all-in-one` | 文档表格/TOC/预览 |
| | `davidanson.vscode-markdownlint` | 文档规范 |
| | `bierner.markdown-mermaid` | 文档里的流程图 |
| | `mechatroner.rainbow-csv` | CSV 列对齐，审阅卡表 / 数值表（2026-10-17 补装） |
| Python | `ms-python.python` | `studio/` 生图线与 `tools/checks/` 都是 Python（2026-10-17 补装） |
| | `charliermarsh.ruff` | Python 静态检查，轻量（2026-10-17 补装） |
| CI / 协作 | `github.vscode-github-actions` | 查看 / 重跑 `.github/workflows/checks.yml`（2026-10-17 补装） |
| | `github.vscode-pull-request-github` | PR / Issue 与评审（origin 在 GitHub）（2026-10-17 补装） |
| | `spmeesseman.vscode-taskexplorer` | 一处运行 `tasks.json` 里的全部任务（2026-10-17 补装） |
| UE 编辑器脚本 | `nilssoderman.ue-python` | 编辑器 Python；官方 Unreal MCP 的工具集也是 Python（2026-10-17 补装） |
| 开发体验 | `usernamehw.errorlens` | C++ 报错直接显示在行尾 |
| | `jeff-hykin.better-cpp-syntax` | C++ 语法高亮增强——UE 反射宏密集的代码可读性明显更好（2026-10-14 补装） |
| | `gruntfuggly.todo-tree` | 汇总 TODO / FIXME / 待裁决点 |
| | `wayou.vscode-todo-highlight` | TODO 高亮（与 todo-tree 互补） |
| | `streetsidesoftware.code-spell-checker` | 中英拼写检查（已配 `cSpell.words`） |
| | `christian-kohler.path-intellisense` | 路径补全 |
| | `sleistner.vscode-fileutils` | 文件操作增强 |
| | `alefragnani.bookmarks` | 大文件内打书签跳转 —— `SolsticeKernel.cpp` 这类上千行文件尤其有用（2026-10-17 补装） |
| 版本控制 | `eamodio.gitlens` | git blame / 历史（查改动出处） |
| | `mhutchie.git-graph` | 图形化 git 历史 |
| 素材 / 二进制 | `jock.svg` | SVG 预览 |
| | `ms-vscode.hexeditor` | 十六进制查看 `.uasset` 头部 |

### 已装但「按设计不启用」（1 个）

`llvm-vs-code-extensions.vscode-clangd` —— **路线 B 专用**，本机已安装。

- 当前走**路线 A**（`cpptools` + `Solstice.sln`），若 clangd 同时启用，两套引擎会**重复报错、重复高亮**。
- **处置（保持路线 A）**：扩展面板 → `llvm-vs-code-extensions.vscode-clangd` → 右键 → **Disable (Workspace)**。
- 彻底移除（推荐，卸掉后想用路线 B 再装）：
  ```powershell
  code --uninstall-extension llvm-vs-code-extensions.vscode-clangd
  ```
- 该扩展**刻意不写进 `extensions.json` 的任何一张清单**，以免被 `-RemoveUnwanted` 一并卸掉。

### 已卸载的无关扩展（36 个，全局移除）

清掉的是**旧形态（Web / Java）的遗留**：**Java / Spring / Maven 12 个** · **前端 Vue / JS / HTML / CSS 13 个** · **前端工具链 4 个**（eslint / prettier / jest 等）· **其他 5 个**（sqltools / svn / containers / code-runner / xml）。

**2026-10-17 追加 2 个**（此前是"已装但清单未覆盖"的残留，已裁决为无关）：
`johnstoncode.svn-scm`（SVN，本项目用 git；它一直在 `unwantedRecommendations` 里却还装着）·
`typescriptteam.native-preview`（TypeScript 语言特性，本项目零 TS）。已一并写进 `unwantedRecommendations`。

- **完整清单的唯一真源**：`.vscode/extensions.json` 的 `unwantedRecommendations`（同时抑制 VS Code 的推荐噪音）。
- **历史明细**：见 git 提交 `c58bc89`（`chore: prune legacy studio and app, vendor dev tooling`）改动前的本文档旧版。
- 日后确实需要某个，重装即可；**不要**把旧形态的扩展重新写回推荐清单。

### 个人工具（未登记，随你保留）

以下扩展与本项目无关也非必需，但属于个人工具，**未列入任何清单**（既不推荐也不抑制，脚本也不会动它们）：

`tencent-cloud.coding-copilot`（**本项目唯一的 AI 助手**）· `intellsmi.comment-translate` ·
`xiaodiyan.translator-helper` · `k--kato.intellij-idea-keybindings` · `orionunique.troll`

> **其他 AI 助手已清理（2026-10-13）**：卸载 `openai.chatgpt`、`openai.codex-audio`；
> 并从 VS Code 用户设置中移除残留死配置：`Lingma.*`（通义灵码）、`claudeCode.*`（Claude Code → MiniMax）、
> `codebot.*` / `codecheck.*`（华为 CodeArts）、`github.copilot.*`。
> **多套 AI 助手并存 = 多套「是否确认执行」机制互相叠加**，这才是「到处弹确认」的主要放大器，故只保留一个。
> CodeBuddy 自身的权限/自动运行配置见 §8。

### 扩展体检 / 增删

```powershell
# 只读体检：列出「已装推荐 / 缺失 / 无关」三类
powershell -ExecutionPolicy Bypass -File tools\env\vscode-extensions.ps1

# 安装缺失的推荐扩展
powershell -ExecutionPolicy Bypass -File tools\env\vscode-extensions.ps1 -InstallRecommended

# 预览要卸载的无关扩展（不执行）
powershell -ExecutionPolicy Bypass -File tools\env\vscode-extensions.ps1 -RemoveUnwanted

# 真正卸载（全局，会影响其它项目）
powershell -ExecutionPolicy Bypass -File tools\env\vscode-extensions.ps1 -RemoveUnwanted -Confirm
```

> **卸载会按依赖关系自动多轮重试**：扩展之间存在依赖（例：`vscjava.vscode-java-test` → `redhat.java`），
> 脚本会先卸依赖方、再卸被依赖方，最后报告仍无法卸载的残留项，不会因单个失败而中断整批。

### UE 相关扩展候选核实（2026-10-14）

用 VSCode 市场 API 按 `unreal engine` / `unreal blueprint` / `uproject` / `unreal` / `ue5` 检索后的结论：

**① 事实前提**：VSCode **没有官方虚幻引擎扩展**，也**没有任何插件能操作蓝图编辑器**。
所以"尽量让 AI 开发"的杠杆在 CodeBuddy 的权限模式（§8），不在插件数量。

**② 已采纳**

| 扩展 | 安装量 | 判断 |
|---|---|---|
| `eamodio.gitlens` | — | 2026-10-14 补装；提交历史 / blame |
| `jeff-hykin.better-cpp-syntax` | 12.78M | 纯语法高亮，零风险；UE 反射宏密集的代码可读性明显更好 |

**③ 条件性候选（暂不安装）**

| 扩展 | 安装量 | 何时才值得装 |
|---|---|---|
| `NilsSoderman.ue-python` | 63.4k | 等 P3 真的用官方编辑器 Python 做资产批处理、且需要在 VSCode 里编辑/执行这些脚本时 |

**④ 明确不采纳**

| 扩展 | 安装量 | 原因 |
|---|---|---|
| `CAPTNCAPS.ue4-snippets` | 147.3k | UE4 时代，API 与语法已过期 |
| `openpixel.unreal-engine-4-helper` | 29.5k | 同上 |
| `WholeTomatoSoftware.VisualAssist` | 635.2k | 商业付费 C++ 扩展，与 cpptools 功能重叠 |
| `AgitoReiKen.GenerateProjectFilesUE` | 2.8k | 本仓库 `tools\build\gen-projectfiles.ps1` 已覆盖 |
| `Hazelight.unreal-angelscript` / `-clang-format` | 25.3k / 4.1k | AngelScript，与本项目无关 |

---

### 关键设置说明（为什么这么配）

| 设置 | 值 | 理由 |
|---|---|---|
| `editor.formatOnSave` | `false` | **关键**：避免任何格式化类扩展去动 C++ / INI / PS1 |
| `files.eol` | `\n` | 统一 LF；`.editorconfig` 对 `.ps1` 单独要求 CRLF |
| `editor.detectIndentation` | `false` | 防止被引擎代码的 Tab 带偏，固定 4 空格 |
| `files.readonlyInclude` | UE 生成物 | `Binaries/Intermediate/Saved` 改也会被覆盖，设为只读防误编辑 |
| `files.exclude` / `search.exclude` / `watcherExclude` | UE 生成物 | `DerivedDataCache` 动辄数十万文件，不排除会让搜索与监听卡死 |
| `terminal.integrated.profiles.windows` | `source: PowerShell` + `-ExecutionPolicy Bypass` | 终端里可直接跑 `tools\*.ps1`，不必先改执行策略 |
| `C_Cpp.default.cppStandard` | `c++20` | 与 `c_cpp_properties.json`、`.clang-format` 一致 |
| `explorer.confirmDelete` | `true` | 覆盖全局的 `false`：游戏资产与生成图删错代价高 |
| `files.autoSave` | `off` | C++ 保存可能触发 UBT/Live Coding，显式保存更可控 |
| `[ini]` / `[json]` / `[yaml]` | `tabSize: 2` | 与 `.editorconfig` 对数据与配置文件的约定一致 |
| `cSpell.words` | 见文件 | 已加 `UCLASS` `USTRUCT` `cppvsdbg` `ComfyUI` `Yearbound` `Standee` `SolsticeBench` `SolsticeGenAssets` 等项目词 |

> **已清理**：原先为 Java / Vue / eslint / prettier / jest / sqltools / svn / xml 等扩展逐条写死的「降噪设置」
> （如 `eslint.enable: false`）在本轮已随扩展一并删除——扩展不在，设置即失效，留着只会误导。
> 日后重装某扩展时，直接在编辑器 UI 里按需关闭即可。

---

### IntelliSense 路线切换

**路线 A（当前，默认）**：`ms-vscode.cpptools` + `game\Solstice.sln`
```powershell
# 执行 task：Solstice: 生成工程文件 (Solution / cpptools)
```

**路线 B（可选）**：`clangd` + `game\compile_commands.json`
1. 确保 `llvm-vs-code-extensions.vscode-clangd` 已安装并在本工作区**启用**（默认已被你 Disable (Workspace)）
2. 执行 task：`Solstice: 生成工程文件 (ClangDb / clangd)`
3. 把 `.vscode/settings.json` 里 `C_Cpp.intelliSenseEngine` 改成 `"disabled"`
4. 取消注释同文件中 clangd 的三行配置

> 两者**不可同时启用 IntelliSense**。若系统内存紧张，留在路线 A。
> 细节与冲突警示已写在 `.vscode/settings.json` 的「C/C++」段落注释里。

---

### 为什么工作区是单根（不拆多根）

所有 task 与 launch 配置都用 `${workspaceFolder}` 定位仓库根（`tools\`、`studio\` 脚本，`game\Solstice.uproject`）。

多根工作区下 `${workspaceFolder}` 会解析到各自子文件夹，**任务与调试路径会全部错乱**。
因此保持单根 = 仓库根，用 `files.exclude` 收纳 `game/Binaries` 等噪音目录。

---

## 5 速查命令

```powershell
# 体检与环境
powershell -ExecutionPolicy Bypass -File tools\env\check-env.ps1
powershell -ExecutionPolicy Bypass -File tools\env\find-ue.ps1 -All
powershell -ExecutionPolicy Bypass -File tools\env\configure-vscode.ps1

# 工程与构建
powershell -ExecutionPolicy Bypass -File tools\build\gen-projectfiles.ps1 -Mode Solution
powershell -ExecutionPolicy Bypass -File tools\build\build.ps1
powershell -ExecutionPolicy Bypass -File tools\build\build.ps1 -Target Solstice

# 内容与回归（新克隆后按顺序各跑一次）
powershell -ExecutionPolicy Bypass -File tools\build\gen-assets.ps1
powershell -ExecutionPolicy Bypass -File tools\run\bench.ps1 -Days 2

# 提交钩子（每个克隆装一次；提交时自动跑 checks/ 两条自检）
powershell -ExecutionPolicy Bypass -File tools\hooks\install-hooks.ps1

# 起编辑器
powershell -ExecutionPolicy Bypass -File tools\run\launch-editor.ps1
```

**新克隆的最小闭环**：`find-ue` → `gen-projectfiles` → `build` → `gen-assets` → `bench`。
前两步没有就编不过，第三步没有就跑不到「纸材质已生成」的状态（会退化到引擎自带的 unlit 调试材质，界面仍能起，但赛璐璐配色不对）。

### 机器相关路径的覆盖方式（**盘符不写死**）

脚本一律"自动探测优先、可显式钉死"。优先级：**显式参数 > 环境变量 > 自动探测 > 内置默认**。

| 环境变量 | 作用 | 示例 |
|---|---|---|
| `SOLSTICE_UE_ENGINE_ROOT` | **钉死**引擎根（有效时直接返回，不再扫描候选）。`.vscode/{c_cpp_properties,launch,settings}.json` 里的 `${env:...}` 引用的就是它 | 由 `tools\env\configure-vscode.ps1` 自动写入（正斜杠、无尾随斜杠）：`D:/Epic Games/UE_5.8` |
| `SOLSTICE_UE_SEARCH_ROOTS` | 引擎搜索根目录，分号分隔 | `setx SOLSTICE_UE_SEARCH_ROOTS "D:\UE;E:\Epic Games"` |
| `SOLSTICE_COMFYUI_ROOT` | ComfyUI 根目录 | `setx SOLSTICE_COMFYUI_ROOT "E:\ComfyUI"` |
| `SOLSTICE_VSCODE_CLI` | VSCode 的 `code.cmd` 完整路径 | `setx SOLSTICE_VSCODE_CLI "D:\Microsoft VS Code\bin\code.cmd"` |

- **每个变量"由哪个脚本读"属脚本层细节**，单一真源表在 [`tools/README.md`](../README.md) §"单一真源"。
- **引擎根是否被"钉死"看 `SOLSTICE_UE_ENGINE_ROOT`**：设了且有效就直接用它；没设或路径失效则回退扫描。所以"临时换引擎试一下"只需改这个变量，不必动脚本。
- ComfyUI 体检**不写死模型文件名**：扫到任一 `models/**/diffusion_models/*.safetensors` 即算就绪，换模型不改脚本。
- **VS Code 侧同走这个变量**：`.vscode` 三份配置写 `${env:SOLSTICE_UE_ENGINE_ROOT}` ⇒ 换主机不用改仓库里任何文件（详见 本文 § §2 / §8.4）。注意环境变量只在进程启动时读取，写完必须**整进程重启 VS Code**，`Reload Window` 不够。

---

## 6 排错（常见问题）


| 症状 | 处理 |
|---|---|
| C++ 满屏红波浪线，但能编译 | `SOLSTICE_UE_ENGINE_ROOT` 未设置 / 未生成 `.sln` → 跑 `tools\env\configure-vscode.ps1`，**重启 VS Code**，再执行 `生成工程文件` task。机制见 §7.1 |
| 换了主机后 `.vscode` 里的引擎路径不对 | 不会发生——`.vscode` 里没有绝对路径，只有 `${env:SOLSTICE_UE_ENGINE_ROOT}`。新主机上跑一次 `tools\env\configure-vscode.ps1` + 重启 VS Code 即可（§7.1） |
| C++ 报错每行出现两次 | clangd 与 cpptools 同时在跑 → 对 clangd 执行 **Disable (Workspace)**（见 §3.4） |
| 中文脚本在终端报语法错 | 脚本须为 **UTF-8 with BOM**。`.editorconfig` 已强制；现存脚本已批量补 BOM。**新建 `.ps1` 最容易漏 BOM**（2026-10-17 实际踩过：`lib/Solstice.Common.ps1` 缺 BOM → PS 5.1 按 ANSI 解析中文注释 → `UnexpectedToken`）。排查：`format-hex` 看头三字节是否为 `EF BB BF` |
| tasks 找不到 `tools\*.ps1` | 确认打开的是**仓库根**（不是 `game/` 子目录） |
| 终端里 `无法加载文件，因为在此系统上禁止运行脚本` | 用 tasks（已带 `-ExecutionPolicy Bypass`），或执行 `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned` |
| 契约校验 / 生图任务报「文件不存在」 | `shared/` 与 `studio/` 目录**均已落地**（状态见 [`../tech/tech.md`](../../docs/TECH.md) §0）。若仍报错，检查是否在**仓库根**运行、以及 `studio/out/manifest.jsonl` 是否存在 |
| 搜索很慢 / 风扇狂转 | 确认 `files.watcherExclude` 生效；必要时关闭搜索中的 `game/Content` |

### 引擎路径为什么改成环境变量

**旧做法**：`.vscode/{c_cpp_properties,launch,settings}.json` 里写占位符 `__UE_ENGINE_ROOT__`，
由 `configure-vscode.ps1` 把它替换成**本机绝对路径**。三个副作用：

1. 每台机器都要重跑一次，否则 IntelliSense 失效；
2. 跑完 `git status` 就一直挂着 `.vscode` 的修改；
3. **一旦误提交**，本机路径被推给其它主机；此时占位符已被替换掉，脚本也修不回来（找不到可替换的标记）。

**现做法**：三个文件统一写 `${env:SOLSTICE_UE_ENGINE_ROOT}`（VS Code 原生支持 `${env:VAR}` 变量），
`configure-vscode.ps1` 只负责**探测引擎并把用户级环境变量写对**，不再改动任何文件。
于是这四个文件与主机无关、可以安全入库，`git status` 也恢复干净。

```powershell
# 看当前值（两个作用域，新进程可能继承不到刚写的用户级值）
[Environment]::GetEnvironmentVariable('SOLSTICE_UE_ENGINE_ROOT', 'User')

# 换引擎 / 换主机后重跑（-DryRun 只体检）
powershell -ExecutionPolicy Bypass -File tools\env\configure-vscode.ps1
```

> ⚠ **改完必须重启 VS Code**（整进程退出后重开）——`${env:...}` 在扩展宿主启动时求值，`Reload Window` 不够。
> 优先级：显式 `-EngineRoot` > 环境变量 `SOLSTICE_UE_ENGINE_ROOT` > 自动探测。见 本文 § §5.1。

---

## 7 CodeBuddy（IDE 内 AI 助手）配置


本项目**只使用 CodeBuddy 一个 AI 助手**（其余 AI 插件已清理，见 §3.6）。

### VS Code 插件侧（`codingcopilot.*`）

**位置**：VS Code 用户设置 `%APPDATA%\Code\User\settings.json`（`application` 作用域，工作区无法覆盖，所以只能改用户级）。

```jsonc
{
  // 命令自动运行，不再逐条弹批准
  "codingcopilot.autoRun": true,

  // 要【跳过】的内置安全检查类别（数组里没有的类别 = 校验保持开启）。
  // 2026-10-17 收窄：只跳过"打断多、后果可逆"的 7 类；
  // diskOps / fileDelete / permissions / systemServices / userManagement / processControl
  // 这 6 类会造成**不可逆后果**，校验保留。
  // 注意：这份设置会随 VS Code 设置同步推到每台主机，收窄它等于把防线同步到所有主机。
  "codingcopilot.disabledSecurityCategories": [
    "windowsSystem", "network", "gitOps", "injection",
    "scriptExec", "powershell", "custom"
  ],

  // 删除仍走回收站（可恢复），但批量删除不再弹确认
  "codingcopilot.safeDeleteEnabled": true,
  "codingcopilot.safeDeleteBulkThreshold": 99999,

  // 额外拦截命令（正则，逐条匹配整条命令）；留空即不额外拦截
  "codingcopilot.customBlacklistCommands": []
}
```

> ⚠ 仍跳过的 7 类里包含 `powershell` / `scriptExec` / `injection`，即 AI 可**静默执行任意命令**；
> 安全网是下面的三层：① 6 类不可逆操作仍有校验；② `safeDeleteEnabled: true`（删除进回收站，可恢复）；
> ③ 工作区级 `explorer.confirmDelete: true` 覆盖全局的 `false`。
> 想**全关**：把上面 6 个类别名补回数组；想**全开**：整个键删掉。
> 若同时使用 CodeBuddy CN 独立 IDE，需在其 `%APPDATA%\CodeBuddy CN\User\settings.json` 做同样配置。

### CLI / agent 层权限模式

配置位于**用户级** `~/.codebuddy/settings.json`（Windows：`C:\Users\<你>\.codebuddy\settings.json`）。

```jsonc
{
  "permissions": {
    "defaultMode": "bypassPermissions", // 直接执行，不再逐条弹批准
    "allow": ["Bash(*)", "PowerShell(*)"]
  }
}
```

| `defaultMode` 合法取值 | 含义 |
|---|---|
| `default` | 最保守：只放行信任目录内的读取，其余都询问（**兜底值**） |
| `acceptEdits` | 自动放行文件编辑，命令仍询问 |
| `auto` | 把「最后仍会询问」的动作交给分类器判定（**仅用户级 settings 可授予**） |
| `dontAsk` | 不弹框，未预批准的动作**直接拒绝** |
| `plan` | 先写计划，确认后再执行 |
| `bypassPermissions` | **跳过绝大多数审批**（本项目当前使用） |

要点：
- 取值优先级：**会话当前值 > CLI `--permission-mode` > `permissions.defaultMode` > `default`**。
  命令面板/快捷键（`Shift+Tab`）临时切过的模式会**压过**文件配置，改文件后请**新开会话**或把模式切回。
- `deny` 规则**永远强于**模式；`allow` / `ask` 会改变模式的实际效果。
- 真正的「任何危险命令都不问」需要进程环境变量 `CODEBUDDY_IS_SANDBOX=1` 配合 `-y`，
  文档明确其仅限隔离沙箱；本项目**未启用**，仅用 `bypassPermissions`。

### 插件集与凭据位置（2026-10-17）

**插件集**：启用清单的真源是 `~/.codebuddy/settings.json` 的 `enabledPlugins`（当前 11 个）。
逐项留用理由、被移除的 4 个专有许可办公插件、以及"本机缺运行时故未纳入"的 3 个插件，
统一记在 本文 § §3.1 —— **本文不重复**，只做指针。

**API 密钥已移出配置文件**：原先 `settings.json` 的 `env.CODEBUDDY_API_KEY` 是**明文**，
现已改为**用户级环境变量** `CODEBUDDY_API_KEY`（`[Environment]::SetEnvironmentVariable(..., 'User')`），
`settings.json` 不再保存它。

```powershell
# 校验（回读应与你设置的值一致）
[Environment]::GetEnvironmentVariable('CODEBUDDY_API_KEY', 'User')

# 回滚：从备份恢复（备份文件名形如 settings.json.bak-<yyyymmdd-HHmmss>）
Copy-Item "$env:USERPROFILE\.codebuddy\settings.json.bak-<时间戳>" "$env:USERPROFILE\.codebuddy\settings.json" -Force
```

> ⚠ **改完必须重启 CodeBuddy** —— 已运行的进程不会重新读取用户级环境变量；不重启会表现为"认证失败"。
> `gateway.password` 仍留在 `settings.json`：它是应用自定义字段、没有对应的环境变量名可迁，
> 保持原样**不做猜测性改动**。它也是**刻意不入库**的那一项（见 §8.4）。

### 跨主机同步（换机后怎么把"我的信息和扩展"搬过去）

**边界一句话**：VS Code 的设置同步**不覆盖** `~/.codebuddy/`。

| 要搬的东西 | 位置 | 谁负责 |
|---|---|---|
| 用户设置 / 快捷键 / 片段 / 任务 / **扩展清单** | `%APPDATA%\Code\User\*` | **VS Code 内置 Settings Sync**（云端，跟随账号） |
| 工作区级配置（编码、排除、C++、任务、调试） | 仓库 `.vscode/*` | **git**（本来就随仓库走） |
| 推荐扩展清单 | 仓库 `.vscode/extensions.json` | **git**；打开仓库时 VS Code 会自动提示安装 |
| CodeBuddy 用户级配置（插件集 / 权限模式 / 记忆开关 / MCP） | `~/.codebuddy/{settings,mcp}.json` | **仓库模板** `tools/ai-config/` + 同步脚本 |

**换机在 VS Code 侧只有两步**：

1. `git clone` 本仓库 → 按 本文 搭引擎与编译链 → `tools\env\configure-vscode.ps1` → **整进程重启 VS Code**（Reload Window 读不到新写的用户级环境变量）。
2. 左下角账户图标 → `Turn on Settings Sync…` → 用 GitHub 或 Microsoft 账号登录 → 勾选 Settings / Keybindings / Snippets / Extensions / Tasks。
   - 用户设置里已预置 `settingsSync.keybindingsPerPlatform: true` 与 `settingsSync.ignoredSettings`（`jdk.jdkhome`、`terminal.integrated.profiles.windows` 等**机器相关项不参与同步**，避免"在 A 机改好、B 机被覆盖成错的"）。
   - 没登录 / 离线时的兜底：打开仓库 → `.vscode/extensions.json` 提示安装推荐扩展，或跑 `tools\env\vscode-extensions.ps1 -InstallRecommended`。

> **CodeBuddy 侧（第三步）不在本文件**：`tools/ai-config/` 模板、`sync-ai-config.ps1` 的 `-Apply` / `-Export` 语义与合并规则见 本文 § §4——那里是这条线的真源。

---

## 8 UE 5.8 内置 Unreal MCP


### 事实（本机已核实）

| 项 | 值 |
|---|---|
| 插件目录 | `D:\Epic Games\UE_5.8\Engine\Plugins\Experimental\{ModelContextProtocol, ToolsetRegistry, Toolsets, Terminal}`（**引擎自带，非第三方**） |
| 形态 | 虚幻编辑器**进程内嵌**一个 MCP 服务器；任何 MCP 兼容客户端可经**本地 HTTP** 驱动编辑器 |
| 传输 | **仅 HTTP / SSE**；**不支持 stdio 与 WebSocket** |
| 默认端点 | `http://127.0.0.1:8000/mcp`（端口与路径可在编辑器偏好设置里改） |
| 安全边界 | 仅环回、拒绝非环回 Origin、**没有认证层** ⇒ **绝不可暴露到局域网 / 公网** |
| 内置工具集 | `SceneTools`、`ActorTools`、`MaterialInstanceTools`、`ObjectTools`（多为 Python 实现） |
| 默认模式 | **工具搜索模式**（`bEnableToolSearch = true`）：`tools/list` 只返回 `list_toolsets` / `describe_toolset` / `call_tool` 三个元工具 |
| 成熟度 | **Experimental** —— 官方明示"很多功能不完善"，API 与数据格式可能随时变 |

### 启动与验证（**GUI 与无头两条链路均已实测跑通**）

**前提已完成，无需 GUI 操作**：`game/Solstice.uproject` 已显式启用 `ModelContextProtocol` 以及所需工具集（`ToolsetRegistry` 随之自动启用）。改插件集请直接改这份清单，不要在编辑器里点。

**但服务器默认「不」开** —— `ShouldAutoStartServer()` 只有三条放行路径（真源 `ModelContextProtocolSettings.cpp`）：

| 路径 | 默认 | 说明 |
|---|---|---|
| 命令行 `-ModelContextProtocolStartServer` | 关 | ✅ 推荐；把作用域限制在这一次启动 |
| 命令行 `-StartModelContextProtocolServer` | 关 | ⚠️ UUID 弃用写法，仍生效但会打警告 |
| 偏好设置「自动开启服务器」（`bAutoStartServer`） | `false` | **commandlet 下被跳过**（源码注释：避免与常驻编辑器抢端口导致 cook 失败） |

> **坑（本机已踩）**：`-NullRHI -unattended` 只是不出渲染窗口，**不会**顺带开 MCP；不带上述参数时进程照常启动、插件照常加载、toolset 照常注册，**唯独 8000 端口没人监听**，且日志里没有 `Starting MCP server` 行 —— 很容易误判成"插件没加载"。

**启动（推荐走仓库脚本，参数不用记）**：

```
# 编辑器 GUI + MCP
powershell -ExecutionPolicy Bypass -File tools\run\launch-editor.ps1 -Mcp

# 无头 + MCP（不开渲染窗口；-McpPort 可改端口）
powershell -ExecutionPolicy Bypass -File tools\run\launch-editor.ps1 -Mcp -Headless
```

等价于手动给 `UnrealEditor.exe` 追加 `-ModelContextProtocolStartServer -ModelContextProtocolPort=8000`（`-Headless` = `-NullRHI -unattended -nopause`）。

> 经 `powershell -File` 调用时**不要**用 `-ExtraArgs -NullRHI,-unattended` 这种逗号数组写法——`-File` 模式下它会在解析期直接报「参数列表中缺少参量」。要传多个额外开关：用 `-Headless`，或在 PowerShell 会话里 `& tools\run\launch-editor.ps1 -Mcp -ExtraArgs @('-NullRHI','-unattended')`。

**验证**（`initialize` 应回 `200` 且带 `Mcp-Session-Id` 响应头）：

```
curl -i -X POST http://127.0.0.1:8000/mcp -H "Content-Type: application/json" -H "Accept: application/json, text/event-stream" -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-06-18\",\"capabilities\":{},\"clientInfo\":{\"name\":\"probe\",\"version\":\"1\"}}}"
```

| 查什么 | 怎么查 |
|---|---|
| 端口 | `Get-NetTCPConnection -LocalPort 8000 -State Listen` → 应指向 `UnrealEditor` 进程 |
| 日志 | 查 `LogModelContextProtocol: Starting MCP server on port 8000`；**无此行 = 没开服**（不是没加载）；可 `LogModelContextProtocol Verbose` 提级 |
| 工具清单 | 默认「工具搜索模式」下 `tools/list` 只回 `list_toolsets` / `describe_toolset` / `call_tool` 三个元工具 |
| 图形客户端 | `npx @modelcontextprotocol/inspector`（传输选 **Streamable HTTP**，指向同一端点） |
| 运行期控制 | 编辑器控制台：`ModelContextProtocol.StartServer` / `.StopServer` / `.RefreshTools`；`ModelContextProtocol.GenerateClientConfig <ClaudeCode\|Cursor\|VSCode\|Gemini\|Codex\|All>` 可生成对照用的客户端配置 |
| 连接 CodeBuddy | 本仓库已写好用户级配置（§2.3）；在 `Settings → MCP` 面板确认条目为**绿色** |

**回滚**：`game/Solstice.uproject` 里把 `ModelContextProtocol` 的 `Enabled` 改为 `false`（或删条目）并重启；CodeBuddy 侧删掉 `~/.codebuddy/mcp.json` 里的 `unreal-mcp` 条目。

#### GUI 链路实测（2026-10-17）

在**装了扩展的编辑器 GUI**里跑一遍，确认"边改边观察"这条路真的通，而不是只有无头能连：

1. 走 `Solstice: 启动编辑器（含 MCP 服务器）`（等价 `launch-editor.ps1 -Mcp`）拉起编辑器；
2. `Get-NetTCPConnection -LocalPort 8000 -State Listen` → 指向 `UnrealEditor.exe`；
3. 对上文的 `curl` 打一条 `initialize` → **`HTTP/1.1 200` + 响应头 `Mcp-Session-Id: …`**，正文回 `protocolVersion 2025-06-18`、`capabilities.tools.listChanged = true`。

第 3 步的 `Mcp-Session-Id` 是关键判据：它证明服务器完成了会话建立，**不只是端口开着**。日志侧对应 `LogModelContextProtocol: Starting MCP server on port 8000`。

> 顺带澄清一个容易误判的相邻现象：编辑器在跑的时候，命令行全量重编会以 `Unable to build while Live Coding is active`（`HotReload.cs:277`）被拒。**这是 Live Coding 的正常互斥，不是 MCP 造成的**。要命令行重编就先关编辑器；只想热补丁则留在编辑器里用 `Ctrl+Alt+F11`（本机已全局关掉 UBA 的 detour，热补丁同样不受影响，见 本文 § §2.4.1）。

#### 调用链与真实能力（2026-10-17 实测，含踩坑）

握手成功只说明"能连"；下面这套流程才是"真的能干活"。

**一次完整调用的四步**（`curl` 视角；客户端之间只有封装差异）：

| 步 | 请求 | 关键点 |
|---|---|---|
| 1 | `initialize` | 从**响应头**取 `Mcp-Session-Id`，后续每个请求都带它 |
| 2 | `notifications/initialized` | 通知（无 `id`），**不带会被服务器拒绝后续调用** |
| 3 | `tools/list` | 默认「工具搜索模式」下**只回 3 个元工具**：`list_toolsets` / `describe_toolset` / `call_tool` |
| 4 | `tools/call` → `call_tool` | 见下 |

**坑（踩了两次）**：`call_tool` 的 `tool_name` **必须去掉工具集前缀** —— 写 `IsPIERunning` 对，写 `EditorToolset.EditorAppToolset.IsPIERunning` 会回 `Unknown tool …`。工具集由同级的 `toolset_name` 指定。

```bash
# 真实调用示例：查 PIE 是否在跑
curl -s -X POST http://127.0.0.1:8000/mcp \
  -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -H "Mcp-Session-Id: $SID" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"call_tool",
       "arguments":{"toolset_name":"EditorToolset.EditorAppToolset",
                    "tool_name":"IsPIERunning","arguments":{}}}}'
# → {"returnValue":true}
```

**本机实际注册的工具集**（`list_toolsets` 原样输出）：`ToolsetRegistry.AgentSkill`、`AutomationTest`、`ConfigSettings`、**`EditorAppToolset`**、`Logs`、`Plugin`、`SlateInspector`，外加 `editor_toolset.toolsets.*` 下 16 个子集（`actor` / `asset` / `blueprint` / `curve_table` / `data_asset` / `data_table` / `material` / `material_instance` / `object` / `primitive` / `scene` / `skeletal_mesh` / `static_mesh` / `string_table` / `programmatic` / `texture`）。

对"边改边观察"直接有用的几组：

| 想干什么 | 工具 |
|---|---|
| 看游戏有没有在跑 | `EditorAppToolset.IsPIERunning` |
| 起 / 停游玩会话 | `EditorAppToolset.StartPIE`（参数 `PIESessionOptions`：`bSimulate` / `playMode` / `startTransform` / `warmupSeconds`）、`StopPIE` |
| **看画面** | `EditorAppToolset.CaptureEditorImage`（无参）、`CaptureViewport`、`CaptureAssetImage` |
| 摆机位 / 找东西 | `GetCameraTransform` / `SetCameraTransform` / `FocusOnActors`、`GetVisibleActors` / `GetSelectedActors`、`WorldPosToScreenCoords` / `ScreenCoordsToWorld` |
| 查 cvar / 读日志 | `EditorAppToolset.SearchCVars`、`LogsToolset`（读输出日志 + 改日志类别详细度） |
| 跑自动化测试 | `AutomationTestToolset`（`DiscoverTests` → `ListTests` → `RunTests` → `GetTestStatus` / `GetTestResults`） |
| 驱动编辑器 UI | `SlateInspectorToolset`（Playwright 风格：`Observe()` 拿控件快照、模拟键盘鼠标；走 Slate 事件 API 而非 `AutomationDriver`，后者在游戏线程调用会死锁） |

**两个截图工具的分工（重要，很容易拿错）**：

- **`CaptureEditorImage`（无参）→ 抓"整个编辑器窗口在桌面上的样子"**，是**唯一能看见游戏画面**的那个。实测：微缩世界 + HUD 的时间 / 温度 / 湿度 / 图鉴面板 + 底部手牌栏全部可见。
- `CaptureViewport` → 抓**关卡视口**。它的 `captureTransform` 与 `annotations` **两个参数都必须显式传**，缺任何一个都报 `input param "…" needs a default value`（想只要画面、不要网格与标签，就把 `annotations.gridSpacing` / `maxLabelDistance` / `maxLabels` 都传 `0`）。而它拍到的**始终是编辑器世界 —— PIE 在跑时依然全黑**，因为游戏画面渲染在 game viewport 上，不是关卡视口的相机视角。这与 本文 § §2.7「编辑器视口全黑是设计」是同一件事的两个侧面，**别把它当成"渲染坏了"**。

### CodeBuddy 侧配置

**两个客户端各写一份**（扩展宿主与引擎宿主读的不是同一个文件，只写一份会出现"一边绿一边红"）：

`~/.codebuddy/mcp.json` —— CodeBuddy 扩展 / 插件侧：

```json
{
  "mcpServers": {
    "unreal-mcp": {
      "type": "http",
      "url": "http://127.0.0.1:8000/mcp"
    }
  }
}
```

`~/.workbuddy/mcp.json` —— 桌面端侧（字段同上，另加一段 `description` 说明启动方式与环回限制）：

```json
{
  "mcpServers": {
    "unreal-mcp": {
      "type": "http",
      "url": "http://127.0.0.1:8000/mcp",
      "description": "Unreal Editor 5.8 内置 MCP（Experimental）。必须是编辑器先跑起来且带着 -ModelContextProtocolStartServer，否则本条目连接失败（不影响其它功能）。启动方式：tools\\run\\launch-editor.ps1 -Mcp，或 VS Code 任务「Solstice: 启动编辑器（含 MCP 服务器）」。仅在环回地址监听、无认证层，切勿暴露到本机之外。详见 `tools/docs/ENVIRONMENT.md` §8。"
    }
  }
}
```

> 两份都指向 `http://127.0.0.1:8000/mcp`（同一端点）；端口改了要**同时**改两处。
> 新写入的 MCP 条目**不会自动生效**：到连接器管理页右上角的自定义连接器入口点一次「信任」才启用。
> CodeBuddy 官方文档只给出 `stdio` 示例；若 `type: http` 不被接受，改用它 MCP 面板的 **Add MCP** 手工添加，并参考第 3 步生成的 `.mcp.json`。
> 与编辑器自生成的 `.mcp.json` **不要提交进仓库**（它是本机状态；仓库的 `.gitignore` 已忽略 `tmp/`，如需长期保留请单独约定）。

### 边界（为什么不是必装）

| 能做 | 不必用 MCP 也能做 |
|---|---|
| 查看 / 选中场景里的 Actor | 改 C++ → `build.ps1` → `bench.ps1` 无头验证 |
| 创建 / 调整材质实例 | 纸材质由 `gen-assets.ps1` 代码生成 |
| 场景装配、批量摆件 | 世界内容由 `SolsticeGenAssets` / 数据 JSON 驱动 |

**红线**：**禁止让 MCP 成为构建 / 回归链路的依赖** —— 否则 headless 跑不起来、CI 失效（本文 § §5 的最小闭环必须保持无需编辑器）。

### 可选的下一步：自定义 Solstice 工具集（🟡 计划中）

`UToolsetDefinition`（**Python 优先**，放 `Content/Python/`；需要引擎未暴露能力时才用 C++）可以把项目既有能力直接暴露给 AI，省掉"AI 走 shell + 解析日志"的一层：

| 设想工具 | 等价于现在做什么 |
|---|---|
| `run_bench(days)` | 跑 `SolsticeBench` 并回世界摘要 / digest |
| `query_card_pool(element, tier)` | 按元素 / 环级查卡池与价值表 |
| `read_world_digest()` | 读世界摘要（种群 / 年表 / 上色率） |
| `regen_paper_materials()` | 跑 `SolsticeGenAssets` |

- 约定：**只读优先**（查询类先做），写操作必须幂等。
- 前提：插件与工具集已启用（§2.2），无需额外准备。
- 触发时机：M1 阶段（当前 M0，先不做——避免在实验性 API 上投工）。

---

## 9 插件集与取舍


### 当前启用的插件（11 个 · 全部来自 `codebuddy-plugins-official`）

**配置真源**：用户级 `~/.codebuddy/settings.json` 的 `enabledPlugins`（改这个文件即改启用集合）。

| 插件 | 作用 | 为什么留 |
|---|---|---|
| `commit-commands` | Git 提交 / 推送 / 建 PR 工作流命令 | 本仓库提交规范严格（英文、简短祈使句、文档同次提交） |
| `pr-review-toolkit` | PR 审查 agent 集（code-reviewer / silent-failure-hunter / type-design-analyzer 等 6 个角色） | `.github/workflows/checks.yml` 是 CI 闸门，评审有固定套路 |
| `code-simplifier` | 只简化**最近改动**的代码、保持功能 | 契合既定的"简洁直白、反对过度设计"偏好 |
| `security-scan` | 代码安全审计（多 agent + hooks + references） | 与 `skills-sec-audit` 互补：前者扫**代码**、后者审 **skill** |
| `development-essentials` | 核心开发命令集（编码 / 调试 / 测试 / 优化 / 文档） | 补齐常用 `/` 命令 |
| `feature-dev` | 功能开发工作流（代码库探索 → 架构设计 → 质量审查） | 每加一个子系统都走同一套流程 |
| `context7` | 拉取库 / 框架**最新文档**（MCP，走 `npx`） | 事实准确性优先；**`npx` 本机可用 ⇒ 开箱即用** |
| `agent-browser` | 浏览器自动化 | 无外部运行时依赖 |
| `playwright-cli` | 浏览器自动化（Microsoft 官方） | 同上 |
| `find-skills` | 技能发现与安装 | 配合 Skills 生态 |
| `skills-sec-audit` | **skill** 安全审查 | 安装第三方 skill 前的闸门 |

**2026-10-17 移除**：`pptx` / `docx` / `xlsx` / `pdf` —— 这四个是**整个官方市场里仅有的 `Proprietary`（专有许可）插件**（其余 205 个为 MIT / Apache-2.0 / 未声明），且都是办公文档处理，与 UE5 + C++ 游戏开发无关。

**未纳入（本机缺运行时，装上也会静默起不来）**：

| 插件 | 缺什么 | 补齐后可启用 |
|---|---|---|
| `clangd-lsp` | PATH 上没有 `clangd` 二进制（VSCode 的 clangd 扩展自带的那份不在 PATH 上） | 装 LLVM/clangd 后 |
| `serena` | 没有 `uvx`（需 Python `uv`） | 装 `uv` 后 |
| `github` | 没有 `GITHUB_PERSONAL_ACCESS_TOKEN` | 配好 token 后 |

> 判断依据：插件的 `.mcp.json` / `.lsp.json` 启动命令分别是 `clangd`、`uvx --from git+…serena start-mcp-server`、`https://api.githubcopilot.com/mcp/`（需 token 头）。**先确认运行时在，再启用**，否则只是加噪声。

### 候选工具（未安装 / 待确认）

| 工具 | 是什么 | 为什么先不上 |
|---|---|---|
| **Serena**（MCP，开源） | 符号级语义检索 / 编辑 / 重构，"给 agent 的 IDE" | 对 `Sim/` 这种大 C++ 文件确实有价值，但需额外 Python 环境（`uv`）与索引成本；**先装 `uv` 再评估收益** |
| **codebase-memory / memory MCP** | 跨会话记忆 | 与 CodeBuddy 自带 `memory` **功能重叠**，不重复引入 |
| **MCP Inspector** | 官方 MCP 调试客户端 | 仅在调试 §2.2 时按需 `npx` 调用，不常驻 |
| 第三方 UE MCP 方案（UnrealClaude、Flopperam `unreal-engine-mcp`、CodeScape、Nwiro 等） | 功能更全的编辑器操作（资产 / 蓝图 / DataTable） | **触及硬约束**：不引入计划外第三方插件 / 中间件，须先经用户裁决；且官方插件已覆盖同类需求的主要场景。详见 §3.3 |

### 第三方 UE MCP 方案对比（2026-10 调研）

**先分清三条技术路线** —— 它们解决的不是同一个问题，混在一张表里比"工具数"没有意义：

| 路线 | 形态 | 代表 | 本质 |
|---|---|---|---|
| **A 引擎内嵌 · 第一方** | 编辑器进程内跑 MCP server | **Epic 官方 Unreal MCP**（UE 5.8 起） | 协议是第一方的；工具靠 `Toolset Registry` 扩展 |
| **B 引擎内嵌 · 第三方插件** | 自带 C++ 插件（+ 一层的 Node / Python 桥） | UnrealClaude、Flopperam `unreal-engine-mcp`、CodeScape、Nwiro | 自己维护一套工具表去覆盖引擎 API |
| **C 引擎外桥接** | 不给引擎装插件，走内置 **Remote Control API** + Python Editor Script Plugin | `FFZackFair92/unreal-engine-mcp`、`ziggymar/unreal-mcp`、npm `unreal-engine-mcp-server` 等 | 拿 Python 反射当"万能层"，不编译 C++ |

#### 事实对照（截至 2026-10-17，取自各自官方仓库 / 文档 / Fab 页面）

| 方案 | 路线 | 引擎版本 | 传输与默认端点 | 许可 / 价格 | 工具规模 | 独门能力 |
|---|---|---|---|---|---|---|
| **Epic Unreal MCP** | A | **5.8**（Experimental） | 仅 HTTP + SSE；`127.0.0.1:8000/mcp` | **随引擎免费**（源码需 UE 源码许可） | 官方口径 30+ toolsets / 数百工具；出厂 `SceneTools` `ActorTools` `MaterialInstanceTools` `ObjectTools`（多为 Python） | 第一方；`UToolsetDefinition` + `AICallable` 注册自定义 C++/Python 工具；`GenerateClientConfig` 生成各客户端配置 |
| **UnrealClaude**（Natfii） | B | **5.7** | 插件内 HTTP `:3000`（Node 桥） | **MIT**，免费（需 Claude 订阅 / Claude Code） | 20+ | **视口截图回读**（AI 自我审查闭环）、动态 UE 5.7 文档上下文、会话持久化、复用 Claude Code 登录免 key |
| **Flopperam `unreal-engine-mcp`**（deltav-deltaverse 是其 fork） | B | **5.5 / 5.6 / 5.7** | MCP stdio（Python server）→ TCP → C++ 插件 | **MIT** | 34 | 蓝图**节点图**增删连（`add_node` / `connect_nodes` / `create_variable`）+ 世界搭建宏工具（`create_town` / `construct_house` / `create_maze` / `create_castle_fortress`） |
| **CodeScape**（iolaCorp） | B | **5.7+** | 原生 C++ MCP server，默认 `:8765`（另提供 stdio 的 `CodeScapeMCP.exe`；两处口径不一致） | **Fab 买断：$99.99 / $199.99**（Personal / Professional，同一插件） | **322** / 79 个引擎系统 | 覆盖最广（蓝图 34 + **NodeForge 真编译器**、GAS、PCG、Niagara、MetaSounds、Sequencer、渲染 15）；11 家 provider + 自定义 OpenAI 兼容端点；每动作 `FScopedTransaction` 撤销 + 写前自动备份；editor-only |
| **Nwiro**（Leartes Studios） | B | Pro 5.6+ / Integration Kit 5.6+ | `127.0.0.1:5353/mcp`，Pro 默认自动开服 | **Pro**：Fab 插件免费 + 按量计费（credits / 订阅）；**Kit**：Fab 买断（2026-08 第三方汇总 $49.99） | 209–224 / 37 个领域 | **能导入官方 UE 5.8 原生工具**（命名空间 `ue.*`），可与官方 MCP 并存；自定义工具只需给 UFUNCTION 打 `meta=(NwiroTool=…)`；Pro 含 Biome 生成与 3D 生成 |
| **Remote Control 类**（`FFZackFair92` / `ziggymar` / npm 包等） | C | FFZackFair92 声明 **5.0+**（实测 5.8）；ziggymar 5.6/5.8 | MCP stdio → Remote Control API `:30010` / 本地 TCP | 多为 **MIT**（个别 MPL-2.0） | 13 – 280 不等（FFZackFair92 162 + 5 resources） | 不用编译任何 C++ 插件；`execute_python` 可触达 `unreal` 模块能碰到的一切；ziggymar 走**只读优先 + 持久索引**（分层摘要、差量编辑）省 token |

> **官方插件的实测事实见 §2.1 / §2.2**（本机已跑通无头握手），此处不重复。

#### 官方 vs 第三方：结构性差异（不是"工具多少"的差别）

| 维度 | 官方 Unreal MCP | 第三方（路线 B / C） |
|---|---|---|
| 协议归属 | **第一方**，跟着引擎版本走 | 各自实现，**跟着作者节奏走** |
| 扩展方式 | `Toolset Registry` 是**开放契约**：项目自己写 C++/Python 工具集 | 改它的源码 / 提 PR / 等更新；CodeScape、Nwiro 另给自家标签体系（`NwiroTool` 等） |
| 进程数 | MCP server 在编辑器**进程内** | 普遍**多一个进程**（Node / Python 桥）+ 端口 |
| 引擎版本绑定 | 5.8 起 | **多数卡在 5.7 或更早**（UnrealClaude 5.7、Flopperam ≤5.7），本项目是 **5.8.3** ⇒ 直接不匹配 |
| 安全 | 仅环回、**无认证层**、官方明示"不要暴露到本机之外" | 同为环回；CodeScape / Nwiro 默认关服 + 会话 token |
| 线程模型 | 工具在 **game thread 串行执行**，客户端不得重叠调用 | 同样受 game thread 限制（这是引擎约束，不是产品差异） |
| 成本 | **0**（随引擎） | $0 – $199.99 买断，或按量计费 |
| 可审计性 | 引擎源码可见（需源码许可） | 闭源商业插件（CodeScape 17.5 万行 C++）无从审计 |

**各家的真优势**（官方目前没有、或做得不如它）：
**视口截图回读**（UnrealClaude —— 让 AI 能看到自己刚摆的东西）、**蓝图节点图细粒度编辑**（Flopperam / CodeScape NodeForge）、**世界搭建宏工具**（Flopperam 的 `create_town` 一类）、**写前自动备份 + 全量撤销栈**（CodeScape）、**只读优先 + 持久索引**（ziggymar，专治"蓝图 dump 塞爆上下文"）、**导入官方工具并可共存**（Nwiro）。

**共同的代价**：① 都要求你**放弃一部分确定性** —— 多一个进程、多一层桥、多一份与引擎版本耦合的二进制；② 都会**写 .uasset**（不可 diff 的二进制），出问题只能靠撤销栈或备份；③ 商业方案的**订阅/计费**与"数据在哪"（Nwiro 明说服务托管在土耳其与德国）需要单独评估。

#### 对本项目的裁决

**继续只用官方，不引入第三方**，三条理由：

1. **硬约束**：本项目不引入计划外第三方插件 / 中间件（[`../tech/tech.md`](../../docs/TECH.md) §3 依赖白名单化、[`START.md`](../../docs/README.md) §6 禁止事项）。
2. **够用**：本项目"逻辑在 C++、资产由代码生成、回归走无头 `bench`"（§2.4），编辑器操作里的刚需只有"看选中 Actor / 调材质实例 / 跑自动化测试"，官方出厂工具集已覆盖。
3. **红线**：任何 MCP **不得成为构建 / 回归链路的依赖**（§2.4）—— 引入第三方桥只会把这条线拉得更脆。

**例外触发条件**（满足任一再重新评估，且必须由用户拍板）：

- 出现**确定性**需求要让 AI 读写 `.uasset`（例如 `game/BLUEPRINT-GUIDE.md` 落地后要**批量生成 / 批改蓝图**）——此时官方出厂工具集不够；
- 需要"AI 截图自审 UI"这类闭环（官方暂无视口回读）。

若触发，**优先级**是：① 先用官方 `Toolset Registry` **自制工具集**（§2.5，Python 优先，零新增依赖）→ ② 仍不够再评估**买断制、可离线**的方案（CodeScape 或 Nwiro Integration Kit，二者都不按量计费、都支持本地模型），并同步登记进 [`../tech/tech.md`](../../docs/TECH.md) 与本文档。

**先划清边界**——最容易踩的坑是以为 VS Code 的设置同步能带走 CodeBuddy：

| 要搬的东西 | 物理位置 | 谁负责 | 是否随 git |
|---|---|---|---|
| 用户设置 / 快捷键 / 片段 / 任务 / **扩展清单** | `%APPDATA%\Code\User\*` | **VS Code 内置 Settings Sync**（云端、跟随账号） | ✗ |
| 工作区级配置（编码、排除、C++、任务、调试） | 仓库 `.vscode/*` | **git** | ✅ |
| 推荐扩展清单 | 仓库 `.vscode/extensions.json` | **git**（打开仓库时 VS Code 自动提示安装） | ✅ |
| CodeBuddy 插件集 / 权限模式 / 记忆开关 / MCP | `~/.codebuddy/{settings,mcp}.json` | **仓库模板** + `tools/ai-config/sync-ai-config.ps1` | 模板 ✅ / 实际 ✗ |
| CodeBuddy 的密钥（`gateway.password`、`CODEBUDDY_API_KEY`） | 同上 / 用户级环境变量 | **刻意不入库**，换机人工补 | ✗ |

### 两步装机

1. **云端（VS Code）**：左下角账户图标 → `Turn on Settings Sync…` → 登录 GitHub / Microsoft → 勾选 Settings / Keybindings / Snippets / Extensions / Tasks。
   - 已预置 `settingsSync.keybindingsPerPlatform: true` 与 `settingsSync.ignoredSettings`（`jdk.jdkhome`、`terminal.integrated.profiles.windows` 等**机器相关项不同步**，避免"在 A 机改好、B 机被覆盖成错的"）。
   - 兜底：打开仓库 → `extensions.json` 提示安装推荐扩展，或 `tools\env\vscode-extensions.ps1 -InstallRecommended`。
2. **仓库（CodeBuddy）**：`powershell -ExecutionPolicy Bypass -File tools\ai-config\sync-ai-config.ps1 -Apply`
   - 模板：`tools/ai-config/codebuddy-settings.json`、`codebuddy-mcp.json`。
   - 合并语义：**只覆盖模板声明的键，本机多出来的键保留**（不会误删未知配置）；写入前自动备份为 `*.bak-<时间戳>`。
   - 跑完会列出仍需人工补的密钥类项；在 IDE 里登录一次即可写入。
   - 不带开关跑 = **只读对比**（逐项列出「本机 / 模板」差异，顺带报告 Settings Sync 是否已开启）。
   - 反向：`-Export` 把本机配置导回模板，自动剥离密钥（`gateway`）与运行态（`codebuddy`）—— 导出后 `git diff` 复核再提交。

### 引擎路径为什么也在这条线上

`.vscode/{c_cpp_properties,launch,settings}.json` 里的引擎路径写成 `${env:SOLSTICE_UE_ENGINE_ROOT}`（而非本机绝对路径），
因此这三个文件**与主机无关、可安全入库**，换机只需 `tools\env\configure-vscode.ps1` + 重启 VS Code，
不会出现"提交了 A 机路径、B 机 IntelliSense 全红"的情况。原因与旧做法的三个副作用见 本文 § §2。

---


| 方案 | 原因 |
|---|---|
| 任何"运行期调用 AI"的集成 | CHARTER.md §2 明确非目标 |
| 多套 AI 助手并存（Copilot / Claude Code 扩展等） | 多套"是否确认执行"机制互相叠加；本文 § §3.6 |
| 把第三方 UE MCP 当作构建 / 回归依赖 | 破坏 headless 与 CI 的可靠性 |
| 为 MCP 而 MCP（在 M0 阶段就投入工具集开发） | 实验性 API 会变，收益滞后于成本 |

---

## 10 更新触发

| 改动 | 同次必须更新 |
|---|---|
| 工具链路径 / 引擎版本 | 本文件 §1–§2 + `tools/lib/Solstice.Common.ps1` 的 `Find-UnrealEngine` |
| `.vscode/*` 任一文件 | 本文件 §4 + [`../tech/tech.md`](../../docs/TECH.md) §4 |
| 扩展增删 | 本文件 §4 + `.vscode/extensions.json` |
| MCP / 插件集变化 | 本文件 §8–§9 + [`../charter/CHARTER.md`](../../docs/CHARTER.md) 一行 |
