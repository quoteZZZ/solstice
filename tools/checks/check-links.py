# -*- coding: utf-8 -*-
"""Pre-commit check 1/2: every markdown link must resolve.

Two passes:

1. **Markdown links** — scans docs/ (except docs/archive, which is frozen and
   non-binding), the root README, and every tracked directory README.
2. **Doc references in source** — scans game/Source, game/Config and shared for
   `docs/**.md` mentions inside comments and JSON. A code comment pointing at a
   doc that no longer exists is how a repo silently rots.

Exits 1 on any broken link or dead doc reference.
Usage: python tools/checks/check-links.py
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
# Frozen history: kept for the record, never used as a spec, so its links are
# allowed to point at files that were merged away.
SKIP_DIRS = {'archive'}

SOURCE_ROOTS = ['game/Source', 'game/Config', 'shared']
SOURCE_EXT = ('.h', '.cpp', '.cs', '.json', '.ini')

LINK = re.compile(r'\]\(([^)\s]+?)\)')
DOC_REF = re.compile(r'docs/[A-Za-z0-9_./-]+\.md')

files = []
for t in TARGETS:
    p = os.path.join(ROOT, t)
    if os.path.isfile(p):
        files.append(p)
    elif os.path.isdir(p):
        for dp, dn, fn in os.walk(p):
            dn[:] = [d for d in dn if d not in SKIP_DIRS]
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

# ---- pass 2: doc paths mentioned in source ----
refs = 0
for root in SOURCE_ROOTS:
    base = os.path.join(ROOT, root)
    if not os.path.isdir(base):
        continue
    for dp, _dn, fn in os.walk(base):
        for f in fn:
            if not f.endswith(SOURCE_EXT):
                continue
            fp = os.path.join(dp, f)
            try:
                with open(fp, encoding='utf-8', errors='replace') as fh:
                    text = fh.read()
            except OSError:
                continue
            for m in DOC_REF.finditer(text):
                ref = m.group(0)
                refs += 1
                if not os.path.exists(os.path.join(ROOT, ref)):
                    bad.append('%s  ->  %s (dead doc ref)' % (os.path.relpath(fp, ROOT), ref))

print('root          : %s' % ROOT)
print('files scanned : %d' % len(files))
print('links checked : %d' % checked)
print('doc refs      : %d' % refs)
print('broken links  : %d' % len(bad))
for b in bad:
    print('  BROKEN ' + b)
sys.exit(1 if bad else 0)
