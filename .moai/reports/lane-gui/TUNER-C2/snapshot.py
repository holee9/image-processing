"""AST/literal-only evidence generator. No inspected module is imported/executed."""
import ast
import difflib
import hashlib
import json
from pathlib import Path
import sys
sys.stdout.reconfigure(encoding='utf-8')
S = Path('D:/workspace/ProjectGit/PE_analysis/param_tuner')
R = Path(__file__).resolve().parent
if sys.argv[1] == 'verify':
    expected=json.loads((R/'source-sha256.json').read_text(encoding='utf-8'))
    for name,digest in expected.items():
        p=Path(name)
        assert (hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else 'MISSING')==digest, name
    records=(R/'workcopy/orig-sha256.txt').read_text(encoding='utf-8').splitlines()
    changed=[]
    for line in records:
        digest,name=line.split('  ',1)
        assert hashlib.sha256((S/Path(name).name).read_bytes()).hexdigest()==digest, name
        p=R/'workcopy'/name
        if hashlib.sha256(p.read_bytes()).hexdigest()!=digest: changed.append(name)
        if p.suffix=='.py': ast.parse(p.read_text(encoding='utf-8-sig'))
    assert changed==['param_tuner/app.py'], changed
    assert (R/'workcopy/param_tuner/app.py').read_text(encoding='utf-8-sig')==(R/'patch/app.py').read_text(encoding='utf-8-sig')
    print('PASS: 19 source hashes unchanged; workcopy only app.py modified; 17 Python files AST-valid; patch copy matches workcopy.')
elif sys.argv[1] == 'workhash':
    files=sorted([*S.glob('*.py'),S/'protocols.json',S/'REQUIREMENTS.md'])
    records=[]
    for p in files:
        digest=hashlib.sha256(p.read_bytes()).hexdigest()
        assert digest == hashlib.sha256((R/'workcopy/param_tuner'/p.name).read_bytes()).hexdigest(), p.name
        records.append(f'{digest}  param_tuner/{p.name}')
    print('\n'.join(records))
elif sys.argv[1] == 'diff':
    a = (S/'app.py').read_text(encoding='utf-8-sig').splitlines(True)
    b = (R/'patch/app.py').read_text(encoding='utf-8-sig').splitlines(True)
    print(''.join(difflib.unified_diff(a,b,fromfile='a/param_tuner/app.py',tofile='b/param_tuner/app.py',n=0)),end='')
else:
    ps = ast.parse((S/'param_space.py').read_text(encoding='utf-8-sig'))
    rp_path = S.parent/'run_pipeline.py'
    rp = ast.parse(rp_path.read_text(encoding='utf-8-sig')) if rp_path.is_file() else ast.Module(body=[],type_ignores=[])
    constants = {}
    for node in rp.body:
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    try: constants[target.id] = (ast.literal_eval(node.value),node.lineno)
                    except (ValueError,TypeError): pass
    definitions = next(n for n in ps.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='_POST_DEFS' for t in n.targets))
    rows = ast.literal_eval(definitions.value)
    mapped = {r[0]:(r,node.lineno) for r,node in zip(rows,definitions.value.elts)}
    out=['# 52-slot static matrix','',f'S = `{S.as_posix()}`. Expected `{rp_path.as_posix()}` is missing; external run_pipeline defaults remain UNRESOLVED, no alternative module search/import. GUI initial-value expressions and fallback behavior are verified, actual numeric startup values are not.', '',
         'Common GUI trace: app.py:223-225 -> 536-564 -> 953-970 -> 1036/1057 -> Worker:180-198. Integer text parsing, no slider or configured step; min/max expanded to include initial. All stored slots: param_space.py:166-174. Fixed rows have no table cell and are re-created by default_params, including after JSON load.', '',
         '|Slot|Name / constant|Declared range|Default @ run_pipeline line|GUI range/init/search|Definition line|Judgment|','|---|---|---|---|---|---|---|']
    fixed={18:'STRUCT_CONTRAST0',22:31,27:0,39:1}
    for i in range(40):
        if i in mapped:
            (_,name,attr,group,lo,hi,en),line=mapped[i]
            val,loc=constants.get(attr,('UNRESOLVED','?'))
            actual=f'{min(lo,val)}..{max(hi,val)}' if isinstance(val,int) else 'min/max(default)'
            enabled='on' if en else 'off'
            if i in (9,32): enabled='Chest on / other off'
            if i==7: enabled='Chest off / other on'
            if i in (11,12,25): enabled='off (user may check)'
            out.append(f'|post.{i}|{name} / {attr}|{lo}..{hi}|{val} @ {loc}|{actual}; {val}; {enabled}|param_space.py:{line}|일치 (table mapping)|')
        else:
            attr=fixed[i];val,loc=constants.get(attr,('UNRESOLVED','?')) if isinstance(attr,str) else (attr,'literal')
            out.append(f'|post.{i}|fixed {attr}|fixed|{val} @ {loc}|no row; default_params restores|param_space.py:13,115-116|코드에 없음 (editable row)|')
    for i in range(12):
        attr=f'LUT_CP{i//3+1}_{"XYW"[i%3]}'; val,loc=constants.get(attr,('UNRESOLVED','?'))
        gui=f'0..100; {val}; on' if i%3!=2 else 'no row; default_params restores'
        judgment='일치 (table mapping)' if i%3!=2 else '코드에 없음 (W editable row)'
        out.append(f'|lut.{i}|{attr}|'+('0..100' if i%3!=2 else 'fixed')+f'|{val} @ {loc}|{gui}|param_space.py:119-121,154-159|{judgment}|')
    out.extend(['','Counts: Post 40 + LUT 12 = 52 serialized; 36 Post + 8 LUT XY = 44 GUI rows; 4 Post + 4 LUT W = 8 hidden/fixed slots. Unused/conditional checkboxes remain user-checkable (app.py:546); automatic masking is not a hard lock.'])
    hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else 'MISSING' for p in [*(S/n for n in ['REQUIREMENTS.md','app.py','widgets.py','param_space.py','rawsize.py','bootstrap.py','results.py','tuner_core.py']),rp_path]}
    print(json.dumps({'matrix':'\n'.join(out)+'\n','hashes':hashes},ensure_ascii=False))
