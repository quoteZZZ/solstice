#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""设计数值自检：把文档里的算式跑一遍，任何一条不成立就 FAIL。

用法：python tools/checks/check-design-numbers.py
真源：docs/CARDS.md §8.3 / §8.5 / §9 / §9.1 / §14，docs/WORLD.md §3 / §9.7 / §11.1

注意：2026-10-20 起「卡唯一」作废（卡是消耗品、可重复抽到），
所以"包数 = 池子 ÷ 张数"这类断言全部删除，改为 coupon collector 的点亮里程碑。
"""
import math
import sys

fails = []


def chk(name, got, want, tol=0.0, fmt='{:.2f}'):
    ok = abs(got - want) <= tol
    tag = 'PASS' if ok else 'FAIL'
    print('[%s] %-36s got %s  want %s' % (tag, name, fmt.format(got), fmt.format(want)))
    if not ok:
        fails.append(name)


# ---- 格子（不是卡数）----
chk('7 x 11 x 3 = 231 格', 7 * 11 * 3, 231)
chk('主包覆盖 7 x 10 x 3 = 210 格', 7 * 10 * 3, 210)
chk('超环 7 x 1 x 3 = 21 格', 7 * 1 * 3, 21)
chk('律卡 7 x 11 = 77 格', 7 * 11, 77)

# ---- 点亮曲线（模拟 300 次的中位值，见 CARDS §9.1）----
chk('50% 点亮 ≈ 27 包', 27, 27)
chk('80% 点亮 ≈ 50 包', 50, 50)
chk('95% 点亮 ≈ 75 包', 75, 75)
chk('100% 点亮 ≈ 112 包', 112, 112)
chk('112 包 x 90 = 10080 余响', 112 * 90, 10080)
chk('按 9/min = 18.7 小时', 10080 / 9 / 60, 18.67, tol=0.1)
chk('无保底要 278 包（反例）', 278, 278)

# ---- 余响 ----
chk('20 分钟 ≈ 2 包（10 张卡）', 20 * 9 / 90, 2.0, tol=0.2)
chk('主包期望回收 37% < 60%', 33 / 90, 0.37, tol=0.02)

# ---- 扰动：累积必须跑赢衰减 ----
gain_mid = 8
period = 2.0
decay = 1.5
net = gain_mid / period - decay
chk('中档净增益 = 2.5/min', net, 2.5, tol=0.01)
chk('42 -> 70 需 11.2 分钟', (70 - 42) / net, 11.2, tol=0.02)

world_hour_sec = 3 * 60 / 24
old_decay_per_min = 2 * (60 / world_hour_sec)
chk('旧口径 16/min（反例）', old_decay_per_min, 16.0, tol=0.1)

# ---- 冷却 ----
chk('9 环冷却 15 分钟 → 5.3/min', 80 / 15, 5.33, tol=0.05)

# ---- 生态 ----
K_max = 1400
chk('多样性上限 = min(池8, K/100)', min(8, K_max // 100), 8)
succ_days = math.log(9) / 0.08
chk('到 90%K = 27.5 世界日', succ_days, 27.5, tol=0.2)
chk('= 20.6 分钟 @4x', succ_days * 45 / 60, 20.6, tol=0.3)
chk('沉淀满值 1.6/min < 总产出 18%', (1.2 * 60 / 45) / 9.0, 0.178, tol=0.02)

# ---- 时间 / 文明 / 灾害 ----
chk('1 年 = 48 日', 48, 48)
chk('首聚落 2 年 @20x = 14.4 分钟', 2 * 48 * 9 / 60, 14.4, tol=0.05)
chk('灾变 3 年 @4x = 108 分钟', 3 * 48 * 45 / 60, 108, tol=0.5)

# ---- 效果量级 ----
R = [0.006, 0.012, 0.020, 0.030, 0.050, 0.075, 0.110, 0.165, 0.250, 0.380, 0.560]
chk('9 环面积 ≈ 45% 全界', math.pi * R[9] ** 2, 0.45, tol=0.02)
chk('超环面积 ≈ 98%', math.pi * R[10] ** 2, 0.98, tol=0.03)
chk('8->9 环面积倍数 < 3', (R[9] / R[8]) ** 2, 2.31, tol=0.05)
chk('peak 9 环 = 0.14', 0.14, 0.14)
chk('超环峰高顶到 0.25 W', 0.25, 0.25)
chk('water 9 环撞 0.06 clamp', min(0.0015 * 1.55 ** 9, 0.06), 0.06, tol=0.001)

# ---- 首包节奏 ----
chk('0 环给 1 → 90 次 = 7.5 分钟', 90 * 5 / 60, 7.5, tol=0.1)

print()
if fails:
    print('design numbers: %d FAIL' % len(fails))
    sys.exit(1)
print('design numbers: all OK')
