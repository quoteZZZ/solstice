#!/usr/bin/env python
"""Guard the master-material generator against silently not rebuilding.

Two failures this catches, both of which have already happened:

1. A `Build*()` function that does not stamp `RecipeVersion`. Only M_Cel and M_Sea
   were stamped; M_Sky rebuilt *only when the file was missing*, so a 200-line
   rewrite of the sky graph left the old 4.8 KB asset on disk and the frame
   rendered pure white from a dome whose material still expected painted vertex
   colours. The rewrite looked applied; it was not.

2. A `Build*()` function that nothing calls. An orphaned builder is a material
   that exists in source and never in the project.

Usage:  python tools/checks/check-materials.py
Exit:   0 clean, 1 violations
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BUILDER = os.path.join(
    ROOT, "game", "Source", "SolsticeEditor", "Private", "SolsticeCelMaterialBuilder.cpp"
)


def read(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def function_bodies(text, names):
    """Return {name: body} for `bool Name()` definitions, by brace matching."""
    bodies = {}
    for name in names:
        match = re.search(r"\n\tbool %s\(\)\s*\n\t\{" % re.escape(name), text)
        if not match:
            continue
        start = text.index("{", match.start())
        depth = 0
        for index in range(start, len(text)):
            char = text[index]
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    bodies[name] = text[start:index + 1]
                    break
    return bodies


def main():
    if not os.path.exists(BUILDER):
        print("materials : builder not found: %s" % BUILDER)
        return 1

    text = read(BUILDER)
    problems = []

    version = re.search(r"constexpr int32 CelRecipeVersion = (\d+);", text)
    if not version:
        problems.append("no CelRecipeVersion constant")
        stamp = None
    else:
        stamp = version.group(1)

    declared = sorted(set(re.findall(r"\n\tbool (Build[A-Za-z]+)\(\)", text)))
    bodies = function_bodies(text, declared)

    missing = [name for name in declared if name not in bodies]
    for name in missing:
        problems.append("%s: definition not found (declared but not written?)" % name)

    for name, body in bodies.items():
        if "RecipeVersion" not in body:
            problems.append(
                "%s: no RecipeVersion stamp - this material will never rebuild after an "
                "edit, and the stale asset looks like a working change" % name
            )

    # Every builder must be reachable from the entry point.
    for name in bodies:
        if not re.search(r"\b%s\(\)" % re.escape(name), text.split("bool BuildAll")[-1]):
            problems.append("%s: never called from BuildAll" % name)

    print("materials : recipe v%s, %d builders (%s)"
          % (stamp, len(bodies), ", ".join(sorted(bodies))))

    if problems:
        for problem in problems:
            print("  FAIL  %s" % problem)
        print("materials : %d problem(s)" % len(problems))
        return 1

    print("materials : 0 problems")
    return 0


if __name__ == "__main__":
    sys.exit(main())
