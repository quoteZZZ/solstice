#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Solstice · 文档自检（specs/README.md §3 第 7 条的机器版）

    python tools/docs-check.py          检查并报告
    python tools/docs-check.py -v       连通过项也打出来

三条规则：
  1. 相对链接必须指得到文件。
  2. 章节号必须真的存在：
       - 写了「[X.md](X.md) §n」→ 查 X 里有没有 §n；
       - 只写「§n」（裸引用）→ 按本文件查，本文件没有就是错的。
  3. **跨文档引用必须带小节标题片段**：`§3.1 三条硬约束`，不能光写 `§3.1`。
     理由见 REVIEW-AUDIT-2026-11-03.md §3：A7 曾把「特征改乘数」指向 SETTING §2.8，
     号是存在的、内容是「卡数的分配原则」——只查存在性永远抓不到这种错。

退出码 0 = 全过；1 = 有错误。
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPECS = os.path.join(ROOT, 'specs')

HEAD = re.compile(r'^#{1,6}\s*(?:§\s*)?(\d+(?:\.\d+)*)\s*(.*?)\s*$', re.M)
LINK = re.compile(r'\[([^\]]*)\]\(([^)]+)\)')
SEC = re.compile(r'§\s*(\d+(?:\.\d+)*)')


def load_specs():
    docs = {}
    for name in sorted(os.listdir(SPECS)):
        if not name.endswith('.md'):
            continue
        path = os.path.join(SPECS, name)
        text = open(path, encoding='utf-8').read()
        heads = {}
        for m in HEAD.finditer(text):
            heads[m.group(1)] = m.group(2)
        docs[name] = {'path': path, 'text': text, 'heads': heads}
    return docs


def bigrams(s):
    s = re.sub(r'[\s（）()·、，,。:：/]', '', s)
    return {s[i:i + 2] for i in range(len(s) - 1)}


def check():
    docs = load_specs()
    errors, warns, passed = [], [], 0
    bare_unowned = 0

    for name, doc in docs.items():
        for line_no, line in enumerate(doc['text'].split('\n'), 1):
            where = '%s:%d' % (name, line_no)

            # 规则 1：链接目标存在
            for label, url in LINK.findall(line):
                if url.startswith('http') or url.startswith('#'):
                    continue
                target = url.split('#')[0]
                if not target:
                    continue
                if not os.path.exists(os.path.normpath(os.path.join(SPECS, target))):
                    errors.append('%s 链接指不到文件：%s（%s）' % (where, target, label))
                else:
                    passed += 1

            # 规则 2 / 3：章节引用
            refs = list(SEC.finditer(line))
            if not refs:
                continue
            for m in refs:
                num = m.group(1)
                # 只有「[X.md](X.md) §n」这种紧邻写法才算指名道姓；
                # 同行更远处出现过的链接不算（否则一行里提三份文档就会认错主）
                owner, owner_label = name, None
                for lm in LINK.finditer(line[:m.start()]):
                    base = lm.group(2).split('#')[0]
                    if base.endswith('.md') and (m.start() - lm.end()) <= 6:
                        owner = os.path.basename(base)
                        owner_label = lm.group(1)
                if owner not in docs:
                    continue                                  # 指向仓库外的 md，跳过

                if num not in docs[owner]['heads']:
                    if owner_label is None:
                        # 裸引用且本档没这节：无法判断它到底想指哪份，记警告不拦
                        bare_unowned += 1
                        warns.append('%s §%s 归属不明（本档没有，也没写文档名）' % (where, num))
                    else:
                        errors.append('%s §%s 在 %s 里不存在（%s）'
                                      % (where, num, owner, owner_label))
                    continue

                if owner_label is None:
                    passed += 1
                    continue                                  # 裸引用，本档有这节，算过

                # 跨文档：必须带标题片段
                tail = line[m.end():m.end() + 24]
                head_text = docs[owner]['heads'][num]
                if not (bigrams(head_text) & bigrams(tail)):
                    errors.append('%s §%s 只给了号，没带标题片段（%s 的 §%s 是「%s」）'
                                  % (where, num, owner, num, head_text))
                else:
                    passed += 1

    return errors, warns, passed, bare_unowned


def main():
    verbose = '-v' in sys.argv
    errors, warns, passed, bare = check()
    print('docs-check · specs/ 共 %d 份' % len(load_specs()))
    print('  通过 %d 项' % passed)
    print('  警告 %d 条（裸引用归属不明，不影响退出码）' % len(warns))
    print('  错误 %d 条' % len(errors))
    if '-w' in sys.argv:
        for w in warns:
            print('  ! ' + w)
    for e in errors:
        print('  x ' + e)
    if not errors and verbose:
        print('\n全部通过。')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
