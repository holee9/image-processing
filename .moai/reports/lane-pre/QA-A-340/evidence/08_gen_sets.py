# QA-A-340: build each locked set's clean calibration (same options as QA-A-331 G4: --bpm-v2 --bpm-margins 6,2,1,1)
# and dump its defect map and line records (defect-xcal --map-out --lines-out). 4 sets at a time.
import json, subprocess, sys, os, concurrent.futures as cf
BIN = 'D:/workspace-github/xpe-pre/build/ci-preprocess/bin'; T = 'D:/workspace-github/xpe-data/qa-340-tmp'
sets = json.load(open(sys.argv[1], encoding='utf-8'))
def one(s):
    o = f"{T}/{s['set']}"; f = s['folder']
    a = [f'{BIN}/xpe_calib_fixture_gen.exe', '--out', o, '--width', '3072', '--height', '3072', '--expiry-ms', '4102444800000']
    for flag, key in (('dark', 'dark'), ('bright', 'gain'), ('bpm-bright', 'bpm')): a += sum([[f'--{flag}', f'{f}/{n}'] for n, _ in s[key]], [])
    a += ['--bpm-v2', '--bpm-margins', '6,2,1,1']
    r1 = subprocess.run(a, capture_output=True, text=True, encoding='utf-8', errors='replace')
    r2 = subprocess.run([f'{BIN}/xpe_bpm_eval.exe', 'defect-xcal', '--in', f'{o}/defect.xcal', '--map-out', f'{o}/map.u8', '--lines-out', f'{o}/lines.tsv'], capture_output=True, text=True)
    return f"{s['set']} gen_rc={r1.returncode} xcal_rc={r2.returncode} | {r1.stdout.strip().splitlines()[-3:-1]} | {r2.stdout.strip().splitlines()[1:4]}"
with cf.ThreadPoolExecutor(4) as ex:
    for line in ex.map(one, sets): print(line, flush=True)
