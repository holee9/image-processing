#!/usr/bin/env python3
"""Fail when a SPEC cites a test that no longer exists.

A test name is the only traceability link between a requirement and its evidence.
Renaming a test breaks neither the build nor the suite, so a stale citation is
silent: the requirement keeps pointing at nothing until someone reads the
document against the source by hand.

That happened twice on #180 in one day — a transcription error (`GsvgProcess`
for `GsvgVirtualGridApi`) and a rename that left the document behind
(`KnownDivergence_*` -> `ProvisionalFloor_*_REQ_GSVG_00N`, commit c638773).
A lane found both by reading the pushed spec; no check did. This is that check.

Scope is deliberately narrow, because a noisy checker gets ignored. Only two
citation shapes are examined:

  1. `…SomeTestName`  - the ellipsis says "this is a test, suite elided".
                        Must suffix-match a real test name.
  2. `Suite.Test`     - examined ONLY when one side already matches something
                        real (a known suite or a known test name). That is what
                        makes it test-shaped rather than a config key, a DICOM
                        tag, or a file path. The pair must then exist.

Anything else in backticks is left alone.
"""

import glob
import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

TEST_DEF = re.compile(r"TEST(?:_F|_P)?\(\s*([A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*\)")
ELLIPSIS_CITE = re.compile(r"`…([A-Za-z0-9_]+)`")
DOTTED_CITE = re.compile(r"`([A-Za-z][A-Za-z0-9_]*)\.([A-Za-z][A-Za-z0-9_]*)`")


def collect_definitions():
    full, suites, names = set(), set(), set()
    for path in glob.glob(os.path.join(REPO, "modules", "*", "tests", "**", "*.cpp"),
                          recursive=True):
        try:
            text = io.open(path, encoding="utf-8", errors="ignore").read()
        except OSError:
            continue
        for suite, name in TEST_DEF.findall(text):
            full.add(suite + "." + name)
            suites.add(suite)
            names.add(name)
    return full, suites, names


def stale_citations(spec_text, full, suites, names):
    bad = []
    for name in ELLIPSIS_CITE.findall(spec_text):
        if name not in names:
            bad.append("…" + name)
    for suite, name in DOTTED_CITE.findall(spec_text):
        token = suite + "." + name
        if token in full:
            continue
        # Only judge it when it is already test-shaped: one half is real.
        if suite in suites or name in names:
            bad.append(token)
    return bad


def main(argv):
    full, suites, names = collect_definitions()
    if not full:
        print("check_spec_test_refs: no TEST definitions found under "
              "modules/*/tests - cannot verify, treating as failure.",
              file=sys.stderr)
        return 2

    specs = argv[1:] or sorted(
        glob.glob(os.path.join(REPO, ".moai", "specs", "*", "*.md")))
    failed = False
    for spec in specs:
        text = io.open(spec, encoding="utf-8").read()
        rel = os.path.relpath(spec, REPO)
        for token in stale_citations(text, full, suites, names):
            failed = True
            print("%s: cites a test that does not exist: %s" % (rel, token),
                  file=sys.stderr)
    if failed:
        print("\nA renamed or mistyped test name breaks traceability silently - "
              "the requirement points at nothing and no build fails. Fix the "
              "citation to match the source.", file=sys.stderr)
        return 1
    print("check_spec_test_refs: %d test definitions, %d spec file(s), "
          "no stale citations" % (len(full), len(specs)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
