# Export Verification Guide (T-004)

## Purpose
Verify that xpe_common.dll exports exactly 18 public API functions as required by SPEC-XPE-P0.

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

### 3. Expected Output (18 public APIs)

**Lifecycle (3):**
- xpe_init
- xpe_shutdown
- xpe_version

**Configuration (2):**
- xpe_configure
- xpe_get_param_range

**Error Handling (1):**
- xpe_error_string

**Alert Queue (3):**
- xpe_get_pending_alert_count
- xpe_get_pending_alert
- xpe_clear_alerts

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
- xpe_test_inject_alert

**Decision Required:**
- Option A: Keep test functions exported (document as XPE_TEST_API)
- Option B: Remove test functions from public API (use separate macro)

### 5. Acceptance Criteria

- [ ] dumpbin shows exactly 18 public API functions
- [ ] All 18 function names match SPEC-XPE-P0 REQ-P0-008
- [ ] Internal test functions are either:
  - Documented as test-only exports, OR
  - Removed from public API

## Status: PENDING VERIFICATION
Note: Requires Windows build environment with dumpbin tool.
