---
id: PALETTE
title: 色板真源
status: active
truth: spec
owner: both
---

# 色板（PALETTE）

> **本文是画面颜色的唯一真源。** 代码里的目视取色常量必须来自这里，且一律经 `CelSrgb()`。
> 改色 = 改这张表 + `Config/Solstice/Data/render/palette.json` + `shared/palettes/*.json`（同一次提交）。

---

## 1 三条法则（比色值更重要）

### 1.1 明度阶梯：要档，不要连续

卡通着色的本质是把连续光照**量化成 band**。实践值：

| 档 | L\* | 用途 |
|---|---|---|
| 高光 | 85–95 | 只给水 / 金属 / 湿石 |
| 亮部 | 65–80 | 受光面 |
| 中间调 | 40–55 | 过渡 |
| 暗部 | 20–35 | 背光面 |

- 相邻档 **ΔL\* < 12** → 玩家读不出受光/背光，只觉得脏。
- **ΔL\* > 35** → 断层，像贴纸。
- 同族色之间要保证 **ΔL\* ≥ 15**（本文件三套色板已校验）。

### 1.2 冷暖对位：由时段决定，不凭喜好

| 时段 | 光 | 影 |
|---|---|---|
| 日间 / 黄金时刻 | 暖（奶油黄、金） | **冷（蓝紫偏移，绝不能用黑色代替）** |
| 阴天 / 黎明 / 月光 | 冷（青灰、银蓝） | 暖（暗褐、紫） |

判据：阴影色相应落在主光色相的**补色侧 30–60°**，而不只是把亮度乘 0.7。

### 1.3 饱和度预算 60-30-10

| 面积 | 角色 | S 范围 |
|---|---|---|
| 60% | 天、海、草（大面积） | 20–45% |
| 30% | 沙、岩、植被（次级） | 40–60% |
| 10% | 太阳、泡沫、花、屋顶（焦点） | 70–90% |

**高饱和超过 15% 面积 = 糖水。**

---

## 2 三套成品色板

> 全部为工程标定值。锚定的是参考作品的**结构参数**（色相角、L\* 带、S 上限），不是原图像素。

### A · 地中海正午（默认白天）

意图：青绿海、近白地平线、通透天真。参考风之杖「亮海暗天」+ 蒙德暖金底。

| 槽位 | HEX | L\* | 说明 |
|---|---|---|---|
| `sky_zenith` | `#2E7FC2` | 51 | 天顶偏深，不是天蓝原色 |
| `sky_mid` | `#62A9DC` | 67 | 中层 |
| `sky_horizon` | `#CFE6F2` | 90 | 近白 —— 动画感的第一要素 |
| `sun` | `#FFF3D0` | 96 | 暖白 |
| `sun_glow` | `#FFD98A` | 88 | 光晕 |
| `sea_shallow` | `#57C8C8` | 75 | 浅滩青 |
| `sea_mid` | `#2E93B8` | 57 | 中层 |
| `sea_deep` | `#17527E` | 33 | 深水 |
| `foam` | `#F4FBF7` | 98 | 浪花 / 岸线 |
| `sand_dry` | `#F0DCB0` | 88 | 干沙 |
| `sand_wet` | `#C9A97A` | 71 | 湿沙 |
| `grass_light` | `#A8D45A` | 80 | 草地亮档 |
| `grass_mid` | `#6FA83C` | 63 | 草地中档 |
| `grass_dark` | `#3E6B2E` | 41 | 草地暗档 |
| `rock_light` | `#C9B79A` | 75 | 岩亮 |
| `rock_dark` | `#6E6152` | 42 | 岩暗 |
| `foliage_dark` | `#2F5A3A` | 34 | 树冠暗部（**比草地更深更冷**） |
| `foliage_lit` | `#7FBF52` | 71 | 树冠受光 |

### B · 北海道初夏（柔和 / 阴天 / 秋）

意图：低饱和柔光，奶油光青灰影。参考吉卜力《魔女宅急便》海港午后 + A Short Hike。

| 槽位 | HEX | L\* |
|---|---|---|
| `sky_zenith` | `#5E97C4` | 60 |
| `sky_mid` | `#9CC4DE` | 77 |
| `sky_horizon` | `#E4EFEF` | 94 |
| `sun` | `#FFF6E2` | 97 |
| `sun_glow` | `#FFE0A8` | 91 |
| `sea_shallow` | `#8FD3C4` | 80 |
| `sea_mid` | `#4E9E9B` | 60 |
| `sea_deep` | `#2B5F6B` | 37 |
| `foam` | `#FBFDF8` | 99 |
| `sand_dry` | `#F2E3C8` | 91 |
| `sand_wet` | `#CBB18D` | 74 |
| `grass_light` | `#C8DB8A` | 84 |
| `grass_mid` | `#8FAE5C` | 67 |
| `grass_dark` | `#55703F` | 44 |
| `rock_light` | `#D6CEC0` | 83 |
| `rock_dark` | `#7A7268` | 48 |
| `foliage_dark` | `#3C5B45` | 36 |
| `foliage_lit` | `#9CC46A` | 75 |

### C · 黄金时刻（日循环高潮）

意图：冷紫天顶 + 橙金地平线，暗部转紫褐。参考《你的名字》三段天空。

| 槽位 | HEX | L\* |
|---|---|---|
| `sky_zenith` | `#3B4E86` | 34 |
| `sky_mid` | `#8A6FA8` | 51 |
| `sky_horizon` | `#F5A76B` | 75 |
| `sun` | `#FFE9B0` | 93 |
| `sun_glow` | `#FF9E5E` | 74 |
| `sea_shallow` | `#7FB2C4` | 70 |
| `sea_mid` | `#3F6E8C` | 44 |
| `sea_deep` | `#22384F` | 23 |
| `foam` | `#FFEFE0` | 95 |
| `sand_dry` | `#EFD2A8` | 86 |
| `sand_wet` | `#B98E68` | 62 |
| `grass_light` | `#C9C070` | 77 |
| `grass_mid` | `#8A8A4E` | 56 |
| `grass_dark` | `#4A5238` | 34 |
| `rock_light` | `#C48F72` | 64 |
| `rock_dark` | `#5B3F42` | 30 |
| `foliage_dark` | `#333F35` | 25 |
| `foliage_lit` | `#8FA05A` | 63 |

---

## 3 来源

| 参考 | 用到的部分 | 来源 |
|---|---|---|
| 旷野之息 / 王国之泪 | 草地由「黄绿高光 + 墨绿阴影 + 金色点缀」构成，不是单一绿 | 社区截图取色色板 [colorswall](https://colorswall.com/palette/266520) |
| 风之杖 Great Sea | 单高彩度青蓝 + 高明度白浪；「亮海暗天」的反常关系 | 截图取色，近似 |
| 原神 · 蒙德 | 低饱和青绿底 + 全局暖金；自然色 S 多在 25–45% | [colormagic](https://colormagic.app/palette/6716c0151fc5d72248a379b7)、[brandchroma](https://brandchroma.com/games/genshin-impact-colors) |
| 纪念碑谷 | 每场景 4–5 色，明度接近、色相跨度极大（**不照搬**，与卡通渲染相反） | [COLOURlovers](https://www.colourlovers.com/palette/1223726/Monument_Valley) |
| 吉卜力 | 自然色占 80–90%，高饱和点缀 10%；光暖影冷 | [Ghibli color theory](https://eathealthy365.com/understanding-ghibli-style-color-theory)、[colour script](https://animepapa.com/studio-ghiblis-use-of-color-and-light-to-convey-mood-and-emotion) |
| 你的名字 / 天气之子 | 天顶钴蓝 → 中层品红 → 地平线橙金，三段色相渐变 | 截图取色，近似 |
| Tiny Glade | 极高明度 + 低饱和 + 强 AO 与描边（色彩几乎不承担信息） | [pastel 区间](https://colorschemecalculator.com/pastel-color-schemes) |
| 60-30-10 | 面积比而非配色建议 | [No Film School](https://nofilmschool.com/60-30-10-color-rule) |

---

## 4 常见"廉价感"判据（自检清单）

1. 饱和度全拉满，没有焦点层级 → 眼睛无处落点。
2. 暗部只压明度不换色相 → 灰脏的中年色。
3. 天空只有一档蓝 → 空间压平成背景板。
4. 明度阶梯过密（草/土/岩全挤在 L\*55–70）→ 剪影糊成一片。
5. 高光纯白 `#FFFFFF` + 暗部纯黑 → 塑料玩具。

---

## 5 落地方式

| 位置 | 内容 |
|---|---|
| 本文件 | 人类可读真源 |
| `shared/palettes/{A,B,C}.json` | 机器可读（键名同上，值为 `#RRGGBB`） |
| `Config/Solstice/Data/render/palette.json` | 运行期选用的那套 + 覆盖项 |
| `Content/Solstice/Core/LUT_*.png` | 32³ 调色 LUT（由 json 生成） |
| `tools/checks/grade-preview.py` | 色板验证：用当前帧重绘，输出 `tmp/review/palette/` |

**验证口径**：重绘图的天空必须能数出三段、海必须能数出三档、草地必须能数出三档、岛的剪影能从海里读出来。
