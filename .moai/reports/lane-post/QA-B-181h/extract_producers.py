"""QA-B-181h: list, mechanically, every XPE_API function in the public headers that writes an output buffer.

A function is a WRITER when a parameter is a non-const pointer to a pixel-bearing type (XpeImageBuffer, float,
uint8/16, void) -- char* (strings) is excluded. The same pass counts ALL XPE_API declarations it parsed, so a
parser that silently skips headers shows up as a count below the grep count."""
import glob
import os
import re

HEADERS = sorted(set(h.replace(os.sep, '/') for h in glob.glob('modules/*/include/xpe/**/*.h', recursive=True)))
DECL = re.compile(r'XPE_API\s+[\w\s\*]+?\b(\w+)\s*\(([^)]*)\)\s*;')
writers = []
total = 0
names = []
for h in HEADERS:
    s = open(h, encoding='utf-8', errors='replace').read()
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    s = re.sub(r'//[^\n]*', '', s)
    for m in DECL.finditer(s):
        total += 1
        names.append(m.group(1))
        params = [' '.join(p.split()) for p in m.group(2).split(',')]
        out = [p for p in params
               if '*' in p and 'const' not in p.split('*')[0] and not re.search(r'\bchar\s*\*', p)
               and re.search(r'XpeImageBuffer|float|uint16_t|uint8_t|int16_t|void', p)]
        if out:
            writers.append((h.split('/')[1], m.group(1), '; '.join(out)))
for r in writers:
    print('%-16s %-36s %s' % r)
print('WRITERS', len(writers))
print('XPE_API declarations parsed', total, 'distinct', len(set(names)))
