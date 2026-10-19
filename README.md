# Solstice

> **一只捧在手里、会自己活起来的动画赛璐璐微缩世界。**
> 上帝沙盒：无失败、无数值成长、无目标。你只能**看**，和**施卡**。
> 你可以让它与真实时间同频（天气 / 昼夜 / 季节），也可以把时间握在手里；攒下的「余响」开出的元素包，决定这次能怎样改写它。

- **形态**：UE5.8 + C++，全 3D 程序化几何，动画赛璐璐着色
- **平台**：Steam 单机买断（Windows），无服务端 / 无账号 / 无内购
- **内容**：卡种 ≥252（7 元素 × 12 档 × 3 类）；M0 做 36

---

## 现在到哪了（2026-10-19）

| 层 | 状态 |
|---|---|
| 模拟内核 | ✅ 能跑、可复现（`bench` 两次进程 digest 一致） |
| 世界生成 | ✅ 可用（128² 实测：水域 43.6%，峰高 18–21 m） |
| 画面 | 🟡 方向已定（Cel + SSGI + TSR），**三张对照图未定标** |
| 卡 / 手牌 / 开包 | 🟡 内核通，交互骨架在 |
| 存档 / 音频 / 多界 | ❌ M1 |

---

## 跑起来

```powershell
powershell -File tools\env\find-ue.ps1             # 引擎在不在
powershell -File tools\build\gen-projectfiles.ps1  # 生成 sln
powershell -File tools\build\build.ps1             # 编译（默认 -NoUBA）
powershell -File tools\build\gen-assets.ps1        # 生成 M_Cel / M_Ink / M_Sky / M_Sea
powershell -File tools\run\launch-editor.ps1       # 起编辑器
```

编辑器视口是黑的——**这是设计**。按 `Alt+P` 才看得到世界（它由 GameMode 在运行时生成）。

```powershell
powershell -File tools\run\bench.ps1 -Days 2       # 无头确定性回归
powershell -File tools\run\shot.ps1 -Preset 0      # 抓一帧 + 自动量具判定
python tools\checks\check-links.py                 # 文档链接
python tools\checks\validate-contracts.py          # JSON 契约 / 双语
```

---

## 文档

**入口：[`docs/README.md`](docs/README.md)**（现状 / 文档地图 / 六条红线 / 开工顺序）

| 文档 | 管什么 |
|---|---|
| [docs/CHARTER.md](docs/CHARTER.md) | 为什么做、不做什么、否决过什么 |
| [docs/WORLD.md](docs/WORLD.md) | 世界形态、尺度、生成、生态、天气 |
| [docs/CARDS.md](docs/CARDS.md) | 卡、手牌、余响、开包、回收 |
| [docs/ART.md](docs/ART.md) | 画风、色板、着色配方、机位、资产 |
| [docs/TECH.md](docs/TECH.md) | 架构、依赖、预算、目录、命名、契约 |
| [docs/PLAN.md](docs/PLAN.md) | 里程碑、验收、当前批次、已知缺陷 |
| [docs/OPS.md](docs/OPS.md) | 构建、跑分、抓帧、试玩、排错 |

M1 及以后：[`docs/LATER.md`](docs/LATER.md)（一页指针，M0 期间不读）。

---

## 仓库结构

```text
solstice/
├─ game/        UE 工程：Source/{Solstice,SolsticeEditor} + Config/Solstice/{Data,Localization}
│                Content/ 二进制不入 git，由 tools\build\gen-assets.ps1 重建
├─ studio/      生图线（ComfyUI 工作流 / jobs / 预设 / 驱动脚本）；产物 out/ 不入 git
├─ shared/      JSON 契约（card / species / world / save）+ 色板
├─ tools/       开发脚本，按用途分 7 层（lib / env / build / run / checks / hooks / ai-config）
├─ docs/        9 份现行文档 + archive/（历史，无约束力）
├─ .github/     CI：文档链接 + 契约自检（不需要引擎）
├─ .vscode/     构建任务 / 调试 / cpptools
├─ .codebuddy/  agents/ 与 rules/（项目数据，勿删）
└─ .workbuddy/  memory/（工作日志；项目数据，勿删）
```

---

## 环境（本机）

| 组件 | 位置 |
|---|---|
| 虚幻引擎 5.8.3 | `D:\Epic Games\UE_5.8` |
| VS2022 生成工具 | `D:\VS2022`（MSVC 14.44 / SDK 10.0.22621） |
| .NET Framework 4.6.2 SDK | `D:\_toolchain\AutoSDK\…`（缺了 UBT 直接报错） |
| 着色器缓存 | `UE-LocalDataCachePath=D:\UE-DDC` |
| 生图 | `D:\ComfyUI` → `http://127.0.0.1:8188` |

**本机编 UE 必须 `-NoUBA`**（`build.ps1` 已默认）。装机与排错细册：[`tools/docs/ENVIRONMENT.md`](tools/docs/ENVIRONMENT.md)。

---

## 沿革

动态壁纸 → 场景引擎 → 卡牌收集 → 自走棋 → 纸雕微缩 → **动画赛璐璐微缩生态界（当前）**。
作废形态与旧稿在 [`docs/archive/`](docs/archive/README.md)，**无约束力**。
