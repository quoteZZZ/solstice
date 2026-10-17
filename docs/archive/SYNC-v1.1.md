---
id: ops/SYNC
title: 双机 git 同步
version: 1.1
date: 2026-10-14
status: active
truth: nav
owner: both
---

# 双机 git 同步（SYNC）

> 约定：**git 是唯一真源**（远端仓库 `origin`）+ 计划任务定时 `pull` / `push`。大二进制资产不进 git（见 §4）。
> 提交规范与文档纪律见 [`START.md`](../START.md) §2 / §3。
> **路径约定**：下文示例用变量 `$repo` 表示**本机仓库根**——两台机器各自填自己的路径（本机为 `E:\solstice`，另一台按实际填写），**不要照抄别的机器的绝对路径**。

---

## 1 基本约定

- **唯一真源 = 远端仓库（origin）**。未 push 的本地改动视为"未落地"。
- 拉取用 **`git pull --rebase --autostash`**，避免无谓 merge commit 与脏工作树阻塞。
- **同一时间只允许一台机器改同一文件**；跨机改同名文件前先对齐。
- 素材 / 产物**单独提交**（便于回溯与回退），不与代码混提。

---

## 2 定时同步（注册计划任务）

### 方式一：`Register-ScheduledTask`（推荐）

```powershell
$repo = 'E:\solstice'          # ← 各机填自己的仓库根
$action  = New-ScheduledTaskAction -Execute 'cmd.exe' `
  -Argument "/c cd /d $repo && git pull --rebase --autostash && git push"
$trigger = New-ScheduledTaskTrigger -Once -At (Get-Date) `
  -RepetitionInterval (New-TimeSpan -Minutes 15)
$settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -StartWhenAvailable
Register-ScheduledTask -TaskName 'Solstice Git Sync' -Action $action -Trigger $trigger -Settings $settings -Force
```

### 方式二：`schtasks`（最简）

```powershell
schtasks /Create /TN "Solstice Git Sync" /SC MINUTE /MO 15 /TR "cmd /c cd /d E:\solstice && git pull --rebase --autostash && git push"
```

### 取消与查看

```powershell
Unregister-ScheduledTask -TaskName 'Solstice Git Sync' -Confirm:$false
Get-ScheduledTask -TaskName 'Solstice Git Sync' | Get-ScheduledTaskInfo
```

- 运行身份用当前用户（复用凭据缓存）。无人值守需配 **SSH key** 或 **Git Credential Manager**。
- 手动兜底：

```powershell
cd E:\solstice
git pull --rebase --autostash
git add -A; git commit -m "chore: sync"; git push
```

---

## 3 避免冲突的实践

1. **目录归属分工**：两台机器各有主战场（一台偏文档 / 工具链，一台偏素材生产）；**同一文件不要在两台机器上同时改**。
   > **目录现状**：`game/`、`studio/`、`shared/` 目前**尚未落地**（计划中，随 M0 建立）——状态见 [`../tech/STRUCTURE.md`](../tech/tech.md) §1。落地后再按"素材线只动 `studio/`，代码线只动 `game/`、`tools/`、`shared/`"分工。
2. **小步提交、频繁同步**：宁可多 push，不攒一整天。
3. **派生文件不入库**：`Intermediate`、`Binaries`、`*_generated.h`、`compile_commands.json`、`.sln` 一律忽略，跨机各自生成。
4. **遇分叉**：`git pull --rebase --autostash`；冲突解决后 `git rebase --continue`。
5. **不要 force push 共享分支**。

---

## 4 `Content/` 下大资产的处理

`game/Content/**` 的 `.uasset` / `.umap` 体积大且二进制不可合并，**默认不入 git**（见 [`../tech/STRUCTURE.md`](../tech/tech.md) §9）。三选一：

- **优先（当前策略）：本地再生 + 契约入库。** 只版本化 `Source/`、`Config/`、`shared/schema/`、`studio/{workflows,jobs,presets,refs}`、`studio/out/manifest.jsonl`（登记规范见 [`../art/ASSETS.md`](../art/art.md) §2）。
- **可选：Git LFS**（仅跟踪少量确需共享的关键资产）：

```powershell
git lfs install
git lfs track "game/Content/**/*.uasset" "game/Content/**/*.umap"
git add .gitattributes game/Content
```

> ⚠ LFS 需远端配额（GitHub/GitLab 均有）；**不要**把 `Intermediate` / `DerivedDataCache` 塞进 LFS。

- **不同步**：两台机器各自持有编辑器生成的资产副本，靠 `Source/` + 脚本重建。**若两台机都改同一批资产，这是唯一不会冲突的方案。**

---

## 5 更新触发

| 改动类型 | 必须同次更新 |
|---|---|
| 仓库根路径 / 分支策略变化 | 本文件 §1 / §2 |
| `.gitignore` 策略变化 | 本文件 §3 / §4 + [`.gitignore`](../../.gitignore) 注释 + [`../tech/STRUCTURE.md`](../tech/tech.md) §9 |
| 目录归属分工变化 | 本文件 §3 + [`../tech/STRUCTURE.md`](../tech/tech.md) §9 |

---

## 沿革

| 版本 | 日期 | 变更 |
|---|---|---|
| 1.0 | 2026-10-14 | 补统一文档头 |
| 1.1 | 2026-10-14 | 迁入 `docs/ops/`；去掉硬编码的他人机器路径（改为 `$repo` 变量）；`studio/` `shared/` 的存在性描述改为"计划中"并指向 STRUCTURE §1 |
