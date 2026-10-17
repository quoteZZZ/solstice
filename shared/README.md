# `shared/` — 契约（JSON Schema）

**双方共读的契约层**：`game/`（运行时读 JSON）与 `studio/`（生图线产素材）都按这里的 schema 校验数据。
定义与状态见 [`docs/tech/tech.md`](../docs/tech/tech.md) §5。

```text
shared/schema/
├─ species.schema.json   物种模板        —— docs/setting/SPECIES.md §1
├─ card.schema.json      干涉卡          —— docs/setting/CARDS.md
├─ world.schema.json     界（生成参数）   —— docs/setting/WORLD.md §2.1 / §2.2 / §3.3
└─ save.schema.json      存档信封骨架     —— 载荷待 M1 收紧
shared/fixtures/
└─ save.sample.json      存档样例（校验器的基准输入）
```

## 规则

1. **契约是共同校验源**：改 schema 必须**同一次提交**改两边的数据
   （`game/Config/Solstice/Data/*.json` 与 `studio/presets/*.json`）。
2. **schema 里带出处**：每个文件的 `description` 都写清"真源是哪份文档的哪一节"，
   避免契约与设定集悄悄分叉。
3. **不在这里放数据**：本目录只有 schema 与样例；数据的两份落地位置见第 1 条。

## 校验

```powershell
python tools\checks\validate-contracts.py
```

把 `schema/` 与 `game/Config/Solstice/Data/` 逐对校验，外加卡 / 物种 id 查重、存档信封样例、卡 `params` 白名单与枚举、`Localization/` 双语言 key 对齐。
**当前 6 对契约 / 41 个 id / 162 个 l10n key，0 错误。** 改 schema 或改数据后、提交前跑一次；pre-commit 钩子与 CI 也会代跑（见 [`docs/INDEX.md`](../docs/INDEX.md) §5）。

> 脚本用 **Python 标准库**实现，无需安装任何依赖；真源在 `tools/checks/`。

> **待 M1**：`style.schema.json`（风格主题参数），以及 `save.schema.json` 的**载荷收紧**（信封骨架已落地）。
> 见 [`docs/tech/tech.md`](../docs/tech/tech.md) §5。
