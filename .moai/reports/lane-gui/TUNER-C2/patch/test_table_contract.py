"""Engine/Qt-free tests of AST-extracted production table parsing, not app import.

Breaks caught: silent invalid-int fallback, inverted search intervals,
invalid LUT geometry, partial definition mutation before rejecting input.
Run: python -B test_table_contract.py [original|candidate]
"""
import ast
from dataclasses import dataclass, field
from pathlib import Path
from types import SimpleNamespace
from typing import List
import sys
import unittest

ROOT = Path(__file__).resolve().parent
S = Path('D:/workspace/ProjectGit/PE_analysis/param_tuner')
MODE = sys.argv.pop(1) if len(sys.argv) > 1 else 'candidate'

def extracted(name, path, class_name=None):
    tree = ast.parse(path.read_text(encoding='utf-8-sig'))
    body = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == class_name).body if class_name else tree.body
    node = next(n for n in body if isinstance(n, (ast.FunctionDef, ast.ClassDef)) and n.name == name)
    ns = {'dataclass': dataclass, 'field': field, 'List': List, 'N_POST': 40, 'N_LUT': 12, 'ParamDef': object}
    exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), 'exec'), ns)
    return ns[name], ns

Params, _ = extracted('Params', S / 'param_space.py')
path = S / 'app.py' if MODE == 'original' else ROOT.parent / 'workcopy/param_tuner/app.py'
parse, scope = extracted('_read_table', path, 'MainWindow')

def defaults():
    return Params([0] * 40, [0, 0, 1, 33, 33, 1, 66, 66, 1, 100, 100, 1])

scope.update(default_params=defaults, Qt=SimpleNamespace(Checked=2))
run, run_scope = extracted('on_run', path, 'MainWindow')

class Cell:
    def __init__(self, text): self.value = str(text)
    def text(self): return self.value
    def checkState(self): return 2

def fixture(rows=None):
    defs = [SimpleNamespace(kind='post', index=0, key='post.0', name='Contrast', default=10, lo=0, hi=30, enabled=False),
            SimpleNamespace(kind='lut', index=3, key='lut.3', name='CP2 X', default=33, lo=0, hi=100, enabled=False)]
    cells = [[Cell(v) for v in row] for row in (rows or [['', '', '', '12', '', '0', '30'], ['', '', '', '33', '', '0', '100']])]
    return SimpleNamespace(defs=defs, table=SimpleNamespace(item=lambda r, c: cells[r][c])), cells

class ContractTests(unittest.TestCase):
    def test_bad_table_is_reported_before_loading_or_worker_start(self):
        warnings = []
        run_scope['QtWidgets'] = SimpleNamespace(QMessageBox=SimpleNamespace(warning=lambda *args: warnings.append(args)))
        def invalid(): raise ValueError('invalid table')
        def forbidden(**kwargs): self.fail('input refresh must not precede validation')
        obj = SimpleNamespace(worker=None, _make_cfg=lambda: object(), _read_table=invalid, refresh_inputs=forbidden)
        run(obj)
        self.assertEqual(len(warnings), 1)

    def test_valid_integer_edit_reaches_init_and_enabled(self):
        obj, _ = fixture()
        p, defs = parse(obj)
        self.assertEqual((p.post[0], p.lut[3], defs[0].enabled), (12, 33, True))
        self.assertEqual(p.lut[2], 1)

    def test_invalid_integer_rejected_in_each_editable_column(self):
        for col in (3, 5, 6):
            for value in ('oops', '1.5', ''):
                with self.subTest(col=col, value=value):
                    obj, cells = fixture(); cells[1][col].value = value
                    with self.assertRaises(ValueError): parse(obj)

    def test_rejection_does_not_partially_change_definitions(self):
        obj, cells = fixture(); cells[1][3].value = 'oops'
        try: parse(obj)
        except ValueError: pass
        self.assertEqual((obj.defs[0].default, obj.defs[0].enabled), (10, False))

    def test_reversed_interval_rejected(self):
        obj, cells = fixture(); cells[0][5].value = '30'; cells[0][6].value = '0'
        with self.assertRaises(ValueError): parse(obj)

    def test_nonmonotone_lut_rejected(self):
        obj, cells = fixture(); cells[1][3].value = '66'
        with self.assertRaises(ValueError): parse(obj)

    def test_explicit_default_expands_search_interval_as_before(self):
        obj, cells = fixture(); cells[0][3].value = '-5'
        p, defs = parse(obj)
        self.assertEqual((p.post[0], defs[0].lo, defs[0].hi), (-5, -5, 30))

if __name__ == '__main__': unittest.main(verbosity=2)
