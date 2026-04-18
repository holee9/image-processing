## Task Decomposition
SPEC: SPEC-XPE-P1B-ENH

| Task ID | Description | Requirement | Dependencies | Planned Files | Status | Owner |
|---------|-------------|-------------|--------------|---------------|--------|-------|
| T-001 | Expand enhance_basic_api.h: 7 API decls + 3 param structs + 1 enum | REQ-ENH-CC-001 | - | modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_api.h, modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_internal.h | pending | algo-impl |
| T-002 | Update CMakeLists.txt: SHARED target + GTest + 6 test sources | REQ-ENH-CC-001 | T-001 | modules/enhance_basic/CMakeLists.txt | pending | algo-impl |
| T-003 | Implement exposure_index.cpp (SWU-2.10): EI/DI, EIT lookup, DI alert | REQ-ENH-023..030 | T-001 | modules/enhance_basic/src/exposure_index.cpp | pending | algo-impl |
| T-004 | Implement log_transform.cpp (SWU-2.1): forward/inverse log, clamping | REQ-ENH-001..006 | T-001 | modules/enhance_basic/src/log_transform.cpp | pending | algo-impl |
| T-005 | Implement noise_reduce.cpp (SWU-2.2): bilateral + NLM + sigma MAD | REQ-ENH-007..012 | T-001 | modules/enhance_basic/src/noise_reduce.cpp | pending | algo-impl |
| T-006 | Implement contrast_enhance.cpp (SWU-2.3): CLAHE tile-based | REQ-ENH-013..017 | T-001 | modules/enhance_basic/src/contrast_enhance.cpp | pending | algo-impl |
| T-007 | Implement edge_enhance.cpp (SWU-2.4): USM + overshoot clamp | REQ-ENH-018..022 | T-001 | modules/enhance_basic/src/edge_enhance.cpp | pending | algo-impl |
| T-008 | Write test_log_transform.cpp: round-trip fidelity, edge cases, perf | REQ-ENH-001..006, AC-01 | T-001 | modules/enhance_basic/tests/test_log_transform.cpp | pending | test-impl |
| T-009 | Write test_noise_reduce.cpp: bilateral, NLM, sigma est, param validation | REQ-ENH-007..012, AC-02,03,05 | T-001 | modules/enhance_basic/tests/test_noise_reduce.cpp | pending | test-impl |
| T-010 | Write test_contrast_enhance.cpp: CLAHE correctness, tile blending, params | REQ-ENH-013..017, AC-04 | T-001 | modules/enhance_basic/tests/test_contrast_enhance.cpp | pending | test-impl |
| T-011 | Write test_edge_enhance.cpp: USM correctness, overshoot bounds, params | REQ-ENH-018..022, AC-05 | T-001 | modules/enhance_basic/tests/test_edge_enhance.cpp | pending | test-impl |
| T-012 | Write test_exposure_index.cpp: EI/DI accuracy, bodyPart lookup, DI alert | REQ-ENH-023..030, AC-06,07 | T-001 | modules/enhance_basic/tests/test_exposure_index.cpp | pending | test-impl |
| T-013 | Write test_enhance_integration.cpp: full pipeline, thread safety, P/Invoke | REQ-ENH-CC-001..005, AC-08..10 | T-003..T-007 | modules/enhance_basic/tests/test_enhance_integration.cpp | pending | test-impl |
