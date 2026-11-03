# 至日 · 合并原型

> 入口：`index.html`（双击，`file://` 离线可开）。Three.js 0.160.0 在 `vendor/`，不走 CDN。
> 这一版把 `archive/world-3d`（真世界）与 `archive/ui-flow`（信息架构）合成一个：世界是真的，面板是齐的，卡表只有一份。

## 验证什么

"施卡造世界 + 世界自己转 + 面板一屏读完"能不能在同一个原型里成立：
3D 的世界、`archive/ui-flow` 的创世 / 卡库 / 年表 / 事件 / 设置 / 键位，共用一张按 `CARD-SCHEMA` 写的卡表。

## 已实现

- **卡**：55 张（29 物 / 16 术 / 10 律），全部按 `specs/CARD-SCHEMA.md` 写字段：
  `id / number / name / elements / tier / class / duration / line(fx,cond,num,hl) / radius_w / disturb / value / reclaim / stats[6] / lore[3] / effect / rel / usage / tips / flavor / evolve / art / runtime`。
  编号、稀有度、回收价、ATK/DEF 全部由 `data/index.js` 派生，数据文件里不写。
- **一张表驱动世界**：每张卡带 `sim.op`（`land / sun / spring / barrier / spawn / rain / burn / ignite / fertile / grow / wound / field / law / human / bell`），
  `sim/cast.js` 只按 op 分发，没有按卡名的 if/else。物种参数也在卡上（`species`），不再有第二张 `SPECIES` 表。
- **生态**：水（水源按曼哈顿距离衰减到 1/0.62/0.26）、光（太阳 + 自发光 − 遮荫）、温、逐实体生长 / 老化 / 旱死 / 饿死 / 捕食 / 繁殖；
  格子的 `mods`（光/温/蒸发）与 `flags`（扎根 / 渗 / 松 / 透 / 不锈 / 隐 / 湿地 / 含矿）由 0 环律与术卡写入，`computeEnv` 真的读它们。
- **生命三层**：植物 → 动物 → 智慧，不能跳级；生命数从实体现场统计，不用手工计数器。
- **时间**：按 `specs/TIME.md` 五档（暂停 / 0.5 / 1 / 2 / 4×），1 tick = 1 世界月。
  昼夜是**日历的投影**（`S.sunHour(month) = 月 × 2 小时`，一年一昼夜，月 0 / 6 至日切换），
  结算只读整数月，所以帧率与档位都不影响生态。
- **确定性**：世界坐标就是随机种子（`S.setSeed` / `S.rand`，mulberry32）；`tests/determinism.js`
  同 seed 两跑 digest 一致、异 seed 不同，只装 `sim/` + `data/`，node 直跑不用浏览器。
- **界面**：创世（掷骰 / 侧剖 / 顶视 / 判据 / 坐标）、抽卡、卡库（元素 × 11 环 × 3 类，卡面 + 读数 + lore + 关系互跳 + 完整效果）、
  图鉴（卡库第二面：天文 / 水文 / 生态各 2 条世界状态成就，点亮 = 金边 + 写年表 + toast）、
  年表（类型筛选 + 条目详情）、设置（像素比 / 阴影 / 雾浓度 / 俯仰 / 缩放，每项真接线）、键位（重绑 + 冲突检测）、帮助。
- **图鉴 6 条**：`sim/codex.js` 的 `S.CODEX`，判定全部从 `W` 现场读（一次 `scan()` 喂 6 条），
  不写卡名；图鉴是账号级的，换界只清连续月计数不撕页。`npm run codex` 给每条一个能点亮它的剧本。
- **拖放**：拖到地面 = 指定落点（射线拾取），向上拖 / 空格 = 随机落点，向下拖 / X = 回收；律卡不需要落点。
- **痕迹**：每次施卡在格子上留一个环（按环级定时长，同格同元素合并升级），右栏列最近 4 条，点开看卡。

## 未实现

- 540 张卡池（现在 55 张）；合成树 `/` 图鉴总数与 `CARD-INDEX` 尚未对齐。
- 图鉴 54 / 60 条没做（只实装 6 条），**成就解锁卡池**（[`SETTING.md`](../specs/SETTING.md) §5.1.3「成就是解锁卡池的途径」）也没接：点亮只写年表与日志。
- 存档 / 读档（确定性回归已经有了，缺的是全量快照）；图鉴因此只活在当前会话里，刷新即归零。
- 音频。
- 生态事件只有"物种归零 → 写年表"，没有迁徙 / 演替。
- 年表的"跳到那一刻"回放。
- 昼夜只切光与夜行物种的活跃，不参与月度结算（这是设计，不是缺口）。

## 已知坑

1. **`file://` 下不能用 ES module**（CORS 会拦），所以这里用经典 `<script>` + 全局 `SOL` 命名空间按序加载；顺序写在 `index.html` 底部，不能乱。
2. **卡表校验有 1 条告警**：夜沼鸟 3 元素却只有 1 环——它是 `CARD-SCHEMA.md` §9「完整数据」的官方样例，规则 §8「校验」表第 6 条与样例本身冲突，暂时保留告警不删卡。
3. **卡面 302×440**，手牌用 `scale(.5)` 到 151×220；与 `specs/ui/CARD-LAYOUTS.html` 的三档实测只对了手牌与图鉴两档，第三档（悬浮大卡）没做。
4. **痕迹点选在右栏，不在 3D 里**：3D 里点不到那个小环（射线只打格子和立牌）。
5. 创世特征里只有 4 条（地热 / 贫瘠 / 隔离 / 富集）真进结算，其余 6 条只在剖面图里出现；
   特征卡现在用「生效」标与虚线灰边区分，别把「仅剖面」当成本界的承诺。

## 自测

```powershell
cd prototypes/solstice
npm install                  # 只装 playwright-core（不下载浏览器）
npm run cards                # 卡表契约：硬校验 0 错（不需要浏览器）
npm test                     # 用本机 Edge 无头跑：控制台报错 / 施卡链路 / 24 月 tick / 面板 / 卡面尺寸
npm run det                  # 确定性回归：同 seed 两跑 digest 一致、异 seed 不同（不需要浏览器）
npm run codex                # 图鉴回归：6 条各自能被点亮 + 换界不丢页（不需要浏览器）
```

需要 Node 18+ 与本机 Edge（`npm test` 才用得到）。`npm test` 会写 `tests/report.json` 与三张截图（已 gitignore）。
除 `npm test` 外四条都不需要浏览器，CI 里跑的就是这四条（另加 `python tools/docs-check.py`）。

## 目录

```text
index.html            装配壳：每层一个 script，顺序即依赖
styles/app.css        外壳与常驻 HUD（顶栏 / 左栏 / 右栏 / 手牌 / 拖影）
styles/card.css       卡面 A 版式（字段来源写在文件头）
styles/panels.css     覆盖层面板 / 创世 / 世界事件
data/world.js         元素与纹样、形制 / 气候 / 特征 / 判据 / 命名词表
data/cards.*.js       卡表（按类分三个文件，一张卡一段）
data/index.js         装配：编号、稀有度、回收、ATK/DEF、校验、索引
sim/core.js           状态、格网、实体、生命统计、世界摘要、钩子多播（S.on / S.emit）
sim/cmd.js            命令层：ui 唯一的写入口（S.cmd.*），每个命令是一个原子动作
sim/eco.js            水光温三场 + 逐实体 tick（不碰 DOM）
sim/cast.js           施卡 op 分发、手牌、抽卡、回收、痕迹
sim/time.js           五档时间、tick、开局
sim/codex.js          图鉴 6 条判定（一次 scan 喂 6 条，账号级，不碰渲染）
sim/events.js         9 条世界事件的表与后果（承受 / 改写）
render/scene.js       渲染器、光、星野、虚空、蓝图、单帧绘制（不持时钟）
render/grid.js        格子 mesh 与立牌贴图
render/cam.js         相机、拾取、世界→屏幕
render/trace.js       痕迹环
ui/cards.js           卡面 / 悬浮 / 读数条（唯一一份卡面实现）
ui/hud.js             顶栏 / 左栏 / 右栏 / 日志 / toast
ui/hand.js            手牌扇形与三段拖放
ui/panels.js          抽卡 / 卡库 / 年表 / 设置 / 键位 / 帮助
ui/creator.js         创世掷骰与剖面 / 顶视
ui/events.js          世界事件面板：选 / 承受 / 改写（后果在 sim/events.js）
ui/boot.js            启动、主循环（rAF + dt）、键盘、调试钩子
tests/                cards / determinism / codex / smoke 四条回归
```
