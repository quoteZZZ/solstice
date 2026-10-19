#!/usr/bin/env python3
"""Solstice studio - batch ComfyUI driver.

Reads a job list (jsonl) and a ComfyUI API-format workflow template, queues every job against a
running ComfyUI, downloads the results into studio/out/<subdir>/ and appends one line per produced
file to studio/out/manifest.jsonl.

Results are pulled back over HTTP (/view), not read from ComfyUI's output folder on disk: the
output root moves with the install layout (portable root vs. the ComfyUI base folder inside it) and
is not worth hard-coding.

Pipeline target: FLUX.2 Klein 4B fp8 - UNETLoader (models/diffusion_models) + CLIPLoader
(type=flux2, qwen_3_4b) + VAELoader (flux2-vae), sampled with Flux2Scheduler at the documented
baseline (steps=4 / cfg=1.0 / euler, docs/art/ASSETS.md SS3). Model filenames, sampler and the
baseline live in the preset (studio/presets/*.json "pipeline" block), not in this script.

The manifest is the point: storefronts require disclosure of AI-generated content, and the repo
rule is that generated media never enters git but its manifest always does
(see docs/TECH.md SS4 and .gitignore section 3). The field set follows
docs/art/ASSETS.md SS2; paths inside the manifest are relative to studio/.

Standard library only, so it runs with any Python on the machine.

Usage
-----
    python studio/tools/comfy_batch.py --check
    python studio/tools/comfy_batch.py --jobs studio/jobs/paper-textures.jsonl
    python studio/tools/comfy_batch.py --jobs studio/jobs/paper-textures.jsonl --only-subject paper-texture
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
STUDIO_OUT = os.path.join(REPO_ROOT, "studio", "out")
MANIFEST = os.path.join(STUDIO_OUT, "manifest.jsonl")

DEFAULT_HOST = "http://127.0.0.1:8188"
DEFAULT_WORKFLOW = os.path.join(REPO_ROOT, "studio", "workflows", "paper-texture.json")
DEFAULT_PRESET = os.path.join(REPO_ROOT, "studio", "presets", "paper-diorama.json")

# subdir -> (id prefix, manifest kind); see docs/art/ASSETS.md SS2.
KIND_BY_SUBDIR = {
    "textures": ("tex", "texture"),
    "cards": ("card", "card"),
    "cutouts": ("cutout", "cutout"),
    "fx": ("fx", "fx"),
    "bg": ("bg", "bg"),
    "ui": ("ui", "ui"),
}


def post_json(url: str, payload: dict, timeout: float = 30.0) -> dict:
    data = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def get_json(url: str, timeout: float = 30.0) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def choices(info: dict, node: str, field: str) -> list:
    """Return the installed-choice list of one combo input, [] when the node or input is absent."""
    node_info = info.get(node) or {}
    spec = ((node_info.get("input") or {}).get("required") or {}).get(field) or []
    return list(spec[0]) if spec and isinstance(spec[0], list) else []


def check(host: str) -> int:
    try:
        stats = get_json(f"{host}/system_stats", timeout=5.0)
    except (urllib.error.URLError, OSError) as error:
        print(f"[solstice] ComfyUI is not reachable at {host}: {error}")
        print("          start it with D:\\ComfyUI\\run_nvidia_gpu.bat")
        return 1

    devices = stats.get("devices") or []
    for device in devices:
        print(f"[solstice] device: {device.get('name')} vram={device.get('vram_total')}")

    info = get_json(f"{host}/object_info", timeout=30.0)
    unets = choices(info, "UNETLoader", "unet_name")
    clips = choices(info, "CLIPLoader", "clip_name")
    vaes = choices(info, "VAELoader", "vae_name")
    ckpts = choices(info, "CheckpointLoaderSimple", "ckpt_name")

    print(f"[solstice] diffusion_models: {', '.join(unets) or '-'}")
    print(f"[solstice] text_encoders:   {', '.join(clips) or '-'}")
    print(f"[solstice] vae:             {', '.join(vaes) or '-'}")
    print(f"[solstice] checkpoints:     {', '.join(ckpts) or '-'}")

    if not unets and not ckpts:
        print("[solstice] no diffusion model installed; the pipeline is ready, it just has nothing to draw with yet")
        return 2
    if not clips or not vaes:
        print("[solstice] diffusion model present but the text encoder / VAE for it is missing")
        return 2
    return 0


ARCHIVED_SUBDIRS = ("jobs", "presets", "workflows")


def require_archived(path: str, label: str) -> None:
    """
    R-25: a manifest line is a compliance record, so everything it names has to live in the repo.

    `prompt_ref` and `preset` point at a job line and a preset file; if those sit under
    studio/work/ or studio/out/ they are gitignored, and the record ends up pointing at a file
    nobody else can fetch - which defeats per-image traceability. Refuse to run from there rather
    than write a record that cannot be audited.
    """
    if not path:
        return
    full = os.path.abspath(path)
    studio = os.path.join(REPO_ROOT, "studio")
    for sub in ARCHIVED_SUBDIRS:
        archived = os.path.join(studio, sub)
        if os.path.commonpath([full, archived]) == archived:
            return
    raise SystemExit(
        "[solstice] %s -> %s is not inside studio/jobs, studio/presets or studio/workflows; "
        "work/ and out/ are gitignored, so a manifest entry pointing there could never be "
        "reproduced. Move the file into an archived folder first (REVIEW R-25)."
        % (label, os.path.relpath(full, REPO_ROOT))
    )


def load_preset(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def resolve_size(preset: dict, job: dict) -> tuple[int, int]:
    subject = preset["subjects"][job.get("subject", "paper-texture")]
    width, height = job.get("size") or subject.get("size") or [512, 512]
    return int(width), int(height)


def build_prompt(preset: dict, job: dict) -> tuple[str, str]:
    subject_key = job.get("subject", "paper-texture")
    subject = preset["subjects"].get(subject_key)
    if subject is None:
        raise SystemExit(f"[solstice] unknown subject '{subject_key}' in preset")

    # A subject may override the theme word list: a flat texture cannot carry the diorama
    # "photograph of a tabletop model" wording that a standee or card face wants.
    words = subject.get("words", preset["words"])
    negative = preset["negative"] + subject.get("negative", []) + job.get("extra_negative", [])
    # A preset may pin one shared clause (e.g. the exact world geometry) that every subject of
    # that preset must repeat; it goes first so the form is stated before the subject matter.
    lead = preset.get("base_prompt")
    positive = ", ".join(([lead] if lead else []) + [subject["prompt"]] + words + job.get("extra", []))
    return positive, ", ".join(negative)


def fill_template(
    template: dict,
    job: dict,
    preset: dict,
    model: str,
    positive: str,
    negative: str,
    width: int,
    height: int,
) -> dict:
    pipeline = preset.get("pipeline") or {}

    substitutions = {
        "__MODEL__": model,
        "__CLIP__": job.get("clip") or pipeline.get("clip"),
        "__VAE__": job.get("vae") or pipeline.get("vae"),
        "__SAMPLER__": job.get("sampler") or pipeline.get("sampler", "euler"),
        "__POSITIVE__": positive,
        "__NEGATIVE__": negative,
        "__WIDTH__": width,
        "__HEIGHT__": height,
        "__SEED__": job.get("seed", 0),
        "__STEPS__": job.get("steps", 4),
        "__CFG__": job.get("cfg", 1.0),
        "__PREFIX__": job.get("prefix", "solstice/" + job["out"]),
    }

    def walk(node):
        if isinstance(node, dict):
            return {key: walk(value) for key, value in node.items()}
        if isinstance(node, list):
            return [walk(item) for item in node]
        if isinstance(node, str) and node in substitutions:
            value = substitutions[node]
            if value is None:
                raise SystemExit(
                    f"[solstice] the workflow uses {node} but nothing supplies it; "
                    "add the field to the preset 'pipeline' block or to the job line"
                )
            return value
        return node

    return walk(template)


def fetch_image(host: str, image: dict, timeout: float = 120.0) -> bytes:
    """Pull one generated image from ComfyUI's /view endpoint (layout independent)."""
    query = urllib.parse.urlencode(
        {
            "filename": image["filename"],
            "subfolder": image.get("subfolder", ""),
            "type": image.get("type", "output"),
        }
    )
    with urllib.request.urlopen(f"{host}/view?{query}", timeout=timeout) as response:
        return response.read()


def log_entry(entry: dict) -> None:
    """Keep one manifest line per produced file: a reworked asset replaces its own line."""
    lines = []
    replaced = False
    if os.path.isfile(MANIFEST):
        with open(MANIFEST, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.strip()
                if not line:
                    continue
                try:
                    existing = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if existing.get("file") == entry["file"]:
                    lines.append(json.dumps(entry, ensure_ascii=False))
                    replaced = True
                else:
                    lines.append(line)
    if not replaced:
        lines.append(json.dumps(entry, ensure_ascii=False))

    with open(MANIFEST, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")


def sha256_of(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def wait_for_result(host: str, prompt_id: str, timeout: float = 900.0) -> dict:
    deadline = time.time() + timeout
    while time.time() < deadline:
        history = get_json(f"{host}/history/{prompt_id}", timeout=30.0)
        if prompt_id in history:
            return history[prompt_id]
        time.sleep(1.0)
    raise TimeoutError(f"prompt {prompt_id} did not finish within {timeout:.0f}s")


def pick_model(args, preset: dict, unets: list, ckpts: list) -> str:
    if args.model:
        if args.model in unets or args.model in ckpts:
            return args.model
        raise SystemExit(
            f"[solstice] model '{args.model}' is not installed "
            f"(diffusion_models: {unets or '-'}; checkpoints: {ckpts or '-'})"
        )
    preferred = (preset.get("pipeline") or {}).get("unet")
    if preferred and preferred in unets:
        return preferred
    if unets:
        return unets[0]
    if ckpts:
        return ckpts[0]
    raise SystemExit("[solstice] no diffusion model installed; nothing to do")


def main() -> int:
    parser = argparse.ArgumentParser(description="Solstice studio batch driver")
    parser.add_argument("--check", action="store_true", help="probe ComfyUI and list installed models")
    parser.add_argument("--jobs", help="jsonl job list")
    parser.add_argument("--workflow", default=DEFAULT_WORKFLOW)
    parser.add_argument("--preset", default=DEFAULT_PRESET)
    parser.add_argument("--host", default=os.environ.get("COMFY_HOST", DEFAULT_HOST))
    parser.add_argument("--model", help="diffusion model filename; defaults to the preset 'pipeline.unet'")
    parser.add_argument(
        "--only-subject",
        action="append",
        default=[],
        metavar="SUBJECT",
        help="run only the listed preset subjects; repeatable",
    )
    args = parser.parse_args()

    if args.check or not args.jobs:
        return check(args.host)

    # R-25: refuse to run from a gitignored folder, because the manifest would then record a
    # prompt/preset reference that no other checkout can resolve.
    require_archived(args.jobs, '--jobs')
    require_archived(args.preset, '--preset')
    require_archived(args.workflow, '--workflow')

    preset = load_preset(args.preset)
    with open(args.workflow, "r", encoding="utf-8") as handle:
        template = json.load(handle)

    info = get_json(f"{args.host}/object_info", timeout=30.0)
    model = pick_model(args, preset, choices(info, "UNETLoader", "unet_name"), choices(info, "CheckpointLoaderSimple", "ckpt_name"))
    print(f"[solstice] model: {model}")

    os.makedirs(STUDIO_OUT, exist_ok=True)
    produced = 0
    failed = 0

    with open(args.jobs, "r", encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, start=1):
            line = line.strip()
            if not line or line.startswith("//"):
                continue
            job = json.loads(line)
            subject = job.get("subject", "paper-texture")
            if args.only_subject and subject not in args.only_subject:
                continue

            subdir = job.get("subdir", "textures")
            id_prefix, default_kind = KIND_BY_SUBDIR.get(subdir, ("asset", "texture"))
            asset_id = job.get("id") or f"{id_prefix}.{job['out']}"
            width, height = resolve_size(preset, job)

            positive, negative = build_prompt(preset, job)
            prompt = fill_template(template, job, preset, model, positive, negative, width, height)

            print(f"[solstice] {line_number}: {job['out']} ({subject}) {width}x{height}")
            try:
                response = post_json(f"{args.host}/prompt", {"prompt": prompt})
            except urllib.error.HTTPError as error:
                detail = error.read().decode("utf-8", "replace")
                print(f"[solstice]   rejected ({error.code}): {detail[:400]}")
                failed += 1
                continue

            prompt_id = response.get("prompt_id")
            if not prompt_id:
                print(f"[solstice]   rejected: {response}")
                failed += 1
                continue

            result = wait_for_result(args.host, prompt_id)
            outputs = result.get("outputs", {})
            for node in outputs.values():
                for image in node.get("images", []):
                    try:
                        payload = fetch_image(args.host, image)
                    except (urllib.error.URLError, OSError) as error:
                        print(f"[solstice]   cannot fetch {image.get('filename')}: {error}")
                        failed += 1
                        continue

                    destination_dir = os.path.join(STUDIO_OUT, subdir)
                    os.makedirs(destination_dir, exist_ok=True)
                    destination = os.path.join(destination_dir, job["out"] + os.path.splitext(image["filename"])[1])
                    with open(destination, "wb") as handle:
                        handle.write(payload)

                    entry = {
                        "id": asset_id,
                        "file": f"out/{subdir}/{os.path.basename(destination)}",
                        "kind": job.get("kind", default_kind),
                        "style": job.get("style", "T1"),
                        "model": model,
                        "prompt_ref": f"studio/jobs/{os.path.basename(args.jobs)}#{job['out']}",
                        "seed": job.get("seed", 0),
                        "size": [width, height],
                        "date": dt.date.today().isoformat(),
                        "source": "generated",
                        "pick": "",
                        # provenance extras (not in ASSETS SS2, kept for rebuild / audit)
                        "steps": job.get("steps", 4),
                        "cfg": job.get("cfg", 1.0),
                        "sampler": job.get("sampler") or (preset.get("pipeline") or {}).get("sampler", "euler"),
                        "preset": preset.get("id"),
                        "sha256": sha256_of(destination),
                        "tool": "studio/tools/comfy_batch.py",
                        "note": job.get("note", ""),
                    }
                    log_entry(entry)
                    produced += 1
                    print(f"[solstice]   -> {entry['file']}  ({entry['id']})")

    print(f"[solstice] done: {produced} file(s) logged to studio/out/manifest.jsonl, {failed} failure(s)")
    if failed and not produced:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
