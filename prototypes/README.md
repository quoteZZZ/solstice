# 原型索引

> 一个原型一个目录，入口都是 `index.html`；全部离线可开，不需要构建、不连 CDN。

## 主线候选

| 目录 | 中文名 | 验证什么 | 状态 |
|---|---|---|---|
| [`solstice/`](solstice/README.md) | 至日 · 合并版 | 真 3D 世界 + 全套面板 + 一张按 schema 写的卡表（55 张） | 可玩 / 在改 |
| [`world-3d/`](world-3d/README.md) | 至日 · 3D 世界版 | 施卡造陆 + 生态自转，用 3D 微缩表现"世界自己活" | 已并入，留作历史 |
| [`ui-flow/`](ui-flow/index.html) | 至日 · 界面全流程 | 从创世到卡库 / 年表 / 事件，信息架构能不能一屏装下 | 已并入，留作历史 |

`solstice/` 是两者的合并结果：世界取自 3D 版（Three.js + 生态仿真），信息架构取自界面版（创世 / 卡库 / 年表 / 事件 / 设置 / 键位），
卡表合成一份并按 [`../specs/CARD-SCHEMA.md`](../specs/CARD-SCHEMA.md) 补齐。
两个旧原型不再修改，也不再当依据——差异见 [`../specs/STATUS.md`](../specs/STATUS.md) §1。

## 冻结的机制试吃（`alt/`）

不进主线，原因是它们都靠失败判定制造紧张，与主线的"无失败"冲突（[CHARTER.md](../specs/CHARTER.md) §4.2）。保留是为了留下第一手实现。

| 目录 | 中文名 | 验证什么 |
|---|---|---|
| [`alt/bid/`](alt/bid/README.md) | 破产 | 拍卖跟价的紧张感 |
| [`alt/ember/`](alt/ember/README.md) | 焚尽 | BOSS 战 + 预燃换资源 |
| [`alt/strand/`](alt/strand/README.md) | 潮痕 | 荒岛生存的完整资源循环 |
| [`alt/tide/`](alt/tide/README.md) | 潮痕 · 核心循环 | 同一循环的回合制轻量版 |
| [`alt/tear/`](alt/tear/README.md) | 撕 | 单键反应的节奏与手感 |

## 加新原型

1. 新建 `prototypes/<代号>/`（代号用小写英文，能一眼看出机制）
2. 入口必须叫 `index.html`，第三方库 vendored 进目录
3. README 四段：验证什么 / 已实现 / 未实现 / 已知坑
4. 在本表加一行；若它取代了某个旧原型，在旧原型 README 里写一句指向新的，并把旧条目改成"已并入，留作历史"
5. 超过 600 行就按 `data/ sim/ render/ ui/` 分文件（经典 `<script>`，见 [TECH.md](../specs/TECH.md) §1）
