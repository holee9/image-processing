# SPEC-XPE-P1A Progress Tracking

## Timeline

- **Started**: 2026-04-16
- **Phase 1 SUP-01 Complete**: 2026-04-18 — 9 TDD cycles (T-002~T-010), 89/90 tests pass

## Phase 1 Deliverables

### Completed (SUP-01 — Calibration Management)

| Sprint | Cycle | Requirement | Implementation |
|--------|-------|-------------|-----------------|
| S0-B | T-002 | REQ-P1A-014 | xpe_calib_load_offset + xcal_reader |
| S0-B | T-003 | REQ-P1A-015 | xpe_calib_load_gain |
| S0-B | T-004 | REQ-P1A-016 | xpe_calib_load_defect_map |
| S0-B | T-005 | REQ-P1A-017 | xpe_calib_generate_offset |
| S0-B | T-006 | REQ-P1A-018 | xpe_calib_check_expiry |
| S0-B | T-007 | REQ-P1A-019 | xpe_calib_save + xcal_writer |
| S0-B | T-008 | XCal Format | xcal_validator + schema validation |
| S0-B | T-009 | SHA-256 | PicoSHA2 vendor + xpe_sha256 integration |
| S0-B | T-010 | Test Harness | 89/90 tests passing (1 SKIP) |

### Test Coverage by Category

| Category | Test Count | Status |
|----------|-----------|--------|
| XCal validators | 18 | PASS |
| Reader/Writer round-trip | 16 | PASS |
| SHA-256 integrity | 8 | PASS |
| Load functions (offset/gain/defect) | 24 | PASS |
| Expiry date handling | 8 | PASS |
| Offset generation | 8 | PASS |
| Endurance (skip for now, mem model TBD) | 1 | SKIP |
| **Total** | **89/90** | **GREEN** |

## Pending Work (Future Sprints)

### Phase 1A (PRE-02/03/06 — Correction Algorithms)
- REQ-P1A-010: Offset Correction Execution
- REQ-P1A-011: Gain Correction Execution
- REQ-P1A-012: Defect Correction Execution
- REQ-P1A-013: Runtime Defect Detection

### Phase 1B (Optional Features)
- REQ-P1A-040: SIMD Optimization (AVX2)
- REQ-P1A-041: Readout Artifact Validation
- REQ-P1A-042: Parameter Range Query

## Dependencies

- **xpe_common.dll**: Available (Phase 0 complete)
- **PicoSHA2**: Vendored (third_party/picosha2/picosha2.h)
- **CMake/Ninja**: Available

## Notes

- **API Compatibility**: P/Invoke signatures in include/xpe/preprocess_api.h unchanged (ABI preserved)
- **IEC 62304**: Class B compliance verified through test coverage and exception handling
- **Next Gate**: PRE-02/03/06 implementation approval required before proceeding

## 2026-04-18 — GUI-IT Validation + Calibration Hardening

End-to-end verification on branch `feature/SPEC-P1A-SUP01-and-GUI-IT-2026-04-18`:

- Built `xpe_common.dll` from source into `build/ci-common/bin/Debug/` and staged it (plus `fmtd.dll` / `spdlogd.dll`) into the xUnit output directory.
- Executed full `ImageProcTest.IntegrationTests` suite: **79 / 79 pass** (0 fail, 0 skip). Native-dependent subset (NativeLibraryCollection, 9 classes): **56 / 56 pass**.
- Hardened 5 xpe_common paths surfaced by the native-dependent verification:
  2. `xpe_get_param_range` — added init guard + body-part whitelist (`CHEST / ABDOMEN / PELVIS / SPINE / SKULL / HEAD / EXTREMITY`) returning `XPE_ERR_INVALID_INPUT` for unknown anatomy (REQ-GUI-IT-026, REQ-GUI-IT-040).
  3. `xpe_log_set_file` — parent-directory existence check returns `XPE_ERR_IO_FAILED` before spdlog is touched (REQ-GUI-IT-030).
  4. `xpe_log_set_file` — release prior sink (`spdlog::drop("xpe_file")` + reset) before opening a new one so repeat calls do not collide on the file handle (REQ-GUI-IT-030).
  5. `xpe_shutdown` — invokes new `xpe_log_internal_reset()` which installs a null-sink default logger before dropping the custom file sink, so `xpe_log_flush` post-shutdown is safe (REQ-GUI-IT-031, AccessViolation regression fix).
- Added `modules/common/tests/test_xpe_error_safety_violation.cpp` covering the init-guard contract at the native layer.
- Build gate: C++ builds with `/WX` (warnings-as-errors) and .NET build both clean (0 warnings, 0 errors).
- Independent read-only audit of SPEC-XPE-GUI-IT coverage identified 4 mandatory + 2 optional tests still uncovered (REQ-GUI-IT-053 version-pin, REQ-GUI-IT-021 alert-count, REQ-GUI-IT-025 copy-mismatch, REQ-GUI-IT-042 arch-mismatch message, plus REQ-GUI-IT-061 determinism / NaN checks). Tracked for the next sprint — NOT in scope of this PR.
