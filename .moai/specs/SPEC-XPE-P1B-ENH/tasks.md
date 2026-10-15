## Task Decomposition
SPEC: SPEC-XPE-P1B-ENH

| Task ID | Description | Requirement | Dependencies | Planned Files | Status | Owner |
|---------|-------------|-------------|--------------|---------------|--------|-------|
| T-001 | Expand enhance_basic_api.h: 7 API decls + 3 param structs + 1 enum | REQ-ENH-CC-001 | - | modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_api.h, modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_internal.h | completed-partial (QA-B-199: 일부 요구 미단언) | algo-impl |
| T-002 | Update CMakeLists.txt: SHARED target + GTest + 6 test sources | REQ-ENH-CC-001 | T-001 | modules/enhance_basic/CMakeLists.txt | completed-partial (QA-B-199: 일부 요구 미단언) | algo-impl |
| T-003 | Implement exposure_index.cpp (SWU-2.10): EI/DI, EIT lookup, DI alert | REQ-ENH-023..030, REQ-ENH-023a | T-001 | modules/enhance_basic/src/exposure_index.cpp | completed-partial (QA-B-199: 일부 요구 미단언) | algo-impl |
| T-004 | Implement log_transform.cpp (SWU-2.1): forward/inverse log, clamping | REQ-ENH-001..006 | T-001 | modules/enhance_basic/src/log_transform.cpp | completed-partial (QA-B-199: 일부 요구 미단언) | algo-impl |
| T-005 | Implement noise_reduce.cpp (SWU-2.2): bilateral + NLM + sigma MAD | REQ-ENH-007..012 | T-001 | modules/enhance_basic/src/noise_reduce.cpp | completed-partial (QA-B-199: 일부 요구 미단언) | algo-impl |
| T-006 | Implement contrast_enhance.cpp (SWU-2.3): CLAHE tile-based | REQ-ENH-013..017 | T-001 | modules/enhance_basic/src/contrast_enhance.cpp | completed-partial (QA-B-199: 일부 요구 미단언) | algo-impl |
| T-007 | Implement edge_enhance.cpp (SWU-2.4): USM + overshoot clamp | REQ-ENH-018..022 | T-001 | modules/enhance_basic/src/edge_enhance.cpp | completed-partial (QA-B-199: 일부 요구 미단언) | algo-impl |
| T-008 | Write test_log_transform.cpp: round-trip fidelity, edge cases, perf | REQ-ENH-001..006, AC-01 | T-001 | modules/enhance_basic/tests/test_log_transform.cpp | completed-partial (QA-B-199: 일부 요구 미단언) | test-impl |
| T-009 | Write test_noise_reduce.cpp: bilateral, NLM, sigma est, param validation | REQ-ENH-007..012, AC-02,03,05 | T-001 | modules/enhance_basic/tests/test_noise_reduce.cpp | completed-partial (QA-B-199: 일부 요구 미단언) | test-impl |
| T-010 | Write test_contrast_enhance.cpp: CLAHE correctness, tile blending, params | REQ-ENH-013..017, AC-04 | T-001 | modules/enhance_basic/tests/test_contrast_enhance.cpp | completed-partial (QA-B-199: 일부 요구 미단언) | test-impl |
| T-011 | Write test_edge_enhance.cpp: USM correctness, overshoot bounds, params | REQ-ENH-018..022, AC-05 | T-001 | modules/enhance_basic/tests/test_edge_enhance.cpp | completed-partial (QA-B-199: 일부 요구 미단언) | test-impl |
| T-012 | Write test_exposure_index.cpp: EI/DI accuracy, bodyPart lookup, DI alert | REQ-ENH-023..030, REQ-ENH-023a, AC-06,07 | T-001 | modules/enhance_basic/tests/test_exposure_index.cpp | completed-partial (QA-B-199: 일부 요구 미단언) | test-impl |
| T-013 | Write test_enhance_integration.cpp: full pipeline, thread safety, P/Invoke | REQ-ENH-CC-001..005, AC-08..10 | T-003..T-007 | modules/enhance_basic/tests/test_enhance_integration.cpp | completed-partial (QA-B-199: 일부 요구 미단언) | test-impl |

### 상태 정정 기록 (2026-10-03, QA-B-199, #251)

- 이전에는 13행 모두 `pending` 이었다. `enhance_basic` 은 main 에 있고 시험 160개가 돈다. 그래서 상태 열은 전부 틀렸다. 반대로 `completed` 로 올릴 수 있는 행도 없다 — 모든 작업에 독립 기대값으로 단언되지 않은 요구(`partial` 또는 `constant_only`)가 한 개 이상 걸려 있다.
- 요구별 판정과 근거 시험: `.moai/reports/lane-post/QA-B-199/enh_report.md` §1, 부록 A (post 레인 워크트리). 판정 요약:

| 작업 | 요구별 판정 |
|---|---|
| T-001, T-002 | CC-001=partial |
| T-003, T-012 | 023=partial, 023a=asserted, 024=asserted, 025=asserted, 026=partial, 027=asserted, 028=partial, 029=partial, 030=partial |
| T-004, T-008 | 001=asserted, 002=asserted, 003=partial, 004=partial, 005=partial, 006=partial |
| T-005, T-009 | 007=partial, 008=constant_only, 009=partial, 010=partial, 011=partial, 012=partial |
| T-006, T-010 | 013=partial, 014=constant_only, 015=asserted, 016=asserted, 017=constant_only |
| T-007, T-011 | 018=partial, 019=constant_only, 020=asserted, 021=partial, 022=partial |
| T-013 | CC-001=partial, CC-002=partial, CC-003=partial, CC-004=partial, CC-005=partial |

- `REQ-ENH-023a` 는 어떤 작업의 요구 열에도 없었다. T-003·T-012 에 넣었다.
- T-001 설명의 "7 API decls" 와 헤더가 선언한 `XPE_API` 10개가 다르다. SPEC `REQ-ENH-CC-001` 을 10으로 고칠지 헤더를 7로 줄일지는 결정 대기다(#251). 설명 문구는 그대로 둔다. → 결정 (2026-10-03, 사용자 #245 코멘트 묶음 ④ "문서를 실제에 맞게"): SPEC `REQ-ENH-CC-001` 을 10 으로 고쳤다(SPEC v1.3.0). T-001 설명의 "7 API decls" 는 당시 작업 기록이라 그대로 둔다.
- `enhance_basic_api.h` 의 EI 식(243행)과 함수 수(8행) 문구는 코드 쪽 문서라 post 레인 카드로 처리한다.
