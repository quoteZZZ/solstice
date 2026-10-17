# -*- coding: utf-8 -*-
"""Pre-commit check 1/2: every relative markdown link must resolve.

Scans docs/, the root README and all tracked directory READMEs. Exits 1 on any
broken link. Contract: docs/INDEX.md SS5. Usage: python tools/checks/check-links.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TARGETS = [
    'docs',
    'README.md',
    '.codebuddy/rules',
    'tools/README.md',
    'tools/docs',
    'shared/README.md',
    'studio/README.md',
    'game/Content/README.md',
]
LINK = re.compile(r'\]\(([^)\s]+?)\)')

files = []
for t in TARGETS:
    p = os.path.join(ROOT, t)
    if os.path.isfile(p):
        files.append(p)
    elif os.path.isdir(p):
        for dp, _dn, fn in os.walk(p):
            for f in fn:
                if f.endswith(('.md', '.mdc')):
                    files.append(os.path.join(dp, f))

bad = []
checked = 0
for p in sorted(files):
    with open(p, encoding='utf-8') as fh:
        text = fh.read()
    for m in LINK.finditer(text):
        link = m.group(1)
        if re.match(r'^[a-zA-Z][a-zA-Z0-9+.-]*:', link):      # http:, mailto:, etc.
            continue
        if link.startswith('#'):
            continue
        path = link.split('#')[0]
        if not path:
            continue
        checked += 1
        tgt = os.path.normpath(os.path.join(os.path.dirname(p), path))
        if not os.path.exists(tgt):
            bad.append('%s  ->  %s' % (os.path.relpath(p, ROOT), link))

print('root          : %s' % ROOT)
print('files scanned : %d' % len(files))
print('links checked : %d' % checked)
print('broken links  : %d' % len(bad))
for b in bad:
    print('  BROKEN ' + b)
sys.exit(1 if bad else 0)
