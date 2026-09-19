#!/usr/bin/env python3
"""Check that @param lists in public headers match the actual signatures.

Why this exists (#159): Doxygen runs with WARN_AS_ERROR in CI only, and lane
sessions have no doxygen installed, so a mismatched @param is found after the
merge rather than before it. This script covers the three mismatch classes that
have actually broken the build so far:

  1. an XPE_API function with no doc block at all
  2. an @param name that is not in the signature
  3. the same @param name written twice

It is deliberately conservative: a declaration it cannot parse is skipped and
counted, never reported as a failure. Doxygen stays the authority; this is the
fast local check that catches the common cases before a push.

Usage:  python tools/docs/check_header_docs.py [--quiet]
Exit:   0 = no findings, 1 = findings printed
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER_GLOB = "modules/*/include/**/*.h"

DECL = re.compile(
    # extern "C" may precede XPE_API on the same line. Include it in the match so
    # doc_block_before() does not read it as non-whitespace sitting between the
    # doc comment and the declaration — that produced a false "no doc block"
    # (found by QA-A-113).
    r"(?:extern\s+\"C\"\s+)?XPE_API\s+[A-Za-z_][\w \t*&:<>,]*?\b(?P<name>xpe_[a-z0-9_]+)\s*\((?P<args>[^;{]*)\)\s*;",
    re.DOTALL,
)
PARAM_TAG = re.compile(r"[@\\]param(?:\s*\[[^\]]*\])?\s+(?P<name>[A-Za-z_]\w*)")


def split_args(arglist: str):
    """Return parameter names, or None when the list is too complex to trust."""
    arglist = arglist.strip()
    if arglist in ("", "void"):
        return []
    if "(" in arglist:  # function pointer parameter — do not guess
        return None
    names = []
    for chunk in arglist.split(","):
        chunk = chunk.strip().rstrip("]").split("[")[0].strip()
        if not chunk:
            return None
        m = re.search(r"([A-Za-z_]\w*)\s*$", chunk)
        if not m:
            return None
        names.append(m.group(1))
    return names


def doc_block_before(text: str, pos: int):
    """The /** ... */ block immediately preceding pos, if there is one."""
    head = text[:pos]
    end = head.rfind("*/")
    if end == -1:
        return None
    between = head[end + 2:]
    if between.strip():  # something other than whitespace sits in between
        return None
    start = head.rfind("/**", 0, end)
    if start == -1:
        return None
    return head[start:end]


def check_file(path: Path):
    text = path.read_text(encoding="utf-8", errors="replace")
    findings, skipped = [], 0
    for m in DECL.finditer(text):
        name = m.group("name")
        line = text.count("\n", 0, m.start()) + 1
        args = split_args(m.group("args"))
        if args is None:
            skipped += 1
            continue
        doc = doc_block_before(text, m.start())
        if doc is None:
            findings.append((line, name, "no doc block"))
            continue
        documented = [t.group("name") for t in PARAM_TAG.finditer(doc)]
        seen = set()
        for p in documented:
            if p in seen:
                findings.append((line, name, f"@param {p} is written twice"))
            seen.add(p)
            if p not in args:
                findings.append((line, name, f"@param {p} is not in the argument list"))
        for a in args:
            if a not in seen:
                findings.append((line, name, f"argument {a} has no @param"))
    return findings, skipped


def main() -> int:
    quiet = "--quiet" in sys.argv
    total, skipped_total, files = 0, 0, 0
    for path in sorted(ROOT.glob(HEADER_GLOB)):
        findings, skipped = check_file(path)
        files += 1
        skipped_total += skipped
        for line, name, what in findings:
            rel = path.relative_to(ROOT).as_posix()
            print(f"{rel}:{line}: {name}: {what}")
            total += 1
    if not quiet:
        print(f"-- {files} headers, {skipped_total} declarations skipped as unparseable, {total} findings")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
