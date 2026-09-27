#!/usr/bin/env python3
"""Fail when code cites a requirement ID that no SPEC defines.

The mirror of `check_spec_test_refs.py`. That one guards SPEC -> test citations;
this one guards code -> SPEC citations. Both failures are silent in exactly the
same way: nothing breaks, the build stays green, and the citation keeps pointing
at nothing until a person reads both documents side by side.

Why this exists (#195, #197). Two independent cases landed within days:

  #195  RTM-CALIB-001 traces `SRS-CALIB-FUNC-004..017` whose meanings moved in
        SRS-CALIB-001. The RTM stayed internally consistent with its own test
        table, so nothing looked wrong.
  #197  `.moai/specs/SPEC-XPE-P1A/spec.md` was replaced wholesale by an
        independently written document that reused the `REQ-P1A-` namespace
        from 001. 66 code citations kept the superseded numbers.

The shared gap is procedural, not textual: changing a requirement ID has no step
that moves the things citing it. This check is that step's alarm.

WHAT IS CHECKED
  A citation is the literal token `REQ-<SERIES>-<NNN>` in a tracked file under
  `modules/`, `clients/`, or `gui/`. The known-ID set for a series is every
  `REQ-<SERIES>-<NNN>` appearing anywhere under `.moai/specs/`. A citation whose
  ID is absent from its series' set is an orphan.

WHAT IS NOT CHECKED — the important limit
  Only ID EXISTENCE. This check cannot see a citation that names a live ID while
  describing a different requirement, which on #197 was the larger and more
  dangerous half: `preprocess_api.h` cites `REQ-P1A-031` twice, once as "No
  memory leak after shutdown" (correct) and once as "Return
  XPE_ERR_OUT_OF_MEMORY on allocation failure" (not what 031 says). Both IDs
  exist, so both pass here. Judging those needs meaning, not set membership.
  Do not read a green run as "citations are correct".

WHY A RATCHET AND NOT A HARD FAIL
  73 orphan citations exist today (20 IDs). A hard fail would make main red on
  the first run and would be switched off, which is the failure mode of a red
  nobody reads. So the baseline below records what exists; the check fails only
  when a series gains an orphan ID it did not have. Ten of twelve series are
  already clean, and this freezes them at clean.

  Lowering a baseline entry is always allowed and is the point: fix citations,
  re-run with --update-baseline, commit the smaller number.

USAGE
  python tools/docs/check_req_citations.py                  # check (CI)
  python tools/docs/check_req_citations.py --report         # list every orphan
  python tools/docs/check_req_citations.py --update-baseline
"""

from __future__ import annotations

import argparse
import collections
import json
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
BASELINE = REPO / "tools" / "docs" / "req_citation_baseline.json"

SPEC_ROOT = ".moai/specs/"
CODE_ROOTS = ("modules/", "clients/", "gui/")

# `REQ-P1A-012`, `REQ-GUI-IT-003`, `REQ-ENH-CC-002`. The series is non-greedy so
# the trailing three digits are never swallowed into it.
CITATION = re.compile(r"REQ-([A-Z0-9]+(?:-[A-Z0-9]+)*?)-(\d{3})")


def tracked_files() -> list[str]:
    """Tracked paths. Decoded explicitly: the repo has Korean filenames and the
    Windows default codec (cp949) raises on them."""
    out = subprocess.run(
        ["git", "-C", str(REPO), "ls-files"], capture_output=True, check=True
    ).stdout
    return [f for f in out.decode("utf-8", "replace").splitlines() if f.strip()]


def collect() -> tuple[dict[str, set[str]], dict[str, dict[str, list[str]]], int, int]:
    """Return (spec ids per series, orphan sites per series per id, spec files, code files)."""
    spec_ids: dict[str, set[str]] = collections.defaultdict(set)
    cited: dict[str, dict[str, list[str]]] = collections.defaultdict(
        lambda: collections.defaultdict(list)
    )
    n_spec = n_code = 0

    for rel in tracked_files():
        is_spec = rel.startswith(SPEC_ROOT)
        is_code = rel.startswith(CODE_ROOTS)
        if not (is_spec or is_code):
            continue
        try:
            text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        hits = CITATION.findall(text)
        if not hits:
            continue
        if is_spec:
            n_spec += 1
            for series, num in hits:
                spec_ids[series].add(num)
        else:
            n_code += 1
            for lineno, line in enumerate(text.splitlines(), 1):
                for series, num in CITATION.findall(line):
                    cited[series][num].append(f"{rel}:{lineno}")

    orphans: dict[str, dict[str, list[str]]] = {}
    for series, per_id in cited.items():
        known = spec_ids.get(series, set())
        missing = {num: sites for num, sites in per_id.items() if num not in known}
        if missing:
            orphans[series] = missing
    return spec_ids, orphans, n_spec, n_code


def self_check(n_spec: int, n_code: int) -> None:
    """A scan that reads nothing reports no orphans, and would pass silently.

    So the corpus is asserted before any conclusion is drawn from its emptiness
    — the same instrument failure that produced a confident wrong answer while
    investigating #195."""
    if n_spec < 10 or n_code < 20:
        sys.exit(
            f"[check_req_citations] corpus too small to trust: "
            f"{n_spec} spec files, {n_code} code files. "
            f"Run from inside the repository."
        )


def load_baseline() -> dict[str, list[str]]:
    if not BASELINE.exists():
        return {}
    return json.loads(BASELINE.read_text(encoding="utf-8"))["orphans"]


def write_baseline(orphans: dict[str, dict[str, list[str]]]) -> None:
    payload = {
        "_comment": (
            "Orphan requirement citations that existed when this check was "
            "introduced (#197). Entries may shrink, never grow. See the module "
            "docstring in check_req_citations.py."
        ),
        "orphans": {s: sorted(ids) for s, ids in sorted(orphans.items())},
    }
    # newline="" so Windows does not rewrite every line ending to CRLF, which
    # would make each --update-baseline run produce a whole-file diff.
    with BASELINE.open("w", encoding="utf-8", newline="") as fh:
        fh.write(json.dumps(payload, indent=2) + "\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--report", action="store_true", help="list every orphan and exit 0")
    ap.add_argument("--update-baseline", action="store_true")
    args = ap.parse_args()

    spec_ids, orphans, n_spec, n_code = collect()
    self_check(n_spec, n_code)

    total_ids = sum(len(v) for v in orphans.values())
    total_sites = sum(len(s) for v in orphans.values() for s in v.values())

    if args.report:
        print(f"scanned {n_spec} spec files, {n_code} code files")
        print(f"{total_ids} orphan ids / {total_sites} citations\n")
        for series in sorted(orphans):
            known = len(spec_ids.get(series, set()))
            print(f"REQ-{series}-*  ({known} ids defined in SPECs)")
            for num in sorted(orphans[series]):
                sites = orphans[series][num]
                print(f"  {num}  x{len(sites):<3} {sites[0]}")
                for s in sites[1:]:
                    print(f"            {s}")
        return 0

    if args.update_baseline:
        write_baseline(orphans)
        print(f"baseline written: {total_ids} orphan ids / {total_sites} citations")
        return 0

    baseline = load_baseline()
    regressions: list[str] = []
    for series in sorted(orphans):
        allowed = set(baseline.get(series, []))
        for num in sorted(orphans[series]):
            if num not in allowed:
                site = orphans[series][num][0]
                regressions.append(
                    f"  REQ-{series}-{num} is cited at {site} "
                    f"but no SPEC under {SPEC_ROOT} defines it"
                )

    if regressions:
        print("[check_req_citations] new orphan requirement citations:\n")
        print("\n".join(regressions))
        print(
            "\nEither the ID is a typo, or a SPEC renumbering left this citation "
            "behind.\nFix the citation; do not add it to the baseline."
        )
        return 1

    fixed = sum(len(v) for v in baseline.values()) - total_ids
    note = f" ({fixed} fewer than baseline — run --update-baseline)" if fixed > 0 else ""
    print(
        f"[check_req_citations] no new orphans "
        f"({total_ids} known orphan ids, {total_sites} citations){note}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
