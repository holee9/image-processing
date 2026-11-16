# QA-A-340 step 1: lock the frame list (QA-A-331 card-rule sets, design 6-3 panels) with sha256.
import json, re, hashlib, os, csv, sys
R = 'D:/workspace-github/xpe-pre/.moai/reports/lane-pre/QA-A-331/evidence/results_34sets.json.txt'
BASE = 'D:/workspace-github/xpe-data/images/pre/Cyan_GT1717C'
CYAN = 'D:/workspace-github/image-processing/tests/test_data/cyan_test/calfiles'
DEV = {'25L29G1','25L29G3','25L29I2','26A14J4','26D02I3'}; VAL = {'25L29G2','25L29G4','26A14F1','26D02I4'}
d = json.load(open(R, encoding='utf-8', errors='replace'))
man = [r['로컬경로'].replace(chr(92),'/') for r in csv.DictReader(open('D:/workspace-github/xpe-data/images/pre/MANIFEST.csv', encoding='utf-8-sig'))]
def resolve(rel):
    # garbled (cp949 mis-decoded) chars in results json -> each non-ASCII run is a wildcard within one path segment
    pat = '^Cyan_GT1717C/' + ''.join('[^/]+?' if not c.isascii() else re.escape(c) for c in re.sub(r'[^\x00-\x7f]+', '\u00e9', rel)) + '$'
    hits = [m for m in man if re.match(pat, m)]
    return hits
sha = lambda p: hashlib.sha256(open(p,'rb').read()).hexdigest()
out = []
for k, v in d.items():
    if v.get('status') != 'ok' or k.split('_')[1].startswith('03'): continue   # retry-14 (03xx dates) are not card-rule sets
    panel = [p for p in DEV|VAL if p.endswith(v['panel'])][0]
    hits = resolve(v['rel'])
    if len(hits) != 1: sys.exit(f'STOP path resolve {k}: {hits}')
    folder = 'D:/workspace-github/xpe-data/images/pre/' + hits[0]
    out.append({'set': k, 'panel': panel, 'side': 'dev' if panel in DEV else 'val', 'folder': folder,
                **{r: [[f, sha(folder + '/' + f)] for f in v[r]] for r in ('dark','gain','bpm','ev')}})
c = lambda pre, ns: [[f'{pre}_{n:02d}.raw', sha(f'{CYAN}/{pre}_{n:02d}.raw')] for n in ns]
out.append({'set': 'cyan_test', 'panel': 'cyan_test', 'side': 'dev', 'folder': CYAN, 'dark': c('Dark', range(7,15)), 'gain': c('Bright', range(23,29)),
            'bpm': c('Bright', list(range(17,29)) + list(range(31,37))), 'ev': c('Bright', [29,30])})
json.dump(out, open(sys.argv[1], 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
for s in out: print(s['side'], s['panel'], s['set'], s['folder'])
