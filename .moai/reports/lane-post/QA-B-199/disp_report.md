# QA-B-199 DISP — SPEC-XPE-P1B-DISP 작업표·요구 실태 대조

범위: `.moai/specs/SPEC-XPE-P1B-DISP/`(spec.md·plan.md·progress.md), `modules/display/`, `docs/display/RTM-DISPLAY-001_Requirements_Traceability_Matrix.md`, `.github/workflows/ci.yml`. 코드는 바꾸지 않았다.

## 0. 방법과 한계

방법은 ENH 보고서 §0 과 같다(기계 추출 → 요구별 판정 → 인용 기계 검증 → 핵심 주장 직접 확인). DISP 에서 다른 점:

- plan.md 의 작업 표(`| M#-## |`)에는 상태 열이 없다. 그래서 "표 상태" 는 `(상태 열 없음)` 이다. 진행 기록은 progress.md 에 따로 있다.
- 검증(`verify_DISP.txt`): 시험 인용 91건과 헤더 약속 6건은 인용한 줄 ±2 안에 실재한다(실패 0). 그러나 **구현 줄 번호 인용은 13건이 실제 파일 길이 밖**이었다(예: `voi_lut.cpp` 는 162줄인데 229-261 을 인용). 이 에이전트의 구현 줄 번호는 전부 쓰지 않았다. 아래의 코드 위치는 내가 직접 grep·읽기로 확인한 것이다(VOI: LINEAR `voi_lut.cpp:58`, LINEAR_EXACT `:108`, SIGMOID `:117`, preset_create `:135`). 3건은 줄 번호 없는 산문 인용이다.
- 헤더 약속 6건 중 줄 번호가 구현 파일을 가리키는 설명(`voi_lut.cpp:229-241` 등)은 같은 이유로 보조 설명으로만 읽어야 하고, 헤더 쪽 인용문과 줄 번호는 검증됐다.
- 실행하지 않았다. 결함 후보는 읽기로 찾은 후보다.

## 1. 작업 행 (plan.md 33행)

| 작업 | 표 상태 | 작업 내용 | 요구 | 실제 | 판정별 개수 |
|---|---|---|---|---|---|
| M1-01 | (no status column) | Create `modules/display/` directory structure (include/, src/, tests/) |  | **(요구 없음)** |  |
| M1-02 | (no status column) | Create `CMakeLists.txt` with xpe_display shared library target |  | **(요구 없음)** |  |
| M1-03 | (no status column) | Define `xpe_display_api.h` with all enums, structs, and 5 function dec |  | **(요구 없음)** |  |
| M1-04 | (no status column) | Create `xpe_display_internal.h` with helper function declarations |  | **(요구 없음)** |  |
| M2-01 | (no status column) | RED: Write `test_modality_lut.cpp` -- linear rescale tests (slope/inte | 001 | **구현됨** | asserted 1 |
| M2-02 | (no status column) | RED: Write LUT-table mode tests (boundary clamping, index offset) | 002 | **구현됨** | asserted 1 |
| M2-03 | (no status column) | RED: Write error handling tests (NULL, wrong format, zero slope, empty | 003, 004, 005, 006, 007 | **부분** | asserted 3, partial 2 |
| M2-04 | (no status column) | GREEN: Implement `xpe_apply_modality_lut` in `modality_lut.cpp` | 001, 002, 003, 004, 005, 006, 007 | **부분** | asserted 5, partial 2 |
| M2-05 | (no status column) | GREEN: Implement shared helpers in `display_helpers.cpp` |  | **(요구 없음)** |  |
| M2-06 | (no status column) | REFACTOR: Performance optimization, verify <= 20ms budget | 008 | **부분** | partial 1 |
| M3-01 | (no status column) | RED: Write `test_voi_lut.cpp` -- LINEAR mode windowing tests | 009 | **구현됨** | asserted 1 |
| M3-02 | (no status column) | RED: Write LINEAR_EXACT mode tests | 010 | **부분** | partial 1 |
| M3-03 | (no status column) | RED: Write SIGMOID mode tests | 011 | **부분** | partial 1 |
| M3-04 | (no status column) | RED: Write output clamping and error handling tests | 012, 013, 014, 015 | **부분** | asserted 2, partial 2 |
| M3-05 | (no status column) | RED: Write `xpe_voi_preset_create` tests (all 4 body parts + invalid) | 017, 018 | **구현됨** | asserted 2 |
| M3-06 | (no status column) | GREEN: Implement `xpe_apply_voi_lut` in `voi_lut.cpp` | 009, 010, 011, 012, 013, 014, 015, 016 | **부분** | asserted 3, partial 5 |
| M3-07 | (no status column) | GREEN: Implement `xpe_voi_preset_create` in `voi_lut.cpp` | 017, 018 | **구현됨** | asserted 2 |
| M3-08 | (no status column) | REFACTOR: Verify <= 16ms interactive latency budget | 016 | **부분** | partial 1 |
| M4-01 | (no status column) | RED: Write `test_presentation_lut.cpp` -- LUT lookup tests (index mapp | 019, 020, 021 | **부분** | asserted 2, partial 1 |
| M4-02 | (no status column) | RED: Write float32->uint16 domain transition tests (format, bits, data | 020 | **구현됨** | asserted 1 |
| M4-03 | (no status column) | RED: Write error handling tests (NULL, wrong format) | 022, 023 | **부분** | asserted 1, partial 1 |
| M4-04 | (no status column) | RED: Write GSDF calibration tests (luminance -> JND mapping) | 025, 026, 027 | **구현됨** | asserted 3 |
| M4-05 | (no status column) | RED: Write GSDF error handling tests (NULL, count < 2) | 026 | **구현됨** | asserted 1 |
| M4-06 | (no status column) | GREEN: Implement `xpe_apply_presentation_lut` in `presentation_lut.cpp | 019, 020, 021, 022, 023, 024 | **부분** | asserted 3, constant_only 1, partial 2 |
| M4-07 | (no status column) | GREEN: Implement `xpe_gsdf_calibrate` in `presentation_lut.cpp` | 025, 026, 027 | **구현됨** | asserted 3 |
| M4-08 | (no status column) | REFACTOR: Verify <= 25ms budget, optimize memory reallocation | 028 | **부분** | partial 1 |
| M5-01 | (no status column) | Write `test_display_integration.cpp` -- full Modality->VOI->Presentati | 029, 030, 031, 032, 033 | **부분** | asserted 1, no_code 1, no_test 2, partial 1 |
| M5-02 | (no status column) | Write `test_display_boundary.cpp` -- 1x1 and 4096x4096 edge cases | 034, 035 | **부분** | asserted 1, constant_only 1 |
| M5-03 | (no status column) | Verify `dumpbin /exports xpe_display.dll` lists exactly 5 functions | 029 | **구현됨** | asserted 1 |
| M5-04 | (no status column) | P/Invoke round-trip test (C# struct layout compatibility) | 029 | **구현됨** | asserted 1 |
| M5-05 | (no status column) | Memory leak check (ASan, 1000-frame cycle) | 033 | **부분** | partial 1 |
| M5-06 | (no status column) | Performance benchmark: all 3 stages within budget | 008, 016, 028 | **부분** | partial 3 |
| M5-07 | (no status column) | Static analysis: cppcheck + clang-tidy 0 warnings |  | **(요구 없음)** |  |

**읽는 법.** 요구 열이 비어 있는 행(M1-01~04 등 디렉터리·CMake·헤더 작업)은 요구와 연결되지 않은 구조 작업이다. 요구가 걸린 행 중 "구현됨" 은 요구가 전부 `asserted` 인 행만이고, `constant_only`·`no_test`·`no_code` 요구가 한 개라도 섞이면 "부분" 이다. 상태 열이 없으므로 표 상태와 실제의 어긋남은 계산하지 않았다.

## 2. 정의된 요구 중 어떤 작업에도 안 걸린 것 — 두 축

| 축 | 정의 | 개수 | 해당 |
|---|---|---|---|
| 이름 | 어느 plan.md 행의 요구 열에도 id 가 없음 | 1 (정의 줄 37개, 고유 id 36개) | REQ-DISP-010a |
| 이름(시험 텍스트) | 시험 소스 어디에도 id 문자열이 없음 | 1 | REQ-DISP-010a |
| 행위 | 시험이 독립 기대값으로 단언하지 않음(no_test·constant_only)이거나 코드 없음(no_code) | 6 | no_test: 029(439줄 정의), 030, 032 / constant_only: 024, 035 / no_code: 031 |
| 행위(넓게) | `asserted` 가 아님(partial 13 포함) | 19 | |

차이: 이름 축 1 대 행위 축 6(넓게 19). 이름 축이 못 잡는 5건은 작업 행·시험 텍스트에 id 가 나오는데도 행위가 비어 있다. 파서 대조군: **REQ-DISP-029 가 spec.md 에 두 번 정의되어 있다**(403줄, 439줄)는 것이 추출기 출력(`duplicates`)에 나온다 — 정의 줄 수가 id 수보다 1 많은 이유. 같은 id 에 서로 다른 내용이 둘이라 에이전트가 `029@403`(asserted)과 `029@439`(no_test)로 갈라 판정했다.

## 3. RTM ✓ 인데 시험이 없는 것

`rtm_out.txt`. DISP RTM 의 ✓ 행 69개: 시험 id(TDS-xxx)가 있는 행 45개, 시험 id 가 아예 없는 행 24개(PERF-TIME-101~108, IF-GUI-301~304, SAFE-DATA-101~103·105, SAFE-DICOM-103, SAFE-CLIN-1xx 등). 고유 TDS id 34종이 시험 소스에 문자열로 나오는 것은 0종이다. 이름 검색기의 합성 대조군은 `rtm_control.txt` — `Tds001_Foo` 에서 TDS-001 을 찾는다.
추가 사실: `docs/display/` 에는 시험 데이터셋 문서(TDS)가 없다(파일 6개: README, RTM, SAD, SHA, SRS, PRD). 다른 모듈은 `docs/README.md` 에 `TDS-<모듈>-001` 문서가 등록되어 있다(예: `TDS-ENHANCE-BASIC-001`). `TDS-001` 이라는 문자열은 다른 모듈 문서에서도 쓰이므로(`docs/README.md`, `docs/panel-defect/INDEX.md`) DISP RTM 의 `TDS-001`~`TDS-403` 은 정의 문서가 없는 id 이고, 다른 모듈의 같은 번호와 이름이 겹친다. 이름 축에서 ✓ 행 69개 중 시험 이름과 연결되는 것은 0개다.
이 역시 이름 축 결과다. FR-MODAL-101 등 SRS 요구를 실제 단언하는 시험이 있는지(행위 축)는 이번에 가르지 않았다 → Gap. 성능 행(PERF-TIME-10x)은 §5 대로 CI 가 단언하지 않는다.

## 4. 헤더가 하지 않는 일을 약속하는 문장

1. `display_api.h:111-113` VOI 선형식 `output[i] = clamp((input[i] - (center - width/2)) / width * …` — 구현은 PS3.3 C.11.2.1.2.1 형태(centre-0.5, width-1, 세 분기)이고 헤더 식은 #156 이전 식이다.
2. `display_api.h:84` “Standard linear windowing with half-value offset” — REQ-DISP-010a 는 "half-value offset" 이라는 말을 쓰려면 어느 함수를 뜻하는지 밝히라고 요구한다. 밝히지 않았다.
3. `display_api.h:187-188` “…an invalid slope or LUT is reported even for a zero-pixel image.” — 0 화소 영상은 `xpe_validate_float32`(`display_helpers.cpp:111-113`)에서 먼저 거부되어 문장의 상황이 일어날 수 없다. 반환 코드는 어느 쪽이든 INVALID_INPUT.
4. `display_api.h:218-219` “An unknown mode is detected inside the per-pixel dispatch…” — 알 수 없는 모드는 switch default 에서 화소 루프 전에 잡힌다. 쓰지 않는다는 행동은 맞지만 "화소별 디스패치" 는 아니다.
5. `display_api.h:142` `gsdfEnabled: non-zero if the LUT was generated by xpe_gsdf_calibrate().` — `xpe_apply_presentation_lut` 은 이 값을 읽지 않는다(내가 확인: 쓰기만 `presentation_lut.cpp:282-283`). SPEC REQ-DISP-024 는 이 값을 조건으로 정의한다.
6. `display_api.h:63` “Input pixel value is rounded, shifted by lutFirstMapped, and clamped…” — float→int32 변환이 범위를 넘으면 미정의(후보 D2).

그 밖에 코드 주석의 요구 번호가 옛 번호(예: `presentation_lut.cpp` 의 "REQ-DISP-024: allocate new uint16 buffer", "REQ-DISP-028: set gsdfEnabled flag")이고, `test_display_integration.cpp`·`test_presentation_lut.cpp` 의 REQ 주석도 번호가 어긋난다(통합 시험 55줄 "REQ-029=Full Pipeline" 등). 주석의 id 는 증거로 쓰지 않았다.

## 5. 빌드·CI 에서 빠진 것

- `ci.yml:318`·`:380` 필터가 시간 단언 시험을 `post-build` 잡에서 뺀다. 그래서 CI 는 REQ-DISP-008/016/028 의 시간 예산을 단언하지 않는다. 제외되는 시험: `test_modality_lut.cpp:227/268`, `test_presentation_lut.cpp:242/425`, `test_voi_lut.cpp:354/379`.
- 시간 단언의 값이 SPEC 과 다르다: `PresentationLut Performance_3072x3072` 는 30 ms 까지 허용하는데 SPEC 은 25 ms 이고, 이 시험의 주석은 REQ-DISP-025 를 인용한다(번호 어긋남). 모두 균일 영상이고 ms 절단 측정이라 20.9 ms 도 20 ms 한계를 통과한다.
- plan.md 가 이름 붙인 `test_display_boundary.cpp` 는 존재하지 않는다(progress.md 는 경계 시험을 통합 시험에 합쳤다고 적는다). 시험 파일 전부 `CMakeLists.txt` 등록(이름 확인).
- 일부 시험은 Windows 전용이다: `test_presentation_lut.cpp:324` 가 `<windows.h>` 를 무조건 포함하고, 가드 페이지 시험은 `_WIN32` 에서만 컴파일, `DisplayEndurance` 는 비 Windows 에서 자체 스킵.
- `test_nonfinite_pixels.cpp:261` 의 `if (rc != XPE_OK) continue;` 때문에 `VoiLutNeverMakesANonFinitePixelFromFiniteInput` 은 모든 경우가 거부되어도 공허하게 통과한다.
- 기존 오라클 약점: 값 시험 다수가 1×1 또는 균일 영상이고 `[0]` 원소만 읽는다. 루프 상한 off-by-one 이나 stride 버그는 안 보인다. GSDF 시험이 자기 소유의 PS3.14 계수 사본을 갖고 있고 Table B-1 은 j≤70(L≤0.964 cd/m²) 10점만 확인한다.
- 테스트 파일 `test_gsdf_characterization.cpp` 머리말(1-51줄)·654-664줄은 stage-2 이전 상태("THE LUT IS A LINEAR RAMP")를 설명하지만 실행 단언은 반대로 바뀌었다 — 설명이 낡았다.

## 6. 결함 후보 (고치지 않음, 읽기로 찾은 후보)

| # | 요구 | 위치 | 후보 |
|---|---|---|---|
| D1 | REQ-DISP-025/026 | `presentation_lut.cpp:152,190-224,233-239,255-274` | 최소 휘도가 0 이하이면 0.01 로 대체하는데(152줄), 0.05 cd/m² 미만이면 jnd_min 이 표준의 1..1023 영역 밖이다(에이전트 산술 j(0.01)=-20.8). 영역 밖 JND 지수를 쓴다. |
| D2 | REQ-DISP-002 | `modality_lut.cpp:78`, `display_internal.h:409-411` | |값| > 2³¹ 인 유한 화소를 float→int32 로 변환 — 미정의, x64 에서 INT_MIN 이 되면 마지막 항목이 아니라 인덱스 0 으로 잘린다. |
| D3 | REQ-DISP-015/009 | `voi_lut.cpp:171,229-241` | LINEAR 에서 width ∈ (0,1) 이면 w=width-1 이 음수가 되어 lo>hi, 세 분기 판정이 한 점 계단으로 퇴화(요구는 width>0 이면 모두 허용). |
| D4 | REQ-DISP-021 | `presentation_lut.cpp:32` | SPEC: [0,1] 밖 클램프. 코드: ±inf·NaN 은 클램프 전에 거부(QA-B-181f). SPEC 문구 미갱신. |
| D5 | REQ-DISP-024 | `presentation_lut.cpp:20-73` | `gsdfEnabled` 는 쓰기만 하고 읽지 않는다. "WHEN gsdfEnabled…" 조건이 코드에 없다(0 이든 1 이든 같은 적용). |
| D6 | REQ-DISP-026 | spec.md 431줄 vs `presentation_lut.cpp:88-92` | SPEC 은 NaN 휘도가 가드를 통과한다고 적는데 QA-B-181d 이후 코드는 모든 비유한 원소를 거부. SPEC 이 낡았다. |
| D7 | REQ-DISP-029@439 | spec.md 439줄 vs `display_api.h:2` | SPEC “All 5 exported functions”, 헤더는 6함수. 같은 id 가 두 번 정의됨(§2). |
| D8 | REQ-DISP-009..012 | `voi_lut.cpp:176-179,240,250,258` | minOut > maxOut 를 거부하지 않는다. 뒤집힌 램프 대신 두 값만 나오는 영상이 된다. |
| D9 | REQ-DISP-030/031 | `modules/display/src` 전체 | 예외를 내부에서 잡는 `try/catch` 도 로깅 호출도 없다(내가 grep 으로 확인). CMake 는 spdlog·fmt 를 PRIVATE 링크하지만 쓰이지 않는다. |

추적 항목: 이 대조를 담을 더 맞는 기존 항목을 못 찾았다. **새 이슈 필요**(리더 판단) — D5·D9·D1 이 우선 후보. 이번 커밋은 `Refs #130`.

## 7. 수정안 초안 (리더 소유 파일)

`drafts/DISP_spec_plan_draft.txt` — spec.md 의 REQ-DISP-029 중복(두 번째를 새 번호로) 정리, 헤더 함수 수 5→6, 010a 를 plan.md 작업 행에 연결, plan.md 의 존재하지 않는 `test_display_boundary.cpp` 정정.

## 8. Gap / 잔여 위험

Gap
- 결함 후보 D1~D9 는 실행 재현을 하지 않았다(D1 의 j 값은 에이전트 산술).
- RTM ✓ 69행의 행위 축을 가르지 않았다. 시험 id 없는 24행은 이름 축으로만 확인했다.
- 에이전트의 구현 줄 번호 13건이 파일 밖이어서 전부 폐기했다. `partial` 사유 문장은 줄 번호가 아니라 서술을 근거로 하므로 사유 자체는 검증 범위 밖이다. 내가 직접 읽은 것: `gsdfEnabled` 미사용, 로깅·try/catch 부재, 프레젠테이션 시험의 30 ms, 주석의 옛 요구 번호.
- 표 상태 열이 없어 "표 상태 대 실제" 는 progress.md 와 대조하지 않았다.

잔여 위험
- 029 가 같은 id 두 정의라 판정이 `029@403`/`029@439` 로 갈린다. 리더가 어느 것을 유지할지 정하면 판정 하나는 사라진다.
- 모든 "Windows 에서만 도는 시험" 은 CI 의 Windows 러너에서만 관측됐다고 가정했다.

## 부록 A. 요구별 판정 전체 표

| 요구 | 판정 | 독립 기대값의 출처 | 시험이 단언하지 않는 문구 | 근거 시험 |
|---|---|---|---|---|
| REQ-DISP-001 | asserted | hand-computed literals (1000*1-1024=-24; 500*0.5+100=350) | per-pixel on a non-uniform image: both tests use uniform fills and read only element [0] | `test_modality_lut.cpp:58` LinearRescale_BasicValues; `test_modality_lut.cpp:73` LinearRescale_SlopeAndIntercept; `test_modality_lut.cpp:259` EdgeCase_1x1Image |
| REQ-DISP-002 | asserted | hand-written 5/3-entry LUT literals and index arithmetic done by hand | rounding of non-integer inputs; inputs beyond int32 range (see defect candidate) | `test_modality_lut.cpp:99` TableMode_BasicLookup; `test_modality_lut.cpp:119` TableMode_LutFirstMappedOffset; `test_modality_lut.cpp:139` TableMode_ClampingBounds |
| REQ-DISP-003 | partial | return code constant from SPEC | 'without modifying any state' for the NULL-params case (image content never compared; NULL img has no state) | `test_modality_lut.cpp:154` Error_NullImg; `test_modality_lut.cpp:161` Error_NullParams |
| REQ-DISP-004 | partial | return code constant from SPEC | 'without modifying the image': the only unchanged-check is buf[0]==0 on an all-zero buffer with slope 1/intercept 0, which could not reveal a write | `test_modality_lut.cpp:179` Error_WrongFormat; `test_modality_lut.cpp:181` Error_WrongFormat |
| REQ-DISP-005 | asserted | return code constant from SPEC |  | `test_modality_lut.cpp:192` Error_TableNullLutData |
| REQ-DISP-006 | asserted | return code constant from SPEC |  | `test_modality_lut.cpp:205` Error_TableZeroLength |
| REQ-DISP-007 | asserted | return code constant from SPEC |  | `test_modality_lut.cpp:217` Error_LinearZeroSlope; `test_modality_lut.cpp:219` Error_LinearZeroSlope |
| REQ-DISP-008 | partial | numeric budget from SPEC (20 ms) | enforcement in CI: ci.yml ctest uses -E "Performance/Within[0-9]+ms/..." so the assertion never runs there; measure-only BenchmarkFreeze test asserts  | `test_modality_lut.cpp:241` Performance_3072x3072_Linear; `test_modality_lut.cpp:268` BenchmarkFreeze_Performance_REQ_DISP_008_Linear3072 |
| REQ-DISP-009 | asserted | DICOM PS3.3 C.11.2.1.2.1 worked-example window boundaries (c=2048 w=4096 -> 0/4095; c=0 w=100 -> -50/49) plus  | interior (non-center, non-edge) pixels of the window; only the centre value and the 4 boundary points are probed | `test_parameter_dependency.cpp:268` VoiLinearMatchesTheStandardsWorkedExamples_156; `test_parameter_dependency.cpp:269` VoiLinearMatchesTheStandardsWorkedExamples_156; `test_voi_lut.cpp:76` Linear_CenterWindow |
| REQ-DISP-010 | partial | midpoint identity at x==center (127.5 = (0+255)/2) | the window slope/extent (width) and the edges of LINEAR_EXACT: at x==center the width term cancels, so a wrong divisor passes; no off-centre literal | `test_voi_lut.cpp:133` LinearExact_CenterValue; `test_parameter_dependency.cpp:186` Fixed156_VoiLinearExactDiffersFromVoiLinear |
| REQ-DISP-010a | partial | PS3.3 window-boundary examples for the LINEAR half; midpoint for the EXACT half | 'LINEAR_EXACT uses center and width unadjusted' beyond the centre value; the statement about '+0.5 present in both' is only implicit | `test_parameter_dependency.cpp:268` VoiLinearMatchesTheStandardsWorkedExamples_156; `test_voi_lut.cpp:133` LinearExact_CenterValue |
| REQ-DISP-011 | partial | sigmoid(0)=0.5 identity at x==center | the -4 steepness factor and the width scaling (only x==center probed, tolerance +-1.0 on 255 and +-0.01 on 1); no off-centre literal | `test_voi_lut.cpp:155` Sigmoid_CenterValue; `test_display_integration.cpp:124` FullPipeline_TableModality_SigmoidVoi; `test_parameter_dependency.cpp:133` VoiLut_EveryParameterReachesTheOutput |
| REQ-DISP-012 | partial | literal clamp ends 0 / 255 for LINEAR; range inequality for SIGMOID | clamping of LINEAR_EXACT with a literal; SIGMOID only a range property (implementation-guaranteed) | `test_voi_lut.cpp:92` Linear_ClampMin; `test_voi_lut.cpp:108` Linear_ClampMax; `test_voi_lut.cpp:172` Sigmoid_OutputClampedToRange |
| REQ-DISP-013 | partial | return code constant from SPEC | 'without modifying any state' (image unchanged after NULL params is never checked) | `test_voi_lut.cpp:187` Error_NullImg; `test_voi_lut.cpp:194` Error_NullParams |
| REQ-DISP-014 | asserted | return code constant from SPEC; buffer initial values are hand-written |  | `test_voi_lut.cpp:212` Error_WrongFormat; `test_voi_lut.cpp:213` Error_WrongFormat |
| REQ-DISP-015 | asserted | return code constant from SPEC |  | `test_voi_lut.cpp:225` Error_ZeroWidth; `test_voi_lut.cpp:226` Error_ZeroWidth; `test_voi_lut.cpp:237` Error_NegativeWidth |
| REQ-DISP-016 | partial | numeric budget from SPEC (16 ms) | enforcement in CI (ci.yml:318 excludes 'Performance*'); measure-only BenchmarkFreeze asserts no time | `test_voi_lut.cpp:370` Performance_3072x3072; `test_voi_lut.cpp:379` BenchmarkFreeze_Performance_REQ_DISP_016_Linear3072 |
| REQ-DISP-017 | asserted | literal 32768 / 65535 stated in the requirement itself; distinct-level count on a DN ramp as behavioural contr | the 'modality LUT identity' semantic beyond the numbers; minOut/maxOut (0/255) not in the requirement and never asserted | `test_voi_lut.cpp:251` Preset_Bone; `test_voi_lut.cpp:252` Preset_Bone; `test_parameter_dependency.cpp:323` VoiPresetDoesNotCrushRawDetectorDn_177 |
| REQ-DISP-018 | asserted | return code constant from SPEC | params left untouched on invalid body part (header promise, not in the requirement) | `test_voi_lut.cpp:289` Preset_InvalidBodyPart; `test_voi_lut.cpp:295` Preset_NullParams |
| REQ-DISP-019 | asserted | hand-computed: round(0.5*1023)=512, round(0.5025126*1023)=514 (x1024 would give 515), identity/sentinel LUT li | per-pixel mapping over a ramp (all probes are single pixels or uniform images) | `test_display_integration.cpp:93` FullPipeline_LinearModality_LinearVoi_PresLut; `test_presentation_lut.cpp:93` LutLookup_HalfValue; `test_presentation_lut.cpp:107` LutLookup_ZeroInput |
| REQ-DISP-020 | asserted | literals: UINT16, 16, 16, 2*2*2 bytes |  | `test_presentation_lut.cpp:72` DomainTransition_FormatBecomesUint16; `test_presentation_lut.cpp:73` DomainTransition_FormatBecomesUint16; `test_presentation_lut.cpp:75` DomainTransition_FormatBecomesUint16 |
| REQ-DISP-021 | partial | literal LUT results (index 0 and 1023 of the identity LUT) for -5.0 and 2.5 | infinite and NaN pixels: the wording 'outside [0,1] -> clamp' includes +-inf, but the code refuses them with INVALID_INPUT (presentation_lut.cpp:32) a | `test_presentation_lut.cpp:136` InputClamp_Negative; `test_presentation_lut.cpp:148` InputClamp_AboveOne; `test_nonfinite_pixels.cpp:168` PresentationLutRefusesANonFinitePixelAndLeavesTheImageAlone |
| REQ-DISP-022 | partial | return code constant from SPEC | 'without modifying any state' (image intact after NULL params not checked) | `test_presentation_lut.cpp:160` Error_NullImg; `test_presentation_lut.cpp:167` Error_NullParams |
| REQ-DISP-023 | asserted | return code constant from SPEC |  | `test_presentation_lut.cpp:183` Error_WrongFormat |
| REQ-DISP-024 | constant_only | none: no test asserts output values produced from a GSDF-calibrated LUT | everything: that gsdfEnabled!=0 selects/applies the calibrated entries; no code branch or test depends on the flag | `test_display_integration.cpp:286` GsdfPipeline_CalibrateThenApply; `test_parameter_dependency.cpp:390` PresentationLut_LutContentsReachTheOutput |
| REQ-DISP-025 | asserted | closed-form inverse of a synthetic gamma display (2.2 and 1.8, 257 samples, 0.5..500 cd/m2) computed with the  | tight accuracy: tolerance is 100 of 65535 counts (measured residual 72/48); luminance range beyond 0.5..500; module's own Eq 7-2 coefficients are neve | `test_gsdf_characterization.cpp:821` Stage2_ReproducesTheAnalyticLutForSyntheticGammaDisplays_155; `test_gsdf_characterization.cpp:845` Stage2_ReproducesTheAnalyticLutForSyntheticGammaDisplays_155; `test_gsdf_characterization.cpp:465` StandardEquationsMatchTableB1_155 |
| REQ-DISP-026 | asserted | return code constant from SPEC; sentinel-filled output (0xBEEF) as untouched oracle |  | `test_presentation_lut.cpp:220` GsdfCalibrate_Error_NullLuminance; `test_presentation_lut.cpp:227` GsdfCalibrate_Error_NullOut; `test_presentation_lut.cpp:235` GsdfCalibrate_Error_CountLessThan2 |
| REQ-DISP-027 | asserted | constant 1 from SPEC; 0 preserved on rejected calls |  | `test_presentation_lut.cpp:198` GsdfCalibrate_BasicOutput; `test_presentation_lut.cpp:487` GsdfCalibrate_Error_NonDecreasingViolation_155 |
| REQ-DISP-028 | partial | numeric budget -- but the test uses 30 ms, not the requirement's 25 ms | the 25 ms budget itself (test limit is 30); CI enforcement (ci.yml:318 excludes 'Performance*'); measure-only benchmark asserts no time | `test_presentation_lut.cpp:254` Performance_3072x3072; `test_presentation_lut.cpp:425` BenchmarkFreeze_Performance_REQ_DISP_028_Lut3072 |
| REQ-DISP-029@403 | asserted | reject: constant rc + sentinel; use of interior: analytic gamma LUT built from closed form at DDL_i=i/(count-1 | that a log-spaced (non-equally-spaced) ascending array yields XPE_OK AND a wrong LUT (the 'SHALL NOT detect' half) -- only 'ascending is accepted' is  | `test_presentation_lut.cpp:479` GsdfCalibrate_Error_NonDecreasingViolation_155; `test_presentation_lut.cpp:500` GsdfCalibrate_AscendingIsAccepted_155; `test_presentation_lut.cpp:532` GsdfCalibrate_EqualNeighboursAreAccepted_155 |
| REQ-DISP-029@439 | no_test | none (only implicit: test exe links against the C header) | extern C linkage, __cdecl, blittable-only types, 'all 5 exported functions' (header declares 6 incl. xpe_display_version) | `test_display_integration.cpp:296` VersionString_NotNull |
| REQ-DISP-030 | no_test | none | that exceptions never cross the ABI; 'All exceptions SHALL be caught internally' (nothing is caught) | `test_display_integration.cpp:102` FullPipeline_TableModality_SigmoidVoi |
| REQ-DISP-031 | no_code | none | all of it: entry/exit DEBUG logs and ERROR logs | `test_display_integration.cpp:139` PresetDrivenPipeline_BonePreset |
| REQ-DISP-032 | no_test | none | any concurrent execution: no test starts a second thread | `test_display_integration.cpp:235` IndependentBuffers_NoInterference |
| REQ-DISP-033 | partial | CRT heap walk before/after 1000 cycles with a positive control (64 B/cycle leak must be seen) | error paths (the cycle runs only successful calls); the bound tolerates up to cycles/10 leaked blocks and 16 KB; no-leak is Windows-only (GTEST_SKIP e | `test_display_integration.cpp:464` ThousandCycles_CrtHeapDoesNotGrow; `test_display_integration.cpp:480` ThousandCycles_ControlLeakIsCaught; `test_display_integration.cpp:257` NoLeak_PresLutReplacesBuffer |
| REQ-DISP-034 | asserted | hand-computed literals on 1x1 images (42*2=84; boundary points of the PS3.3 windows; LUT entries 999/65535) | xpe_voi_preset_create/xpe_gsdf_calibrate are not image functions; EdgeCase_1x1FullPipeline itself asserts no values (only format/width/height) | `test_modality_lut.cpp:259` EdgeCase_1x1Image; `test_parameter_dependency.cpp:277` VoiLinearMatchesTheStandardsWorkedExamples_156; `test_presentation_lut.cpp:107` LutLookup_ZeroInput |
| REQ-DISP-035 | constant_only | none | VOI and presentation at 4096x4096; the 'allocated memory budget' (never defined or measured); output values | `test_display_integration.cpp:213` EdgeCase_4096x4096NocrashModalityLut |
