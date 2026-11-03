Parity fixtures for the C# port of xpe-2g-pre-comparison/2 (QA-A-273).
contract: tools/calibration/comparison_manifest.py at main-local commit 89b662a5, sha256 fd8dcc412eb641626e4933d9c51c5fd6dea1006b6ed48016f3f6b86f262d61c9
expected.json = the Python contract validate_pair() / compare_raw_pair() for the same folder. Synthetic data; not a 2G run, not a quality statement.
regenerate: python -X utf8 make_fixtures.py <comparison_manifest.py of 89b662a5> <this folder>
