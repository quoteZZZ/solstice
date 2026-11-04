# 状态机审计 · Solstice 原型

- 审计对象：`E:/solstice/prototypes/solstice`，起始 HEAD `334b2d8`；最终复核于工作区 17:08（HEAD `0cfbe56` + 未提交的 `boot.js`/`creator.js` 第三轮改动）
- 复核：`89216f8` 修掉 S01–S04/S07/S08/S20（S05 被按键守卫顺带堵死）；工作区第三轮又修掉 S06、S25、S26。已用 `state-probe7/8/9` 逐条复测（对应 `.txt`）。**共 11 条已收掉**；未修 15 条读数对应 `334b2d8`（S09 在两版上一致复现）。
- 视口：1680×1000，msedge headless，`file://` 直开
- 取证脚本（均在 `tests/review-run/`，未改任何源码）：`state-probe.js`（A–E 28 场景）、`state-probe2.js`（F 面板/焦点）、`state-probe3.js`（G 拖拽交叉）、`state-probe4.js`（H 调用栈）、`state-probe5.js`（I 跨世界/撤销）、`state-probe6.js`（拖放两次 cast 的栈）、`state-probe7.js`（工作树复核）、`state-probe8.js`（第二轮提交复核）、`state-probe9.js`（第三轮复核）
- 读数：`state-probe*.json` / `state-probe6.txt` / `state-probe7.txt` / `state-probe8.txt` / `state-probe9.txt`；截图：`state-shots/`
- 结论：**26 条问题**（P1×4、P2×11、P3×11），其中 11 条已修复并复核通过，**当前未修 15 条**（P2×6、P3×9）。抽卡主流程（撕开→挑选→跳过）是稳的；未修问题集中在**事件叠层、跨世界残留、焦点与模式互斥**三块。

---

## 30 项勾选表

| # | 项 | 结论 | 关联 |
|---|---|---|---|
| 1 | 0→撕开→挑选→收下一张 | OK（手牌满时走替换，替换目标有坑见 S05） | S05 |
| 2 | 0→撕开→挑选→跳过 | OK（关面板、stage 归 0；已付费 6 张丢弃，文案即「跳过这一包」） | — |
| 3 | 0→撕开→关面板→再开 | OK（stage 2 / 6 张保留） | — |
| 4 | 0→撕开→切包 | OK（被拦，toast「这一包还没挑」） | — |
| 5 | 0→撕开→时素归零→再点 | OK（主按钮变「跳过这一包」并关闭；再撕开因 disabled 不可点） | — |
| 6 | 手牌满时收下 | 已修（按键守卫后 1–8 改不了选，标签与替换槽一致；根因仍在 S05） | S05 |
| 7 | 连抽两次 | OK（两轮 −12，手牌恒 8：收下即补位） | — |
| 8 | 撕开后按 B 再开 | 部分修复（空格/X 穿透已修；B 同键仍只重绘不关闭） | S15 |
| 9 | 四面板互切 | OK（B/L/C/O 各自替换，无叠加） | — |
| 10 | 开面板→Esc | OK（关闭） | — |
| 11 | 开面板→点遮罩 | OK（关闭） | — |
| 12 | 同键再按关闭 | 有问题（不关，只重建 innerHTML，滚动位置丢） | S15 |
| 13 | 卡库→详情→Esc | 有问题（直接退出整个卡库；重开仍停在详情） | S22 |
| 14 | 设置分组→关→再开 | 有问题（分组保留本身可接受，但 K 之后按 O 落在「键位」） | S16 |
| 15 | 键位→改绑→Esc | 已修（工作树复核：等待中关面板/切分组都会清等待态） | S04 |
| 16 | 年表筛选→关→再开 | OK（筛选保留，条目数一致：气候 7 组） | — |
| 17 | 摆件→开面板→退出摆件 | 有问题（面板正常，但退出摆件后面板仍开、模式被降级为「全部」） | S24 |
| 18 | 摆件→Esc | OK（回「全部」） | — |
| 19 | 摆件→拖手牌 | 有问题（手牌只下移 222px，仍有 27px 命中区，可拖可施卡） | S11 |
| 20 | 创世→返回当前界 | 部分修复（空世界退路 S01 已修；重掷改 RNG 仍未修） | S13 |
| 21 | 创世→进新区 | 部分修复（旧包残局 S06 已修；事件跨世界 S10 仍在） | S10 |
| 22 | 事件三路→Esc→再触发 | 有问题（承受/改写/Esc 都关且能再触发；但可与面板/创世叠层） | S09 |
| 23 | WASD→Enter→Esc | OK（游标移动并施卡；无游标时 Enter 静默，见 S21） | S21 |
| 24 | 面板里按 WASD | OK（WASD 不穿透；焦点在面板里时 Space 交给原生按钮，不再施卡） | — |
| 25 | Tab 绕一圈 | 部分修复（Enter/Space 已能激活面板按钮；但焦点仍要 25 次才进面板） | S12 |
| 26 | 拖拽中按功能键 | 已修（Esc 真取消；拖拽中 Space/X/数字被吞，松手只施 1 张） | S02/S08/S20 |
| 27 | 暂停→施卡→恢复 | OK（月份不动、卡生效、恢复后继续走） | — |
| 28 | 4×→连续施卡 | OK（6 连施无异常，档位保持） | — |
| 29 | 时素归零 | OK（抽包 disabled、重抽/改写不可用；可回收/施卡，等时素自然回涨） | — |
| 30 | 牌池抽空后抽卡 | 有问题（构造态：仍扣 12 时素、摊 0 张、无空态） | S18 |

> 说明：手牌上限 8，`S.consume` 会立刻补位，所以「手牌张数」不变不代表没施卡；本文所有施卡判定都用 `UI.cast` 调用次数 / 格子数 / 卡 id。

---

### S01 · 模式 · 空世界按 G 进创世后没有任何退路

- **路径**：
  1. 进入任意一界，**一格都不打**
  2. 按 G
  3. 按 Esc（两次）
- **期望**：能回到当前界；至少 Esc 可退。
- **实际**：`openCreator` 用 `cells.size` 判 `fromWorld`，空世界直接 false → 底部没有「← 返回当前界」，Esc 分支 `if (UI.creator.fromWorld) closeCreator()` 也不执行。两次 Esc 后创世仍在。唯一出口是「进入这一界 ▸」＝放弃当前界的界名/气候/坐标。
- **证据**：`state-shots/19-creator-emptyworld.png`（无返回按钮，主按钮「进入这一界 ▸」）；`state-probe.json` C4：`{cells:0, fromWorld:false, hasBack:false, creatorAfterEsc:true, creatorAfterEsc2:true}`
- **严重度**：P1（工作树已修复）
- **建议**：用「本局是否已开局」的标志判可退（boot 时置位），不要用 `cells.size`；或让 Esc 在任何时候都能关创世，「第一次不许退」只绑在 boot 的首次。
- **状态**：工作树已按此修（`ui/creator.js` 改用 `UI.state.started`）。`state-probe7.txt` S01：`fromWorld:true, hasBack:true, creatorAfterEsc:false`。

---

### S02 · 拖拽 · 按 Esc「取消」会立刻把牌打出去

- **路径**：
  1. 世界有地
  2. 按住第一张手牌，往上拖到画布内（出现落点预览）
  3. 按 Esc
  4. 松手
- **期望**：取消本次拖拽，不施卡、不消耗。
- **实际**：`drag.esc` 调 `endDrag(null)`，但 `endDrag` 不读参数，`d.mode === 'cast'` 时照样 `UI.cast(d.i,…)`。Esc 当场施出拖着的牌；松手时画布 `pointerup` 又走 `cam.onClick`，再施一张。实测一次「取消」两次成功施卡：`fertile@(-1,3)`、`rowan@(-1,-1)`，手牌首位从 fertile 变 rowan。
- **证据**：`state-shots/p4-2-drag-esc.png`；`state-probe4.json` H2：`slots` 两条 `ok:true`，栈分别为 `endDrag (hand.js:183) << esc (hand.js:90)` 与 `onClick (boot.js:184)`
- **严重度**：P1（工作树已修复）
- **建议**：`endDrag` 增加真正的取消分支（`endDrag(ev, {cancel:true})`），Esc 先 `drag=null` 再清理；同时修掉画布 `pointerup` 的重复施卡（见 S03）。
- **状态**：工作树已修（`hand.js` 新增 `if (ev == null) return`，`cam.js` 新增 `downOnCv`）。`state-probe7.txt` S02：Esc 后 `casts:0`、`ghost:0`，随后 mouseup 仍 `casts:0`。

---

### S03 · 拖拽 · 一次拖放会调用两次 `UI.cast`，第二张是补位后的新牌

- **路径**：
  1. 空世界（0 格）
  2. 把 0 号位设为「土地」，拖到虚空，松手
- **期望**：施 1 张、铺 1 片。
- **实际**：`cam` 的 `pointerup` 先触发 `onClick → UI.cast(W.sel)`；紧接着 `document pointerup` 触发 `endDrag → UI.cast(d.i)`。第一次成功后 `consume` 在 0 号位补了一张新牌，第二次拿**这张新牌**再打同一个点：
  - 虚空场景：第二张不能落在虚空 → 铺地成功的同时弹「要放在地块上 / 虚空承不住这张卡」；
  - 有格场景：第二次同样 `ok`，格子 25→94。
- **证据**：`state-shots/p5-1-drag-void.png`（25 格 + 红 toast）；`state-probe6.txt` 两组都记录到两次 `UI.cast`，栈为 `onClick (boot.js:184)` 与 `endDrag (hand.js:183)`
- **严重度**：P1（工作树已修复）
- **建议**：二选一——拖拽结束后抑制画布 click（`drag.consumed=true`），或把 `cam.onClick` 只绑在「pointerdown 也发生在画布」的点击上；`endDrag` 成功后置一个短锁。
- **状态**：工作树已修（`cam.js` 的 `downOnCv`）。`state-probe7.txt` S03b（虚空土地）：`casts` 1 条、`cells:25`、无「要放在地块上」的假 toast。

---

### S04 · 键位 · 改绑等待态能活过面板关闭 / 分组切换，之后任意键都会静默改键

- **路径**：
  1. K 打开设置→键位
  2. 点「抽卡」右侧的 B（显示「等待按键…」）
  3. **点遮罩关闭面板**
  4. 按 W
- **期望**：关面板即取消改绑，W 移动游标。
- **实际**：`waitingFor` 只在 `captureKey` 里清；`closePanel` 不清。`captureKey` 在 `onKey` 最前面，于是 W 被当成新键：toast「已改键『回收选中』→ W」，游标不动。变体：等待中点左侧「画面」→ 等待提示消失（`keysBody` 重绘）但 `waitingFor` 还在，按 B 又改一次键。W 后来又会被 WASD 分支抢走，等于把「回收」绑成了一个死键。
- **证据**：`state-shots/13-rebind-leak.png`（面板已关，顶部「已改键『回收选中』→ W」）；`state-shots/p2-08-rebind-group-switch.png`；`state-probe.json` B5：`keysAfterW: recycle=W`、`cursorAfterW: null`；`state-probe2.json` F7：`waitingAfterSwitch: []` 但 `panelAfterB: settings` + 「已改键『抽卡』→ B」
- **严重度**：P1（工作树已修复）
- **建议**：`closePanel` / `openPanel` / 点 `[data-sg]` 时统一 `waitingFor=null`（最好把等待态放进 `UI.state`，随面板重建一起销毁）；`captureKey` 里对「没有 `[data-k].等待` 元素」的情况直接视为过期。
- **状态**：工作树已按此修（`panels.js` 三处清 `waitingFor`）。`state-probe7.txt` S04：关面板后按 W 只移动游标（`cursor:[-1,-1]`），键位表 `recycle=X` 未变；等待中切分组后按 B 打开的是抽卡面板（`panel:"pack"`），不再改键。

---

### S05 · 抽卡 · 手牌满时「换掉 X」不跟随 1–8 改选，实际换掉另一张

- **路径**：
  1. 手牌 8/8，选中第 3 张（不锈）
  2. B → 撕开
  3. 按数字键 5（选中变成第 5 张「苔」）
  4. 点包内任意一张
- **期望**：面板写着换掉哪张，就换掉哪张；或数字键在抽卡面板里改的是包内选牌。
- **实际**：`packBody` 在渲染时把 `swap` 快照进 HTML，数字键只改 `W.sel` + `renderHand()`，不重绘 overlay。面板仍写「换掉 不锈」，`takeFromPack` 按新 `sel=4` 换掉「苔」。手牌被遮住，玩家没有任何办法看出选中的变了。
- **证据**：`state-shots/07-pack-full-hand.png`（标签「换掉 不锈」）；`state-probe.json` A6：`{sel0:2, label1:"不锈", selAfterKey5:4, labelAfterKey5:"不锈", replacedSlot:4, targetNameBefore:"苔"}`
- **严重度**：P2（已修复）
- **建议**：`W.sel` 变化时若 `UI.state.panel==='pack'` 就 `renderOverlay()`；或在面板里画一条 8 格手牌，允许直接点选替换目标。
- **状态**：`89216f8` 的面板按键守卫让「面板开着按 1–8」不可达（`state-probe8.txt` S05：按 5 后 `sel` 仍 2、标签仍「不锈」、替换槽 2 与标签一致）。根因（`packBody` 只在渲染时快照 `swap`）还在，若日后有其它途径改 `W.sel` 会复发。

---

### S06 · 创世 · 新区继承旧世界里没挑的包

- **路径**：
  1. 撕开一包（−12 时素，stage 2，6 张）
  2. Esc 关面板
  3. G → 进入这一界 ×2
  4. B
- **期望**：新世界抽卡状态归零。
- **实际**：`enterWorld` 不重置 `UI.packStage / UI.packCards`。新世界 0 格、0 月、时素 0，但 B 打开后直接摊着上一界的 6 张，可免费挑一张，还能「换掉 土地」。
- **证据**：`state-shots/20-creator-new-world-pack.png`（时素 0、0 格、却是 stage 2 的 6 张）；`state-probe.json` C5：`{monthAfter:0, cellsAfter:0, essenceAfter:0, stage2:2, cards2:6}`
- **严重度**：P2（已修复）
- **建议**：`enterWorld` 里清 `packStage/packCards`（`packSel` 可选保留）；任何 `newWorld` 都把「UI 临时状态」列一张清单一起重置。
- **状态**：工作区第三轮已在 `creator.js` 的 `enterWorld` 里清 `UI.packStage / packCards`。`state-probe9.txt` S06：新世界 `{month:0, cells:0, ess:0}` 后按 B 得到 `{stage:0, cards:0}`。

---

### S07 · 面板 · 面板开着时 Space / X / R / J 穿透到世界与手牌

- **路径**：
  1. L 打开卡库
  2. 依次按 Space、X、R、J
- **期望**：面板期间禁用世界操作（至少不静默执行）。
- **实际**：`onKey` 只对方向键/Enter 判 `panel`；空格 → `castRandom` 施卡（世界在面板后变化，`flyTo` 的「+5 格」还飘在面板上）、X → 回收选中、R → 重抽、J → 换档。面板自身不刷新，手牌与面板读数跟真实状态脱节。
- **证据**：`state-shots/p2-01-panel-keythrough.png`（卡库面板 + 顶部「回收 +1 时素 撤销」+ 右侧「+5 格」）；`state-probe2.json` F1：`castDelta:1`（空格）、`handChanged:true`（X/R）、`gear:3`（J）
- **严重度**：P2（已修复）
- **建议**：`onKey` 在 `UI.state.panel || UI.state.event` 时只放行 Esc 与「面板入口键 + 数字键（如果确实要做包内选牌）」，其余直接 return。
- **状态**：`89216f8` 加了面板/拖拽守卫；工作区第三轮把放行列表改成按 `UI.KEYS` 的「面板」组判定，并让焦点在 `#panel/#eventcard` 内的 Enter/Space 直接 `return` 给原生按钮。`state-probe9.txt`：卡库开着按 Space 时 `castDelta:0`、`cells` 不变；拖拽中 `spaceInPanel:0`、松手 1 次。

---

### S08 · 拖拽 · 拖拽中按 Space / X / 数字键会叠加施放，一次手势最多 2–3 张

- **路径**：
  1. 按住手牌拖到世界上方
  2. 按 Space
  3. 松手
- **期望**：空格或松手，只生效一个。
- **实际**：Space 先 `castRandom` 施一张；松手时 `endDrag` 用旧下标再施一张。HEAD `334b2d8` 还有画布 `pointerup` 的第三次（land / warm / reed）；工作树修掉重复 click 后仍是 **2 张**（`state-probe7.txt`：land / rustless）。X、数字键同理会先回收/改选，再按旧下标施卡。
- **证据**：`state-shots/p2-03-drag-space-count.png`；`state-probe3.json` G2（HEAD）：`{mid:["…land"], end:["…land","…warm","…reed"]}`；`state-probe7.txt` S08（工作树）：`{midCasts:1, endCasts:2}`
- **严重度**：P2（已修复）
- **建议**：`beginDrag` 置 `UI.state.dragging=true`，`onKey` 在拖拽期间屏蔽所有世界键（Esc 除外，且 Esc 要真取消）；`endDrag` 之后复位。
- **状态**：`89216f8` 用 `UI.isDragging()`（`hand.js:71`）在 `boot.js:99` 拦住拖拽期间除 Esc 外的所有键。`state-probe8.txt` S08：拖拽中按 Space / X / 1 均 `midCasts` 不增，松手各只 +1；`state-probe7.txt` 的 `{mid:1,end:2}` 已是修复前的历史读数。

---

### S09 · 面板 · 事件与面板 / 创世可以三层叠加，Esc 逐层剥

- **路径**：
  1. 有地块 → E 开事件
  2. L 开卡库（事件仍在最上）
  3. Esc → Esc
- **期望**：事件期间不响应面板键；或开面板前先收事件。
- **实际**：`fireEvent` 会把 `U.panel` 置 null，但**反过来** `openPanel` 不检查 `U.event`。codex（z60）被事件（z82）盖住；Esc 只关事件，露出卡库。`89216f8` 的按键守卫只挡「面板开着按 G」，事件开着（panel 为 null）时 G 仍会开；创世（z80）塞在事件下面，Esc 两次才回到世界。
- **证据**：`state-shots/22-event-panel-stack.png`（事件前景、卡库背景可见）；`state-shots/p2-07-event-creator.png`（事件前景、创世剖面在背后）；`state-probe.json` C6：`stackEvent:true, stackPanel:"codex"`；F6：`afterG:{event:true, creator:true}`；`state-probe8.txt` S09：`{event1:true, panel1:"codex"}`，事件-only → G 仍 `{event:true, creator:true}`
- **严重度**：P2
- **建议**：`openPanel` / `openCreator` 入口处 `if (UI.state.event) return UI.toast('先处理事件', …)`，或在打开前 `closeEvent()`。

---

### S10 · 事件 · 事件能跨世界：旧世界的后果写进新世界年表

- **路径**：
  1. 有地块 → E（抽到「冰川期」）
  2. G → Enter ×2 进入新区
  3. 点「承受」
- **期望**：进新区时关闭/丢弃旧事件。
- **实际**：`U.event` 不清。新世界 0 格 0 月，事件卡仍在最上层，点「承受」后年表出现 `0 月 · 灾变 · 冰川期：被承受`（`apply` 对 `anyCell()` 为 null 做了兜底，所以不崩，但记录落在新世界）。
- **证据**：`state-shots/p5-2-event-new-world-bear.png`（「世界是空的」+ toast「已承受『冰川期』写进年表」）；`state-probe5.json` I2：`after.chron: ["0|灾变|冰川期：被承受", …]`
- **严重度**：P2
- **建议**：`enterWorld` / `closeCreator` 清 `U.event` 并 `renderEvent()`；事件对象若绑定世界，应随世界一起失效。

---

### S11 · 摆件 · 摆件模式下仍能从手牌残条拖牌施卡

- **路径**：
  1. P 进摆件模式
  2. 在屏幕最底部露出的约 27px 手牌上按住，往上拖
  3. 松手
- **期望**：摆件模式禁用手牌交互（或整条移出视口）。
- **实际**：`#stage.ambient #handwrap{transform:translateY(222px)}` 只是位移，卡仍可命中；拖拽照常创建 ghost 并施卡，cells 81→86。玩家在「观赏模式」误触底部会凭空打出一张看不见的牌。
- **证据**：`state-shots/17-ambient-drag.png`（ghost + 落点提示 + 底部露出的卡头）；`state-probe.json` C2：`{ghostWhileDrag:1, cellsBefore:81, cellsAfter:86}`
- **严重度**：P2
- **建议**：ambient 时 `#handwrap{pointer-events:none}`，或 `translateY(100%)` + `inert`；`beginDrag` 里也判一次 `UI.state.uiMode==='全隐'`。

---

### S12 · 焦点 · 面板不接管焦点，Tab 在背景绕 24 次才进面板

- **路径**：
  1. L 开卡库
  2. 连按 Tab
- **期望**：焦点进入面板控件（或做焦点陷阱）。
- **实际**：前 24 个焦点全在背景：顶栏档位按钮、`bPack`、左栏 5 项、右栏、8 张手牌、actbar……第 25 个才进面板。焦点停在左栏「年表」时按 Enter，**当前开着的卡库被直接换成 年表**（背景控件在面板下照常可操作）。
- **证据**：`state-shots/p2-06-tab-count.png`；`state-shots/p8b-rail-enter.png`（Enter 前是卡库，之后变成「年表 · 25 条」）；`state-probe2.json` F5：`{firstPanelTab:25, total:30}`，前 20 个全是 `@bg`；Tab×6 后 `focus.className:"rail", dataset.panel:"chron"`，Enter 后 `panel:"chron"`
- **严重度**：P2
- **建议**：打开面板时 `#overlay` 设 `role=dialog` + `aria-modal`，聚焦第一个可操作项；Tab 循环限制在 `#panel` 内。

---

### S13 · 创世 · 「重掷后返回当前界」会改掉当前世界的 RNG

- **路径**：
  1. 世界已有 81 格、6 月
  2. G → 点「重掷」→ 点「← 返回当前界」
- **期望**：世界与进入创世前完全一致（同一局）。
- **实际**：`rollWorld` 调 `S.cmd.seed(新坐标)`，返回时不恢复。格子/月份/时素不变，但 `rngState` 变了：`digest.hash 75e21069 → 163fe1d3`、`rng 455945787 → 2384935785`。之后的繁殖/抽卡流与「没进过创世」的世界分叉。
- **证据**：`state-probe.json` C3（两次 digest 对比）
- **严重度**：P2
- **建议**：进创世时保存 `rngState`，`closeCreator` 且未换界时恢复；或 `rollWorld` 用一个独立的临时 RNG（`S.rand` 之外）。

---

### S14 · 键位 · 改绑到 W / 1 / F1 不报冲突，新键永远不生效

- **路径**：
  1. K → 点「抽卡」→ 按 W（提示「已改键『抽卡』→ W」，无冲突）
  2. Esc 关面板
  3. 按 W / 按 1
- **期望**：绑定时警告冲突，或 W 能打开抽卡。
- **实际**：`keyConflicts` 只比对 `UI.KEYS` 里的字符串，而 WASD / 1…8 / F1–F4 存的是整串（`'WASD'`、`'1 … 8'`、`'F1–F4'`），永远不冲突。`onKey` 里 DIRS/数字/F1–F4 分支都在 `actOf` 之前：W 只移动游标（cursor→[-1,-1]，面板不打开），1 只选卡。新绑的键是死键。
- **证据**：`state-shots/28-rebind-conflict.png`；`state-probe.json` D4：`pack=W` 后按 W `panelAfterW:null`、`cursorAfterW:[-1,-1]`；`recycle=1` 后按 1 `selAfterKey1:0`
- **严重度**：P2
- **建议**：冲突表把 WASD、1–8、F1–F4 展开成单键；或把这些键标为不可绑定（`ed:false` 已有这个字段，只是没拦住）。

---

### S15 · 面板 · 同一个功能键再按不会关闭，只重绘（滚动位置被重置）

- **路径**：
  1. C 开年表（滚到中段）
  2. 再按 C
- **期望**：toggle 关闭；至少不动。
- **实际**：`openPanel` 无条件 `U.panel=p; renderOverlay()`，面板不关；`innerHTML` 重建让滚动位置回到顶部。B/L/O/K 同样。
- **证据**：`state-probe.json` B1：`panelBeforeRepeat:"settings" → panelAfterSameKey:"settings"`；A8：`panelAfterB:"pack"`、stage 2 保留
- **严重度**：P3
- **建议**：`onKey` 里入口键做 toggle：`UI.state.panel===p ? closePanel() : openPanel(p)`；重绘时保留 `#pbody` 内滚动容器的 `scrollTop`。另：第二轮守卫放行 B/L/C/O 却漏了 K/H，见 S26。

---

### S16 · 设置 · 分组跨开合保留，导致 O 打开的可能是「键位」

- **路径**：
  1. O 开设置（画面）
  2. 点左侧「界面」
  3. Esc → 再按 O
- **期望**：O 是「设置」入口，至少与 K（键位）有区分。
- **实际**：`U.setGroup` 持久；`openPanel('keys')`/`('help')` 会强改 `setGroup`。B4 实测：`video → ui → O 仍 ui → K → keys → O 仍是 keys`。K 与 O 打开同一屏，左栏高亮仍在「设置」，玩家分不清入口。
- **证据**：`state-shots/11-settings-group.png`（标题「设置」，左栏高亮「键位」）；`state-probe.json` B4
- **严重度**：P3
- **建议**：O 显式 `setGroup='video'`（或各入口记各自的默认组），只有 K/H 才强制。

---

### S17 · 设置 · 「常驻栏」高亮永远停在「全部」，与真实模式相反

- **路径**：
  1. O → 界面
  2. 点「全隐」（左右栏/手牌已隐藏）
  3. 看这一行
- **期望**：高亮「全隐」。
- **实际**：`cur: () => UI.uiMode || '全部'`，而 `setUiMode` 写的是 `UI.state.uiMode`；`UI.uiMode` 永远是 undefined。于是无论选哪个，都回退到「全部」并高亮它。
- **证据**：`state-shots/p2-04-settings-uibar-on.png`（背景已全隐，按钮仍高亮「全部」）；`state-probe2.json` F3：`{modeAfterSet:"全隐", highlighted:["全部*","仅右栏","全隐"]}`
- **严重度**：P3
- **建议**：改成读 `UI.state.uiMode`（全仓唯一真源）。

---

### S18 · 抽卡 · 牌池抽空时仍扣 12 时素、摊 0 张、无空态（构造态）

- **路径**：
  1. 让 `S.drawPool()` 的 50 张全部在手牌里（模拟池空）
  2. B → 撕开
  3. 观察面板；关闭再开
- **期望**：拒绝开包或退款。
- **实际**：`drawPack` 先 `W.essence -= cost`，再逐张 `drawOne` 全返回 null → `got=[]`。面板进入「挑选」阶段但一张牌都没有，也没有空态文案，只能按「跳过这一包」；关掉再开时 `openPanel` 因 `packCards.length===0` 把 stage 重置回 0，钱和包都不留痕迹。
- **证据**：`state-shots/32-empty-pool.png`（时素 488、池 50/53、0 张）；`state-probe.json` E4：`essence 500→488, cards:0, picks:0, label:"跳过这一包"`
- **严重度**：P3（正常手牌上限 8，构造态才可达；但逻辑本身没有兜底）
- **建议**：先抽后扣，或 `got.length===0` 时退款 + toast；面板加空态。

---

### S19 · 面板 · 打开任意面板会清空「回收 · 撤销」的 toast

- **路径**：
  1. 回收一张 >0 环的卡（出现「回收 +1 时素 撤销」）
  2. 3.8 秒内按 L 开卡库
- **期望**：撤销入口在生命期内仍可用。
- **实际**：`openPanel` 第一行 `#toasts.innerHTML=''`（为了清世界提示），把带 action 的撤销条一起清掉。`recycleLog` 还在，但界面上没有任何入口。
- **证据**：`state-shots/p5-3-undo-cleared.png`（卡库里无撤销条）；`state-probe5.json` I3：`hasUndo:true → canUndo:false`
- **严重度**：P3
- **建议**：只清 `.toast:not(:has(.toact))`，或把撤销移到顶栏状态区。

---

### S20 · 拖拽 · 拖拽中开面板：ghost 浮在面板上，松手后静默取消

- **路径**：
  1. 按住手牌拖到画布
  2. 按 B（抽卡面板打开）
  3. 松手
- **期望**：开面板时主动结束拖拽并提示。
- **实际**：`#ghost`（z64）压在面板（z60）上继续跟随，还带着「松手铺在 (0,0) 周围」的提示；松手后因 `elementFromPoint` 已是面板、`inWorld=false`，拖拽被判 idle，什么都不发生（cells 81 不变），也没有 toast。
- **证据**：`state-shots/p7-ghost-over-panel.png`（ghost 盖在抽卡面板中央）；`state-probe4.json` H3：`ghost 1→1→0, casts 0`
- **严重度**：P3（已修复）
- **建议**：`beginDrag` 期间打开面板/事件/创世 → 先 `endDrag(null, cancel)`，并给一条「已取消拖拽」。
- **状态**：`89216f8` 的拖拽守卫把 B 也挡在拖拽期间，面板不再可能在拖拽中打开。`state-probe8.txt` S20：`panelMid:null, overlayMid:false, ghostMid:1`，松手 `casts:1, cells:81`。原「ghost 压在面板上」的症状已不可达。

---

### S21 · 键盘 · 没有游标时 Enter 完全静默

- **路径**：
  1. 进入世界后不碰鼠标/方向键
  2. 选中「土地」
  3. 按 Enter
- **期望**：落在相机中心，或提示「先移动游标」。
- **实际**：`cursor===null` 时 `if (ev.key === 'Enter' && cursor && !blocked)` 直接跳过；无 toast、无标记，键位表却写着「Enter 在游标处施卡」。
- **证据**：`state-probe2.json` F8：空手牌补一张后 `castAfterEnter:0`；`state-probe.json` D1 只有先按 W/W/D 后 Enter 才施卡
- **严重度**：P3
- **建议**：首次 Enter 用 `R.cam.target` 四舍五入建立游标，并复用同一套 `syncCursor`。

---

### S22 · 面板 · 卡库详情按 Esc 直接退出整个卡库（不是回列表），重开仍停在详情

- **路径**：
  1. L 开卡库
  2. 点一张卡（右列变详情）
  3. Esc → 再按 L
- **期望**：Esc 先回卡库列表，再按才关；或重开回到列表。
- **实际**：详情不是独立一层（`codexSel` 只控制右列），Esc 走 `closePanel()` 全关；`codexSel` 保留，重开还停在同一张，`codexEl` 也跟着这张卡的元素。
- **证据**：`state-shots/10-codex-detail.png`；`state-probe.json` B3：`{panelAfterEsc:null, reopenSel:"terrain.mineral.0.matter.water"}`
- **严重度**：P3
- **建议**：`onKey` 的 Esc 分支加一层：`if (panel==='codex' && codexSel) { codexSel=null; renderOverlay(); return; }`。

---

### S23 · 提示 · 带操作的 toast 走 z70，压在面板头（P40 复现）

- **路径**：
  1. L 开卡库
  2. 按 X 回收
- **期望**：提示进 `#phead` 的行内小条（源码注释承诺的路径）。
- **实际**：`UI.toast` 只有在 `!act` 时才走 `#phead`；回收 toast 带「撤销」按钮，于是走 `#toasts`（z70 > 面板 60），浮在面板顶端、与标题分离，并遮住面板上沿。
- **证据**：`state-shots/p2-01-panel-keythrough.png`（面板上方独立的「回收 +1 时素 撤销」框）；`hud.js:82` 的注释与实际分支
- **严重度**：P3
- **建议**：带 action 的 toast 也进 `#phead`（按钮一起塞进去），或面板打开时把 `#toasts` 降到 55。

---

### S24 · 摆件 · 摆件与面板互不感知，且退出摆件会把「仅右栏」降级成「全部」

- **路径**：
  1. 设置→界面→常驻栏选「仅右栏」
  2. P → P
  3. 或：P 进摆件 → L 开面板 → P 退出
- **期望**：回到进入摆件前的模式；退出摆件时面板状态有明确约定。
- **实际**：`toggleAmbient` 只在「全隐 ↔ 全部」之间翻，`仅右栏 → 全隐 → 全部`，原模式丢失。摆件中开的面板在退出摆件后仍然开着（`panelStill:'codex'`），两套状态互不知道对方。
- **证据**：`state-shots/16-ambient-panel.png`；`state-probe.json` C1（`panelStill:"codex"`）；`state-probe3.json` G6：`["仅右栏","全隐","全部"]`
- **严重度**：P3
- **建议**：`toggleAmbient` 记住进入前的 mode；退出摆件时按约定关闭面板（或明确保留并在文档里写死）。

---

### S25 · 面板 · 面板内 Enter/Space 曾被守卫吞掉；只放行不 return 时 Space 又会穿透施卡

- **路径**：
  1. L 打开卡库
  2. 聚焦「图鉴 0/6」页签，按 Enter；再聚焦「卡 8/55」，按 Space
- **期望**：两个键都激活聚焦的页签，且世界不变。
- **实际**：`89216f8` 的守卫在原生 button 默认激活之前执行，Enter/Space 都被 `preventDefault`，按不动（对照 `334b2d8`：Enter 能 `cards → ach`）。第三轮第一个补丁改成「只放行、不 return」后，Space 又继续落到 `actOf(' ')` 的「随机释放」分支：页签没切、世界反而多施一张（cells 25→94、casts 1 张土地）。最终版 `if (inPanel && (k==='Enter'||k===' ')) return;` 才把两条都收掉。
- **证据**：`state-shots/p9-panel-enter.png`（旧态：焦点在「图鉴」但页签仍是卡）；`state-probe9.txt` S25（最终态：`enter: cards→ach`，`space: ach→cards`，`castDelta:0`，`cells: 81→81`）
- **严重度**：P2（已修复）
- **建议**：保留「焦点在面板内 → Enter/Space 直接 return」的写法；回归里加一条「聚焦页签 + Space 后世界格子数不变」。

---

### S26 · 键位 · 面板开着时 K / H 变成死键，B/L/C/O 却能切

- **路径**：
  1. L 打开卡库
  2. 按 K
  3. 按 H
  4. 按 O
- **期望**：K/H 和 B/L/C/O 一样能切到 设置→键位 / 操作（`UI.KEYS` 里它们同属「面板」组）。
- **实际**：`89216f8` 的面板守卫放行列表是手写的 `['b','l','c','o']`，K/H 被 `preventDefault` 吞掉；O 照常把面板切成设置。结果同一类入口键里四个能用、两个静默失效。
- **证据**：`state-probe8.txt` S15：`{afterK:"chron", afterH:"chron", afterO:"settings"}`（K 前是年表，K/H 后仍是年表）
- **严重度**：P3（已修复）
- **建议**：放行列表改成「所有面板入口键」——直接读 `UI.KEYS` 里 act ∈ {pack, codex, chron, settings, keys, help, creator} 的键，避免下次再加面板又漏一个。
- **状态**：工作区第三轮已按此改成 `UI.KEYS.some(x => x.group === '面板' && …)`。`state-probe9.txt` S26：年表开着按 K → `{panel:"settings", group:"keys"}`，按 H → `{panel:"settings", group:"input"}`。

---

## 附：不属于上表但值得记一笔的观察

- `onKey` 的 Esc 优先级是 event → panel → creator → ambient，叠层时用户要按几次 Esc 完全取决于当前叠了几层（S09/S10）。建议在任何叠层入口就互斥，而不是靠 Esc 顺序兜底。
- `UI.recycle` 回收最后一张后 `W.sel` 会被夹成 `0`（`cmd.recycle`），此时手牌为空、sel 指向不存在的卡；所有读 `hand[sel]` 的地方都靠「取不到就 early return」兜住，没有崩，但这是一个不该存在的表示（应变 `-1`）。
- 探针把 `W.hand` 直接塞到 50 张时，抽卡面板照常显示「手牌 50/8」，没有任何越界保护；正常流程不可达，记在这里备查。
