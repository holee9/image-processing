"""Export existing synthetic producer properties without inventing quality gates (Refs #245)."""
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def reject_constant(value):
    raise ValueError('Nonfinite JSON constant: ' + value)


def build_report(xml_bytes):
    root = ET.fromstring(xml_bytes)
    producers = []
    cases = list(root.iter('testcase'))
    for ordinal, case in enumerate(cases):
        for prop in case.findall('./properties/property'):
            if prop.get('name') != 'xpe_synthetic_metrics_v1':
                continue
            if case.get('status') not in ('run', 'completed') or case.find('skipped') is not None:
                raise ValueError('Unexecuted producer property is not a measurement')
            payload = json.loads(prop.get('value', ''), parse_constant=reject_constant)
            if not isinstance(payload, dict) or payload.get('kind') not in ('ghost', 'nonlinearity'):
                raise ValueError('Unknown synthetic producer payload')
            # Reject numeric overflow such as 1e999 as well as NaN/Infinity tokens.
            json.dumps(payload, allow_nan=False)
            failed = case.find('failure') is not None or case.find('error') is not None
            producers.append({'ordinal': ordinal, 'test': case.get('classname', '') + '.' + case.get('name', ''),
                              'test_status': 'failed' if failed else 'passed', 'measurement': payload})
    if not producers:
        raise ValueError('No measured synthetic producer properties; never synthesize zero values')
    failures = len(list(root.iter('failure'))) + len(list(root.iter('error')))
    disabled = sum(c.get('status') not in ('run', 'completed') for c in cases)
    skipped = sum(c.find('skipped') is not None for c in cases)
    return {'schema': 'xpe-pre-e2e-report-v1', 'evidence_scope': 'existing synthetic test producers only; not a real RAW or full pipeline report',
            'source_xml_sha256': hashlib.sha256(xml_bytes).hexdigest(),
            'input_preserved': None, 'raw_sha256_before': None, 'raw_sha256_after': None,
            'synthetic_producers': producers, 'degraded_evidence': True,
            'run_succeeded': failures == 0 and disabled == 0 and skipped == 0,
            'test_execution': {'registered': len(cases), 'failure_error_tags': failures, 'disabled_or_notrun': disabled, 'skipped': skipped},
            'metrics': {'calibration_effect_score': None}, 'phase1_pass': False, 'release_pass': False,
            'gates': [{'id': 'PRE-E2E-COMPLETE', 'result': 'unmeasured'}],
            'quality_limit': 'Producer/control test success does not establish ghost >=90%, real-data quality, or CES; no new thresholds.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xml', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path, help='Fresh directory; never overwrite existing evidence')
    parser.add_argument('--binary', type=Path, help='Optional current producer binary snapshot; not proof of XML execution linkage')
    args = parser.parse_args()
    report = build_report(args.xml.read_bytes())
    report['source_xml_path'] = str(args.xml.resolve())
    if args.binary:
        report['producer_binary_snapshot'] = {'path': str(args.binary.resolve()),
            'sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(),
            'scope': 'current file at export; execution linkage is caller-provided, not attested by XML'}
    args.out.mkdir(exist_ok=False)
    text = json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + '\n'
    (args.out / 'report.md.pending').write_text('# Synthetic preprocessing producer evidence\n\n' + report['evidence_scope'] +
                                      '\n\n' + report['quality_limit'] + '\n\n```json\n' + text + '```\n', encoding='utf-8')
    # Publish complete JSON last; a failed partial directory is preserved without a complete report.
    (args.out / 'report.json.pending').write_text(text, encoding='utf-8')
    (args.out / 'report.md.pending').rename(args.out / 'report.md')
    (args.out / 'report.json.pending').rename(args.out / 'report.json')
    return 0 if report['run_succeeded'] else 3


if __name__ == '__main__':
    raise SystemExit(main())
