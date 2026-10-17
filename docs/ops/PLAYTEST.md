---
id: ops/PLAYTEST
title: 游玩与测试指南
status: active
truth: ops
owner: both
---

# 游玩与测试指南

> 谁用：自己反复试玩、以及每次改完之后做回归的人。
> 目的：**同一套动作重复执行、每次结果可比**。不确定的操作一律先查本文，不要临时发挥。
> 键位的真源是 [`../gameplay/UI-UX.md`](../later/UI-UX.md) §2；本文只做「按顺序怎么按」。

## 1 启动

| 方式 | 命令 | 说明 |
|---|---|---|
| **编辑器（常用）** | `powershell -File tools/run/launch-editor.ps1`，或直接开 `game/Solstice.uproject` | 打开后按 **`Alt+P`** 开始播放 |
| **VS Code 调试** | VS Code 里 `F5`（`cppvsdbg`） | 断点调试用，见 [`ENVIRONMENT.md`](../../tools/docs/ENVIRONMENT.md) |
| **无头回归** | `powershell -File tools/run/bench.ps1` | 不开窗口，跑 `SolsticeBench` 输出数值 |

**三条必知**：

1. **编辑器视口全黑是正常的**——世界在 `ASolsticeGameMode::StartPlay()` 里运行时生成，编辑器世界永远是空的、也没有灯。看到黑就按 `Alt+P`。
2. 播放落在**新编辑器窗口**里，默认 1920×1080；改分辨率/覆盖规则见 [`ENVIRONMENT.md`](../../tools/docs/ENVIRONMENT.md) §2.7。
3. 每轮改完**必须重启 PIE**（`StopPIE` → `Alt+P`）才能吃到新的生成逻辑，热重载不重建世界。

## 2 一局是怎么开始的

开局数据来自 `game/Config/Solstice/Data/world.json` 的 `default` 段：

| 字段 | 当前值 | 意思 |
|---|---|---|
| `size` | **128**（**文件现值**） | 规格默认档是 **512**（1 格 = 1 m → 界边长 512 m），但**当前 `world.json` 写的是 128**——施工侧为跑分 / 帧率临时降档，改回 512 要重新实测（`R-32`）。**测试时以文件里的数字为准**，别按 512 的预期看画面 |
| `algo` | `ridges` | 地形算法（M0 共 6 种：`ridges` / `islands` / `canyon` / `plateau` / `basin` / `archipelago`）。**骰子化尚未接入**，见 `R-30` |
| `seed` | **0** | **0 = 每局随机掷一个**（[`../setting/WORLD.md`](../setting/WORLD.md) §2.4）；填具体数字 = 钉死那一个世界 |
| `latitude` | 31.23 | 纬度，决定太阳高度角、季节幅度与昼夜长短 |
| `base_water_level` | 6.0 | 基准水位（米） |
| `tides` | false | 潮汐开关；打开后岸线随日周期进退 |

> **开局手牌 10 张**（0 环，保底含地形类 + 天气类各 ≥1）；**余响 0**。
> **世界骰六轴参数**（`land_ratio` / `relief` / `aridity` / `weather_bias` / `sky` / `start_season` 等，取值域见 `WORLD.md` §2.4）**规格已定、数据字段尚未接入**——当前开局只受上表字段控制。缺口记 [`../plan/work.md`](../plan/work.md) `R-30`。
> **`chaos_start` / `life_seed` 两个字段已删除**（第 54 轮）：世界开局即有完整生态，不存在"混沌"与"播种"开关。

### 2.1 本轮实测环境（2026-10-17 14:19，128²）

这几条不是缺陷，是**当前跑起来的事实**——不知道它们会误判成 bug：

| 现象 | 实情 | 依据 |
|---|---|---|
| **世界钟几乎不动** | 默认跑在**真实时间模式**（`reality anchor: 上海, offline=0`），4 秒真实时间世界钟还停在同一分钟。想看天气 / 昼夜变化，要么真的等，要么在创世面板切虚拟时间 | `run1_a.png` / `run1_b.png` 两帧世界钟都是 14:15 |
| **看不到种子码** | HUD 只有「世界 / 纪年 / 季节 / 天气」，没有 `SOL-XXXX-XXXX`。**两局是不是同一个世界，现在没法判断**（`plan/work.md` `R-30`） | — |
| **画面全灰白** | 不是显示器问题：渲染材质还是 `M_Paper`（日志 `paper mode = authored`），生态色没进材质链（`R-31`） | `LogSolstice` 日志 |
| **雾会突然糊满全屏** | 云量被算进了雾密度（`Wet = Rain*0.7 + Cloud*0.3`），多云就等于起雾（`R-35`） | `live_02.png` → `live_03.png` |
| **低洼处有黑椭圆** | 水面渲染问题（`R-34`），不是洞穴也不是阴影 | `live_03.png` |

> **截图命名约定**：`tmp/review/run<N>_<a|b>.png`（同一局的前后帧同编号），`live_<NN>.png` 留作现场快照。写工单时**必须带文件名**，否则下一轮没法对照。

### 2.2 第二轮实测事实（2026-10-17 14:45，128²）

同样是"看起来像 bug、其实是当前状态"的东西：

| 现象 | 实情 | 依据 |
|---|---|---|
| **画面上半部分是纯灰、没有天空** | **不是渲染失败，是穹顶根本没建**：`BuildSkyMesh` 里 `Gen::BuildSkyDome(...)` 被注释掉了（`World/SolsticeRealm.cpp:286`），等半透明天空材质 `M_Sky`。所以别报"天空颜色不对" | `r2_01.png` |
| **世界轮廓是不规则的撕边，像手撕的纸** | 故意的：`Gen::TornRadius()` 给半径叠了 4.6% 的三阶谐波。但**现行规格已经不要这个做法**（`WORLD.md` §1.1 圆盘 + `art.md` §0.5 红线 4），代码引的是旧口径 → `R-40` | `r2_01.png` / `r2_base.png` |
| **世界读成一只碗：四周高、中间低** | 山高 `MaxHeightMeters=64` = **1.0R**（规格上限 0.55R），且 `basin` 剖面把径向距离直接加进高度 → `R-38` | `r2_base.png` |
| **按 Esc，整个编辑器消失了** | 不是崩溃：`Esc` 走 `ConsoleCommand("quit")`，在 PIE 里退的是编辑器。日志是 `LogExit: Exiting.` + `reason: UUnrealEdEngine::CloseEditor()` → `R-39` | `game/Saved/Logs/` |
| **滚轮推到最远就推不动了** | 128² 下 `Fit=36017`，`Zoom` 上限 60000 = **1.67 × Fit**，撞顶后没有任何反馈。512² 更糟（`Fit≈144000`，上限只有 0.42×Fit）→ `R-36` | 日志 `[diag] cam: dist=60000` |
| **从全景缩到贴地要滚 20 多下** | 每档 12%；实测玩家滚到 `dist=27864` 就停手了。改中键推拉时一并解决（`D-10`） | 日志 `[diag] cam` 序列 |

> **别用眼睛判断"画面灰不灰"**——用 `python tools/checks/measure-frame.py <png> --box <世界区域>`，它按 `art/art.md` §2.1 给三项 PASS / FAIL。
> 2026-10-17 基线（`r2_01.png`，box `330,150,760,430`）：**饱和度 0.095 / 亮度中位 0.910 / 高光占比 0.597 —— 三项全不达标**（要 > 0.25 / 0.45–0.60 / < 0.10）。修 `R-41` 之后重跑这一行，数字变好才算真的变好。

## 3 键位速查

**手牌**（核心：上＝用出去，下＝收回来）

| 键 | 作用 |
|---|---|
| `1`–`0` / `Q`·`E` | 选卡 / 前后切槽 |
| `空格` / 左键 | 释放到光标处 |
| 右键 | 切换「生效即散 / 落地成实体」 |
| `X` / `R` | 回收当前卡 / 换一手 |

**视角**

| 键 | 作用（**= 2026-10-17 代码实际行为**） |
|---|---|
| 中键拖拽 | 环绕（方位连续，俯仰 4°–88°） |
| `WASD` / 滚轮 | 平移 / 比例缩放（一档 12%） |
| `F1`–`F4` | 全景 35° · 卫星 76° · 低角 14° · 顶视 87° |
| `V` | 循环 6 个机位（含上一个的表内四项） |
| `Y` | 自动环绕开关（3.5°/秒） |

> ⚠️ **这张表记录的是"代码现在怎么响应"，不是"最终怎么响应"**。`CHARTER` 第 59 轮已定案改离散机位，**代码还没跟进**：改完之后中键变推拉、`[` `]` 转 45°、`V` 只走 4 个俯仰档、`Y` 改档间巡游（规格 [`../art/art.md`](../art/art.md) §2.6，施工 [`../plan/work.md`](../plan/work.md) `D-10`）。
> 现在按旧表测出来的"转过去看见穿帮"，**不要当成新缺陷报**——`R-19`/`R-34` 已经记着这类问题了。

**时间与面板**

| 键 | 作用 |
|---|---|
| `T` / `K` / `J` | 虚拟·现实模式 / 暂停 / 倍速（仅虚拟模式生效） |
| `H` `C` `B` `L` `G` `O` | 帮助 · 年表 · 开包 · 卡库 · 创世 · 中英切换 |
| `P` / `Tab` | 摆件·截图模式 / 显示隐藏界面 |
| `F5`–`F8` / `N` | 真实节奏 / 开包（随机·特殊）/ 生态预设 / 播种生命 |
| `Esc` | 退出 |

> `Z` 撤回**已废**（第 49 轮 A-02）：没有撤销是硬红线，3 秒未干期只是「纸还湿着」。

## 4 标准测试流程

### 4.1 冒烟（3 分钟，每次改完必跑）

1. `Alt+P` 开局，等 3 秒（世界生成 + 预热）。
2. `F1` 全景：**轮廓是不是半圆**（上弧 + 底部水平切线）？**最下面是不是只有一条细暗边**——看不出厚度、看不出侧壁、更没有圆柱 / 托板？（这一条最容易做错：地一厚就读成山，玩家会以为自己在山上看石台）
3. 看左面板：天气、温度、湿度有数；生态七项有数。
4. 看底部手牌：卡名能不能不放大就读出来？
5. 按 `1` → `空格`，卡能不能释放出去。
6. 按 `F3` 低角：有没有露空背景 / 穿帮。

六条过 = 冒烟通过。

### 4.2 全项（15–20 分钟，里程碑前跑）

| 段 | 动作 | 看什么 |
|---|---|---|
| **形态** | `F1`→`F2`→`F3`→`F4` 依次切 | 四机位下轮廓是否都是半球、底部是否平；**底部厚度是否始终只是一条暗边**（`0.03R`，任何机位都不该看出侧壁）；有没有圆柱/托板/桌板 |
| **地平线** | `Y` 自动环绕一圈 | 界外是雾、不是空背景；罩的边界是**极淡的渐变**而不是实体壳 |
| **卡牌** | `1`–`0` 逐张选 → 释放几张 → `X` 回收 → `R` 换一手 | 释放/回收/重抽都通；卡面字读得清 |
| **时间** | `T` 切虚拟 → `K` 暂停 → `J` 倍速 → 切回现实 | 现实模式时间控件应隐藏；倍速不崩 |
| **面板** | `H` `C` `B` `L` `G` | 帮助默认关、按 H 才开；各面板内容不空 |
| **天气** | 切到雨天（`F8` 生态预设或等天气变） | 天空能看出雨/云，不是只有数字 |
| **夜** | 虚拟模式加速到夜里 | 月亮**悬在天空体积内、不落地**（贴顶或落地都不对）；星星界内界外都有 |
| **摆件** | `P` → `Tab` 复原 | 截图模式隐藏 UI、可退回 |

### 4.3 回归（改完对照上一轮）

固定三帧，**同机位同分辨率**，存 `tmp/review/<日期>_<主题>_<机位>.png`：

1. `F1` 全景 · 开局第 10 秒
2. `F3` 低角 · 开局第 10 秒
3. `F1` 全景 · 释放一张地形卡之后

不满足「三帧能逐帧对比」，说明机位或时机没定住，这一轮结果不作数。

## 5 自动化截帧（可选）

编辑器开着时，MCP 可以直接截当前窗口，省手工：

```
python tmp/ue_mcp.py capture tmp/review/live_01.png              # 截整个编辑器
python tmp/ue_mcp.py call IsPIERunning '{}' "EditorToolset.EditorAppToolset"
python tmp/ue_mcp.py call StartPIE '{"options":{"bSimulate":false,"playMode":"PlayMode_InViewPort","warmupSeconds":5}}' "EditorToolset.EditorAppToolset"
python tmp/ue_mcp.py call StopPIE '{}' "EditorToolset.EditorAppToolset"
```

已知限制：截的是**整个编辑器窗口**（含面板与工具栏），尺寸跟窗口走；PIE 内的 `CaptureViewport` 拿不到画面（见 `plan/work.md` `R-14`）。

**编辑器被关掉 / 被沙箱连带杀掉之后怎么重拉**（`WMI` 已被安全策略拦，`Start-Process` 起的进程会跟着工具调用一起被回收；一次性计划任务能脱离进程树）：

```powershell
$exe = 'D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$arg = '"D:\GitCode\solstice\game\Solstice.uproject" -ModelContextProtocolStartServer -ModelContextProtocolPort=8000'
$action = New-ScheduledTaskAction -Execute $exe -Argument $arg -WorkingDirectory 'D:\GitCode\solstice'
$trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddYears(1)
Register-ScheduledTask -TaskName 'SolsticeEditorMcp' -Action $action -Trigger $trigger -Force | Out-Null
Start-ScheduledTask -TaskName 'SolsticeEditorMcp'
Unregister-ScheduledTask -TaskName 'SolsticeEditorMcp' -Confirm:$false   # 起来了就删，别留
```

编辑器启动约 30–60 秒后 MCP 才应 `POST /mcp`；探针返回 HTTP 200 即就绪。
**日志最后一行是 `LogExit: Exiting.` = 正常退出**（不是崩溃），别去翻崩溃报告。

## 6 重置与重复游玩

| 想重置什么 | 怎么做 |
|---|---|
| **换一个世界**（推荐，每次都不一样） | `StopPIE` → `Alt+P`（`seed: 0` → 每局掷新骰） |
| **复现某个世界** | 把 `world.json` 的 `default.seed` 填成那局的数字 → 重启 PIE（同 `size` + 同 `algo` + 同代码） |
| 换地形算法 | 改 `default.algo` → 重启 PIE |
| 换尺寸 | 改 `default.size`（128 / 256 / 512）——**512 是默认档，首帧与帧率尚未实测**，卡就先降到 256 |
| 清掉 UI 干扰截图 | `P`（摆件模式）；`Tab` 复原 |

> **可重复性靠 seed 三元组**：同一 `seed` + 同一 `size` + 同一份代码 = 同一个世界。报问题时要带上这三项。
> **每局世界不同的前提是 `seed: 0`**；一旦为了复现填了数字，之后每次开局都会是同一个世界——测完记得改回 0。

## 7 记录模板

每轮填一行，贴在 issue / 提交说明里：

| 字段 | 填什么 |
|---|---|
| 版本 | 提交号或 `docs/INDEX.md` 的版本号 |
| 世界 | `algo` / `seed` / `size` |
| 机位 + 时机 | 例：`F3` 低角 · 开局 10 s |
| 通过 / 不通过 | 按 §4.1 六条 |
| 异常 | 现象 + 截帧文件名 |
| 对应工单 | `plan/work.md` 的 `R-nn`（没有就新开一条） |

## 8 当前这一版能玩到什么

- **能**：开局生成一座界（**512² 默认档**，每局随机种子）；四机位看；自动环绕；释放 / 回收 / 换手牌；开包；年表 / 卡库 / 创世面板；时间模式切换；中英切换；摆件模式。
- **不能 / 还在做**：世界骰六轴参数与种子码输入（`R-30`）、开局成种与去混沌（`R-31`）、尺寸真尺度化（`R-29`）、512² 的帧率与跑分（`R-32`）；半球与大气边界（`R-19`）、云与天气视觉（`R-20`）、日月星（`R-21`）、面板可读性（`R-03` / `R-04`）——都以 [`../plan/work.md`](../plan/work.md) 为准。

