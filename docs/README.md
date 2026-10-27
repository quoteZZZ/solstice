---
id: README
title: 文档入口
status: active
---

# Solstice · 文档入口

> 一座卡牌驱动的造物沙盒：从虚无起手，抽卡造世界，然后看它自己运转。
> 仓库里没有引擎工程，规格在 markdown，验证物是 `demo/index.html`。

---

## 1 分工：真源 / 投影 / 原型

| 类 | 是什么 | 能不能当依据 |
|---|---|---|
| **规格** `docs/*.md` | 设定、卡池、字段、时间、机制借鉴 | 是 |
| **投影** `docs/*.html` | 线框、可点击原型、卡面版式、单卡稿 | 否，只是规格的可读版本 |
| **原型** `demo/` `drafts/` | 可跑的前端验证物 | 验证行为，不定义规格 |

冲突时按 **规格 md > 投影 html > 原型** 的顺序取，并把差异记到对应规格文档里，不要私自改一边。

---

## 2 文档地图

### 2.1 规格

| 文档 | 管什么 |
|---|---|
| **[SETTING.md](SETTING.md)** | 设定集（单一真源）：虚无开局、卡的六层、判据与组件 |
| **[TIME.md](TIME.md)** | 时间只由 UI 控制（暂停 / 加速），卡不涉及时间 |
| **[CARD-TREE.md](CARD-TREE.md)** | 合成树：环级 = 合成深度，图鉴兼工作台 |
| **[CARD-INDEX.md](CARD-INDEX.md)** | 卡总目录（按类分组，图鉴目录） |
| **[CARD-COMPONENT.md](CARD-COMPONENT.md)** | 卡 → 组件映射（参数、点数） |
| **[CARD-SCHEMA.md](CARD-SCHEMA.md)** | 卡片字段表：卡面 / 悬浮 / 图鉴的字段来源 |
| **[GAME-SCAN.md](GAME-SCAN.md)** | 卡牌游戏机制扫描：抄什么 / 不抄什么 |

### 2.2 投影

| 文件 | 是什么 |
|---|---|
| **[WIREFRAMES.html](WIREFRAMES.html)** | 静态线框板：抽屉 / 卡比例 / 设置 / 键位 / 视角 / 缩放 / 形制 |
| **[PROTOTYPE.html](PROTOTYPE.html)** | 可点击原型：创世 → 进场 → 世界（左功能栏 + 右信息栏）→ 拖卡落点 / 回收 → 抽卡 → 卡库 / 年表 / 设置 → 视角缩放 |
| **[CARD-LAYOUTS.html](CARD-LAYOUTS.html)** | 卡面版式四套方案 + 三档尺寸实测 |
| **[CARD-NIGHTBIRD.html](CARD-NIGHTBIRD.html)** | 「夜沼鸟」单张卡的完整设计稿（CARD-SCHEMA 的实例） |

### 2.3 原型

| 入口 | 是什么 |
|---|---|
| **[`demo/index.html`](../demo/index.html)** | 当前主原型，单文件（虚无开局 → 施卡造陆 → 日月推进 → 生态演化 → 抽卡） |
| `drafts/*.html` | 五个早期方向验证稿：bid 竞价 / ember 余烬 / strand 潮痕 / tear 撕 / tide 潮痕核心循环 |

主原型的自测在 `demo/test.js` 与 `demo/layout-check.js`（Playwright，可选），截图证据是 `demo/shot-*.png`。

---

## 3 改文档的规矩

1. **数字只写一处**：规格数字写进对应域文档，别在第二份文档里抄一遍。
2. **markdown 是依据，HTML 是投影**：改规格先改 md，再回头改投影。
3. **提交**：英文简短祈使句；文档与代码同次提交。
4. **新增 / 作废文档**：同次改本文 §2 的表。
