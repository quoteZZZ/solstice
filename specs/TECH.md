---
id: TECH
title: 技术与原型约定
status: active
---

# 技术与原型约定

## §1 现在用什么

| 项 | 值 |
|---|---|
| 运行形态 | 纯静态：HTML + CSS + 原生 JS（ES module），无构建、无打包 |
| 入口 | 各原型目录的 `index.html`，`file://` 直接打开 |
| 唯一第三方库 | Three.js **0.160.0**，已 vendored 到 `prototypes/world-3d/vendor/three.module.js` |
| 自测 | Playwright（仅 `prototypes/world-3d/tests/`），需 Node 18+ |
| 数据形态 | **内联在 HTML 里**（`CARDS` / `SPECIES` 常量），尚未抽成 JSON |

---

## §2 原型约定（`prototypes/` 下全部条目适用）

1. **一个原型一个目录**，入口固定 `index.html`。
2. **离线可开**：第三方库必须 vendored 进目录，禁止 CDN——断网也要能跑。
3. **互不引用**：原型之间不共享代码与 CSS；共享结论只能写进 `specs/`。
4. **自带 README 四段**：验证什么 / 已实现 / 未实现 / 已知坑。
5. **冻结即不改**：`prototypes/alt/**` 是试吃证据，保持原样；要改就复制成新目录。
6. **自测就地放**：`tests/` 与 `package.json` 只服务本原型，不往仓库根塞。

---

## §3 转正后的分层（`game/`，现在不建）

```text
game/
├─ index.html          挂载壳，只做 mount
├─ src/
│  ├─ sim/             纯逻辑：卡结算、生态 tick、世界状态
│  ├─ render/          表现层：three / canvas，只读 sim 状态
│  ├─ ui/              手牌、面板、图鉴
│  └─ data/            卡 / 物种 / 世界配方 JSON
├─ assets/
└─ tests/
```

### 3.1 三条硬约束

1. **`sim/` 零渲染依赖**：不 import three、不摸 DOM、不读 `window`，必须能直接在 node 里跑。
   理由：确定性回归只能在没有渲染的一侧做；这也是把 1283 行单文件切开时唯一可靠的接缝。
2. **数据与表现分离**：卡的所有可显示内容来自 `data/*.json`，字段只认 [CARD-SCHEMA.md](CARD-SCHEMA.md)；渲染层不许自己拼文案。
3. **一个状态一个真源**：`sim` 持有唯一状态树，`render` / `ui` 只读不写。

### 3.2 允许引什么

- 渲染：Three.js（版本跟 `vendor/` 走，升级要同步改本节）
- 其余零依赖：不引 UI 框架、不引状态库
- 真要上构建器（Vite 之类），先改本节并写清"单文件为什么走不下去了"

---

## §4 确定性（转正后必须补，原型阶段没有）

同一种子跑两次，`sim` 导出的状态摘要必须一致。摘要至少覆盖：格子地形、物种计数、时素、卡池剩余。

原型阶段的 `tests/smoke.js` 只测"施卡链路不报错 + 卡确实生效"，**不是**确定性回归，别当回归用。
