---
id: OPS
title: 构建与测试
status: active
---

# 构建与测试（OPS）

> 一切"怎么把它跑起来、怎么看它对不对"。装机细册（VSCode / MCP / 扩展 / 排错）在 [`tools/docs/ENVIRONMENT.md`](../tools/docs/ENVIRONMENT.md)。

---

## 1 前置（本机）

| 项 | 值 |
|---|---|
| 引擎 | `D:\Epic Games\UE_5.8`（5.8.3） |
| VS 生成工具 | `D:\VS2022`（MSVC 14.44 / SDK 10.0.22621） |
| DDC | `UE-LocalDataCachePath=D:\UE-DDC` |
| 必备补丁 | `.NET Framework 4.6.2 SDK`（装到 `D:\_toolchain\AutoSDK\...`，否则 UBT 直接报错） |

---

## 2 从零到能玩（五步）

```powershell
powershell -ExecutionPolicy Bypass -File tools\env\find-ue.ps1            # 引擎在不在
powershell -ExecutionPolicy Bypass -File tools\build\gen-projectfiles.ps1 # 生成 sln
powershell -ExecutionPolicy Bypass -File tools\build\build.ps1            # 编译（已默认 -NoUBA）
powershell -ExecutionPolicy Bypass -File tools\build\gen-assets.ps1       # 生成 M_Cel / M_Ink / M_Sky / M_Sea
powershell -ExecutionPolicy Bypass -File tools\run\launch-editor.ps1      # 起编辑器
```

编辑器打开后**视口是黑的，这是设计**——世界由 `ASolsticeGameMode::StartPlay()` 运行时生成。按 `Alt+P` 才看得到。

**三个坑**：
1. 本机编 UE **必须关 UBA**（`build.ps1` 已默认）；否则 PCH 报 `C1083 拒绝访问`。
2. **不要直接跑 `UnrealBuildTool.exe`**（本机只有 .NET 8/9 → `EXIT=150`）。
3. 新建 `.ps1` **必须 UTF-8 带 BOM**。

---

## 3 三条常用命令

| 命令 | 用途 | 期望输出 |
|---|---|---|
| `python tools/checks/check-links.py` | 文档相对链接 | `broken links : 0` |
| `python tools/checks/validate-contracts.py` | 6 对契约 / id / 双语 | `errors : 0` |
| `powershell -File tools\run\bench.ps1 -Days 2` | 无头确定性 | `digest A == digest B`，`bench: ok` |

`bench` 额外会打印世界骰摘要（原型 / 气候 / 目标海面 / landR / relief / 省数 / 峰高）与种群普查——**"有没有水""山多高"从看图变成一行数字**。

---

## 4 抓帧（不要靠眼睛判断灰不灰）

```powershell
powershell -File tools\run\shot.ps1 -Preset 0 -Delay 5            # 默认 12:00
powershell -File tools\run\shot.ps1 -Preset 0 -Minute 1080 -Delay 5  # 黄昏
powershell -File tools\run\shot.ps1 -Preset 0 -Minute 60 -Delay 5    # 夜
```

- 输出 `tmp/review/shot.png`；日志里的 `[shot] box=` 是世界圆盘的屏幕包围盒，脚本自动喂给 `measure-frame.py` 判 PASS/FAIL。
- **别自己 crop 全帧**：深蓝背景会把亮度中位拉到 0.44，造成假通过。
- `-Seed` 默认 20261016，与 bench 同种子——**换种子等于换世界，指标不可比**。
- 判定分档按时段（见 [ART.md](ART.md) §6），用正午口径量黄昏会把诚实的黄昏判成失败。

---

## 5 键位

**操作语义**：**上 = 用出去，下 = 收回来。**

| 组 | 键 | 作用 |
|---|---|---|
| 手牌 | `1`–`0` / `Q`·`E` | 选卡 / 切槽 |
| | `空格` / 左键 | 释放到光标处 |
| | 右键 | 切「生效即散 / 落地成实体」 |
| | `X` / `R` | 回收 / 换一手 |
| 4K 抓帧 | `shot.ps1 -Width 3840 -Height 2160` | 脚本已带 `-ForceRes`；不带则引擎用存档窗口尺寸（实测 888×500） |
| 视角 | `F1`–`F4` | 四个俯仰档：14° 低角 / 35° 标准 / 62° 巡界 / 88° 图纸 |
| | `V` / `[` `]` | 循环俯仰 / 转 45° 方位（8 档） |
| | 滚轮 / 中键拖动 | 推拉（中键纵向 = 推拉，**不是环绕**） |
| | `Y` | 档间巡游：每档停 3 s，0.8 s 过渡 |
| 时间 | `T` / `K` / `J` | 虚拟·现实 / 暂停 / 倍速 |
| 面板 | `H` `C` `B` `L` `G` `O` | 帮助 / 年表 / 开包 / 卡库 / 创世 / 语言 |
| 其他 | `P` / `Tab` / `I` / `Esc` | 摆件模式 / 显隐界面 / 收起读数 / 退出 |
| 调试 | `F5`–`F8` / `N` | 节奏 / 开包 / 生态预设 / 播种 |

> 相机只有 **8 方位 × 4 俯仰 = 32 个格点**，距离连续。**没有连续环绕入口**——中键只推拉。
> `Esc` 在 PIE 里是空操作（以前会 `ConsoleCommand("quit")` 把编辑器一起关掉）。

---

## 6 测试流程

### 6.1 冒烟（3 分钟，每次改完必跑）

1. `Alt+P`，等 3 秒。
2. `F1` 全景：轮廓是半圆吗？最下面只有一条细暗边吗（无侧壁 / 无圆柱 / 无托板）？
3. 左面板：天气 / 温度 / 湿度 / 生态七项有数。
4. 底部手牌：卡名不放大能读出来吗？
5. `1` → `空格`：卡能释放吗？
6. `F3` 低角：有露空背景 / 穿帮吗？

### 6.2 全项（15–20 分钟，里程碑前）

| 段 | 动作 | 看什么 |
|---|---|---|
| 形态 | `F1`→`F4` | 四机位轮廓都成立；底部厚度始终是一条暗边 |
| 地平线 | `Y` 转一圈 | 界外是雾不是空背景；边界是渐变不是壳 |
| 卡 | 选 → 释放 → 回收 → 换手 | 三条链路都通；卡面字可读 |
| 时间 | `T` / `K` / `J` | 现实模式时间控件应隐藏；倍速不崩 |
| 面板 | `H` `C` `B` `L` `G` | 帮助默认关；面板内容不空 |
| 天气 | 切雨 / 夜 | 天空能看出来，不是只有数字；月不落地、不贴顶 |
| 摆件 | `P` → `Tab` | 隐藏 UI 可复原 |

### 6.3 回归（改完对照上一轮）

固定三帧、**同机位同分辨率**，存 `tmp/review/<日期>_<主题>_<机位>.png`：

1. `F1` 全景 · 开局第 10 秒
2. `F3` 低角 · 开局第 10 秒
3. `F1` 全景 · 释放一张地形卡之后

三帧不能逐帧对比 = 机位或时机没定住，这轮结果不作数。

---

## 7 重置

| 想要 | 怎么做 |
|---|---|
| 换一个新世界 | 退出重进（`world.json` 的 `seed: 0` = 每次开局摇新世界） |
| 复现指定世界 | 加 `-SolsticeSeed=20261016`（`shot.ps1 -Seed`） |
| 重置材质 | 删 `game/Content/Core/*.uasset`，跑 `gen-assets.ps1` |
| 重置世界状态 | 重进 PIE（当前无存档） |

---

## 8 常见故障

| 症状 | 原因 | 处理 |
|---|---|---|
| 编译 `C1083 拒绝访问`（PCH） | UBA 的文件系统 detour | 确认 `build.ps1` 带 `-NoUBA` |
| `EXIT=150` | 直接跑了 `UnrealBuildTool.exe` | 用引擎自带 dotnet 挂 `UnrealBuildTool.dll` |
| `could not find the class ... commandlet` | 模块 `LoadingPhase` 不是 `Default` | 改 `.uproject` 并重编 |
| 天空纯黑 | 改了 `r.SupportSkyAtmosphere` 这类 permutation 开关但没重编 | 回退开关或重编 permutation |
| 材质改了但画面没变 | 资产没重建 / 参数没人读 | 跑 `gen-assets.ps1`；`check-materials.py` 查 RecipeVersion |
| 编辑器"闪退" | 很可能是 PIE 里按了 `Esc`（G-10） | 查日志 `UUnrealEdEngine::CloseEditor()` |
| 抓帧分辨率不对 | PIE 落在编辑器视口内 | 用 `shot.ps1`（`-game -RenderOffscreen`），不用 MCP 截 PIE |
