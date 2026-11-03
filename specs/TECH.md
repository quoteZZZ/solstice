---
id: TECH
title: 技术与原型约定
status: active
---

# 技术与原型约定

## §1 现在用什么

| 项 | 值 |
|---|---|
| 运行形态 | 纯静态：HTML + CSS + 原生 JS，**经典 `<script>` 按序加载**，无构建、无打包 |
| 入口 | 各原型目录的 `index.html`，`file://` 直接打开 |
| 唯一第三方库 | Three.js **0.160.0**（UMD），已 vendored 到各原型的 `vendor/three.min.js` |
| 自测 | Playwright / playwright-core（`prototypes/world-3d/tests/`、`prototypes/solstice/tests/`），需 Node 18+ |
| 数据形态 | 卡表是 `data/cards.*.js`（按 `CARD-SCHEMA` 字段写），编号 / 回收 / ATK·DEF 由 `data/index.js` 派生（仅合并原型） |

> **为什么不是 ES module**：`file://` 下浏览器按 CORS 拦 ES module 的外部 import，双击会白屏。
> 经典 `<script>` + 全局命名空间（`window.SOL`）是"离线双击可开"与"分文件"唯一能同时成立的写法。
> 转正进 `game/` 之后才上 module + 本地静态服务（§3）。

---

## §2 原型约定（`prototypes/` 下全部条目适用）

1. **一个原型一个目录**，入口固定 `index.html`。
2. **离线可开**：第三方库必须 vendored 进目录，禁止 CDN——断网也要能跑。
3. **互不引用**：原型之间不共享代码与 CSS；共享结论只能写进 `specs/`。
4. **自带 README 四段**：验证什么 / 已实现 / 未实现 / 已知坑。
5. **冻结即不改**：`prototypes/alt/**` 是试吃证据，保持原样；要改就复制成新目录。
6. **自测就地放**：`tests/` 与 `package.json` 只服务本原型，不往仓库根塞。
7. **能分层就分层**：单个 HTML 超过约 600 行时，按 `data/` `sim/` `render/` `ui/` 分到子目录，入口只做装配；
   层级依赖单向（ui → render → sim → data），`sim/` 不碰 DOM 与 `window`。

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
