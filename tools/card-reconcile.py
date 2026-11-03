#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Solstice · 卡表对账（CARD-INDEX ↔ data/cards.*.js）**报表，不是闸门**

    python tools/card-reconcile.py

[STATUS.md](../specs/STATUS.md) §3 第 2 条记的「合成树 75 张与卡表尚未自动对账」就是这件事。
但量过之后发现它不是「漂移」，是**口径不同**，所以不做硬闸门：

    CARD-INDEX 是「按名字列物类」的目录（540 张的目标清单，含没写的）
    data/cards.*.js 是实装卡表（55 张，含术与律）
    术 / 律 / 基础地形在 CARD-INDEX 里没有对应名字 —— 那份清单压根不按名字列它们

把清单改成从 data/ 生成是错的：会抹掉尚未实装的 485 张设计意图。
所以这里只报三件事：实装多少、在清单里找得到几个、差的那些属于哪一类。
差集怎么处理由人决定（补进清单 / 认下这是口径差距 / 改名对齐），脚本不替人拍板。

退出码恒为 0。
"""
import io
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INDEX = os.path.join(ROOT, 'specs', 'CARD-INDEX.md')
DATA = os.path.join(ROOT, 'prototypes', 'solstice', 'data')
FILES = ('cards.matter.js', 'cards.rite.js', 'cards.law.js')

CLS = {'matter': '物', 'rite': '术', 'law': '律'}


def read_cards():
    import re
    out = []
    for f in FILES:
        text = io.open(os.path.join(DATA, f), encoding='utf-8').read()
        for m in re.finditer(r"id:\s*'([^']+)'[^\n]*\n?\s*name:\s*\{\s*zh:\s*'([^']+)'", text):
            cid, zh = m.group(1), m.group(2)
            cls = cid.split('.')[0] if cid.split('.')[0] in CLS else 'matter'
            if '.rite.' in cid:
                cls = 'rite'
            elif '.law.' in cid:
                cls = 'law'
            out.append((cid, zh, cls))
    return out


def main():
    index_text = io.open(INDEX, encoding='utf-8').read()
    cards = read_cards()
    if not cards:
        print('没解析到卡表，检查 data/cards.*.js 的写法')
        return 0

    found = [c for c in cards if c[1] in index_text]
    missing = [c for c in cards if c[1] not in index_text]

    print('card-reconcile · 实装 %d 张 / CARD-INDEX 目标 540 张' % len(cards))
    print('  名字能在清单里找到：%d' % len(found))
    print('  找不到：%d' % len(missing))

    by_cls = {}
    for cid, zh, cls in missing:
        by_cls.setdefault(cls, []).append(zh)
    if by_cls:
        print('\n差的按类分：')
        for cls, names in sorted(by_cls.items()):
            print('  %s %d 张：%s' % (CLS[cls], len(names), ' / '.join(names)))

    if 'rite' in by_cls or 'law' in by_cls:
        print('\n注意：CARD-INDEX 按「名字」列的是物类；术与律在那份清单里没有名字条目。')
        print('      这不是漂移，是口径不同——要么给术/律补名字表，要么在清单开头写明不收术律。')

    print('\n结论文本不要写在这里，写进 specs/STATUS.md §3。')
    return 0


if __name__ == '__main__':
    main()
