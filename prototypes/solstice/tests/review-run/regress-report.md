# 回归验证报告（2026-11-04 第二轮 · HEAD 0cfbe56）

**范围**：只回答「上轮 31 条到底修没修」，不找新问题、不改源码。
**环境**：1080×700 主测（+ 1366×800 补证）；Edge headless；`deviceScaleFactor:1`（读数=CSS px）。
**快照**：**HEAD `0cfbe56`**（含第二轮全部修复提交）。31 条在本 HEAD 上整轮复跑过；`regress2-*` 目录里的读数就是这一轮刷新后的。
期间工作树被我上一版报告引用时的 11 个未提交修改（`cam.js 16:36`、`hand.js 16:46`、`boot.js 16:46`、`scene.js 16:46`、`creator.js 16:38`、`events.js/hud.js 16:39`、`panels.js/app.css 16:47`）已在 `89216f8` / `0cfbe56` 一系提交里落定。

**取证**
- 四场景：`tests/review-run/regress2-{full,narrow,keyboard,stress}/` —— 27 / 29 / 41 / 29 张，`failures:0 console:0`，1080×700 下 `--hand-scale` 全部 `.88`
- 全量读数：`tests/review-run/regress2-audit/`（`audit.json` + 9 张图）
- 指针/拖拽：`tests/review-run/regress2-pointer/`（`probes.json` + 8 张图，真实 pointerdown/move/up + 键盘）

| # | 编号 | 结论 | 读数/证据 | 备注 |
|---|---|---|---|---|
| 1 | F14 | PASS | `playSlot` → `{ok:false,msg:'这一带已经是地块了'}`；hand 8→8（同 id 仍在）、cells 81→81；toast「放不下 / 这一带已经是地块了」（`regress2-pointer/01-f14.png`） | 拒绝有说明、不扣牌 |
| 2 | P01 | PASS | 进阶包：标题「抽卡 · 进阶包（6 张 / 24 时素）」、底栏「撕开 · −24 时素」；时素 15 时 basic `disabled:false`、mid `disabled:true`；顶栏仍「抽包 12」 | 标题/价格/可买判定三处都跟随 |
| 3 | P02 | PASS | 撕开后点基础包：`packSel:'mid'`、`stage:2`、`packCards:6`，toast「这一包还没挑…」 | 牌没被吞 |
| 4 | P03 | PASS | 挑选阶段按钮「跳过这一包」，class `btn ghost` | — |
| 5 | U01 | PASS | `#bPack` 文案「抽包 12」= `min(packList.cost)` | — |
| 6 | U19 | PASS | cells 81、traces 空 →「最近的痕迹都已消散 · 见年表 ▸」；`neverTouched:false` | — |
| 7 | H01 / U25 / F24 | PASS | 拖到已有格：tip「松手放在 (-7, -2)」、`marks{n:1,g:0}`、松手 1 次 cast（`水@-7,-2`）。拖到虚空：tip「松手在这片虚空造陆」、**`marks{n:0,g:25}`（5×5 虚格预览已画出来）**、松手 1 次 cast `土地@-12,-1`、cells 225→250、飘字「+25 格」（`regress2-pointer/03b-h01-void-middrag.png`） | 上一版的「空心虚空无预览」已修；提示不再是「随机落点」 |
| 8 | H02 | PASS | 向下拖 62px：tip 仍 idle 灰字（非回收红）、`marks 0/0`、松手 `casts:[]`、cells/hand 不变 | — |
| 9 | U38 | PASS | 拖「水」（已有格必成功）→ Esc：`#ghost` 1→0、`casts:[]`、hand 8 张原样；随后 mouseup 仍 `casts:[]` | `334b2d8` 上这是真 bug（`endDrag(null)` 会顺手施卡），`hand.js` 的 `if (ev == null) return` 已修；我上一版曾误判为 PASS，见下节 |
| 10 | H04 / E01 | PASS | 合成连两次 `pointerdown`：`#ghost` 1→1（不出现第 2 张），left `148.95px`（不是 0,0）；松手后 0 张（`regress2-pointer/06-h04.png`） | — |
| 11 | H06 | PASS | 施土地后 `#gainfly`「+21 格」opacity 1；`pulses:8` 其中 `vis:1`（峰值 0.92、780ms、scale→2.23） | 飘字稳定；环是瞬时的，静帧靠飘字取证 |
| 12 | P04 / F26 | PASS | 撕开后 `.packpick` 6 张 / `.card` 6 张 / `.dealcard` 0 张；步骤条 2 站「1. 封印 / 2. 挑选」（`regress2-audit/12-pack-deal.png`） | 空白卡背那一站已并掉 |
| 13 | F12 / F13 | PASS | 同一句连发 4 次 → 1 条；再发 6 条不同句 → 上限 3 条 | — |
| 14 | P40 | PASS | 开面板前 `#toasts` 3 条 → 开后 0 条；新提示落 `#phead .toast.ih`（top 80–101），正文顶 120 | 不压标题/步骤条 |
| 15 | U22 / P12 | PASS | `.cdscroll::after`、`#pbody::after`、`#rightpanel .rp.grow::after` 三者均 `content:""`、`height:20px`、`position:sticky`、渐隐到 `rgba(10,14,19,.88)`；两个滚动容器分别溢出 378 / 358px | 上一版此处两个读数为空是探针先 closePanel 导致的，已按面板打开状态重测 |
| 16 | U32 | PASS | `#log` 底色 `linear-gradient(90deg, rgba(7,11,15,.80) … 0)`、12px、`#b9c6d4`；1366 下 8 行、**1080 下保留 3 行（176×64）**都可读（`regress2-audit/16-log.png`） | 早期快照 1080 下整段 `display:none`，`6107ee6` 已改成「窄屏留三行」 |
| 17 | U21 / P14 / E39 | PASS | 顶栏第 0 年：`#cYear` `display:none`，画面只剩「1 月 春」（`regress2-audit/17-clock-year0.png`）；事件弹窗「1 年 8 月」与顶栏一致（`regress2-full/24-event.png`）；年表「1 年 1 月–2 年 1 月 / 1 月」；`第 N 年`、小数年、`第 N 月` 命中均为 0 | 上一版的两处旧写法（顶栏 0 年、事件「第 19 月」）都已修 |
| 18 | E13 / U13 | PASS | 连续开关 chron/codex/settings 后 `#log` 子节点 0、文本空 | — |
| 19 | U09 | PASS | 图标 x：rest 20.5 → hover 21.0（Δ0.5px）；标签 left 58、宽 22.9、opacity 0→1 | 从右侧淡入，图标不横移 |
| 20 | U12 | PASS | 空世界环境段在：`.fbar` 3 条（水/光/温 = `–`）+「还没有地块」 | — |
| 21 | U24 | PASS | `.rp .h::after` / `::before` content 均 `none` | — |
| 22 | U17 | PASS | 生态色块 5 个全部 **8×8 px** + `inset 0 0 0 1px rgba(255,255,255,.14)`；草原 `rgb(93,138,74)` vs 林地 `rgb(47,90,60)` 明度差约 2 倍 | 条目指右栏生态列，不是创世页剖面图例 |
| 23 | U18 | PASS | 空世界三宫格取字符：`–` / `–` / `–`（无 `●`） | — |
| 24 | P20 | PASS | 图鉴详情 `未定` 0 处；指标行只剩「◔ 0.050 W 半径」「↩ +2 回收」两格（无 disturb 时不出行）；全文 `—` 只出现在散文破折号「——」（`铸给死人听的——后来…`）（`regress2-audit/24b-codex-detail.png`） | 非占位符 |
| 25 | §1.5 / F32 | PASS | 关系链「共生 铁屑 矿上传得远」；裸键 `sym/foe/req/where/food` 命中 0；`note` 已显示 | — |
| 26 | P11 | PASS | 筛选行只有「已见 12 / 55」；全文无「池 x / y」 | — |
| 27 | P28 / E12 | PASS | 20 行名高全部 20px（单行、无省略号溢出 `overflow:0`）；键名按钮 20/20 同宽 130px | 不是 856px |
| 28 | E16 | PASS | 1080×700 下 `--hand-scale: .88`（四场景 log 每张图一致） | — |
| 29 | 空格键 | PASS | 「随机释放」行按钮文案 `空格`（`regress2-audit/27b-keys-open.png`） | — |
| 30 | U23 | PASS | 右栏四段 164 / 125 / 208 / 246 = 743，面板 744（差 1px 边框），`overflow:0`；末段 `scrollHeight 460 > clientHeight 246` 走滚动 | 无死区，同意上轮「不复现」 |
| 31 | U28 / U31 | PASS | U31：48 条痕迹 ↔ `traceGroup` 48 个 `RingGeometry`，opacity 0.16、外径 0.158，坐标与 `W.traces` 一一对应；U28：立牌 3 变体（旋转 ±0.12rad、缩放 0.92/1/1.08）+ 草 3 套叶型 + 每株 0.84–1.18 缩放 + 位置/明度抖动 | 同意上轮「误读/非 bug」 |

**合计**：PASS 31 / FAIL 0 / PARTIAL 0 / 无法验证 0。

## 相对我上一版报告的四处变化（都在工作树里，不是我改的）

1. **S03 一次拖拽施两张牌** → `cam.js` 的 `downOnCv` 守卫；复测一次拖拽 1 次 cast（`casts:["水@-5,-1"]`），点世界施卡与 `D`+`Enter` 仍各 1 次。
2. **S02 Esc 反向施卡** → `hand.js` 的 `if (ev == null) return`；复测 Esc 后 0 次 cast。**我上一版把这条判成 PASS 是假 PASS**（当时拖的是会在该落点失败的土地卡），已在 #9 更正。
3. **S08 拖拽中 Space/X 再施一张** → `boot.js` 的 `UI.isDragging()` 守卫；复测 Space、X 在拖拽中 `casts:[]`，松手各 1 次。
4. **#17 U21 月份写法 / #7 虚空 5×5 预览** → `hud.js`、`events.js`、`hand.js` 已修；复测见 #17、#7。

## 附带观察（不在 31 条内，本轮验证窗口里撞到/被问到的）

- **面板键盘可达（r-state 的 S25/S26）——在 `0cfbe56` 上已修**，我独立复核：Tab 到「图鉴 0/6」→ Enter，`codexTab: cards → ach`；面板开着按 K → `settings/keys`、按 H → `settings/input`。当前守卫按 `UI.KEYS` 的「面板」组放行，并给面板内聚焦的 Enter/Space 开了口子。
- **事件叠层（r-state 的 S09）——仍在**：事件开着（`panel:null`）按 L 会把卡库面板开在事件遮罩后面（`event:true, panel:'codex'`），接着按 Esc 先关事件、再露出面板。不在 31 条内，未计入结论。
- `ui/hand.js` 的扇形让位从 `-24` 改成了 `-24-34`（窄屏最外卡压右栏），本轮未测该项（不在 31 条内）。
