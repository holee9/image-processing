"""Engine/Qt-free tests of AST-extracted production table parsing, not app import.

Breaks caught: silent invalid-int fallback, inverted search intervals,
invalid LUT geometry, partial definition mutation before rejecting input.
Run: python -I -B -X utf8 test_table_contract.py [original|candidate]
"""
import ast
from dataclasses import dataclass, field
from pathlib import Path
from types import SimpleNamespace
from typing import List
import sys
import time
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
    defs = [SimpleNamespace(kind='post', index=0, key='post.0', name='Contrast', group='tone', default=10, lo=0, hi=30, enabled=False),
            SimpleNamespace(kind='lut', index=3, key='lut.3', name='CP2 X', group='lut', default=33, lo=0, hi=100, enabled=False)]
    cells = [[Cell(v) for v in row] for row in (rows or [['', '', '', '12', '', '0', '30'], ['', '', '', '33', '', '0', '100']])]
    return SimpleNamespace(defs=defs, table=SimpleNamespace(item=lambda r, c: cells[r][c])), cells

class Control:
    """Inert boundary adapter for UI display calls; no Qt or thread is created."""
    def __getattr__(self, name):
        return lambda *args, **kwargs: None

def run_fixture(present=(True, True)):
    obj, cells = fixture()
    observed = SimpleNamespace(warnings=[], workers=[], reads=0, refreshes=0, started=0)
    class WorkerBoundary:
        def __init__(self, cfg, init, defs, single):
            self.cfg, self.init, self.defs, self.single = cfg, init, defs, single
            self.log = self.stage = self.evaled = self.done = self.failed = Control()
            observed.workers.append(self)
        def start(self): observed.started += 1
    def read_table():
        observed.reads += 1
        return parse(obj)
    def refresh(**kwargs):
        observed.refreshes += 1
        obj.src = object() if present[0] else None
        obj.tgt = object() if present[1] else None
    run_scope.update(QtWidgets=SimpleNamespace(QMessageBox=SimpleNamespace(warning=lambda *args: observed.warnings.append(args))),
                     Worker=WorkerBoundary, time=time, GROUP_ORDER=['tone', 'lut'])
    obj.worker = obj.src = obj.tgt = None
    obj._make_cfg = lambda: SimpleNamespace(opt=object())
    obj._read_table, obj.refresh_inputs = read_table, refresh
    for name in ('_show_params', '_save_settings', '_set_status', '_set_running', '_on_stage', '_on_eval', '_on_done', '_on_failed'):
        setattr(obj, name, lambda *args, **kwargs: None)
    for name in ('plot', 'card_loss', 'card_psnr', 'card_ssim', 'card_mae', 'card_eval', 'stepper', 'progress', 'logbox', 'timer'):
        setattr(obj, name, Control())
    obj.panels = [SimpleNamespace(arr=None, qimg=None, subtitle='') for _ in range(3)]
    return obj, cells, observed

def definitions(obj):
    return [(d.default, d.lo, d.hi, d.enabled) for d in obj.defs]

class ContractTests(unittest.TestCase):
    def test_missing_input_preserves_defs_and_does_not_start_worker(self):
        for present in ((False, False), (False, True), (True, False)):
            with self.subTest(present=present):
                obj, _, observed = run_fixture(present)
                before = definitions(obj)
                run(obj)
                self.assertEqual(definitions(obj), before)
                self.assertEqual((observed.reads, observed.refreshes, observed.started), (0, 1, 0))
                self.assertEqual(observed.workers, [])

    def test_valid_inputs_bad_table_warns_preserves_defs_and_does_not_start_worker(self):
        obj, cells, observed = run_fixture()
        cells[1][3].value = 'oops'
        before = definitions(obj)
        run(obj)
        self.assertEqual(definitions(obj), before)
        self.assertEqual((observed.reads, observed.refreshes, observed.started), (1, 1, 0))
        self.assertEqual(len(observed.warnings), 1)
        self.assertEqual(observed.workers, [])

    def test_valid_inputs_valid_table_commits_and_passes_values_to_worker(self):
        obj, _, observed = run_fixture()
        run(obj)
        self.assertEqual(definitions(obj), [(12, 0, 30, True), (33, 0, 100, True)])
        self.assertEqual((observed.reads, observed.refreshes, observed.started), (1, 1, 1))
        self.assertEqual(observed.warnings, [])
        self.assertEqual((observed.workers[0].init.post[0], observed.workers[0].init.lut[3]), (12, 33))
        self.assertIs(observed.workers[0].defs, obj.defs)

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
