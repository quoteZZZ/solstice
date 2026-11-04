# Solstice 新问题猎手 · hunt-issues

> 取证目录：`tests/review-run/`。基础四场景在 `hunt-full-1080/`、`hunt-stress-1080/`、`hunt-kb-1080/`、`hunt-narrow-1080/`；定点取证在 `hunt-shots/p1-*`（第一轮）、`p2-*`（第二轮）、`p3-*`（几何/布局）、`p4-*`（第二轮修复后的当前树）。所有结论都至少看过一张图；纯读数条目在报告里写了具体数字。
>
> **重要：源码在本轮取证期间移动过。** 开工时 HEAD 是 `334b2d8`；中途落了 `89216f8`（“cancel means cancel, one drag one card …”），收尾时工作区还有未提交的 `ui/hud.js` + `styles/app.css`（1080 下恢复 log）。下面的“问题”全部在**当前工作区**（HEAD `430eb48` + 上述未提交改动，2026-11-04 17:00 左右）复现过；被 `89216f8` 修掉的条目集中列在“本轮被修掉的”一节，不重复计分。

## 我压了哪些组合

- **四个基线场景** 1080×700：full 27 张、stress 29 张、keyboard 41 张、narrow 29 张（这组在 `89216f8` 之前跑；图仍可用于“修改前长什么样”的对照）。
- **拖拽**：落世界/落左栏/落右栏/落顶栏/落手牌带 480/落卡面 600/向下 660·680·698/Esc 取消/窗口失焦/拖拽中按 P/拖拽中 1440→900 缩窗/连拖/起手在卡边缘/松手不动。
- **抽卡**：撕开→关→重开、撕开→Esc→重开、撕开后切包、撕开后跳过、时素 0 撕、连抽、手牌满收下、**换界后重开**。
- **toast**：面板开时触发世界事件、面板内连发两条、长文案、回收撤销、openPanel 清空、关闭面板后立刻触发。
- **左栏**：1440 hover 的时序、Tab 聚焦、展开时点图标、展开/收起时 log 的位移。
- **滚动**：卡库详情滚到底、右栏痕迹滚到底、设置列滚到底、抽卡面板底部。
- **月份**：month=0/11/12/13/23/24 六次，对顶栏/UI.mon/事件/年表/图鉴/右栏痕迹。
- **手牌**：1080 满 8 张、1366、1440、1440→1080 缩窗（重排/不重排）、名字可读性。
- **图鉴**：关系链标签/note、指标条 1 格与 2 格。

**没压的**：真实多点触控/pointercancel 硬件、>1680 与 4K、无 `:has` 的浏览器、300 条满年表、创世页深流程、长跑性能/GPU、真机窗口拖拽（只用 `setViewportSize` 模拟）。

---

## 本轮被修掉的（不再计分，但留证据）

1. **Esc / 失焦取消拖拽反而施卡**：`89216f8` 在 `endDrag` 加了 `if (ev == null) return`。复测 `fixed-esc-drag`：Esc 后 `calls 0 / cells 121→121 / hand 8→8 / #ghost 0`，松手后仍是 0。
2. **一次拖拽施两张牌（canvas pointerup + endDrag 各一次）**：`cam.js` 加了 `downOnCv` 守卫。复测 `fixed-one-drag-one-card`：只有 1 次 `UI.cast`（来自 `endDrag`），cells 121→146，手牌 8。
3. **面板开着时 R/空格/X/J 穿透到世界**：`boot.js` 加了面板守卫。复测 `fixed-keys-panel`：连按 R、空格、X、J 后 `essence 999 / hand 8 / gear 2 / panel pack` 全部未变。
4. **1080 满手时动作栏被手牌完全盖住**：`app.css` 把 `#actbar` 提到 `z-index:300`，`hand.js` 的 `avail` 再减 34。复测 `hand-current`：动作栏 4 个按钮的 `elementFromPoint` 全部命中 `.ab`；`p3-11` 可见。
5. **手牌最右一张压住右栏 20px**：`-34` 后末卡 bbox right 818，右栏 left 812，只剩 6px 透明/旋转角；`elementFromPoint(828,540)` 已是右栏（差值 6px）。视觉上 `p3-11` 不再压字。
6. **世界事件弹窗的“第 0 月/第 18 月”**：`89216f8` 改成 `UI.mon(W.month)`。复测 month 0/11/12/13/23/24：`1 月 / 12 月 / 1 年 1 月 / 1 年 2 月 / 1 年 12 月 / 2 年 1 月`，与顶栏一致（`p4-13`）。
7. **1080 下 `#log` 被 `display:none`，垫高/重抽没有文本回执**：工作区未提交改动把它改成 176px 窄条 + 只留 3 条。复测土丘垫高：`#log display:block`，末条“把 9 格地垫高”，`p4-seeded.json` 的 `land.log`。**但这条修复本身带来 X20 / X24**。
8. **数字键在抽卡面板里改选择、侧栏“换掉 X”不更新**：面板守卫拦掉了 1–8。复测不再发生。
9. **带“撤销”的回收 toast 压在面板上**：面板守卫拦掉了 X，这条不再可达。
10. **拖拽中按 P 进摆件模式**：`isDragging` 守卫拦掉非 Esc 键。复测 P 不再生效。
11. **左栏 hover 让位 1px 抖动**：1440 实测 t=90ms 时 `railW 114 / logLeft 120`（同为 93.5%），t=490ms `118 / 124`，同步；无抖动。
12. **`#phead` 长 toast 撑高面板头**：长文案 chip 297px，title 186、close x=981、headH 46，无溢出；只保留 1 枚（见 X08）。
13. **抽卡阶段被关面板洗掉**：撕开→Esc→B 重开，`stage 2 / cards 6 / picks 6` 保持；切包被拦的 chip 也正常。

---

## 当前仍存在的问题

### X01 · 界面 · 面板里的按钮全部失去 Enter / Space 激活（第二轮守卫的新回归）
- **触发场景**：打开任意面板（抽卡/卡库/年表/设置），用鼠标点一下“撕开 · −12 时素”（按钮获得焦点），或 Tab 到面板按钮，按 Enter 或空格。
- **现象**：按钮毫无反应；`stage` 仍 0、时素仍 999。`boot.js` 的面板守卫在 `onKey` 最前面：`if (UI.state.panel) { const pass = Esc/Tab/b/l/c/o; if (!pass) preventDefault(); return; }`，而原生 `<button>` 的 Enter/Space 激活是 keydown 的默认行为，被 `preventDefault` 直接掐掉。守卫只放行了面板开关键，没有放行“焦点就在面板里”的 Enter/Space。鼠标点击仍可用，所以鼠标路径测不出来。
- **证据**：`hunt-shots/p4-01-panel-btn-keyboard.png`；`hunt-shots/p4-report.json` → `panel-button-keyboard`：focused=`撕开 · −12 时素`，Enter 后 `stage 0 / ess 999`，空格后 `stage 0 / ess 999`。
- **严重度**：P2（键盘用户无法买包、切图鉴 tab、改设置；键位表还承诺“可改键”，但面板按钮本身不可键盘操作）
- **建议**：守卫里放行 `ev.key === 'Enter' || ev.key === ' '` 且 `document.activeElement.closest('#panel')` 的情况；或者对 `<button>` 不 `preventDefault`，只 `return`。
- **新引入（89216f8 面板守卫）**

### X02 · 界面 · 抽卡“撕开 / 收下”的 `#phead` toast 被 `renderOverlay` 当场洗掉；撕开时回执在面板开着时看不到
- **触发场景**：抽卡面板点“撕开 · −12”，停在里面挑牌超过 4.6 秒；或点一张候选牌收下（手牌满时换掉选中那张）。
- **现象**：`packNext` / `packPick` 先调 `UI.toast(...)`（面板开着 → 写进 `#phead` 的 chip），紧接着 `UI.renderOverlay()` 重建 `#phead`，chip 被删。撕开后 `#phead` 0 条、`#toasts` 0 条。`S.drawPack` / `takeFromPack` 确实各写了一条世界 log（“撕开「基础包」· −12 时素”/“换下「土地」，换入「扬尘」”），但 `#log` 的 z-index 26 < overlay 60，面板开着时被 #ovbg 盖住；log 条目 4.6 秒后自删。于是撕开这一下的“−12 时素”在面板里完全看不到（只有顶栏 987 这个数字），收下后关面板才看得到那行 log（`p4-21`）。原本设计要放在面板头部的 toast 成了死代码。
- **证据**：`hunt-shots/p4-20-pack-torn-log.png`（面板开着，头部无 chip，左上 log 被遮罩吃掉）；`hunt-shots/p4-21-pack-picked-log.png`（关面板后 log 才露出“换下「土地」，换入「扬尘」”）；读数 `torn.head [] / torn.top [] / log ["撕开「基础包」 · −12 时素"]`，`picked.head [] / picked.top [] / log [..., "换下「土地」，换入「扬尘」"]`。
- **严重度**：P2（撕开的花费在玩家正看着的面板里没有回执；收下那张有事后 log，降为 P3）
- **建议**：把 `UI.toast` 调用移到 `renderOverlay()` **之后**；或让 `UI.renderOverlay` 保留 `#phead` 里未过期的 chip；或在 `packNext/packPick` 里直接写 `#phead` 并给 chip 更长的存活。
- **新引入（本轮 #phead toast 分流 + 二段式抽卡）**

### X03 · 界面 · 换界后上一包已付费的 6 张还留在新世界，可免费取一张
- **触发场景**：抽包并撕开（扣 12），点“关闭”；按 G 打开创世 → 进入新世界（0 格、0 时素）；再按 B 打开抽卡。
- **现象**：`UI.packStage` 仍是 2、`UI.packCards` 仍是旧的 6 张，面板直接显示“2. 挑选”的六张牌；时素 0 也能点一张收下，等于把上个世界的 12 时素带进了新世界。`enterWorld` / `S.resetWorld` 只重置 sim，没有重置 `packStage/packCards/packSel`。
- **证据**：`hunt-shots/p4-05-pack-cross-world-current.png`（顶栏 0 年 1 月 / 0 格 / 0 时素，面板仍是 6 张候选）；`p4-report.json` → `pack-cross-world`：torn `stage 2/cards 6/ess 987`，world `cells 0/ess 0/stage 2/cards 6`，reopened `picks 6`。
- **严重度**：P2（跨局状态泄漏 + 时素经济漏洞）
- **建议**：`UI.enterWorld`（或 `S.cmd.newWorld` 的 UI 侧订阅）里重置 `packStage=0 / packCards=[]`；或者在 `openPanel('pack')` 时校验 `packCards` 属于当前世界。
- **新引入（本轮“已付费的牌不能被关面板洗掉”的持久化规则）**

### X04 · 界面 · “跳过这一包”一键丢掉已付费的牌，无确认也无 toast
- **触发场景**：抽包撕开（扣 12），点底部“跳过这一包”。
- **现象**：面板立即关闭，`packStage 0 / packCards 0 / essence 987`（12 已扣），`#phead` 0 条、`#toasts` 0 条。按钮与旁边的“关闭”同为 ghost、同字号、相邻；一个保留牌、一个丢弃牌，视觉上分不出来。
- **证据**：`p4-report.json` → `pack-skip`：torn `stage 2/ess 987`，btn `跳过这一包 / btn ghost`，after `stage 0/cards 0/ess 987/head []/top []`；`p2-03-pack-after-skip.png`（关闭后世界，无提示）。
- **严重度**：P2（一次误点损失 12 时素；没有二次确认/回执）
- **建议**：跳过前用 `confirm` 式二次点击（按钮文字变“确认丢弃？”）或至少一条 `UI.toast('已丢弃这一包','−12 时素')`；把“跳过”改成 warn 色，和“关闭”拉开。
- **遗留（按钮本身早就有，二段式后仍是唯一出口）**

### X05 · 界面 · 有未取牌且时素 0 时，顶栏“抽包 12”被禁用，pending pack 只剩 B 键入口
- **触发场景**：撕开一包（时素 987），关闭面板；把时素花到 0；看顶栏“抽包 12”。
- **现象**：`bPack.disabled = true`（`updateHud` 只看“最便宜可买包的钱”），但它同时是玩家重新打开**已付费未取**那一包的鼠标入口。此时点不动，只能记得按 B（左栏没有抽包图标）。
- **证据**：`hunt-shots/p4-06-top-pack-disabled.png`（灰色“抽包 12”）；`p4-report.json` → `top-pack-disabled-pending`：`stage 2/cards 6/text 抽包 12/disabled true`。
- **严重度**：P3
- **建议**：`disabled` 条件加 `&& !(UI.packStage >= 2 && UI.packCards.length)`；有 pending 时按钮文案改成“取牌 6”之类。
- **新引入（持久化 + 原有禁用条件叠加）**

### X06 · 界面 · 锁定的包种仍能点成金色选中，只有购买按钮禁用
- **触发场景**：抽卡面板里点最下面的“高阶包（图鉴 0/1）”行。
- **现象**：该行 `class="packrow lock"`（`cursor:not-allowed` + opacity .48），但点击后 `.on` 移到它，标题变“抽卡 · 高阶包”，封印卡变成锁定虚线；底部“撕开 · −48 时素”disabled。视觉上“选中了”和“不能选”同时成立。
- **证据**：`hunt-shots/p4-07-locked-pack-current.png`；`p4-report.json` → `locked-pack-select`：rows high lock，after `sel high / on ["high"] / btn "撕开 · −48 时素" disabled true`。
- **严重度**：P3
- **建议**：`data-pack` 的 click 先判 `S.packOpen(p)`，锁定行直接 return（可给一句“图鉴 0/1 未解锁”的 `#phead` chip）；或允许选中但去掉 `.lock` 的 not-allowed 光标。
- **遗留**

### X07 · 界面 · 打开面板会清空 `#toasts`，回收的“撤销”被吞掉
- **触发场景**：手牌回收一张可回收卡（出现“回收 +2 时素 / 撤销”toast），3.8 秒内按 B 打开抽卡面板。
- **现象**：`openPanel` 第一件事 `stack0.innerHTML = ''`；`#toasts` 清空，撤销按钮消失，被回收的卡已不在手牌，无法再撤回。世界事件 toast 也一起被清。
- **证据**：`p4-report.json` → `undo-cleared`：before `top ["回收+2 时素 撤销"] / hand 7`，after open `top [] / panel pack`；`p2-11-undo-cleared.png`。
- **严重度**：P2
- **建议**：只清“无动作”的世界 toast；带 `act` 的保留（移到面板底部的撤销条），或把撤销从 toast 改成可持久 5 秒的 `#phead` 行内按钮。
- **新引入（本轮 openPanel 清 #toasts）**

### X08 · 界面 · `#phead` 只有一枚 toast 槽：同时点亮两个图鉴只看到后一个
- **触发场景**：在面板里连续触发两条不同的面板级 toast（如两个图鉴同时点亮）。
- **现象**：`UI.toast` 插入第二枚后 `while (head.querySelectorAll('.toast.ih').length > 1) head.querySelector('.toast.ih').remove()`，先到的被删。实测只剩“气候 · 长夜 12/42”，“生态 · 芦苇 11/42”无影。世界里的 `#toasts` 是 3 条队列，两套行为不一致。
- **证据**：`p4-report.json` → `phead-two`：chips `["图鉴点亮气候 · 长夜 12 / 42"]`，top `[]`；`p2-10-phead-two-toasts.png`。
- **严重度**：P3
- **建议**：像 `#toasts` 一样按 `t+d` 去重、最多 2 条；或合并成“图鉴 +2 · 生态/气候”一条。
- **新引入（#phead 单槽）**

### X09 · 界面 · 年表左栏筛选没有“纪年”，六类合计比“全部”少 1
- **触发场景**：跑够 12 个月让 `S.addChron('纪年',…)` 落一条，打开年表。
- **现象**：中间“50 条”，左栏六类（干涉/气候/生态/灾变/文明/图鉴）合计 **49**；`kinds` 数组里没有“纪年”，那 1 条只在“全部”里出现，点任何分类都看不到它，四项数字加起来对不上“全部”。
- **证据**：`hunt-shots/p4-seeded.json` → `chron`：`total 50 / sum 49 / hasYear false`；`hunt-full-1080/19-panel-chron.png`（左栏 7 项、顶部“年表 · 54 条”）。
- **严重度**：P3
- **建议**：`kinds` 加“纪年”，并给它 `CHRON_ICON/COLOR`（见 X10）。
- **遗留**

### X10 · 界面 · 纪年行没有图标/颜色，行首只剩一个蓝点
- **触发场景**：年表/右栏“痕迹·刚刚发生”里看年份记录。
- **现象**：`UI.CHRON_ICON['纪年']` / `UI.CHRON_COL['纪年']` 都是 undefined，渲染回退成 `'·'` 和 `#3f7fa8`；其它行有 ✦/☁/✿/❖，纪年行的时间列只有一个蓝点、没有符号，和“干涉”抢同一个颜色。
- **证据**：`hunt-shots/p4-seeded.json` → `chron.yearRows[0]`：`icon null / col null / mon "1 年 1 月"`；`hunt-full-1080/19-panel-chron.png` 顶部“1 年 1 月”行。
- **严重度**：P3
- **建议**：`CHRON_ICON.纪年='◷'`（或“年”），`CHRON_COL.纪年` 用一个独立色（如 `--gold`）。
- **遗留**

### X11 · 界面 · Tab 聚焦左栏时标签仍是 0 宽 0 透明，键盘只看到图标
- **触发场景**：1440 宽，反复 Tab 直到焦点落在左栏 `.rail`。
- **现象**：焦点环在（`:focus-visible` true），但 `.rail span` 仍 `width:0px / opacity:0`；只有 `#leftrail:hover .rail span` 会展开。键盘用户看不出“年表/卡库/换界/设置”。
- **证据**：`hunt-shots/p4-09-rail-tab-current.png`；`p4-report.json` → `rail-tab`：`railW 56 / labelW 0px / labelOp 0 / focusVisible true / text 年表`。
- **严重度**：P3
- **建议**：把展开条件从 `#leftrail:hover` 扩成 `#leftrail:hover, #leftrail:focus-within`；或 `.rail:focus-visible span{width:auto;opacity:1}`。
- **遗留（标签本轮改成绝对定位，但仍是 hover-only）**

### X12 · 界面 · 滚动渐隐滚到底仍压暗最后一行；`.cdscroll` 与 `#pbody` 两层叠加
- **触发场景**：打开卡库，右列 `.cdscroll` 滚到底；设置页右列滚到底；抽卡面板底部。
- **现象**：`.cdscroll::after` 是 `position:sticky;bottom:0` 的 20px 渐变，滚到底也不消失；`#pbody::after` 在同一底边再叠 20px；设置页又多一个 `.setpane::after`。卡库最后一行 `id terrain.mineral.0.matter.water` 被压到近黑（`p4-10`），而它已经是最后一行，“还有内容”的暗示变成假信号。
- **证据**：`hunt-shots/p4-10-codex-bottom-current.png`（最后一行 id 明显比上一行暗）；`p4-report.json` → `scroll-fade`：`sh 863 / ch 485 / st 378 / max 378 / atEnd true`；同一组 `::after` 规则还挂了 `#rightpanel .rp.grow`、`#pbody`、`.setpane`（89216f8 新增），设置列/抽卡面板底部同样叠。
- **严重度**：P3
- **建议**：给滚动容器加 `scroll` 监听，到底时给 `::after` 加 `.end` 隐藏；或把渐隐高度收到 12px，且只保留最内层那一条（去掉 `#pbody::after` / `.setpane::after` 的叠加）。
- **新引入（本轮渐隐；`.setpane` 是 89216f8 新增）**

### X13 · 界面 · 1440→1080 缩窗后手牌不重排：首卡在屏幕外、末卡压住右栏 124px
- **触发场景**：1440×900 进入世界（手牌 8 张），把窗口缩到 1080×700；不施卡、不重抽。
- **现象**：`--hand-scale` 从 1 变 .88，但每个 `.handslot` 的内联 `left`/步长是 1440 那次 `renderHand` 烘死的；重排前 `first x=-72`、`last right=936`（右栏 left 812），手牌同时溢出左右两边，末卡直接盖住右栏；要等下一次 `renderHand`（施卡/重抽/选中）才恢复（89..791）。`p3-12` 是全屏证据。
- **证据**：`hunt-shots/p3-12-resize-stale-1080.png`（左端切掉、右端“铜钟”压住右栏）；`hunt-shots/evidence-1080.json` → `at1440 first 85/last 1109`、`stale first -72/last 936/sw 911`、`rerendered first 89/last 791`；`p3-13` 为重排后。
- **严重度**：P1（缩窗是常规操作；恢复前手牌不可读、末卡拦截右栏）
- **建议**：`window.addEventListener('resize', debounce(UI.renderHand))`；或在 `renderHand` 里把 `sp` 改成 CSS `calc`/`clamp` 让它跟着容器走，不再烘死像素。
- **遗留（手牌 step 一直是烘死的；本轮断点让视觉尺寸和 step 进一步分叉）**

### X14 · 界面 · `-34` 后动作栏反过来压住末卡右侧 36px（含回收数字）
- **触发场景**：1080 满 8 张手牌，看最右一张的底栏（回收值）。
- **现象**：`#actbar z-index:300` 后按钮永远在卡上；末卡 `x 672..815`，动作栏 `x 770..806`，四个按钮把卡的右 36px 盖住，底栏 `↩ 2` 正好在按钮下面。动作栏可点了（修复生效），代价是末卡的可回收读数看不见，只能 hover 看 tip。
- **证据**：`hunt-shots/p3-11-fresh-1080.png`（末卡“铜钟”右侧被 ✦/↩/↻/1× 覆盖）；`hunt-shots/evidence-1080.json` → `fresh.actbar 770..806`、`lastCard 672..815`。
- **严重度**：P3
- **建议**：把动作栏从卡面区域移到 `#handwrap` 上方（`bottom: calc(100% + 8px)`），或给 `.handslot:last-child` 留 `padding-right`；至少把 `↩` 数字挪到卡面左侧。
- **新引入（89216f8 的 z300 取舍）**

### X15 · 界面 · 1080 下牌堆计数仍被首卡压住，只剩左边缘 10px
- **触发场景**：1080 满 8 张，看左下 `8 手 / 53 池`。
- **现象**：`#deck 56..142`，首卡 `66..208`；`elementFromPoint(86,662)` 命中 `.card`。牌堆是 `pointer-events:none`，不会挡操作，但“手/池”两个数字被吃掉，玩家不知道手牌已满（抽卡面板里才看得到 8/8）。
- **证据**：`hunt-shots/p4-seeded.json` → `hand.deck 56..142 / deckHitCard true`；`evidence-1080.json` → `deck 56..142`；`p3-11` 左下只剩“手”字的一角。
- **严重度**：P3
- **建议**：把 `#deck` 提到 `z-index:301`（和 actbar 同级），或在满手时把它移到左上；至少要保证数字可读。
- **遗留（`-34` 只让出了 10px）**

### X16 · 界面 · 手牌扇面整体右偏 8px，左右留白 14px / −3px
- **触发场景**：1080 满 8 张，量首/末卡与容器。
- **现象**：`#handwrap 52..812`（中心 432），扇面 `66..815`（中心 440.5）；首卡左留 14px，末卡右 **超出 3px**。原因是 `left:calc(50% + d*sp - cw/2)` 用的是缩放后的 `cw=119.68`，而槽位布局宽仍是 136，`transform-origin:50% 100%` 把视觉盒再右移 `(136-119.68)/2≈8px`。`.88` 时 8px，`.82` 时 12px。
- **证据**：`hunt-shots/evidence-1080.json` → `fresh.first 89 / last 791 / firstCard 66 / lastCard 815`，`hw 52..812`；`p3-11` 可见左边留白明显大于右边。
- **严重度**：P3
- **建议**：`left` 里减去**未缩放**的 `CW/2`（`- ${CW/2}px`），让视觉盒中心落在 `50% + d*sp`；或把 `transform-origin` 改到 `50% 50%` 再用 `translateY` 对齐底边。
- **遗留**

### X17 · 界面 · 关系链 note 与卡名挤在同一枚 pill，读成“共生芦苇芦苇沿水”
- **触发场景**：卡库 → 水 → 详情列滚到底，看关系链。
- **现象**：`<span class="rl"><em>共生</em><b>芦苇</b><i>芦苇沿水</i></span>`，三个片段之间只有 4/5px margin，视觉上连成一串；note 和卡名都是“芦苇…”，更像重复而不是注释。note 是第一次渲染（本轮新加），所以这是新暴露的可读性问题。
- **证据**：`hunt-shots/p4-11-codex-rel-current.png`；`p4-report.json` → `rel-note`：text `共生芦苇芦苇沿水`，html `<em>共生</em><b>芦苇</b><i>芦苇沿水</i>`。
- **严重度**：P3
- **建议**：note 前加 `·` 或换行，并给 `<i>` 左侧加 1px 分隔线；note 用 `--ink-4`、卡名用 `--acc`，让“标签 / 对象 / 注释”三级别分开。
- **新引入（本轮首次渲染 rel.note）**

### X18 · 界面 · 摆件模式不隐藏 `#toasts`，进入后图鉴 toast 还挂在世界中央
- **触发场景**：触发一条 toast 后立即按 P 进摆件模式。
- **现象**：`#stage.ambient` 只隐了左栏/右栏/deck/actbar/log，`#toasts`（z70，在 stage 内）原样保留；`p4-12` 里“图鉴点亮 / 生态·芦苇 11/42”横在世界中央，所谓“纯世界”不干净。
- **证据**：`hunt-shots/p4-12-ambient-toast-current.png`；`p4-report.json` → `ambient-toast`：`top ["图鉴点亮生态 · 芦苇 11 / 42"] / mode 全隐`。
- **严重度**：P3
- **建议**：`#stage.ambient #toasts{display:none}`；或进摆件时清空 `#toasts`（顶栏仍留，见 `edge-issues` E?）。
- **遗留**

### X19 · 界面 · 设置里的“阴影”开/关已无视觉作用（场景 0 个 castShadow）
- **触发场景**：设置 → 画面 → 阴影，点“开”。
- **现象**：`89216f8` 把 `R.shadows` 默认改 false，理由是全场景没有 castShadow；但设置项仍保留，`cur()` 显示“关”，点“开”只把 shadowMap 打开，场景里 `casters=0`，画面一像素不变。玩家会以为设置没生效。
- **证据**：`hunt-shots/p4-18-settings-shadow.png`（阴影行“关”）；`p4-report.json` → `shadow-setting`：`shadows false / casters 0`；`git show 89216f8 -- render/scene.js`。
- **严重度**：P3
- **建议**：要么删掉这个设置行（和 `W04` 的结论一致），要么给地形/立牌加回 castShadow 后再保留。
- **新引入（89216f8 W04）**

### X20 · 界面 · 1080 下“3 条 log”实际是 3 个条目，长消息每条 3 行，整块 175px
- **触发场景**：1080 宽，`UI.log` 连写 3 条长事件（改写/灾变/文明）。
- **现象**：未提交的 log 修复把 `#log` 从 `display:none` 改成 `width:176px` + `cap=3`；cap 按**条数**裁，不按行。实测 3 条各 56px（3 行），`#log` 高 175px，占左上 176×175 的世界；“压成 3 行的窄条”的注释与实现不符。
- **证据**：`hunt-shots/p4-16-log-wrap.png`；读数 `rect h 175 / count 3 / 每条 h 56`。
- **严重度**：P3
- **建议**：按行裁（`-webkit-line-clamp:3` 或给每条 `white-space:nowrap;text-overflow:ellipsis`），或 cap 改 1–2 条；注释与实现选一个。
- **新引入（未提交的 1080 log 修复）**

### X21 · 界面 · 0 年时顶栏写“0 年 1 月”，`UI.mon` 写“1 月”
- **触发场景**：开局 month=0，对比顶栏、年表、图鉴、事件。
- **现象**：顶栏 `cYear` 恒显示 `S.year()`=0，于是“0 年 1 月”；`UI.mon(0)` 按设计省略 0 年，只写“1 月”；事件已改用 `UI.mon` 也写“1 月”。同一时刻两种写法，虽然只差一个 0 年。
- **证据**：`hunt-shots/p4-seeded.json` → `months[0]`：`clock "0年1月" / mon "1 月" / event "1 月"`；`p4-13-event-month0.png`。
- **严重度**：P3
- **建议**：`updateHud` 在 `S.year()===0` 时隐藏“0 年”两个节点，和 `UI.mon` 对齐；或让 `UI.mon` 也保留 0 年（二选一）。
- **遗留**

### X22 · 界面 · idle 落点松手没有任何“已取消”，牌静默回手
- **触发场景**：按住一张牌，拖到手牌带内（y=480）、拖到另一张牌上（y=600）、或拖到右栏（x=990,y=300）后松手。
- **现象**：ghost 的 tip 一直是“拖到世界 = 指定落点 · 拖回手牌带下端 = 回收”，`mode=idle`；松手后 cells 121→121、hand 8→8、`#toasts` 0、`#ghost` 0。没有施卡、没有回收、也没有“已取消”。`H02` 的建议“idle 松手给一句已取消”只做了“不施卡”这一半。
- **证据**：`hunt-shots/p4-17-idle-drop.png`；读数 `hand-band-480 / over-card-600 / right-panel` 三次均 `tip 默认 / toasts [] / cells 121`。
- **严重度**：P3
- **建议**：`endDrag` 的 idle 分支 `UI.toast('已取消', '松手处不是世界')`；或把 tip 在 idle 时改成“松手取消”。
- **新引入（本轮判定改后 idle 才成为一个真正的落点）**

### X23 · 界面 · 面板内 toast 的存活依赖“有没有 re-render”，年表 tick 会吞图鉴 toast
- **触发场景**：打开年表面板，等一个世界 tick 让图鉴点亮（或手动 `S.cmd.tick`）。
- **现象**：`S.on('codex')` → `UI.toast` 面板开着 → 写进 `#phead`；同一 tick 末尾 `if (UI.state.panel === 'chron') UI.renderOverlay()` 重建面板，chip 被删。和 X02 同一个机制，但触发者不是抽卡动作，而是“面板自己每秒重绘”。（其余面板不会 tick 重绘。）
- **证据**：`hunt-shots/p4-seeded.json` → `chipWipe`：`before ["图鉴点亮生态 · 芦苇 11 / 42"] → after []`（手动在 chron 面板插入 chip，再调 `UI.renderOverlay()`，chip 消失）；`boot.js:206` 的 tick 正是每 tick 调一次 `renderOverlay`。
- **严重度**：P3
- **建议**：`renderOverlay` 重建时把 `#phead` 的 chip 文本/剩余时间一起带过去；或把 chip 放到 `#overlay` 外层、不受面板 innerHTML 影响。
- **新引入（#phead toast 分流）**

### X24 · 界面 · 宽屏有 8 条日志时缩到 1080，日志变 176px 宽、454px 高的巨块，直到下一条日志才裁到 3 条
- **触发场景**：1680×1000 跑出 8 条世界日志（长文案），把窗口缩到 1080×700。
- **现象**：CSS 在 `max-width:1080` 把 `#log` 从 244 改成 176，但条数裁切在 `UI.log` 里、只在**新日志**时执行（`cap=innerWidth<=1080?3:8`）。缩窗后 8 条全部保留并重新换行，`#log` 从 363px 高变成 **454px 高**，盖住左上 176×454 的世界；直到下一条日志才变成 3 条 138px。
- **证据**：`hunt-shots/p4-19-log-resize.png`；读数 `1680: n8 h363 w244` → `1080: n8 h454 w176` → 下一条后 `n3 h138`。
- **严重度**：P3
- **建议**：在 `resize` 时也跑一次日志裁剪（把 `UI.log` 的裁剪抽成 `UI.trimLog()`，监听 resize）；或改成 CSS `max-height` + `overflow:hidden` 的硬约束（配合最新的“最新在下”顺序）。
- **新引入（未提交的 1080 log 修复）**

### X25 · 界面 · 抽卡面板开着时时素涨到够买，封印卡/底部文案/“撕开”按钮仍停在旧值
- **触发场景**：时素 0 时打开抽卡面板（封印卡 0/12、按钮禁用、底部“时素 0 · 每 10 秒 +1”），等时素自然涨到 12（或直接从 0 改到 12）。
- **现象**：顶栏的时素从 0 变 12、顶栏“抽包 12”也可用了，但面板是打开那一刻的静态 HTML：封印卡仍 `0 / 12`、底部仍“时素 0”、`[data-act=packNext]` 仍 disabled。玩家会以为“够了还是买不了”，只能关面板再开。`updateHud` 每帧跑，但它只改顶栏，不重绘 `#overlay`；`S.on('tick')` 也只重绘年表。
- **证据**：`hunt-shots/p4-22-pack-stale-essence.png`（顶栏 12 时素，面板 0/12 + 底部 0 + 按钮灰）；读数 `before {ess 0, btn disabled true, seal "0 / 12", foot "时素 0"}` → `after {ess 12, btn disabled true, seal "0 / 12", foot "时素 0", top "抽包 12"}`。
- **严重度**：P2（面板最核心的购买条件不跟随实时时素；等 120 秒也买不了）
- **建议**：时素变化时若 `UI.state.panel==='pack'` 就轻量更新封印条/`psNum`/footer/按钮 `disabled`（不要整块 `renderOverlay`，以免洗掉 chip 和滚动）；或在 `essenceTick` 的 `S.on` 里做同样的事。
- **遗留（面板静态渲染；本轮的“每 10 秒 +1”让这个等待变得显眼）**

---

## 压过、没问题的

- **抽卡阶段保持**：撕开→Esc→B 重开 `stage 2 / cards 6 / picks 6`；关闭→重开同样。`#phead` 的切包拦截 chip（255px）与 title（186）/close 不重叠，头部 46px 不变。
- **月份一致性（除 X21 的 0 年）**：month 11/12/13/23/24 的顶栏、UI.mon、事件、年表、图鉴、右栏痕迹全部一致；跨年 `纪年` 条目在 12 月正确置顶，顺序不乱。
- **左栏 hover 时序**：1440 t=90ms rail 114 / log 120，t=490ms 118 / 124，同一比例；没有 1px 抖动，也没有 log 跑到栏下面。
- **右栏空环境**：空世界显示水/光/温三条空轨道 + “还没有地块”，生态 0% + 未成形；没有错位。
- **`.88` 的名字**：1080 满 8 张，8 个卡名都露在邻卡之间（“土地/太阳/水/渗/透/光斑/萌发/铜钟”），没有被完全盖住。
- **图鉴指标条**：只有半径时 1 格居中；铜钟这种有 reclaim 的 2 格（`◔ 0.050 W 半径` + `↩ +2 回收`）均匀分栏。
- **抽卡面板 1080**：6 张候选 4+2 居中，第二步标签正确，时素 0 时撕开按钮禁用、`0/12` 条为空。
- **手牌 `.88` 的卡面**：卡内 5 段（色条/名带/环条/画芯/说明/底栏）没有被裁，名字行高正常。
- **面板长 toast**：`#phead` 里 297px 的 chip + 186px title + close 全部放得下，headH 46，不换行、不撑高。
- **拖到右栏/左栏/顶栏**：不再穿透施卡（X22 只是没有“已取消”的反馈）。

## 说明

- 图片命名：`p1-*` 是第一轮（旧 HEAD）、`p2-*` 第二轮、`p3-*` 几何/布局、`p4-*` 当前树；`hunt-full-1080/*` 等四组是基线。
- `p3-12` / `p3-13` / `evidence-1080.json` 是缩窗问题的一手读数；`p4-report.json` 是当前树 21 个复核步骤的完整读数；`p4-seeded.json` 是“有地块”世界下的月份/年表/垫高/手牌/chip 读数。
- 本轮只新建了 `tests/review-run/hunt-*` 脚本/截图/报告，没有改任何产品源码；`git status` 里的 `ui/hud.js`、`styles/app.css` 未提交改动不是我的（是主代理在收尾时落的 log 修复）。
