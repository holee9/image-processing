"""Read-only gate evidence summary; does not modify sources or verification."""
from collections import Counter
from pathlib import Path
import json
import re
import subprocess
import sys
sys.stdout.reconfigure(encoding='utf-8')
root=Path(__file__).resolve().parent
for name in ['gate-before.log','gate-after-two-files.log','gate-after.log','build.log']:
    p=root/name
    if not p.is_file(): continue
    data=p.read_bytes()
    text=data.decode('utf-16' if data.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig', errors='replace')
    files=Counter(re.findall(r'([A-Za-z]+\.cs)\(\d+,\d+\)',text))
    print(name,json.dumps(dict(files)), 'lines=',len(text.splitlines()))
    if name in ['gate-after.log','build.log']: print('\n'.join(text.splitlines()[-18:]))
git='C:/Program Files/Git/cmd/git.exe'
files=['clients/ImageProcTest/Diagnostics/NativeDependencyLoader.cs','clients/ImageProcTest/Services/NativePreprocessPreviewService.cs','clients/ImageProcTest/Services/NativePresentationExportService.cs']
r=subprocess.run([git,'diff','-w','--',*files],capture_output=True,text=True)
print('diff -w exit=',r.returncode,'stdout_bytes=',len(r.stdout.encode()))
for file in files:
    old=subprocess.check_output([git,'show','HEAD:'+file])
    new=Path(file).read_bytes()
    # Independent byte check: all non-whitespace bytes, not formatting only by assertion.
    assert re.sub(rb'\s',b'',old)==re.sub(rb'\s',b'',new),file
print('PASS: non-whitespace bytes identical in all three C# files.')
