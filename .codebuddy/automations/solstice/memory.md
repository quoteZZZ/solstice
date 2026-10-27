# 素材机循环 · 执行记忆（automation: solstice）

> 每轮只记高层结论：环境事实、修好的坑、本轮产出与提交、下一步卡在谁手里。

## 2026-10-17 · 第 1 轮

**任务书现状**：本机任务书里指向的旧素材机契约（`studio/contract/**`、`docs/PACKS_GUIDE.md`、`docs/DESIGN.md`、`studio/tools/gen.py|pack.py`、`app/assets/packs/`）在现仓库中**都不存在**——那是已作废形态（动态壁纸 / 场景引擎）的路径，`docs/archive/SYNC-v1.1.md` 也已把双机分工归档。现行素材线 = `studio/`（ComfyUI）+ `studio/out/manifest.jsonl`（入库）。后续轮次按 `studio/{jobs,tools,workflows,presets,out}` 干活，别再等 SET 任务书。

**环境事实（本机）**
- ComfyUI 0.39.0 在 `http://127.0.0.1:8188`，RTX 5060 8GB；隐藏窗口启动脚本 `studio/scripts/start-comfy.ps1`（**未入库**，`studio/scripts/` 不在 STRUCTURE §4 目录表里）。
- 可用模型：`UNETLoader=flux-2-klein-4b-fp8.safetensors`、`CLIPLoader(type=flux2)=qwen_3_4b.safetensors`、`VAELoader=flux2-vae.safetensors`；`CheckpointLoaderSimple` 列表为空（盘上有 `sd_xl_base_1.0.safetensors` 但 ComfyUI 看不到 ⇒ 老工作流必挂）。
- ComfyUI 实际输出根是 `D:\ComfyUI\ComfyUI\output`（不是 `D:\ComfyUI\output`）。
- Python 用 `D:\ComfyUI\python_embeded\python.exe`。

**本轮修好的坑**
1. 工作流与已装模型不匹配：原 `studio/workflows/paper-texture.json` 是 checkpoint + `dpmpp_2m/karras` + 28~32 步的 SD 图 ⇒ 换 `UNETLoader + CLIPLoader(type=flux2) + VAELoader + Flux2Scheduler(+KSamplerSelect/SamplerCustomAdvanced/CFGGuider/RandomNoise)`，基线 `steps=4 / cfg=1.0 / euler`（ASSETS §3）。
2. **API 格式工作流不能有顶层非节点键**（如 `_comment`）——ComfyUI `validate_prompt` 会抛 `AttributeError: 'str' object has no attribute 'get'`，表现为 `/prompt` 返回 500。
3. 取图改为走 `/view` 接口，别硬编码输出目录。
4. manifest 字段按 ASSETS §2（id/kind/style/model/prompt_ref/seed/size/date/source/pick + 溯源附加项），并且**一个文件一行**（返工覆盖自己的行）。
5. 纹理类 subject 需要自己的 `words` 覆盖：全局风格词的 `tilt-shift macro photograph of a tabletop model` / `studio tabletop lighting from one direction` 会把平铺贴图变成「桌上模型照片」。

**本轮产出**：10 张纹理（paper×4 / ground×4 / water×2）落 `studio/out/textures/`（不入库），manifest 10 行入库；其中 `paper_kraft_01`、`water_sheet_01` 各返工 1 次（色板）。提交 `f351e05` 已 push（连带把此前本地未推的 `94b5cde` 一并推上）。

**下一步卡在谁手里**
- **用户**：挑图（10 张纹理，manifest `pick` 全为空）。
- **代码机**：① 卡面规格冲突——preset 里 `card-face` 提示词是「带框卡片」，与 ART-STYLE §4「只出画芯、2048×2732、UMG 画版式」冲突，故本轮**未出卡面**；② `studio/scripts/start-comfy.ps1` 归位（STRUCTURE §4/§8 需补一行）。

**踩坑提醒（给下一轮自己）**
- 工作区常驻代码机的大量未提交 + **已暂存**改动（`game/**`、`docs/**`、`tools/**` 改名）。提交素材必须用 pathspec：`git commit -m "..." -- <studio 下具体文件>`，否则会把代码机的改动一起卷进素材提交。
- 单个 subject 重跑用 `--only-subject <名字>`（可重复），别整批重烧。

## 2026-10-17 12:32 · 第 2 轮（空转，无提交）

**同步**：`git fetch` 无新提交；本地 = 远程 = `f351e05`。工作区仍挂着代码机的已暂存改动。
**任务书**：`studio/contract/**`、`docs/PACKS_GUIDE.md`、`docs/DESIGN.md`、`studio/tools/gen.py|pack.py`、`app/` 逐条 Test-Path 复核 —— 仍然全不存在。

**发现：同机另一会话正在同一条生图线上干活（12:20–12:32 活跃）**
- 未提交新增：`studio/presets/{style-anchors,world-form,anime-world}.json` + 对应 `studio/jobs/*.jsonl`；`studio/out/world/` 已 35 张 / 68MB。
- 对方还改了 `studio/tools/comfy_batch.py`（加 `preset.base_prompt` 前置子句，与本版兼容）与 `studio/README.md`（新增「生图模型与路径」表）。
- manifest 行数在本轮两次检查间 44 → 45，最新文件 mtime 距检查 15 秒 ⇒ **当时并发在跑**（12:32:31 收尾，队列随后清空）。

**本轮决定**：空转、不提交。无新任务；studio 有他人 WIP，一提交就会把对方在途改动卷进素材提交。

**待办（只汇报，未动手）**
1. `studio/README.md` 新段已过期：它说 `workflows/paper-texture.json` 仍是 SD checkpoint 图、加载不了 Flux2 —— `f351e05` 已换成 UNETLoader+CLIPLoader(flux2)+VAELoader 三段式。等对方写完再改，避免并发编辑。
2. **manifest 并发风险**：`log_entry()` 是整体读-改-写，两方同时跑批会丢行。要么约定同一时刻只一方跑批，要么加锁（须在无人跑批时改）。
3. 世界图在 manifest 里记成 `asset.* / kind=texture`（`world/` 子目录无映射）；`ASSETS §2` 的 kind 枚举没有 study/anchor —— 要入库得先由代码机定枚举，我再补映射。
4. 24 张日式动画方向图属用户的风格对照实验；docs 仍是「只 T1 / T2–T5 否决」，方向若有变要走 GDD 裁决，不是素材职能定的。

## 2026-10-17 14:33 · 第 3 轮（空转，无提交）

**同步**：远程仍 `f351e05`，本地无领先/落后；`studio/` 未提交清单比上轮更长（对方会话 12:59–13:10 又跑了一轮）。

**对方会话新增（仍未提交）**：`studio/jobs/world-globe.jsonl`、`studio/presets/{world-globe,anime-img2img}.json`、`studio/workflows/img2img-anime.json`、`studio/tools/gen_world_ref.py`、`studio/refs/`、`studio/previews/`；`studio/out/world/` 已 53 张。最后活动 13:10:33，队列空 ⇒ 本轮检查时**已停手约 1.4h**（但仍是"未收尾的在途工作"）。

**本轮复核结论**
- manifest 59 行 / 磁盘 63 个 PNG：**4 个孤儿** `world_hemi_ref.png`、`world_plate_p30/p45/p65.png` —— 由 `gen_world_ref.py`（**程序化 3D 正投影**，不是 ComfyUI）直接写进 `out/world/`，PNG 无 prompt 元数据。属"放置错了"而非漏登记 AI 图：脚本自己的用法示例是 `--out studio/work/ref/…`。
- `studio/refs/`（`world-form-reference.jpg`）与 `studio/previews/`（3 个文件）**没有被 .gitignore 覆盖** ⇒ 一旦有人 `git add studio/`，二进制会混进库。`.gitignore` 不在本机域内，只汇报。
- 对方的工作属**临时研究**（preset 注释写明 "TEMPORARY … folded back once the user locks a direction"），不得由素材机代为提交。

**决定**：仍空转、不提交。**连续两轮空转的根因**：任务书指向的形态已作废，而当前真正的活儿是用户侧交互式风格对照 + 挑图；自动化这边既无契约可领，也不该替人做审美裁决。

**建议（给用户）**：要么把本自动化的 prompt 改到现行流程（studio/jobs 下具体批次 + 明确验收），要么暂停该自动化，等风格定标后再开——否则每两小时只会在同一状态上空转。

## 2026-10-17 16:34 · 第 4 轮（空转，无提交）

**与第 3 轮逐项一致**：远程仍 `f351e05`；任务书 7 条路径仍全不存在；`studio/` 未提交清单未变（对方最后一笔是 `studio/README.md` 14:02，之后 **2.5h 静默**）；manifest 仍 59 行 / 63 文件、同 4 个程序化孤儿；`refs/`、`previews/` 仍未被忽略；ComfyUI 在 (VRAM free 2.76GB，模型有缓存)。

**决定**：空转，不提交。**不再重复解释理由**，见第 3 轮。

**若获授权，下一轮可做的三件小事（都小、都可独立提交）**
1. 把 4 张程序化参考图从 `out/world/` 迁到 `work/ref/`，manifest 保持只描述 AI 产物（或补 `source: procedural` 行）。
2. 修 `studio/README.md` 那段过期"硬约束"（工作流已不是 SD checkpoint 图）。
3. 给 `log_entry()` 加 manifest 写锁，关闭并发丢行风险。

**关键判断**：本自动化已连续 3 轮空转。**再次触发前应由用户改 prompt 或暂停**，否则只会重复同一份报告。

## 2026-10-17 18:15 · 清理轮（用户指令：去除空转与无用程序）

**自动化**：id `solstice` 已改名为「Solstice 素材线（按需）」并置 **PAUSED**；prompt 整段重写——去掉已不存在的 SET 契约 / `studio/tools/gen.py|pack.py` / `app/assets/packs` 引用，任务来源改为「`studio/jobs/*.jsonl` + `studio/presets/*.json` 即任务书」，保留路径限定提交与审美裁决红线。

**删除（用户确认「全部删除」）**：5 个 TEMPORARY preset（style-anchors / world-form / world-globe / anime-world / anime-img2img）、4 份 job 清单、`workflows/img2img-anime.json`、`tools/gen_world_ref.py`、`refs/`、`previews/`、`out/world/` 53 张（约 70 MB 合计）、`work/` 下临时图；manifest 用 `git checkout` 回退到已提交的 10 行（= 10 张纹理，一致性 10/10 校验通过）。
**代码**：`comfy_batch.py` 删掉研究专用的 img2img 上传路径（`upload_image` / `__IMAGE__` / `__DENOISE__` / init 块），**保留** `require_archived()`（R-25 守卫，落在 work/ 或 out/ 直接拒绝运行）；`studio/scripts/start-comfy.ps1` 入库（`docs/TECH.md` §7 已登记 scripts/）；README 过期段落已改（工作流早已是三段式 Flux2）。
**提交**：`0dd75e3` 已 push，studio 工作区已干净。

**文档层已被代码机整体重组**（未提交）：`docs/ART.md`、`docs/TECH.md`、`docs/charter/CHARTER.md`、`docs/PLAN.md`（原 REVIEW）、`docs/later/*`。README 里的链接已由对方同步，`check-links` 616 条 0 断链。

**并发风险（仍未解）**：18:13:45 有**另一个会话**在清理过程中重新生成了 `studio/previews/world-model.html`（已再次删除）。说明同机还有活跃会话在写 studio/；清理类操作前应先关掉它，否则删掉的东西会回流。`.gitignore` 仍未覆盖 `studio/refs`、`studio/previews`（若重建仍需补）。
