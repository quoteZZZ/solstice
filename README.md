# Solstice

> 一盒**会自己活起来的纸雕微缩世界**——你可以让它与真实时间同频（天气、昼夜、季节皆真），也可以把时间握在手里；你攒下的「余响」开出的元素包决定这次能怎样改写它；而它永远不输。
>
> 形态：**永续运行的上帝沙盒（生态界）** · **全 3D 程序化几何（纸板块面）** · 干涉卡 = 七元素 × 12 档 × 3 类（**卡种 ≥ 252**）· 无失败、无数值成长 · Steam 单机买断（Windows）。
> 工程与模块名沿用 `Solstice`；中文名可后补。

---

## 一、当前状态（2026-10-16）

| 项 | 真实情况 |
|---|---|
| 设计 | ✅ **立项已定稿**，细化到规格级 —— 唯一真源 [`docs/charter/CHARTER.md`](docs/charter/CHARTER.md)（版本号见 [`docs/INDEX.md`](docs/INDEX.md) §1 状态矩阵） |
| 文档体系 | ✅ 已按"一种事务一份文档"重建，入口：[`docs/INDEX.md`](docs/INDEX.md)（含状态矩阵与更新触发矩阵） |
| 环境脚本 | ✅ `tools/` 按用途分 7 层（`lib` / `env` / `build` / `run` / `checks` / `hooks` / `ai-config`），14 个 PowerShell + 2 个 Python 自检 + 提交钩子 + `.github/` CI —— 自检自动执行 |
| IDE 配置 | ✅ `.vscode/` 7 份 + `.clang-format` + `.editorconfig` |
| 项目数据 | ✅ `.codebuddy/agents/`（3 个子代理）与 `.codebuddy/rules/`（核心约束） |
| 工具链 | ✅ **虚幻引擎 5.8.3**（`D:\Epic Games\UE_5.8`）+ **VS2022 生成工具**（`D:\VS2022`）+ 补齐的 `.NET Framework 4.6.2 SDK`。见 [`docs/ops/ENVIRONMENT.md`](tools/docs/ENVIRONMENT.md) §1.3 |
| **游戏代码** | ✅ **M0 骨架已落地并跑通** —— `game/Solstice.uproject` + 主模块 `Solstice`（Core / Sim / Gen / World / God / UI / Render）+ 编辑器模块 `SolsticeEditor` |
| **可编译性** | ✅ **两个目标编译通过**：`SolsticeEditor`（编辑器）与 `Solstice`（游戏，无编辑器依赖，产出 `Solstice.exe`） |
| **内容可重建** | ✅ `tools\build\gen-assets.ps1` → `-run=SolsticeGenAssets` 由代码生成 `/Game/Core/M_Paper` 与 `M_Ink`（新克隆自愈，二进制不入 git） |
| **无头回归** | ✅ `tools\run\bench.ps1` → `-run=SolsticeBench`：同种子跑两遍世界内核比对结构摘要，**实测确定性成立且生态是活的** |

**M0 实测证据**（本机，`LogSolstice` 取自 `-abslog` 落盘日志）：

```text
realm realm.valley ready: 36 cards from 3 file(s), 5 species, 6 preset(s), ok
reality anchor: 上海, offline=0, weather_from_net=0
paper: /Game/Core/M_Paper.M_Paper / /Game/Core/M_Ink.M_Ink -> authored
realm built: 128x128 tiles in 4 chunk mesh(es); paper mode = authored
bench: run A 2880 world minutes in 0.50s (5773 world-min/s), population 1157, chronicle 294
bench: species A moss 0, shrub 4, grazer 1043, raptor 110, fish 0
bench: digest A 19ABB70199419F90 (147473 mixed) / digest B 19ABB70199419F90 (147473 mixed)
bench: ok - deterministic and alive
```

> 也就是说：**这个微缩世界现在能自己生成了**——地形长出来、植被把纸面染上色、放牧者和掠食者建立种群，跑一整个世界日只要 0.25 秒机时。画面还是最简的纸板色块，交互只有 HUD 骨架，但那正是 M0 该有的样子。下一步见 [`docs/plan/ROADMAP.md`](docs/plan/ROADMAP.md)。

---

## 二、立刻上手（三条）

1. **读文档**：先 [`docs/INDEX.md`](docs/INDEX.md)（3 分钟看懂现状、状态矩阵与硬规则），再 [`docs/charter/CHARTER.md`](docs/charter/CHARTER.md) §0–§3。
2. **看计划**：[`docs/plan/ROADMAP.md`](docs/plan/ROADMAP.md) §1（三阶段切分）+ §5（P0–P5 执行计划）。
3. **动手跑**：

```powershell
# 新克隆的最小闭环（前三步缺一不可）
powershell -ExecutionPolicy Bypass -File tools\env\find-ue.ps1                  # 引擎在不在
powershell -ExecutionPolicy Bypass -File tools\build\gen-projectfiles.ps1         # 生成 Solstice.sln
powershell -ExecutionPolicy Bypass -File tools\build\build.ps1                    # 编译（编辑器目标）
powershell -ExecutionPolicy Bypass -File tools\build\gen-assets.ps1               # 生成两个纸材质
powershell -ExecutionPolicy Bypass -File tools\run\bench.ps1 -Days 2            # 无头确定性回归
powershell -ExecutionPolicy Bypass -File tools\run\launch-editor.ps1            # 起编辑器
```

VSCode 中 `Ctrl+Shift+B` 运行构建任务，`F5` 用 `cppvsdbg` 调试（配置说明见 [`docs/ops/ENVIRONMENT.md`](tools/docs/ENVIRONMENT.md)）。

> **编辑器打开后视口是黑的，这是设计不是故障** —— 世界由 `ASolsticeGameMode::StartPlay()` 运行时生成，只有"播放中的世界"才有它，编辑器世界永远为空且无灯。按 **`Alt+P`**（或工具栏播放）看世界；默认落在 1920×1080 的**新编辑器窗口**里，配置与覆盖规则见 [`docs/ops/ENVIRONMENT.md`](tools/docs/ENVIRONMENT.md) §2.7。

---

## 三、环境（本机已就绪）

| 组件 | 位置 | 说明 |
|---|---|---|
| 虚幻引擎 | `D:\Epic Games\UE_5.8` | 5.8.3，含引擎源码 |
| VS2022 生成工具 | `D:\VS2022` | MSVC 14.44.35207 + Windows SDK 10.0.22621.0 |
| .NET Framework 4.6.2 SDK | `D:\_toolchain\AutoSDK\…` | `SwarmInterface` 的硬要求；**原本缺失**，补法见 [`docs/ops/ENVIRONMENT.md`](tools/docs/ENVIRONMENT.md) §1.3 |
| 生图（ComfyUI） | `D:\ComfyUI` | `http://127.0.0.1:8188` |
| 缓存外置 | `UE-LocalDataCachePath=D:\UE-DDC` | 必须设，否则吃 C 盘 |

**本机编译必须关掉 UBA**（UBA 的文件系统 detour 在本机报 access denied，会把 PCH 编译全部打挂）；`tools\build\build.ps1` **已默认追加 `-NoUBA`**，直接跑即可（想换回 UBA 加速时加 `-UseUBA`），手动直调方式见 [`docs/ops/ENVIRONMENT.md`](tools/docs/ENVIRONMENT.md) §2.4.1。

**磁盘结论**：**不需要清理 C 盘**（实测需求约 10–15 GB；`check-env.ps1` 的 120 GB 只是保守阈值）。

---

## 四、仓库结构

```text
solstice/
├─ game/        ✅ UE 工程（进发行包）——Source/{Solstice,SolsticeEditor} + Config/Solstice/{Data,Localization}
│                 └─ Content/ 二进制不入 git，由 tools\build\gen-assets.ps1 重建
├─ studio/      ✅ 生图线（ComfyUI 工作流 / jobs / 预设 / 驱动脚本）；产物 out/ 不入 git
├─ shared/      ✅ JSON 契约（schema/：card · species · world）
├─ tools/       ✅ 开发期脚本，按用途分 7 层：lib（共享库）/ env / build / run / checks（Python 自检）/ hooks / ai-config（AI 工具链配置模板）
├─ docs/        ✅ 文档体系（charter / setting / gameplay / art / tech / plan / ops / archive）→ 入口 docs/INDEX.md
├─ .github/     ✅ CI 自检（checks/ 两条，push / PR 自动跑，无需引擎）
├─ .vscode/     ✅ 构建任务 / 调试 / cpptools / 代码片段
├─ .codebuddy/  ✅ agents/ 与 rules/（项目数据，勿删）
└─ .workbuddy/  ✅ memory/（工作日志与长期项目笔记；项目数据，勿删）
```

> 定义与状态列见 [`docs/tech/tech.md`](docs/tech/tech.md) §0（含"新增东西放哪"的归位决策表）。

---

## 五、协作约定（速查，全文见 [`docs/ops/START.md`](docs/START.md)）

- **提交信息**：英文、简短祈使句；文档与代码同次提交。
- **文档纪律**：文档先行；任何文档增删改必须同次更新 [`docs/INDEX.md`](docs/INDEX.md)。
- **分工**：AI ≈ 85%（代码 / 脚本 / 文档 / 数据 / 生图调度）；用户 ≈ 15%（安装、GUI 操作、审美裁决）。
- **逻辑在 C++、表现层在蓝图**：确定性内核（模拟 / 时间 / 存档）不得依赖蓝图。
- **git 版本管理（单机）**：未提交视为未落地；`origin`（GitHub）仅作异地备份与 CI 自检（见 [`docs/ops/START.md`](docs/START.md) §5）。
- **素材合规**：AI 生成素材逐张留档（模型 / prompt / seed / 日期），满足 Steam AI 披露（见 [`docs/art/art.md`](docs/art/art.md) §2、§5）。

---

## 六、形态沿革

动态壁纸 → 场景引擎 → 卡牌收集 → 自走棋卡牌（**均已作废**）→ **当前：纸雕微缩永续生态界**。
完整裁定记录见 [`docs/charter/CHARTER.md`](docs/charter/CHARTER.md)；作废稿见 [`docs/archive/`](docs/archive/README.md)。
