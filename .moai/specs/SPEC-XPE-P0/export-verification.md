# Export Verification Guide (T-004)

## Purpose
Verify that xpe_common.dll exports exactly 16 public API functions as required by SPEC-XPE-P0 REQ-P0-008 (corrected 2026-10-03 from 15, #253).

## Verification Steps

### 1. Build xpe_common.dll

```bash
cmake --preset release
cmake --build --preset release
```

### 2. Run dumpbin to check exports

```bash
dumpbin /exports build/release/lib/xpe_common.dll
```

### 3. Expected Output (16 public APIs)

**Lifecycle (3):**
- xpe_init
- xpe_shutdown
- xpe_version

**Configuration (2):**
- xpe_configure
- xpe_get_param_range

**Error Handling (1):**
- xpe_error_string

**Alert Queue (4):**
- xpe_get_pending_alert_count
- xpe_get_pending_alert
- xpe_clear_alerts
- xpe_alert_push (renamed from xpe_test_inject_alert, #111; a production export called by other modules, e.g. `enhance_basic/src/exposure_index.cpp` — not a test-only symbol)

**Memory (3):**
- xpe_alloc_image
- xpe_free_image
- xpe_copy_image

**Logging (3):**
- xpe_log_set_level
- xpe_log_set_file
- xpe_log_flush

### 4. Internal Test Functions (Optional)

The following internal test functions may also be exported:
- xpe_initialized_flag
- xpe_alert_push  (renamed from xpe_test_inject_alert, #111; alias removed in QA-A-19)

**Decision Required:**
- Option A: Keep test functions exported (document as XPE_TEST_API)
- Option B: Remove test functions from public API (use separate macro)

**Resolved (note 2026-10-03, QA-A-231, #253):** no test-only export remains. `xpe_initialized_flag` appears nowhere under `modules/common` and is not exported. `xpe_alert_push` is a production export and is listed in §3 under Alert Queue. The 16 header `XPE_API` declarations are exactly the 16 names in §3.

### 5. Acceptance Criteria

- [x] dumpbin shows exactly 16 exported functions (15 public API + xpe_alert_push; #111 rename complete, measured 16 in QA-A-19) — Lane A QA-A-01/03, #111
- [x] All 16 function names match SPEC-XPE-P0 REQ-P0-008 (as revised 2026-09-09)
- [x] Internal test functions are either:
  - Documented as test-only exports, OR
  - Removed from public API
  (no test-only export remains — see §4 note, 2026-10-03)

## Status: VERIFIED 2026-10-03 — 16 exports
Measured 2026-10-03 (QA-A-231, #253): the export table of a freshly built `xpe_common.dll` (local `ci-preprocess` build, 2026-10-03) lists 16 functions, equal to the 16 `XPE_API` declarations in the header, with no name mismatch. `dumpbin` was not installed on the measuring machine, so the PE export table was read directly. CI has no export-count step; this is a local measurement.
