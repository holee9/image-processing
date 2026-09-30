#!/usr/bin/env python3
"""Compare an existing CMake build directory against the preset it claims to be.

Why this exists
---------------
`cmake --build <dir>` never asks what `<dir>` was configured with. A build
directory created before a preset changed keeps its old cache forever, and the
build still succeeds -- it just quietly builds a different set of targets.

That is not hypothetical. On 2026-09-30 a lane reported `ci-post 705/705` green
on a new test file. Its build directory had been configured with `BUILD_AI=OFF`
three weeks before the preset gained `BUILD_AI=ON`, so `xpe_ai_tests` was never
built and the 705 contained zero AI tests. The real number was 895. The file did
not compile under `/WX`, and one line took down two workflows and four jobs.

The asymmetry is the point: a change to a `CMakeLists.txt` is caught, because
Ninja regenerates. A change to a preset *value* is not caught by anything.

Reporting more numbers does not close this. In that incident the missing 190
tests were already printed in the lane's previous report -- the signal existed
and no human compared two documents. So the check has to be mechanical.

Usage
-----
    python tools/ci/check_cache_matches_preset.py <build-dir> [preset-name]

The preset name is inferred from the build directory's leaf name when omitted
(`build/ci-post` -> `ci-post`). Exit code 0 when every cache variable the preset
declares matches; 1 on any drift, missing cache, or unknown preset.
"""

import json
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PRESETS = os.path.join(REPO, "CMakePresets.json")


def load_presets():
    with open(PRESETS, encoding="utf-8") as fh:
        return {p["name"]: p for p in json.load(fh).get("configurePresets", [])}


def effective_cache(presets, name, seen=None):
    """Resolve a preset's cacheVariables through its inherits chain.

    A preset's own values win over anything it inherits, and `inherits` may be a
    string or a list. Cycles are refused rather than followed -- a malformed
    preset must not make this script hang while pretending to check something.
    """
    seen = seen or []
    if name in seen:
        raise ValueError("inherits cycle: " + " -> ".join(seen + [name]))
    if name not in presets:
        raise KeyError(name)
    node = presets[name]
    parents = node.get("inherits") or []
    if isinstance(parents, str):
        parents = [parents]
    merged = {}
    for parent in parents:
        merged.update(effective_cache(presets, parent, seen + [name]))
    merged.update(node.get("cacheVariables") or {})
    return merged


def normalise(value):
    """Reduce a preset value and a cache value to a comparable form.

    Preset values may be bare strings or `{"type": ..., "value": ...}` objects,
    and CMake spells booleans several ways. `${sourceDir}` is expanded so a path
    variable compares as the path the cache actually holds.
    """
    if isinstance(value, dict):
        value = value.get("value", "")
    if isinstance(value, bool):
        value = "ON" if value else "OFF"
    text = str(value).replace("${sourceDir}", REPO).replace("\\", "/").strip()
    upper = text.upper()
    if upper in ("ON", "TRUE", "YES", "1"):
        return "ON"
    if upper in ("OFF", "FALSE", "NO", "0", "NOTFOUND"):
        return "OFF"
    return text


CACHE_LINE = re.compile(r"^([A-Za-z_][A-Za-z0-9_.\-]*):([A-Z]+)=(.*)$")


def read_cache(path):
    entries = {}
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith(("#", "//")):
                continue
            m = CACHE_LINE.match(line)
            if m:
                entries[m.group(1)] = m.group(3)
    return entries


def main(argv):
    if not 2 <= len(argv) <= 3:
        print(__doc__.strip())
        return 1

    build_dir = argv[1]
    preset = argv[2] if len(argv) == 3 else os.path.basename(os.path.normpath(build_dir))
    cache_path = os.path.join(build_dir, "CMakeCache.txt")

    if not os.path.isfile(cache_path):
        print("GAP  no CMakeCache.txt at %s" % cache_path)
        print("     Nothing was checked. An absent cache is not a pass.")
        return 1

    presets = load_presets()
    try:
        declared = effective_cache(presets, preset)
    except KeyError:
        print("GAP  preset '%s' is not in CMakePresets.json" % preset)
        print("     known: %s" % ", ".join(sorted(presets)))
        return 1
    except ValueError as exc:
        print("GAP  %s" % exc)
        return 1

    cache = read_cache(cache_path)
    drift, absent = [], []
    for key in sorted(declared):
        want = normalise(declared[key])
        if key not in cache:
            absent.append((key, want))
        elif normalise(cache[key]) != want:
            drift.append((key, want, normalise(cache[key])))

    print("preset %s  vs  %s" % (preset, cache_path))
    print("  %d variables declared by the preset" % len(declared))

    for key, want, got in drift:
        print("  DRIFT   %-34s preset=%-12s cache=%s" % (key, want, got))
    for key, want in absent:
        print("  ABSENT  %-34s preset=%-12s cache=(not present)" % (key, want))

    if drift or absent:
        print("")
        print("FAIL  this directory was configured from a different preset than the one")
        print("      it is named after. A suite total measured here does not describe")
        print("      the preset. Reconfigure: cmake --preset %s" % preset)
        return 1

    print("  OK      every declared variable matches")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
