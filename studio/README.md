# `studio/` — 生图线（ComfyUI）

本目录是**产出素材**的本地生图线，与 `game/` 的运行时工程解耦。真源：
[`docs/TECH.md`](../docs/TECH.md) §4。

## 为什么需要它

[`game/Content/`](../game/Content/README.md) 的二进制资产不入 git，前提是**能重建**。
目前需要重建的只有两类：

| 类别 | 现状 | 谁生成 |
|---|---|---|
| 纸雕母材质 `M_Paper` / `M_Ink` | ✅ 已由 `SolsticeEditor` 按代码建图生成 | 引擎侧 |
| 纸纹 / 地表 / 水纹 / 卡面 | 🟡 待生图 | **本目录** |

也就是说：**目前游戏不依赖任何生图产物就能跑**。本目录是给"纸的质感再上一个台阶"准备的，
不是启动前提。这一点是刻意的——先让最小闭环成立，再叠质感。

## 目录

```text
studio/
├─ workflows/   ComfyUI 工作流（API 格式 JSON，入 git）
├─ jobs/        批量任务清单（jsonl：prompt / seed / 尺寸 / 输出名，入 git）
├─ presets/     风格主题参数（入 git，与 shared/schema 对齐）
├─ tools/       comfy_batch.py —— 读 jobs 打 ComfyUI HTTP API，写 manifest
└─ out/         ⚠ 产物不入 git；out/manifest.jsonl **入 git**（Steam AI 内容披露留档）
```

## 用法

```bash
# 1. 起 ComfyUI（便携版自带；本机在 D:\ComfyUI）
D:\ComfyUI\run_nvidia_gpu.bat
# 本机另有一份隐藏窗口启动脚本（日志转 studio/out/，可反复执行）：
#   powershell -ExecutionPolicy Bypass -File studio\scripts\start-comfy.ps1
#   ⚠ 该脚本目前未入库 —— studio/scripts/ 不在 docs/TECH.md §4 的目录表里，归位待定

# 2. 检查模型与连通性
python studio/tools/comfy_batch.py --check

# 3. 跑任务（默认工作流 studio/workflows/paper-texture.json）
python studio/tools/comfy_batch.py --jobs studio/jobs/paper-textures.jsonl

# 4. 只跑其中一类（纸纹 / 地表 / 水纹 / 卡面按 subject 分开出图）
python studio/tools/comfy_batch.py --jobs studio/jobs/paper-textures.jsonl --only-subject paper-texture
```

产物由脚本从 ComfyUI 的 `/view` 接口取回（不依赖输出目录布局），按 job 的 `out` 名字落进
`studio/out/<子目录>/`，并往 `studio/out/manifest.jsonl` 追加一行（字段见
[`../docs/ART.md`](../docs/ART.md) §2：id / kind / style / model / prompt 引用 /
seed / 尺寸 / 日期 / source），这是 **AI 生成内容的合规留档**，必须入库。
模型三元组与采样基线写在 `presets/paper-diorama.json` 的 `pipeline` 块里，脚本不硬编码模型名。

## 风格锚

`presets/paper-diorama.json` 是与 [`docs/ART.md`](../docs/ART.md) 对齐的风格参数：
纸张纤维、切边毛茬、四档硬光、无高光。生图 prompt 由这个 preset 拼出来，不手写。

## 生图模型与路径（本机实测真源）

**ComfyUI 的程序根与模型根不是同一层** —— 这是"找不到模型"的唯一原因，把这张表钉死：

| 要什么 | 路径 |
|---|---|
| ComfyUI 程序根（`main.py` 所在） | `D:\ComfyUI\ComfyUI\` |
| 启动解释器（embedded python） | `D:\ComfyUI\python_embeded\python.exe` |
| **模型根** | `D:\ComfyUI\ComfyUI\models\` |
| 服务端口 | `127.0.0.1:8188` |

本机在位模型是**分离三件套**，**不在** `checkpoints\`：

| 部件 | 文件 | 大小 |
|---|---|---|
| 扩散模型 | `models\diffusion_models\flux-2-klein-4b-fp8.safetensors` | 3.79 GB |
| 文本编码器 | `models\text_encoders\qwen_3_4b.safetensors` | 7.49 GB |
| VAE | `models\vae\flux2-vae.safetensors` | 0.31 GB |

**由此产生的硬约束**（`f351e05` 已落实）：`workflows\paper-texture.json` 必须走
`UNETLoader + CLIPLoader(type=flux2) + VAELoader` 三段式 + `Flux2Scheduler`，采样基线
`steps=4 / cfg=1.0 / euler`（[`../docs/ART.md`](../docs/ART.md) §3）。旧稿是
`CheckpointLoaderSimple` 的 SD 图，在只装分离式 Flux2 的机器上必然报 checkpoint not found，
**别退回那种写法**。机器上另有一份可参照的官方图（UI 格式，非 API 格式）：
`D:\ComfyUI\ComfyUI\user\default\workflows\Flux2 Klein 4B - Text to Image.json`。

**启动 / 停止**（无 cmd 窗口，日志落 `studio\out\`）：

```
powershell -ExecutionPolicy Bypass -File studio\scripts\start-comfy.ps1
powershell -ExecutionPolicy Bypass -File studio\scripts\start-comfy.ps1 -Stop
```

`studio\tools\comfy_batch.py --check` 通过 ComfyUI API 取模型清单、**不猜路径**——任何时候不确定，先跑它。
## `out/` 保留策略（2026-10-17 定，用户裁决「用文档」）

**`studio/out/` 整个目录可以随时删除**，因为它不承载任何不可重建的信息：

| 东西 | 删了会怎样 | 怎么重建 |
|---|---|---|
| 生成图（当前 10 张 / 约 4 MB） | 只是本地缓存，仓库里没有它 | 按 `jobs/*.jsonl` + `presets/*.json` 重跑 `comfy_batch.py` |
| `manifest.jsonl` | **这一份不能丢**（合规留档：逐张的模型 / prompt / seed / 日期） | 它在 git 里；图删了、档还在，仍可逐张复现 |

规则三条：

1. `out/` 里的图**不入 git**（`.gitignore` 已覆盖），`manifest.jsonl` **入 git**；
2. 想看历史批次就把整个 `out/` 备份出去，**不要在 `out/` 里造 `_archive/` 之类的第二层语义**（`REVIEW R-27` 就是在治这个）；
3. 清理动作永远由人执行——**没有自动清理脚本**，避免误删正在对照的那一批。
