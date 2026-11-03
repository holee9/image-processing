"""Meaningful contract checks for the synthetic evidence adapter (Refs #245)."""
import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path
import unittest
from xml.sax.saxutils import quoteattr

spec = importlib.util.spec_from_file_location('evidence', Path(__file__).resolve().parents[1] / 'tools' / 'xpe_pre_test_evidence.py')
evidence = importlib.util.module_from_spec(spec)
spec.loader.exec_module(evidence)


def fixture(payload, status='run', extra=''):
    value = quoteattr(json.dumps(payload))
    return ('<testsuites><testsuite><testcase name="producer" classname="Known" status="' + status + '">' + extra +
            '<properties><property name="xpe_synthetic_metrics_v1" value=' + value + '/></properties></testcase></testsuite></testsuites>').encode()


class EvidenceTest(unittest.TestCase):
    def test_preserves_values_without_quality_promotion(self):
        metrics = {'kind': 'ghost', 'scope': 'existing synthetic profile', 'rows': [{'removal_pct': -15.0}]}
        report = evidence.build_report(fixture(metrics))
        self.assertEqual(metrics, report['synthetic_producers'][0]['measurement'])
        self.assertIsNone(report['metrics']['calibration_effect_score'])
        self.assertFalse(report['release_pass'])
        self.assertTrue(report['degraded_evidence'])

    def test_failure_never_becomes_success(self):
        report = evidence.build_report(fixture({'kind': 'nonlinearity'}, extra='<failure message="bad"/>'))
        self.assertFalse(report['run_succeeded'])
        self.assertEqual('failed', report['synthetic_producers'][0]['test_status'])

    def test_disabled_or_skipped_measurements_are_not_measured(self):
        for status, extra in [('notrun', ''), ('run', '<skipped/>')]:
            with self.assertRaises(ValueError):
                evidence.build_report(fixture({'kind': 'ghost'}, status, extra))

    def test_missing_properties_and_nonfinite_are_rejected(self):
        for raw in [b'<testsuites/>', fixture({'kind': 'ghost', 'metric': float('nan')})]:
            with self.assertRaises(ValueError):
                evidence.build_report(raw)

    def test_duplicate_case_names_are_preserved(self):
        one = fixture({'kind': 'nonlinearity'}).decode()
        case = one[one.index('<testcase'):one.index('</testcase>') + len('</testcase>')]
        report = evidence.build_report(('<testsuites><testsuite>' + case + case + '</testsuite></testsuites>').encode())
        self.assertEqual(2, len(report['synthetic_producers']))
        self.assertEqual([0, 1], [p['ordinal'] for p in report['synthetic_producers']])

    def test_cli_preserves_xml_and_refuses_existing_output(self):
        with tempfile.TemporaryDirectory(prefix='xpe_evidence_contract_') as directory:
            root = Path(directory)
            original = fixture({'kind': 'nonlinearity', 'value': 0.0})
            xml = root / 'producer.xml'
            xml.write_bytes(original)
            out = root / 'out'
            command = [sys.executable, '-B', str(spec.origin), '--xml', str(xml), '--out', str(out)]
            first = subprocess.run(command, capture_output=True)
            self.assertEqual(0, first.returncode, first.stderr)
            report = (out / 'report.json').read_bytes()
            self.assertTrue((out / 'report.md').is_file())
            self.assertFalse((out / 'report.json.pending').exists())
            self.assertNotEqual(0, subprocess.run(command, capture_output=True).returncode)
            self.assertEqual(report, (out / 'report.json').read_bytes())
            self.assertEqual(original, xml.read_bytes())


if __name__ == '__main__':
    unittest.main()
