"""Read-only AST/source inspection for TUNER-C2; never imports the inspected package."""
import ast
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import urlsplit
sys.stdout.reconfigure(encoding='utf-8')

SOURCE = Path('D:/workspace/ProjectGit/PE_analysis/param_tuner')
ALLOWED = {'REQUIREMENTS.md', 'app.py', 'widgets.py', 'param_space.py', 'rawsize.py',
           'results.py', 'bootstrap.py', 'parallel_eval.py', 'tuner_core.py', 'optimizer.py', 'protocols.py'}

def read(name):
    if name not in ALLOWED:
        raise ValueError('File is outside the explicit inspection list')
    return (SOURCE / name).read_text(encoding='utf-8-sig')

mode = sys.argv[1]
if mode == 'outline':
    for name in ['app.py', 'widgets.py', 'param_space.py', 'rawsize.py', 'results.py', 'bootstrap.py']:
        source = read(name)
        tree = ast.parse(source)
        print(name, 'lines=', len(source.splitlines()), 'sha256=', hashlib.sha256((SOURCE / name).read_bytes()).hexdigest())
        for node in ast.walk(tree):
            if isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)):
                print(' ', node.lineno, getattr(node, 'end_lineno', None), node.name)
    print('DOC_NAMES', json.dumps(sorted(p.name for p in (SOURCE / 'doc').iterdir()), ensure_ascii=False))
elif mode == 'read':
    name = sys.argv[2]
    lines = read(name).splitlines()
    start = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    end = int(sys.argv[4]) if len(sys.argv) > 4 else len(lines)
    for number in range(start, min(end, len(lines)) + 1):
        print(str(number) + ': ' + lines[number - 1])
elif mode == 'matches':
    name, pattern = sys.argv[2:4]
    for number, line in enumerate(read(name).splitlines(), 1):
        if re.search(pattern, line, re.I):
            print(str(number) + ': ' + line)
elif mode == 'repo':
    url = subprocess.check_output(['C:/Program Files/Git/cmd/git.exe', 'remote', 'get-url', 'origin'],
                                  cwd='D:/workspace-github/xpe-gui', text=True).strip()
    if '://' in url:
        parsed = urlsplit(url)
        print('repository=' + parsed.path.strip('/').removesuffix('.git'))
    else:
        print('repository=' + url.split(':', 1)[-1].removesuffix('.git'))
