# QA-A-340 step 3: cyan_test must not be inside the validation panels, by path and by sha256 of every .raw under them.
import hashlib, glob, csv, sys
CYAN = 'D:/workspace-github/image-processing/tests/test_data/cyan_test/calfiles'
BASE = 'D:/workspace-github/xpe-data/images/pre/Cyan_GT1717C/'
VAL = ['25L29G2', '25L29G4', '26A14F1', '26D02I4', '26A14K1']   # 26A14K1: validation panel with no card-rule set, checked too
sha = lambda p: hashlib.sha256(open(p, 'rb').read()).hexdigest()
cy = {sha(p): p for p in glob.glob(CYAN + '/*.raw')}
print('cyan_test raw files:', len(cy))
for r in csv.DictReader(open('D:/workspace-github/xpe-data/images/pre/MANIFEST.csv', encoding='utf-8-sig')):
    if 'ghost test' in r['NAS원본'] and 'cyan test' in r['NAS원본']:
        print('cyan NAS row:', r['로컬경로'], '| serial:', r['시리얼'], '| under a val panel:', any(('/' + v + '/') in r['로컬경로'].replace(chr(92), '/') for v in VAL))
n = hit = 0
for v in VAL:
    for p in glob.glob(BASE + v + '/**/*.raw', recursive=True):
        n += 1
        h = sha(p)
        if h in cy: hit += 1; print('DUPLICATE', p, cy[h])
print('val-panel raw files hashed:', n, 'sha256 matches with cyan_test:', hit)
