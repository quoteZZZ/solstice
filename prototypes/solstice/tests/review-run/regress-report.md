# 回归验证报告（2026-11-04 第二轮）

**范围**：只回答「上轮 31 条到底修没修」，不找新问题、不改源码。
**环境**：1080×700（+ 1366×800 补证）；Edge headless；`deviceScaleFactor:1`（读数=CSS px）。
**版本**：全部条目在 HEAD `334b2d8` 上测；#7 / #8 / #9 因工作树随后出现 `render/cam.js`、`ui/hand.js` 的未提交修复（16:36 落盘，即 S02/S03），又按**当前工作树**复测了一遍，两套读数都写在下面。
**取证**：
- 四场景截图 `tests/review-run/regress-{full,stress,keyboard,narrow}/`（27/29/41/29 张，`failures:0 console:0`）
- 交互探针 `tests/review-run/regress-probe{,2,3,4,5}/`（`probes.json` + 截图），均由只读脚本驱动真实指针/键盘

| # | 编号 | 结论 | 读数/证据 | 备注 |
|---|---|---|---|---|
| 1 | F14 | PASS | `S.cmd.playSlot` 返回 `{ok:false,msg:'这一带已经是地块了'}`；hand 8→8（同 id 仍在），cells 81→81；toast「放不下 / 这一带已经是地块了」（`probe3/f14-toast.png`） | 带回执、不扣牌 |
| 2 | P01 | PASS | 切进阶包后标题「抽卡 · 进阶包（6 张 / 24 时素）」、底栏「撕开 · −24 时素」；时素 15 时 basic 可点、mid 禁用（`probe/04-p01-mid-pack.png`） | 三处都跟随选中包 |
| 3 | P02 | PASS | 撕开后点基础包：`packSel` 仍 `mid`、stage 仍 2、`packCards` 仍 6，toast「这一包还没挑…」 | 牌没被吞 |
| 4 | P03 | PASS | 挑选阶段按钮文案「跳过这一包」、class `btn ghost`（无填充描边） | — |
| 5 | U01 | PASS | 顶栏 `#bPack` 文案 `抽包 12`，与 `min(packList.cost)=12` 一致 | 0 时素时按钮 disabled（合理） |
| 6 | U19 | PASS | cells 81 / traces 空 → 文案「最近的痕迹都已消散 · 见年表 ▸」；`saysNeverTouched:false`（`regress-full/26-long-run.png`） | — |
| 7 | H01 / U25 / F24 | **PARTIAL** | **格上完全通过**：tip「松手铺在 (-4, 0) 周围」、`marks{n:15,g:10}`、格心像素 (93,96,76)→(112,111,82)（+19/+15/+6，肉眼可辨）、松手 cells 81→91。**工作树复测：一次拖拽只施 1 次**（`casts:["水@-5,-1"]`；修前同一步是 `["水@-5,-1","草@-5,-1"]`，烧 2 张牌） | 唯一残留：拖到**虚空**（土地卡唯一正常落点）`marks{n:0,g:0}`，只有文字「松手在这片虚空造陆」没有范围预览（`regress-probe2/void-drag-afterfix.png`、`regress-probe/12-h01-drag-over-world.png`）。`castRange(card,null)===[]` |
| 8 | H02 | PASS | 向下拖 62px：tip 仍 idle 灰字（非回收红）、`marks` 未初始化、松手 `casts:[]`、cells 81→81、hand 8→8 | 工作树复测同结论 |
| 9 | U38 | PASS | **工作树**：拖一张「水」（在已有格必成功）→ tip「松手放在 (-5, 0)」→ Esc：`#ghost` 1→0、`casts:[]`、hand 8 张原样；随后 mouseup 仍 `casts:[]` | HEAD `334b2d8` 上这条是真 bug（`endDrag(null)` 会在 `mode==='cast'` 时施卡，r-state `state-probe4.json` 也是两次 `ok:true`）；工作树加的 `if (ev == null) return` 已修。我第一次测之所以「通过」，是因为拖的是土地卡且落点在已有地块——施放失败，cells 不变，属于假 PASS，已按上面重测 |
| 10 | H04 / E01 | PASS | 合成连续两次 `pointerdown`：`#ghost` 1→1（不出现第 2 张），left/ top = `136px/659px`（不是 0,0）；松手后 0 张（`probe2/06-h04b…png`） | — |
| 11 | H06 | PASS | 施土地成功后 `#gainfly` 文本 `+15 格`、opacity 1；`visiblePulses:2`（pulse 池 8，峰值 0.92、780ms、scale→2.23） | 环的静帧不易截，飘字稳定可见 |
| 12 | P04 / F26 | PASS | 撕开后 `packpick` 6 张、`.dealcard`（空白卡背）0 张、每张 136×216 带插画+名；步骤条 2 站「1. 封印 / 2. 挑选」 | 少了一站，符合要求 |
| 13 | F12 / F13 | PASS | 连发同一句 4 次 → 仅 1 条；再发 6 条不同句 → 上限 3 条 | — |
| 14 | P40 | PASS | 开面板前 `#toasts` 3 条 → 开后 0 条；新提示落 `#phead .toast.ih`（top 74–96），正文顶 114，不压标题/内容 | — |
| 15 | U22 / P12 | PASS | `.cdscroll::after` / `#rightpanel .rp.grow::after` / `#pbody::after` 均 `content:""`、`height:20px`、sticky、`linear-gradient(rgba(10,14,19,0),…,.88)`；两处可滚容器分别溢出 296 / 290px | 深色底上观感很淡，但确在（`probe3/u22-*.png`） |
| 16 | U32 | PASS | `#log` 底色 `linear-gradient(90deg, rgba(7,11,15,.80) … 0)`，字 12px `#b9c6d4`，7 行可读（`probe2/10-u32b-log.png`） | 1080px 下 `#log` 被 `@media(max-width:1080px)` 整段 `display:none`，故在 1366 取证 |
| 17 | U21 / P14 / E39 | **PARTIAL** | 年表/右栏/图鉴已统一：`1 年 1 月–2 年 1 月`、`1 月`；无「第 N 年」、无小数年。**但**两处仍是旧写法：顶栏第 0 年显示「0 年 1 月」（`probe3/clock-year0.png`）；事件弹窗右上角「第 19 月」（`regress-full/24-event.png`，同刻顶栏为 1 年 8 月） | 「第 0 年只写 M 月」未落实；`ui/events.js:33` 仍 `第 ${W.month} 月` |
| 18 | E13 / U13 | PASS | 连续开关 chron/codex/settings 后 `#log` 子节点 0 个、文本空、无「打开」 | — |
| 19 | U09 | PASS | 图标 x：rest 20.5 → hover 21.0（Δ0.5px，肉眼不动）；标签 left 58、opacity 0→1、宽 22.9（从右侧淡入） | — |
| 20 | U12 | PASS | 空世界：环境段在，`.fbar` 3 条（水/光/温 值 `–`）+「还没有地块」（`regress-full/04-enter-empty.png`） | — |
| 21 | U24 | PASS | `.rp .h::after` content `none`、`::before` `none`；右栏段标题右侧无孤线 | `padding-right:12px` 仍在但不产生线 |
| 22 | U17 | PASS | 右栏生态色块实测 **8×8 px** + `inset 0 0 0 1px rgba(255,255,255,.14)` 描边；五色 `rgb(90,82,64)/(93,138,74)/(47,90,60)/(63,127,168)/(91,107,120)`，草原 vs 林地两种绿明度差约 2 倍、肉眼可分（`probe/u17-eco-chips.png` 2× 放大） | 原条目指的就是右栏生态那一列，不是创世页剖面图例 |
| 23 | U18 | PASS | 空世界三宫格取字符：`–` / `0` / `植物`（首字符为短横），无 `●` | — |
| 24 | P20 | PASS | 图鉴详情：`未定` 0 处；指标行只剩一格「◔ 0.028 W 半径」（无 disturb/reclaim 时不出行）；全文 `—` 仅出现在散文破折号「——」中 | 非占位符 |
| 25 | §1.5 / F32 | PASS | 关系链渲染为「共生 芦苇 芦苇沿水」/「共生 铁屑 矿上传得远」；裸键 `sym/foe/req/where/food` 命中 0；`note` 已显示 | — |
| 26 | P11 | PASS | 筛选行只有「已见 15 / 55」；全文无「池 x / y」（池分母已移到抽卡面板） | — |
| 27 | P28 / E12 | PASS | 20 行名高全部 20px（单行，`scrollWidth==clientWidth`，无截断/无省略号）；键名按钮 20/20 同宽（1080 下 131px、1366 下 130px，不是 856px） | — |
| 28 | E16 | PASS | 1080×700 下 `--hand-scale: .88`（log 里每张图一致） | — |
| 29 | 空格键 | PASS | 「随机释放」行按钮文案 `空格`（`probe/09-p28-keys.png`） | — |
| 30 | U23 | PASS | 右栏四段 156 / 117 / 200 / 170 = 643，面板 644（差 1px 边框），`docOverflow:0`；末段 `scrollHeight 452 > clientHeight 170` 走滚动 | 无死区，同意上轮「不复现」 |
| 31 | U28 / U31 | PASS | U31：26 条痕迹 → `traceGroup` 26 个 `RingGeometry`，opacity 0.16、外径 0.158，坐标与 `W.traces` 一一对应 → 圆环就是痕迹环；U28：`standeeTex` 3 变体（旋转 ±0.12rad、缩放 0.92/1/1.08），草另有 3 套叶型，每株再叠 0.84–1.18 缩放 + 位置抖动 + ±11% 明度抖动 | 同意上轮「误读/非 bug」判定；痕迹多时地面观感偏花，但每条都有数据依据 |

**合计**：PASS 29 / PARTIAL 2（#7 H01、#17 U21）/ FAIL 0 / 无法验证 0。

## 必须回炉的条目

1. **#17 U21 — PARTIAL**：① 顶栏第 0 年仍写「0 年 1 月」，应按 `UI.mon(0)="1 月"` 隐藏年位（`ui/hud.js:135-140` + `index.html` 的 `#cYear`）；② 事件弹窗仍写「第 19 月」（`ui/events.js:33`），应改用 `UI.mon(W.month)`。
2. **#7 H01 / U25 / F24 — PARTIAL（只剩虚空预览）**：拖到无格处时 `castRange(card, null) === []`，`marks` 全 0 —— 土地卡的正常落点没有范围预览，只有一行文字。复现：选土地卡，拖到虚空，`regress-probe2/void-drag-afterfix.png`（地面无任何高亮）。改法：`ui/hand.js:142` 在 `cell` 为空时用 `drag.pt` 造一个 fake cell 再算 range（`render/cam.js` 的 hover 分支已有同样做法）。
   - 注意：重复施卡（S03）与 Esc 反向施卡（S02）在工作树里已修，**不在回炉项内**；复测见 #7/#9 行。

## 附带观察（不计入 31 条，供排期参考）

- 工作树的 `cam.js` 守卫（`downOnCv`）没有误伤另外两条施卡路径：点世界施卡 1 次（`水@-4,-2`）、键盘 `D`+`Enter` 1 次（`水@-3,-3`）。
- 1080px 宽下 `#log` 被媒体查询整段隐藏，世界事件文本没有替代出口。
