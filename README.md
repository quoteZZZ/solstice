# Solstice · 至日

> **从一片虚无开始，用抽到的卡创造世界，然后看它自己运转。**
> 卡牌驱动的造物沙盒：无终局、无数值成长，唯一的交互是**施卡**与**观察**。

仓库只装两样东西：**可玩原型**和**它的设计文档**。无引擎工程、无构建链、无服务端。

---

## 从哪进去

| 你要 | 去哪 |
|---|---|
| 直接玩 | `prototypes/solstice/index.html`（合并版，双击即开） |
| 看设计 | [`specs/README.md`](specs/README.md)（文档地图） |
| 看实装到哪了 | [`specs/STATUS.md`](specs/STATUS.md) |
| 看还有哪些原型 | [`prototypes/README.md`](prototypes/README.md) |
| 准备动手开发 | [`specs/TECH.md`](specs/TECH.md)（原型约定 + 转正分层） |

原型**离线可开**：Three.js 已 vendored 进 `prototypes/solstice/vendor/`，双击 `index.html` 即可，不需要 `npm install`。
`prototypes/archive/` 里两个旧版（3D 世界版 / 界面全流程版）只作历史对照，不再修改。

---

## 目录

```text
solstice/
├─ specs/                    设计文档（唯一真源）
│  ├─ README.md              文档地图 + 写作规矩
│  ├─ STATUS.md              实装现状 / 数字口径 / 缺口 / 下一步
│  ├─ CHARTER.md             定位 / 支柱 / 非目标 / 已否决
│  ├─ SETTING.md  TIME.md    设定集 · 时间
│  ├─ CARD-TREE.md  CARD-INDEX.md  CARD-COMPONENT.md  CARD-SCHEMA.md
│  ├─ GAME-SCAN.md           32 部卡牌游戏机制借鉴
│  ├─ TECH.md                原型约定 + 转正后的代码分层
│  └─ ui/                    线框 / 卡面版式 / 单卡稿（规格的可读投影）
├─ prototypes/               可玩原型（一个原型一个目录，入口统一 index.html）
│  ├─ solstice/              至日 · 合并版（主线，唯一在改的）
│  ├─ alt/                   五个机制试吃（冻结，只作对照）
│  └─ archive/               两个旧版：world-3d/ · ui-flow/（冻结，只作历史）
├─ tools/                    文档自检脚本（docs-check / card-reconcile）
├─ .codebuddy/  .workbuddy/  项目数据，勿删
```

`game/` 是转正后的主干，现在不建；分层与硬约束见 [`specs/TECH.md`](specs/TECH.md) §3。

---

## 自测（可选，需 Node 18+）

原型本身不需要构建；下面四条只做回归，不产生产物：

```powershell
cd prototypes/solstice
npm test               # 冒烟：施卡链路 + 控制台报错
npm run det            # 确定性：同种子两次跑出同一个 digest
npm run codex          # 图鉴：条目可点亮、不重复、不退
npm run cards          # 卡表：schema 硬校验 + 跨卡引用

cd ../..
python tools/docs-check.py       # 文档：断链 / 缺章节 / 引用没带标题片段
python tools/card-reconcile.py   # 报表：卡表与 CARD-INDEX 的口径差
```

分工和"什么算通过"写在 [`specs/TECH.md`](specs/TECH.md) §4 与 [`specs/FILL.md`](specs/FILL.md) §1。

---

## 沿革

动态壁纸 → 场景引擎 → 卡牌收集 → 自走棋 → 纸雕微缩 → UE5 模拟沙盒 → **卡牌造物沙盒**。

2026-10-27 两次清仓：先删掉 UE 工程、工具链、生图线与旧文档；再把仓库重组为 `specs/ + prototypes/`（取消 `docs/` `demo/` `drafts/` 三个平级目录）。旧仓库完整历史打包在仓库外 `E:\solstice-legacy-2026-10-27.bundle`。
