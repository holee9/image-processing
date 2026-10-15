#!/usr/bin/env python3
"""cppcheck baseline gate (#255; follows QA-A-234/234c, the measurement-only cppcheck job).

Compares the findings in a cppcheck --xml result file with a baseline file. A finding is identified by (check id,
repository-relative path, message, text of the flagged source line) -- not by line number, which moves with every edit.
Each identity is counted. An identity above its baseline count fails the run; one below it is reported so the
baseline can be tightened. Same identity rules and the same accepted limit as tools/ci/check_clang_tidy.py: two
findings with the same id, message and flagged line text in one file share one identity.

A tool that did not really run is never a pass:
  * the result file must exist and parse, and its root must be <results> with an <errors> element,
  * an <error> without a <location> (cppcheck's own failure messages, e.g. a missing configuration) fails the run
    unless it is one of the ids listed in IGNORED_TOOL_IDS,
  * with --control <cppcheck-exe>, a positive control runs first: cppcheck on a scratch file with a certain null
    pointer dereference must exit 0 and report nullPointer for THAT file. The control uses the same flags the CI step
    uses (passed after --), so a flag change that blinds the tool also blinds the control.

usage: check_cppcheck.py <result.xml> <baseline-file> [--write-baseline] [--control <cppcheck-exe> -- <flags...>]
Baseline lines are tab-separated: id, relative path, message, line text, count.
"""
import collections
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

# Informational ids cppcheck emits without a source location that do not mean the analysis failed.
IGNORED_TOOL_IDS = {'checkersReport', 'missingIncludeSystem', 'normalCheckLevelMaxBranches'}


def fail(msg):
    sys.exit('check_cppcheck: ' + msg)


def rel(path):
    try:
        p = os.path.relpath(os.path.realpath(path), os.path.realpath(os.getcwd()))
    except ValueError:  # another drive (the positive control's scratch file can live on one)
        p = os.path.realpath(path)
    return p.replace('\\', '/')


def line_text(path, line):
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            for i, text in enumerate(f, 1):
                if i == line:
                    return ' '.join(text.split())
    except OSError:
        pass
    fail(f'cannot read line {line} of {path} (a finding points at a file this checkout does not have)')


def parse(xml_path, label):
    try:
        root = ET.parse(xml_path).getroot()
    except (OSError, ET.ParseError) as e:
        fail(f'{label}: cannot parse {xml_path}: {e}')
    if root.tag != 'results' or root.find('errors') is None:
        fail(f'{label}: {xml_path} has no <results>/<errors> -- cppcheck did not produce an analysis')
    found = []
    for e in root.iter('error'):
        loc = e.find('location')
        if loc is None:
            if e.get('id') in IGNORED_TOOL_IDS:
                continue
            fail(f'{label}: cppcheck reported a problem with no source location: {e.get("id")}: {e.get("msg")}')
        path, line = loc.get('file'), int(loc.get('line', '0'))
        found.append({'id': e.get('id'), 'path': rel(path), 'abs': path, 'line': line,
                      'msg': ' '.join((e.get('msg') or '').split())})
    return found


def control(exe, flags):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, 'control.cpp')
        with open(src, 'w', encoding='utf-8') as f:
            f.write('int control_null() {\n    int* p = nullptr;\n    return *p;\n}\n')
        out = os.path.join(d, 'control.xml')
        with open(out, 'w', encoding='utf-8') as err:
            r = subprocess.run([exe] + flags + [src], stderr=err, stdout=subprocess.PIPE, text=True)
        if r.returncode != 0:
            fail(f'positive control: cppcheck exited {r.returncode}')
        want = os.path.normcase(os.path.realpath(src))
        hits = [x for x in parse(out, 'the positive control')
                if x['id'] == 'nullPointer' and os.path.normcase(os.path.realpath(x['abs'])) == want]
        if not hits:
            fail('positive control: cppcheck did not report nullPointer on a certain null dereference -- '
                 'the tool or its flags are blind')
    print('check_cppcheck: positive control reported nullPointer on its scratch file')


def main(argv):
    flags = []
    if '--' in argv:
        i = argv.index('--')
        argv, flags = argv[:i], argv[i + 1:]
    exe = None
    if '--control' in argv:
        i = argv.index('--control')
        exe = argv[i + 1]
        del argv[i:i + 2]
    write = '--write-baseline' in argv
    argv = [a for a in argv if a != '--write-baseline']
    if len(argv) != 2:
        fail(__doc__.split('usage: ')[1].splitlines()[0])
    xml_path, baseline = argv
    if exe:
        control(exe, flags)

    counts = collections.Counter()
    for x in parse(xml_path, xml_path):
        counts[(x['id'], x['path'], x['msg'], line_text(x['abs'], x['line']))] += 1

    if write:
        with open(baseline, 'w', encoding='utf-8', newline='\n') as f:
            for key in sorted(counts):
                f.write('\t'.join(key) + '\t' + str(counts[key]) + '\n')
        print(f'check_cppcheck: wrote {len(counts)} baseline lines, {sum(counts.values())} findings')
        return

    base = collections.Counter()
    try:
        with open(baseline, encoding='utf-8') as f:
            for n, raw in enumerate(f, 1):
                raw = raw.rstrip('\r\n')
                if not raw:
                    continue
                parts = raw.split('\t')
                if len(parts) != 5 or not parts[4].isdigit():
                    fail(f'{baseline}:{n}: expected 5 tab-separated fields ending in a count')
                base[tuple(parts[:4])] += int(parts[4])
    except OSError as e:
        fail(f'cannot read baseline {baseline}: {e}')

    new = {k: counts[k] - base[k] for k in counts if counts[k] > base[k]}
    gone = {k: base[k] - counts[k] for k in base if base[k] > counts[k]}
    print(f'{sum(counts.values())} findings; baseline {sum(base.values())}')
    for k, d in sorted(gone.items()):
        print(f'  below baseline (tighten it): {k[0]} {k[1]} | {k[3][:60]} (-{d})')
    for k, d in sorted(new.items()):
        print(f'  NEW: {k[0]} {k[1]} | {k[2]} | {k[3][:80]} (+{d})')
    if new:
        fail(f'{sum(new.values())} finding(s) above the baseline')


if __name__ == '__main__':
    main(sys.argv[1:])
