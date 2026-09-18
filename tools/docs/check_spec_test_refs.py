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
                        Matched by SUFFIX against every known test name, so the
                        customary shortening (`…OutputClampedAndSourceIntact`)
                        is accepted.
  2. `Suite.Test`     - examined ONLY when one side already matches something
                        real (a known suite or a known test name). That is what
                        makes it test-shaped rather than a .NET API call, a
                        config key, or a file path.

KNOWN LIMIT (QA-B-127 adversarial probe): a citation whose suite AND test are
both unknown passes silently — e.g. an entire suite deleted, or both halves
mistyped. Widening evidence collection (below) shrinks that blind spot by making
more suites known, but does not close it. A shape heuristic was measured and
rejected: on this repo it flagged 16 tokens of which most were .NET API names
(`Marshal.GetDelegateForFunctionPointer`, `GC.GetTotalMemory`,
`System.AccessViolationException`) — indistinguishable from test names by shape,
so the heuristic would have made the checker noisy enough to ignore.
"""

import glob
import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

GTEST_DEF = re.compile(r"TEST(?:_F|_P)?\(\s*([A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*\)")
# xUnit: a [Fact]/[Theory] method inside a class. Collected coarsely — the class
# is the suite, the method the test — which is all a citation ever names.
CS_CLASS = re.compile(r"\b(?:public|internal)\s+(?:sealed\s+|static\s+|partial\s+)*class\s+([A-Za-z0-9_]+)")
CS_TEST = re.compile(r"\[(?:Fact|Theory)[^\]]*\][\s\S]{0,400}?\b(?:public|private|internal)\s+(?:async\s+)?(?:void|Task(?:<[^>]+>)?)\s+([A-Za-z0-9_]+)\s*\(")

ELLIPSIS_CITE = re.compile(r"`…([A-Za-z0-9_]+)`")
DOTTED_CITE = re.compile(r"`([A-Za-z][A-Za-z0-9_]*)\.([A-Za-z][A-Za-z0-9_]*)`")

# Every tree that can hold a test a SPEC might cite. `modules/*/tests` alone left
# `tests/**` and the C# suites invisible, so citations there could not be judged
# at all (QA-B-127 finding 3).
CPP_ROOTS = [
    os.path.join(REPO, "modules", "*", "tests", "**", "*.cpp"),
    os.path.join(REPO, "tests", "**", "*.cpp"),
]
CS_ROOTS = [
    os.path.join(REPO, "clients", "**", "*.cs"),
    os.path.join(REPO, "gui", "**", "*.cs"),
]


def _read(path):
    try:
        return io.open(path, encoding="utf-8", errors="ignore").read()
    except OSError:
        return ""


def collect_definitions():
    full, suites, names = set(), set(), set()

    for root in CPP_ROOTS:
        for path in glob.glob(root, recursive=True):
            for suite, name in GTEST_DEF.findall(_read(path)):
                full.add(suite + "." + name)
                suites.add(suite)
                names.add(name)

    for root in CS_ROOTS:
        for path in glob.glob(root, recursive=True):
            text = _read(path)
            methods = CS_TEST.findall(text)
            if not methods:
                continue
            classes = CS_CLASS.findall(text)
            names.update(methods)
            suites.update(classes)
            for cls in classes:
                for m in methods:
                    full.add(cls + "." + m)

    return full, suites, names


def stale_citations(spec_text, full, suites, names):
    bad = []
    for cited in ELLIPSIS_CITE.findall(spec_text):
        # Suffix match: a document may elide the front of a long test name.
        if not any(n == cited or n.endswith(cited) for n in names):
            bad.append("…" + cited)
    for suite, name in DOTTED_CITE.findall(spec_text):
        token = suite + "." + name
        if token in full:
            continue
        if suite in suites or name in names:
            bad.append(token)
    return bad


def _rel(path):
    try:
        return os.path.relpath(path, REPO)
    except ValueError:
        # Different drive (a probe file under the temp dir, say). The absolute
        # path is still a usable pointer; crashing here is not.
        return path


def main(argv):
    full, suites, names = collect_definitions()
    if not full:
        print("check_spec_test_refs: no test definitions found - cannot verify, "
              "treating as failure.", file=sys.stderr)
        return 2

    specs = argv[1:] or sorted(
        glob.glob(os.path.join(REPO, ".moai", "specs", "*", "*.md")))
    failed = False
    for spec in specs:
        for token in stale_citations(_read(spec), full, suites, names):
            failed = True
            print("%s: cites a test that does not exist: %s" % (_rel(spec), token),
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
