#!/usr/bin/env python3
"""clang-tidy baseline gate (QA-A-234, 234c, 234d; #253). Draft for tools/ci/check_clang_tidy.py.

Runs clang-tidy over modules/<module>/src/*.cpp with the module's own .clang-tidy and compares the findings with a
baseline file. A finding is identified by (check, repository-relative path, normalized message, text of the flagged
source line) -- not by line number (moves with every edit), not by basename (two folders can share one), and not by a
per-file count (cannot tell "one fixed, another made elsewhere" from "nothing changed"). Each identity is counted. An
identity above its baseline count fails the run; one below it is reported so the baseline can be tightened.

A tool that did not really run is never a pass:
  * a non-zero exit of any clang-tidy run fails; "error:" / "Error while processing" in stdout or stderr fails,
  * diagnostics are collected from stdout AND stderr; a line that looks like a diagnostic (` warning: ` / ` error: `)
    but cannot be parsed FAILS -- it is never dropped silently,
  * an empty compile database, or a source file with no command in it, fails; no sources, or no clang-tidy, fails,
  * a positive control runs first: on a scratch file with a certain finding the tool must exit 0, print no error, and
    report an empty-catch diagnostic for THAT file that parses.

KNOWN LIMIT (accepted, QA-A-234d): two findings with the same check, the same message and exactly the same flagged
line text in ONE file share one identity. Fixing one and creating another with identical text keeps the count equal and
passes. Closing that needs surrounding-context tracking, which this gate does not do. The gate lists the identities
that have a count of 2 or more so they are visible ("these can miss a swap"); that listing never fails the run.

usage: check_clang_tidy.py <module> <build-dir> <baseline-file> [--write-baseline]
The build dir must be configured with Ninja; the compile database is taken from `ninja -t compdb`.
Baseline lines are tab-separated: check, relative path, message, line text, count.
"""
import collections
import glob
import json
import os
import re
import subprocess
import sys
import tempfile

EXTRA = ['--extra-arg=-D_CRT_USE_BUILTIN_OFFSETOF']  # MSVC offsetof is not a constant expression for clang
PAT = re.compile(r'^(?P<path>.+?):(?P<line>\d+):(?P<col>\d+): warning: (?P<msg>.*?) \[(?P<check>[\w\-.]+)\]\s*$', re.M)
LOOKS_LIKE_DIAGNOSTIC = re.compile(r'^.*(?: warning: | error: ).*$', re.M)
FAILED = re.compile(r'(: error: |Error while processing)')


def fail(msg):
    sys.exit('check_clang_tidy: ' + msg)


def find_tidy():
    for cand in (os.environ.get('CLANG_TIDY'), 'clang-tidy'):
        if not cand:
            continue
        try:
            if subprocess.run([cand, '--version'], capture_output=True).returncode == 0:
                return cand
        except (FileNotFoundError, OSError):
            pass
    vs = os.environ.get('VCINSTALLDIR', '')
    cand = os.path.join(vs, 'Tools', 'Llvm', 'x64', 'bin', 'clang-tidy.exe')
    if vs and os.path.exists(cand):
        return cand
    fail('clang-tidy was not found (not on PATH, CLANG_TIDY unset, none under VCINSTALLDIR\\Tools\\Llvm): '
         'this runner image has no clang-tidy')


def norm(text):
    return re.sub(r'\s+', ' ', text).strip().replace('\t', ' ')


def relpath(path):
    """Repository-relative, forward slashes, lower case (the Windows file system ignores case)."""
    try:
        rel = os.path.relpath(os.path.realpath(path), os.path.realpath(os.getcwd()))
    except ValueError:  # another drive (a toolchain header): not in the repository
        rel = os.path.realpath(path)
    return rel.replace(os.sep, '/').lower()


def source_line(path, line):
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            return norm(f.read().split('\n')[line - 1])
    except (OSError, IndexError):
        return '?'


def diag_key(check, path, msg, text):
    return (check, relpath(path), norm(msg), text)


def parse_diagnostics(out, where):
    """All diagnostics of one run, from stdout and stderr together. Unparseable diagnostic-looking lines fail."""
    parsed = {}
    for m in PAT.finditer(out):
        parsed[(m['path'], m['line'], m['col'], m['check'])] = m  # the same diagnostic printed twice counts once
    matched_lines = {m.group(0).strip() for m in parsed.values()} | {m.group(0).strip() for m in PAT.finditer(out)}
    for ln in LOOKS_LIKE_DIAGNOSTIC.findall(out):
        if ln.strip() not in matched_lines:
            fail(f'{where}: a diagnostic-looking line could not be parsed, so the findings would not be complete:\n  ' + ln.strip()[:300])
    return list(parsed.values())


def run_tidy(tidy, args, where):
    p = subprocess.run([tidy, *args], capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = p.stdout + '\n' + p.stderr
    if p.returncode != 0:
        fail(f'clang-tidy exited {p.returncode} on {where}: the findings would not be complete\n' + out[:2000])
    if FAILED.search(out):
        fail(f'clang-tidy reported an error while analyzing {where}: the findings would not be complete\n' + out[:2000])
    return out


def positive_control(tidy):
    """A tool that exits 0 and prints nothing would pass a module whose baseline is empty. It is first run on a scratch
    file with a certain finding (an empty catch) and must exit 0, print no error, and report that finding for that file."""
    d = tempfile.mkdtemp()
    src = os.path.join(d, 'control.cpp')
    with open(src, 'w', encoding='utf-8') as f:
        f.write('int control() {\n    try {\n        return 1;\n    } catch (...) {\n    }\n    return 0;\n}\n')
    out = run_tidy(tidy, ['--quiet', '--checks=-*,bugprone-empty-catch', src, '--', '-std=c++17'], 'the positive control')
    # The whole path must be the scratch file itself: a finding for some other control.cpp (same basename, another
    # folder) would not prove that this file was analyzed (Codex #140).
    want = os.path.normcase(os.path.realpath(src))
    found = [m for m in parse_diagnostics(out, 'the positive control')
             if m['check'] == 'bugprone-empty-catch' and os.path.normcase(os.path.realpath(m['path'])) == want]
    if not found:
        fail('the positive control failed: clang-tidy did not report the empty catch planted in control.cpp, '
             'so its silence on the modules would mean nothing\n' + out[:1000])


def main():
    module, build, baseline_path = sys.argv[1:4]
    write = '--write-baseline' in sys.argv
    tidy = find_tidy()
    positive_control(tidy)
    db = tempfile.mkdtemp()
    dbfile = os.path.join(db, 'compile_commands.json')
    with open(dbfile, 'w', encoding='utf-8') as f:
        if subprocess.run(['ninja', '-C', build, '-t', 'compdb'], stdout=f).returncode != 0:
            fail('ninja -t compdb failed: the build directory is not a configured Ninja build')
    with open(dbfile, encoding='utf-8') as f:
        entries = json.load(f)
    if not entries:
        fail('the compile database is empty: nothing would be analyzed')
    known = {os.path.normpath(e['file'] if os.path.isabs(e['file']) else os.path.join(e['directory'], e['file'])).lower()
             for e in entries}
    files = sorted(glob.glob(f'modules/{module}/src/*.cpp'))
    if not files:
        fail(f'no sources under modules/{module}/src: a run over nothing is not a pass')
    seen = collections.Counter()
    for src in files:
        if os.path.normpath(os.path.abspath(src)).lower() not in known:
            fail(f'{src} has no command in the compile database: it would not be analyzed')
        out = run_tidy(tidy, ['-p', db, '--quiet', *EXTRA, src], src)
        for m in parse_diagnostics(out, src):
            if not relpath(m['path']).startswith('modules/'):
                continue  # a finding inside a toolchain or third-party header is not this module's
            seen[diag_key(m['check'], m['path'], m['msg'], source_line(m['path'], int(m['line'])))] += 1
    lines = sorted('\t'.join((*k, str(n))) for k, n in seen.items())
    repeated = sorted(k for k, n in seen.items() if n >= 2)
    if write:
        with open(baseline_path, 'w', encoding='utf-8', newline='\n') as f:
            f.write('\n'.join(lines) + ('\n' if lines else ''))
        print(f'wrote {len(lines)} baseline lines, {sum(seen.values())} findings over {len(files)} files; '
              f'{len(repeated)} identities have a count of 2 or more')
        return
    base = {}
    with open(baseline_path, encoding='utf-8') as f:
        for ln in f.read().split('\n'):
            if ln.strip():
                *k, n = ln.split('\t')
                base[tuple(k)] = int(n)
    bad = [(k, n, base.get(k, 0)) for k, n in sorted(seen.items()) if n > base.get(k, 0)]
    better = [(k, base[k], seen.get(k, 0)) for k in sorted(base) if seen.get(k, 0) < base[k]]
    print(f'{sum(seen.values())} findings over {len(files)} files; baseline {sum(base.values())}')
    for k in sorted(k for k, n in base.items() if n >= 2):
        print(f'  note (accepted limit): {k[0]} {k[1]} | {k[3][:60]} | count {base[k]} -- a swap among these can pass')
    for k, was, now in better:
        print(f'  below baseline (tighten it): {k[0]} {k[1]} | {k[3][:60]} | {was} -> {now}')
    for k, n, b in bad:
        print(f'  NEW: {k[0]} {k[1]} | {k[2][:70]} | {k[3][:60]} | {n} (baseline {b})')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
