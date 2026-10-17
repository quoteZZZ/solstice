# `game/Content/` — 二进制资产（不入 git）

本目录的 `.uasset` / `.umap` 及媒体文件**不入 git**（见仓库根 `.gitignore` §2 与
[`docs/tech/tech.md`](../../docs/tech/tech.md) §0 的核心不变量）。目录结构本身入 git，
所以每个子目录都放一个 `.gitkeep`。

## 为什么可以不入 git

全部内容必须能由「**种子 + 生成器 + 契约 + manifest**」重新长出来：

| 子目录 | 放什么 | 由谁生成 |
|---|---|---|
| `Core/` | 纸雕母材质 `M_Paper` / 描边墨材质 `M_Ink` | 跑 **`tools\build\gen-assets.ps1`**（内部是 `-run=SolsticeGenAssets`），由 `Source/SolsticeEditor/Private/SolsticePaperMaterialBuilder.cpp` 按代码建材质图并保存 |
| `World/` | 地形纸板 / 岩石 / 水体 / 植被模板 | 目前**不需要**：地形、水、纸雕穹顶全部由 `Gen/` 程序化生成 |
| `Cards/` | 卡面贴图 | `studio/`（ComfyUI 生图线），登记在 `studio/out/manifest.jsonl` |
| `UI/` | 图标 / 九宫格 | 目前**不需要**：HUD 是纯 C++ Slate，无 UMG 资产 |
| `Data/` | DataTable / DataAsset | 目前**不需要**：运行时直接读 `Config/Solstice/Data/*.json` |
| `Levels/` | 关卡 | 目前**不需要**：启动关卡用引擎自带的空关卡 `Engine/Maps/Entry`，世界由 `ASolsticeGameMode` 从代码生成 |

## 新克隆后怎么做

```powershell
powershell -ExecutionPolicy Bypass -File tools\build\gen-assets.ps1
```

约 10 秒，产出 `M_Paper.uasset`（11.3 KB）与 `M_Ink.uasset`（5.0 KB）。**幂等**：两个包已存在时直接跳过
（只花两次 `FileExists`）。只加内容、不依赖已编译的游戏目标，只要 `SolsticeEditor` 编过就行。

### 为什么不是一个"启动时自动生成"

生成材质要 `UnrealEd`，而**运行模块不能链接 `UnrealEd`**（`Solstice` 目标要能不带编辑器编译）。
所以分工是：

1. `SolsticeEditor` 模块在 `StartupModule` 里把"怎么生成"这件事作为回调交给运行模块
   （`Solstice::FPaperMaterials::SetBuilder`）。它**不**在启动时生成——编辑器启动阶段存包会和资产注册表的
   rescan 抢时序，而且编辑器启动不该有副作用。
2. 运行模块在 `/Game/Core/M_Paper` 缺失时才调那个回调，每次装配只调一次。
3. 于是：**编辑器里 Play（PIE）** 会懒生成自愈；**`-game` / 打包版**因为没有编辑器模块，必须靠
   `gen-assets.ps1` 事先把资产放到盘上；都没有时渲染降级到引擎自带的顶点色材质
   `/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly`——世界依然可玩，只是失去日照着色。
   运行时会打一行 `LogSolstice: paper: ... -> authored / fallback` 说明走的哪条路。

> **`SolsticeEditor` 的 `LoadingPhase` 必须是 `Default`**：`-run=XxxCommandlet` 的类解析发生在
> `PostEngineInit` 阶段模块加载**之前**，写成 `PostEngineInit` 会直接报
> `looked like a commandlet, but we could not find the class`。原因见
> [`docs/ops/ENVIRONMENT.md`](../../tools/docs/ENVIRONMENT.md) §2.6 第 9 条。

> **结论**：删掉整个 `Content/` 目录，重跑 `gen-assets.ps1` 即可完全重建。这是本仓库"资产不入 git"的前提。
