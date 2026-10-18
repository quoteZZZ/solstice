"""Layer guard: keep the kernel free of the engine, and the domains pointing one way.

Two rules, both mechanical:

1. **Kernel purity.** Anything under `Sim/` (and later `SolsticeKernel/`) may only include
   engine headers that are pure math/containers (`CoreMinimal`, `Math/*`). No `UObject`,
   no `Tick`, no render, no input, no `Engine/`. That is what lets `SolsticeBench` drive the
   simulation headless and deterministically - the moment a `UCLASS` sneaks in, the bench
   stops being able to run without an engine, and nobody notices until a regression.

2. **Dependency direction.** `Sim -> Gen -> Render -> World -> God/UI -> Dev`. A file may
   include its own layer or a lower one, never a higher one. Rendering may know about the
   world; the world must not know about a HUD.

Run from the repo root:  python tools/checks/check-layers.py
Exit code 0 = clean, 1 = violations found (CI and pre-commit both call this).
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCE = os.path.join(ROOT, "game", "Source")

# Layer index: a file may include itself or anything with a *lower* index.
# `Dev` is -1 on purpose: it is the diagnostics surface (shot / bench hooks), and any layer is
# allowed to talk to it. `Core` is 0 because it holds the cross-domain contracts.
LAYERS = {
    "Dev": -1,
    "Core": 0,
    "Sim": 0,
    "Gen": 1,
    "Render": 2,
    "World": 3,
    "God": 4,
    "UI": 4,
}

# Known, owned exceptions. Every entry must say what removes it - an exception without an exit
# date is how a layer guard turns into decoration.
ALLOWED = {
    ("Core/SolsticeGameInstance.cpp", "Render/SolsticeCelMaterial.h"):
        "GameInstance owns the material set; moves up to World/ in B5",
}

# Headers a kernel file is allowed to pull in from the engine.
KERNEL_ALLOWED_ENGINE = (
    "CoreMinimal.h",
    "Math/",
    "Containers/",
    "Templates/",
    "Misc/",
    "Algo/",
    "HAL/",
)

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')


def layer_of(rel_path):
    """Return (layer_name, index) for a path relative to Source/."""
    parts = rel_path.split(os.sep)
    # Skip the module root and Public/Private.
    for part in parts:
        if part in LAYERS:
            return part, LAYERS[part]
    return None, None


def first_segment(include, known_dirs):
    """Return the layer a `#include` points at, if it points inside our source tree."""
    head = include.split("/")[0]
    if head in known_dirs:
        return head
    # Same-directory include such as "SolsticeKernel.h"
    return None


def main():
    if not os.path.isdir(SOURCE):
        print(f"source tree not found: {SOURCE}")
        return 1

    known = {name for name in LAYERS}
    violations = []
    scanned = 0

    for dirpath, _dirnames, filenames in os.walk(SOURCE):
        for filename in filenames:
            if not filename.endswith((".h", ".cpp", ".inl")):
                continue
            full = os.path.join(dirpath, filename)
            rel = os.path.relpath(full, SOURCE)
            layer, index = layer_of(rel)
            if layer is None:
                continue
            scanned += 1

            with open(full, encoding="utf-8", errors="replace") as handle:
                for lineno, line in enumerate(handle, 1):
                    match = INCLUDE_RE.match(line)
                    if not match:
                        continue
                    include = match.group(1).replace("\\", "/")

                    # ---- rule 1: kernel must not touch the engine ----
                    if layer == "Sim":
                        # A kernel file may include its own layer's headers (that is the module
                        # itself, not the engine).
                        if include.startswith("Sim/"):
                            continue
                        if not include.startswith(KERNEL_ALLOWED_ENGINE):
                            violations.append(
                                (rel, lineno, include,
                                 "kernel includes engine header (Sim must stay UObject-free)")
                            )
                        continue

                    # ---- rule 2: dependency direction ----
                    target = first_segment(include, known)
                    if target is None or target == layer:
                        continue
                    if LAYERS[target] <= index:
                        continue
                    if (f"{layer}/{os.path.basename(rel)}", include) in ALLOWED:
                        continue
                    violations.append(
                        (rel, lineno, include,
                         f"{layer} includes higher layer {target}")
                    )

    print(f"source root  : {SOURCE}")
    print(f"files scanned: {scanned}")
    if violations:
        print(f"violations   : {len(violations)}")
        for rel, lineno, include, why in violations[:40]:
            print(f"  {rel}:{lineno}  {include}  <- {why}")
        if len(violations) > 40:
            print(f"  ... and {len(violations) - 40} more")
        return 1

    print("violations   : 0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
