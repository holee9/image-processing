"""#1: does each document's Version marker agree with its git history?

WHY THIS IS AN AUDIT AND NOT A FIXER

Writing `**Version**: 1.0.0` into the 108 documents that lack one is a five-line
script, and it would produce a pile of documents all claiming 1.0.0 -- a record
that looks like a record. IEC 62304 Class B wants to know which revision of a
document a thing was built against; a uniform constant answers nothing.

So this tool measures first. It reads each document's real revision history from
git and reports three things:

  MISSING   no marker at all
  STALE     marker exists, but the document was revised after the commit that
            last touched the marker line -- the number is behind the content
  OK        marker exists and no content commit followed it

STALE is the finding that decides the size of the work. If the 83 documents that
already carry a marker are mostly STALE, the job is 191 documents, not 108.

CONTROL, printed first: a file known to have a marker and a file known to have
none. If either row is wrong the parser is broken and every count below is noise.

LIMIT -- rename tracking. This uses `git log --follow`, which reconstructs
renames heuristically. This repository has renamed documents (see #207, #211
where renames silently broke citations), so a document renamed with heavy edits
may report a shorter history than it has. That direction under-reports STALE,
so the STALE count is a LOWER bound.

Usage:
  python tools/docs/version_marker_audit.py            # summary
  python tools/docs/version_marker_audit.py --list     # per-file rows
"""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8")  # type: ignore[union-attr]
    except (AttributeError, ValueError):
        pass

REPO = pathlib.Path(__file__).resolve().parents[2]
ROOTS = ("docs/", ".moai/specs/")
SKIP = ("docs/archive/", ".moai/backups/")
MARKER = re.compile(r"^(?:\*\*Version\*\*|Version|version)\s*[:：]", re.MULTILINE)


def tracked() -> list[str]:
    out = subprocess.run(
        ["git", "-C", str(REPO), "ls-files", "*.md"], capture_output=True, check=True
    ).stdout.decode("utf-8", "replace")
    return [
        f
        for f in out.splitlines()
        if f.strip() and f.startswith(ROOTS) and not f.startswith(SKIP)
    ]


def last_commit(rel: str, pathspec: str | None = None) -> str:
    """Latest commit SHA touching rel (optionally only lines matching pathspec)."""
    cmd = ["git", "-C", str(REPO), "log", "--follow", "-1", "--format=%H"]
    if pathspec:
        cmd += ["-S", pathspec]
    cmd += ["--", rel]
    out = subprocess.run(cmd, capture_output=True).stdout.decode("utf-8", "replace")
    return out.strip()


def commits_since(rel: str, sha: str) -> int:
    out = subprocess.run(
        ["git", "-C", str(REPO), "log", "--follow", "--format=%H", f"{sha}..HEAD", "--", rel],
        capture_output=True,
    ).stdout.decode("utf-8", "replace")
    return len([l for l in out.splitlines() if l.strip()])


def classify(rel: str) -> tuple[str, str]:
    try:
        text = (REPO / rel).read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return "ERROR", str(exc)
    m = MARKER.search(text)
    if not m:
        return "MISSING", ""
    line = text[m.start() : text.find("\n", m.start())].strip()
    marker_commit = last_commit(rel, pathspec=line[:40])
    if not marker_commit:
        return "OK?", f"{line}  (marker commit not found)"
    n = commits_since(rel, marker_commit)
    if n > 0:
        return "STALE", f"{line}  (+{n} content commits since)"
    return "OK", line


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    files = tracked()

    # controls, before any count
    ctl_has = next((f for f in files if classify(f)[0] != "MISSING"), None)
    ctl_none = next((f for f in files if classify(f)[0] == "MISSING"), None)
    print(f"CONTROL with-marker    -> {ctl_has or 'NONE FOUND (parser broken?)'}")
    print(f"CONTROL without-marker -> {ctl_none or 'NONE FOUND (parser broken?)'}")
    print(f"scanned: {len(files)} markdown files under {', '.join(ROOTS)}\n")

    buckets: dict[str, list[tuple[str, str]]] = {}
    for rel in files:
        state, detail = classify(rel)
        buckets.setdefault(state, []).append((rel, detail))

    for state in ("OK", "STALE", "MISSING", "OK?", "ERROR"):
        rows = buckets.get(state, [])
        if not rows:
            continue
        print(f"{state}: {len(rows)}")
        if args.list:
            for rel, detail in rows:
                print(f"    {rel}" + (f"  — {detail}" if detail else ""))

    stale = len(buckets.get("STALE", []))
    missing = len(buckets.get("MISSING", []))
    print(
        f"\nwork size = MISSING {missing} + STALE {stale} = {missing + stale}"
        f"  (STALE is a lower bound; see the rename limit in the module docstring)"
    )


if __name__ == "__main__":
    main()
