# DLL exports vs requirement text (spec 1.3.3 from origin/main)

- DLL: build/ci-preprocess/bin/xpe_preprocess.dll, named exports 53 (C names 48, other 5: ?read_xcal_file@@YAHPEBDAEAUXCalFileHeader@@AEAV?$vector@EV?$allocator@E@std@@@std@@2_NHPEAUXpeConfigDoc@@@Z, ?validate_xcal_header@@YAHAEBUXCalFileHeader@@H@Z, ?write_xcal_file@@YAHPEBDUXCalFileHeader@@PEBE_K23@Z, ?write_xcal_file_ex@@YAHPEBDUXCalFileHeader@@PEBE_K23_N@Z, ?xcal_bytes_per_pixel@@YA_KI@Z)
- SPEC requirement blocks parsed: 48
- control +: xpe_defect_correct -> ['REQ-P1A-012', 'REQ-P1A-020a'] ; control -: fabricated -> NONE (correct)

| # | export | REQ-P1A by name | SRS-CALIB (same line as name) | RTM (same line) |
|---|---|---|---|---|
| 1 | `xpe_binning_correct` | REQ-P1A-090 | SRS-CALIB-FUNC-012 | - |
| 2 | `xpe_bpm_generate` | **none** | - | SRS-CALIB-FUNC-022 |
| 3 | `xpe_calib_cache_clear` | REQ-P1A-102, REQ-P1A-104 | SRS-CALIB-FUNC-038 | - |
| 4 | `xpe_calib_cache_set_max_size` | REQ-P1A-102, REQ-P1A-104 | SRS-CALIB-FUNC-038 | - |
| 5 | `xpe_calib_check_expiry` | REQ-P1A-018 | - | - |
| 6 | `xpe_calib_generate_gain` | REQ-P1A-011 | SRS-CALIB-FUNC-026, SRS-CALIB-FUNC-031 | - |
| 7 | `xpe_calib_generate_gain_polynomial` | REQ-P1A-011 | SRS-CALIB-FUNC-027, SRS-CALIB-FUNC-031 | - |
| 8 | `xpe_calib_generate_nonlin_lut` | **none** | - | SRS-CALIB-FUNC-006 |
| 9 | `xpe_calib_generate_offset` | REQ-P1A-017 | SRS-CALIB-FUNC-028 | SRS-CALIB-FUNC-034 |
| 10 | `xpe_calib_get_max_points` | **none** | SRS-CALIB-FUNC-038 | - |
| 11 | `xpe_calib_get_mode` | **none** | SRS-CALIB-FUNC-031 | - |
| 12 | `xpe_calib_get_poly_degree` | **none** | SRS-CALIB-FUNC-038 | - |
| 13 | `xpe_calib_get_quality_meta` | REQ-P1A-103 | SRS-CALIB-FUNC-038 | - |
| 14 | `xpe_calib_load_defect_cached` | REQ-P1A-016, REQ-P1A-104 | - | - |
| 15 | `xpe_calib_load_defect_map` | REQ-P1A-016, REQ-P1A-104 | - | - |
| 16 | `xpe_calib_load_gain` | REQ-P1A-015, REQ-P1A-103 | SRS-CALIB-FUNC-026 | - |
| 17 | `xpe_calib_load_gain_cached` | REQ-P1A-015, REQ-P1A-103 | - | - |
| 18 | `xpe_calib_load_nonlin_lut` | **none** | - | SRS-CALIB-FUNC-006 |
| 19 | `xpe_calib_load_offset` | REQ-P1A-003, REQ-P1A-014, REQ-P1A-102 | - | - |
| 20 | `xpe_calib_load_offset_cached` | REQ-P1A-014, REQ-P1A-102 | - | - |
| 21 | `xpe_calib_save` | REQ-P1A-019 | - | - |
| 22 | `xpe_calib_set_mode` | **none** | SRS-CALIB-FUNC-031 | - |
| 23 | `xpe_calib_state_load` | REQ-P1A-003, REQ-P1A-016a, REQ-P1A-105 | - | - |
| 24 | `xpe_calib_state_release` | **none** | SRS-CALIB-FUNC-038 | - |
| 25 | `xpe_calib_unload_nonlin_lut` | **none** | - | SRS-CALIB-FUNC-006 |
| 26 | `xpe_defect_correct` | REQ-P1A-012, REQ-P1A-020a | - | SRS-CALIB-FUNC-019 |
| 27 | `xpe_defect_detect_runtime` | REQ-P1A-013 | SRS-CALIB-FUNC-010 | - |
| 28 | `xpe_gain_correct` | REQ-P1A-011, REQ-P1A-020a, REQ-P1A-103 | - | SRS-CALIB-FUNC-017 |
| 29 | `xpe_ghost_correct` | REQ-P1A-032, REQ-P1A-086, REQ-P1A-087, REQ-P1A-088, REQ-P1A-099 | - | - |
| 30 | `xpe_ghost_create` | REQ-P1A-085 | - | - |
| 31 | `xpe_ghost_destroy` | REQ-P1A-085, REQ-P1A-086 | - | - |
| 32 | `xpe_ghost_reset` | REQ-P1A-086, REQ-P1A-088 | SRS-CALIB-FUNC-011, SRS-CALIB-FUNC-012, SRS-CALIB-FUNC-014 | - |
| 33 | `xpe_nonlinearity_correct` | **none** | - | SRS-CALIB-FUNC-006 |
| 34 | `xpe_offset_correct` | REQ-P1A-010, REQ-P1A-014, REQ-P1A-020a | - | SRS-CALIB-FUNC-016 |
| 35 | `xpe_preprocess_get_param_range` | REQ-P1A-042 | - | - |
| 36 | `xpe_preprocess_init` | REQ-P1A-001, REQ-P1A-020 | SRS-CALIB-FUNC-038 | - |
| 37 | `xpe_preprocess_is_initialized` | **none** | SRS-CALIB-FUNC-038 | - |
| 38 | `xpe_preprocess_pipeline` | REQ-P1A-095, REQ-P1A-105 | - | - |
| 39 | `xpe_preprocess_pipeline_batch` | **none** | SRS-CALIB-FUNC-038 | - |
| 40 | `xpe_preprocess_pipeline_ex` | REQ-P1A-016a, REQ-P1A-105, REQ-P1A-106 | - | - |
| 41 | `xpe_preprocess_shutdown` | REQ-P1A-020, REQ-P1A-102 | - | - |
| 42 | `xpe_preprocess_version` | REQ-P1A-106 | - | - |
| 43 | `xpe_temp_compensate` | REQ-P1A-080, REQ-P1A-082 | - | - |
| 44 | `xpe_validate_readout_artifact` | REQ-P1A-041 | - | - |
| 45 | `xpe_verify_defect` | **none** | - | SRS-CALIB-FUNC-019 |
| 46 | `xpe_verify_gain` | **none** | SRS-CALIB-FUNC-036 | SRS-CALIB-FUNC-017 |
| 47 | `xpe_verify_offset` | **none** | SRS-CALIB-FUNC-036 | SRS-CALIB-FUNC-016 |
| 48 | `xpe_verify_pipeline` | **none** | - | SRS-CALIB-FUNC-015, SRS-CALIB-FUNC-021 |
