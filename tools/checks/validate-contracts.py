# -*- coding: utf-8 -*-
"""Pre-commit check 2/2: validate game data against shared/schema contracts.

Stdlib-only mini JSON Schema (draft-07) subset: type / required / properties /
additionalProperties / enum / pattern / items / minimum / maximum / oneOf /
$ref (#/definitions/...). That covers the four Solstice schemas without a pip
dependency; if a schema starts using more keywords, extend VALIDATE below or
switch to `pip install jsonschema`.

Also checks duplicate card / species ids across files, and localization key
parity between en.json and zh-Hans.json (missing/extra keys, {placeholder}
mismatches, empty values). Exits 1 on any error.
Contract: docs/INDEX.md SS5. Usage: python tools/checks/validate-contracts.py
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCHEMA_DIR = os.path.join(ROOT, 'shared', 'schema')
DATA_DIR = os.path.join(ROOT, 'game', 'Config', 'Solstice', 'Data')
FIXTURE_DIR = os.path.join(ROOT, 'shared', 'fixtures')
L10N_DIR = os.path.join(ROOT, 'game', 'Config', 'Solstice', 'Localization')
L10N_FILES = ['en.json', 'zh-Hans.json']
PLACEHOLDER = re.compile(r'\{[A-Za-z_][A-Za-z0-9_]*\}')

PAIRS = [
    ('card.schema.json', ['cards.earth.json', 'cards.fire.json', 'cards.metal.json'], 'cards', DATA_DIR),
    ('species.schema.json', ['species.json'], 'species', DATA_DIR),
    ('world.schema.json', ['world.json'], None, DATA_DIR),
    ('save.schema.json', ['save.sample.json'], None, FIXTURE_DIR),
]


def load(path):
    with open(path, encoding='utf-8') as fh:
        return json.load(fh)


def resolve(ref, root_schema):
    assert ref.startswith('#/'), 'only local refs are supported: %s' % ref
    node = root_schema
    for part in ref[2:].split('/'):
        node = node[part]
    return node


def type_ok(value, name):
    if isinstance(name, list):
        return any(type_ok(value, n) for n in name)
    if name == 'object':
        return isinstance(value, dict)
    if name == 'array':
        return isinstance(value, list)
    if name == 'string':
        return isinstance(value, str)
    if name == 'integer':
        return isinstance(value, int) and not isinstance(value, bool)
    if name == 'number':
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if name == 'boolean':
        return isinstance(value, bool)
    if name == 'null':
        return value is None
    raise ValueError('unsupported type keyword %r' % name)


def validate(value, schema, root_schema, where, errors):
    if '$ref' in schema:
        validate(value, resolve(schema['$ref'], root_schema), root_schema, where, errors)
        return
    if 'oneOf' in schema:
        hits = 0
        for sub in schema['oneOf']:
            probe = []
            validate(value, sub, root_schema, where, probe)
            if not probe:
                hits += 1
        if hits != 1:
            errors.append('%s: expected exactly one oneOf branch, got %d' % (where, hits))
        return
    if 'type' in schema and not type_ok(value, schema['type']):
        errors.append('%s: expected type %r' % (where, schema['type']))
        return
    if 'enum' in schema and value not in schema['enum']:
        errors.append('%s: %r not in enum' % (where, value))
    if 'pattern' in schema and isinstance(value, str) and not re.search(schema['pattern'], value):
        errors.append('%s: %r does not match %s' % (where, value, schema['pattern']))
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        if 'minimum' in schema and value < schema['minimum']:
            errors.append('%s: %r below minimum %r' % (where, value, schema['minimum']))
        if 'maximum' in schema and value > schema['maximum']:
            errors.append('%s: %r above maximum %r' % (where, value, schema['maximum']))
    if isinstance(value, dict):
        for key in schema.get('required', []):
            if key not in value:
                errors.append('%s: missing required key %r' % (where, key))
        props = schema.get('properties', {})
        for key, sub in props.items():
            if key in value:
                validate(value[key], sub, root_schema, '%s.%s' % (where, key), errors)
        extra = schema.get('additionalProperties', True)
        if extra is False:
            for key in value:
                if key not in props:
                    errors.append('%s: unexpected key %r' % (where, key))
        elif isinstance(extra, dict):
            for key in value:
                if key not in props:
                    validate(value[key], extra, root_schema, '%s.%s' % (where, key), errors)
    if isinstance(value, list) and 'items' in schema:
        for i, item in enumerate(value):
            validate(item, schema['items'], root_schema, '%s[%d]' % (where, i), errors)


import re

SOURCE_DIR = os.path.join(ROOT, 'game', 'Source')
LITERAL_RE = re.compile(r'Loc\(\s*TEXT\("([A-Za-z0-9_.]+)"\)\s*\)')
PREFIX_RE = re.compile(r'Printf\(\s*TEXT\("([A-Za-z0-9_]+\.)%s"\)')


def scan_loc_keys(errors):
    """
    R-17: the l10n alignment pass above only compares the two JSON files, so it cannot see a key
    that is assembled in C++. That is exactly how `species.sp.moss-grass` (R-05) leaked a raw key
    into the panel: the string never existed in any JSON, so nothing could miss it.

    Two shapes are scanned: literal `Loc(TEXT("a.b"))` keys, and runtime prefixes of the form
    `Printf(TEXT("species.%s"), ...)`. A literal key that no l10n file defines is an error; a
    runtime prefix is reported as INFO because its suffix is resolved at run time.
    """
    known = set()
    for name in L10N_FILES:
        path = os.path.join(L10N_DIR, name)
        if os.path.exists(path):
            with open(path, 'r', encoding='utf-8') as handle:
                known.update(json.load(handle).keys())

    literals, prefixes = [], []
    if not os.path.isdir(SOURCE_DIR):
        return literals, prefixes

    for dirpath, _dirnames, filenames in os.walk(SOURCE_DIR):
        for fname in filenames:
            if not fname.endswith(('.cpp', '.h')):
                continue
            path = os.path.join(dirpath, fname)
            rel = os.path.relpath(path, ROOT)
            try:
                with open(path, 'r', encoding='utf-8', errors='replace') as handle:
                    text = handle.read()
            except OSError:
                continue
            for line_no, line in enumerate(text.splitlines(), 1):
                for match in LITERAL_RE.finditer(line):
                    key = match.group(1)
                    literals.append((rel, line_no, key))
                    if key not in known:
                        errors.append('loc key %r used at %s:%d is not defined in any l10n file'
                                      % (key, rel, line_no))
                for match in PREFIX_RE.finditer(line):
                    prefixes.append((rel, line_no, match.group(1)))
    return literals, prefixes


def scan_world_scale(errors):
    """
    Config drift guard.

    world.json once shipped size=512 while the code default and docs/setting/WORLD.md SS2.1 both
    said 128. That one number made the realm four times wider: the camera had to sit at 144k units
    to frame it, the fog went opaque at that range, and the mesh grew sixteen-fold. It was found by
    eye after three wrong guesses - which is exactly the kind of thing a check exists to prevent.
    """
    world = os.path.join(DATA_DIR, 'world.json')
    types = os.path.join(ROOT, 'game', 'Source', 'Solstice', 'Public', 'Sim', 'SolsticeTypes.h')
    if not (os.path.exists(world) and os.path.exists(types)):
        return None
    with open(types, 'r', encoding='utf-8', errors='replace') as handle:
        match = re.search(r'int32\s+Size\s*=\s*(\d+)', handle.read())
    if not match:
        return None
    expected = int(match.group(1))
    with open(world, 'r', encoding='utf-8') as handle:
        data = json.load(handle)
    seen = []
    if isinstance(data.get('default'), dict):
        seen.append(('default', data['default'].get('size')))
    for index, preset in enumerate(data.get('presets') or []):
        if isinstance(preset, dict):
            seen.append(('presets[%d]' % index, preset.get('size')))
    for where, value in seen:
        if value is not None and int(value) != expected:
            errors.append('world.json %s size=%s but the code default is %d' % (where, value, expected))
    return (expected, len(seen))


def main():

    errors = []
    pairs_checked = 0
    seen_ids = {}

    for schema_name, data_names, list_key, data_dir in PAIRS:
        schema = load(os.path.join(SCHEMA_DIR, schema_name))
        for data_name in data_names:
            data = load(os.path.join(data_dir, data_name))
            validate(data, schema, schema, data_name, errors)
            pairs_checked += 1
            if list_key:
                for i, entry in enumerate(data.get(list_key, [])):
                    entry_id = entry.get('id')
                    if entry_id:
                        seen_ids.setdefault(entry_id, []).append('%s[%d]' % (data_name, i))

    for entry_id, spots in sorted(seen_ids.items()):
        if len(spots) > 1:
            errors.append('duplicate id %s in %s' % (entry_id, ', '.join(spots)))

    l10n_keys = 0
    tables = {}
    for name in L10N_FILES:
        try:
            table = load(os.path.join(L10N_DIR, name))
        except (OSError, ValueError) as exc:
            errors.append('localization %s: %s' % (name, exc))
            continue
        if not isinstance(table, dict):
            errors.append('localization %s: top level is not an object' % name)
            continue
        tables[name] = table

    if len(tables) == len(L10N_FILES):
        base_name = L10N_FILES[0]
        base = tables[base_name]
        l10n_keys = len(base)
        for name in L10N_FILES[1:]:
            other = tables[name]
            for key in sorted(set(base) - set(other)):
                errors.append('localization key missing in %s: %s' % (name, key))
            for key in sorted(set(other) - set(base)):
                errors.append('localization key extra in %s: %s' % (name, key))
            for key in sorted(set(base) & set(other)):
                if key.startswith('_'):
                    continue
                for where, table in ((base_name, base), (name, other)):
                    if not isinstance(table[key], str) or not table[key].strip():
                        errors.append('localization %s: empty value for %s' % (where, key))
                if isinstance(base[key], str) and isinstance(other[key], str):
                    here = sorted(PLACEHOLDER.findall(base[key]))
                    there = sorted(PLACEHOLDER.findall(other[key]))
                    if here != there:
                        errors.append('localization placeholder mismatch for %s: %s vs %s'
                                      % (key, here, there))

    loc_literals, loc_prefixes = scan_loc_keys(errors)
    world_scale = scan_world_scale(errors)
    if world_scale:
        print('world scale   : %d x %d (checked %d entries)' % (world_scale[0], world_scale[0], world_scale[1]))

    print('schema dir    : %s' % SCHEMA_DIR)
    print('data dir      : %s' % DATA_DIR)
    print('fixtures dir  : %s' % FIXTURE_DIR)
    print('l10n dir      : %s' % L10N_DIR)
    print('pairs checked : %d' % pairs_checked)
    print('ids checked   : %d' % len(seen_ids))
    print('l10n keys     : %d' % l10n_keys)
    print('loc literals  : %d' % len(loc_literals))
    print('loc prefixes  : %d' % len(loc_prefixes))
    for rel, line_no, prefix in loc_prefixes:
        print('  INFO runtime-built key %s* at %s:%d' % (prefix, rel, line_no))
    print('errors        : %d' % len(errors))
    for e in errors:
        print('  ERROR ' + e)
    sys.exit(1 if errors else 0)


if __name__ == '__main__':
    main()
