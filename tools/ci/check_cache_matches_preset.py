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
    # CMake resolves a conflict between multiple `inherits` parents in favour of
    # the EARLIER one, so the later parents are applied first and the earlier
    # ones overwrite them. The obvious loop (apply in listed order) gets this
    # backwards; xpe-reviewer caught it by running the function on a synthetic
    # left/right conflict. No preset here has two parents today, which is why
    # none of my own arms could have hit it.
    for parent in reversed(parents):
        merged.update(effective_cache(presets, parent, seen + [name]))
    merged.update(node.get("cacheVariables") or {})
    return merged


ENV_MACRO = re.compile(r"\$env\{[^}]*\}")


def raw_value(value):
    if isinstance(value, dict):
        value = value.get("value", "")
    if isinstance(value, bool):
        value = "ON" if value else "OFF"
    return str(value)


def normalise(value, source_dir):
    """Reduce a preset value and a cache value to a comparable form.

    Preset values may be bare strings or `{"type": ..., "value": ...}` objects,
    and CMake spells booleans several ways.

    `${sourceDir}` expands to `source_dir` -- the repository the BUILD DIRECTORY
    belongs to, read from its own cache, NOT the repository this script happens
    to live in. Those differ whenever the check is run across checkouts, which
    is the normal case here: every lane works in a worktree. Expanding against
    the script's repo reported `VCPKG_MANIFEST_DIR` as drifted for a directory
    that had just been configured from the preset -- a false FAIL, and one
    indistinguishable from real drift by anything but reading the paths.
    """
    text = raw_value(value).replace("${sourceDir}", source_dir).replace("\\", "/").strip()
    upper = text.upper()
    if upper in ("ON", "TRUE", "YES", "1"):
        return "ON"
    if upper in ("OFF", "FALSE", "NO", "0"):
        return "OFF"
    # NOTFOUND is deliberately NOT folded into OFF: "this path was not found"
    # and "this option is disabled" are different states, and collapsing them
    # made a missing toolchain compare equal to a disabled feature.

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

    # The build directory's own repository root. `${sourceDir}` must expand
    # against THIS, not against the checkout the script sits in.
    source_dir = (cache.get("CMAKE_HOME_DIRECTORY") or REPO).replace("\\", "/").rstrip("/")

    drift, absent, unresolved, unset = [], [], [], []
    for key in sorted(declared):
        if declared[key] is None:
            # A preset value of null means "do not set this variable". The cache
            # agreeing means the key is absent; a present key is the drift.
            if key in cache:
                drift.append((key, "(unset)", normalise(cache[key], source_dir)))
            else:
                unset.append(key)
            continue
        raw = raw_value(declared[key])
        if ENV_MACRO.search(raw):
            # The VALUE cannot be reconstructed, but the key still has to exist.
            # Skipping the presence check too let a cache that never set the
            # variable pass -- xpe-reviewer's finding on line 153.
            if key not in cache:
                absent.append((key, raw))
                continue
            # `$env{...}` resolves from the environment CMake was configured
            # in, which this process cannot reconstruct. Comparing it would
            # produce a verdict in both directions with nothing behind it, so
            # it is reported as unchecked rather than guessed.
            unresolved.append((key, raw))
            continue
        want = normalise(declared[key], source_dir)
        if key not in cache:
            absent.append((key, want))
        elif normalise(cache[key], source_dir) != want:
            drift.append((key, want, normalise(cache[key], source_dir)))

    print("preset %s  vs  %s" % (preset, cache_path))
    print("  source dir (from cache): %s" % source_dir)
    print("  %d declared, %d compared, %d not comparable" %
          (len(declared), len(declared) - len(unresolved), len(unresolved)))

    for key, want, got in drift:
        print("  DRIFT   %-34s preset=%-12s cache=%s" % (key, want, got))
    for key, want in absent:
        print("  ABSENT  %-34s preset=%-12s cache=(not present)" % (key, want))
    for key, raw in unresolved:
        print("  UNCHECKED %-32s %s   (key present, value not reconstructible)" % (key, raw))
    for key in unset:
        print("  UNSET-OK  %-32s preset declares null, cache has no key" % key)

    if drift or absent:
        print("")
        print("FAIL  this directory was configured from a different preset than the one")
        print("      it is named after. A suite total measured here does not describe")
        print("      the preset. Reconfigure: cmake --preset %s" % preset)
        return 1

    if unresolved:
        print("  OK      every comparable variable matches"
              " (%d left unchecked above)" % len(unresolved))
    else:
        print("  OK      every declared variable matches")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
