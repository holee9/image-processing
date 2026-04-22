# BP-06~09 Post-B Benchmark Freeze Baseline

Issue: `#55`
Worktree: `xpe-post`
Branch: `dev/postprocess`
**Status**: Frozen — 4/4 PASS (2026-04-22)

| BP | Test | Result | Date |
|----|------|--------|------|
| BP-06 | BenchmarkFreeze.BP06_GsvgVersionProbeBaseline | ✅ PASS (0 ms) | 2026-04-22 |
| BP-07 | CollimationDetectTest.BenchmarkFreeze_BP07_CollimationDetectionBaseline | ✅ PASS (10 ms) | 2026-04-22 |
| BP-08 | ExposureIndex.BenchmarkFreeze_BP08_EICalcTimeBaseline | ✅ PASS (0 ms) | 2026-04-22 |
| BP-09 | ExposureIndex.BenchmarkFreeze_BP09_DICalcTimeBaseline | ✅ PASS (0 ms) | 2026-04-22 |

This file freezes the first Post-B benchmark gates used by
`.github/workflows/benchmark-regression.yml`. The thresholds are intentionally
host-tolerant GTest gates, not clinical performance claims.

| BP | Module | GTest gate | Frozen threshold |
| --- | --- | --- | --- |
| BP-06 | `gsvg.dll` | `BenchmarkFreeze.BP06_GsvgVersionProbeBaseline` | 1024 version probes under 5000 us |
| BP-07 | `xpe_enhance_advanced.dll` | `CollimationDetectTest.BenchmarkFreeze_BP07_CollimationDetectionBaseline` | 512x512 synthetic collimation under 500 ms |
| BP-08 | `xpe_enhance_basic.dll` | `ExposureIndex.BenchmarkFreeze_BP08_EICalcTimeBaseline` | 512x512 EI calculation under 25 ms |
| BP-09 | `xpe_enhance_basic.dll` | `ExposureIndex.BenchmarkFreeze_BP09_DICalcTimeBaseline` | 512x512 DI calculation under 25 ms |

Degraded-mode coverage is provided by `DegradedMode.BP06_*` through
`DegradedMode.BP10_*` in `test_xpe_common.exe`. Those tests run from a staged
directory after the CI driver removes the target optional DLL, so module
readiness can degrade to `R0` without process loader failure.
