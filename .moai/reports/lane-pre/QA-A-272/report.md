[2026-10-18 08:11 KST] QA-A-272 결과 — dev/preprocess CI 정적 분석 빨강 4건 수정(헤더 @param, 반올림 3건, memset), 다크 9장 재측정 바이트 동일, 271 보고서 정정과 사용자 결정 기록

이슈 #273. 기반 `4978ef5b`(원격 `a5015dd7`, 실행 39041153717 실패). 4건은 모두 266/266b 에서 들어온 것이다.

## 0. 결론
| CI 빨강 | 수정 | 로컬 확인 |
|---|---|---|
| Header docs (doxygen), text-lint: `xpe_verify_offset_signed_file` 의 `@param` 3개 없음 | `raw_image`, `offset_xcal_path`, `quality` 의 `@param` 추가 | 같은 검사기(`tools/docs/check_header_docs.py`)가 수정 전 헤더에서 CI 와 같은 3건을 내고(대조군), 수정본에서 0건. doxygen 자체는 이 PC 에 없어 실행하지 못함 |
| clang-tidy `bugprone-incorrect-roundings` (기준선 0 → 3) | 두 곳의 `static_cast<uint16_t>(x + 0.5f)` 를 출하 단계의 규칙 그대로인 `xpe_offset_apply_scalar_reference` 호출로 교체 | CI 와 같은 게이트 스크립트(VS 번들 clang-tidy 19.1.5)가 수정 전 소스에서 신규 3건(같은 파일, 기준선 0), 수정본에서 신규 0건 |
| cppcheck `memsetClassFloat` | `std::memset(out, 0, 32)` 를 값 초기화한 지역 구조체 복사로 교체 | cppcheck 가 이 PC 에 없어 **로컬 실행 못 함** (§3) |
| (시험·측정) | 반올림 경계 시험 추가, 변이 4개, 다크 9장 재측정 | 시험 69건 통과, 변이 4개 모두 빨강, 재측정 바이트 동일 |

## 1. 수정 내용
- **반올림 (옛 함수와 비트 동일 계약)**: 옛 경로(offset 보정 단계)의 UINT16 변환은 `v = raw − offset`(float32), `[0, 65535]` 로 자른 뒤 `static_cast<uint16_t>(v + 0.5f)`(float32 덧셈 뒤 절단)이다. AVX2 커널의 병렬 시험이 이 규칙의 스칼라 기준(`xpe_offset_apply_scalar_reference`)과 비트 동일을 지킨다. `std::lround(x)` 로 바꾸면 float32 덧셈의 동점 처리가 달라진다: x = 0.5 − 2⁻²⁵ 는 0.5 를 더하면 1 − 2⁻²⁵ 로 float32 에서 1.0 으로 반올림되어 출하 단계는 1, `lround` 는 0 이다. 그래서 형태를 바꾸는 대신 **같은 함수를 호출**한다(`offset_output_u16`: 원소 1개로 `xpe_offset_apply_scalar_reference` 호출). 값은 같고 `+ 0.5f` 캐스트 표현식은 이 파일에서 사라졌다. 헤더의 inline 함수는 기존대로 기준선에 있다(1건, 이 카드가 바꾸지 않음).
- **memset**: `quality_failure` 는 `XpeOffsetQuality zero{}; zero.structSize = ...; std::memcpy(out, &zero, 32)` 로 바꿨다. 머리 32바이트는 정수 필드만(`structSize`, `writtenSize`, `validMask`, `roiPixels`, `pixelCount`, `mapWidth`, `mapHeight`)이고 패딩이 없으며, 필드 경계까지만 쓴다는 R237-2 계약은 그대로다. `publish_quality` 의 그룹 영 채우기는 `uint8_t*` 캐스트 위의 memset 이고 CI 가 그 줄(`a5015dd7` 에도 있음)은 지적하지 않았으므로 건드리지 않았다.
- **헤더**: `@param raw_image / offset_xcal_path / quality` 3개 추가.

## 2. 확인 (명령과 결과)
1. **시험**: `xpe_preprocess_tests.exe --gtest_filter='*Verify*'` → 69건 통과(기존 68 + 신규 1). 옛 함수와 비트 동일을 지키는 기존 시험(`TheOutputFiguresEqualTheOldFunctionOnTheCorrectedImageBitForBit`, `ItMatchesTheReferenceOnAMixedFrame`)이 포함된다.
2. **신규 경계 시험** `TheUint16OutputRoundsHalfUpAndClampsLikeTheShippedStageAtTheBoundaries`: 잔차 0.49609375, 0.5, 0.50390625, 1.5, 2.5, −0.5, −100, 0, 65534.5, 65534.99609375, 65535, 65535.5, 70000 (float32 에서 정확히 표현되는 값)에 대해 기대 출력이 표(0, 1, 1, 2, 3, 0, 0, 0, 65535, 65535, 65535, 65535, 65535)와 같고, 같은 값이 출하 단계 함수의 결과와 같으며, `darkBiasOutput`, `dsnuOutput`, `zeroOutputRatePct`, `clampRatePct` 가 맞는다. 추가로 float32 동점 사례(x = 0.5 − 2⁻²⁵, raw 0 화소 5개가 ROI)에서 출력이 1 이어야 한다.
3. **변이 4개** (`evidence/20_mutation.py.txt`, 예측 먼저, 소스 해시 `1c5935b441a1841b` 복원 확인):
   | 변이 | 예측 | 결과 |
   |---|---|---|
   | M1 `std::lround` | 빨강(동점 사례만) | 빨강 1건: 신규 경계 시험만 |
   | M2 절단(+0.5 없음) | 빨강 | 빨강 4건 |
   | M3 `nearbyint`(짝수 반올림) | 빨강 | 빨강 4건 |
   | M4 위쪽 클램프 없음 | 빨강 | 빨강 2건 |
   M1 은 **신규 경계 시험만** 잡는다(기존 시험은 이 동점 사례를 담지 않았다). 변이 스크립트의 첫 실행은 빌드 도우미 경로(`/tmp/b.py`)가 Windows 파이썬에서 다른 디렉터리로 풀려 빌드가 전부 실패했고(`built False`) 그 "survived" 줄은 무의미하다. 경로를 고쳐 다시 돌린 결과가 위 표다.
4. **다크 9장 재측정** (`QA-A-266/evidence/50_real_darks.py.txt` 그대로, 수정 후 DLL): 출력이 `QA-A-266b/evidence/51_real_darks_result.txt` 와 **바이트 동일**(1547바이트, SHA-256 앞 16자 `19f3306f72331056` 양쪽 같음), `ALL P1/P2/P5/P7 CHECKS OK`. (`evidence/10_real_darks_rerun.txt`)
5. **헤더 문서 검사**: `python tools/docs/check_header_docs.py` — 수정 전 헤더(HEAD 복사본)에서 `xpe_verify_offset_signed_file: argument raw_image / offset_xcal_path / quality has no @param` 3건(CI 지적과 같음), 수정본 `21 headers, 0 findings`. `tools/ci/Test-TrackedTextFiles.ps1` 통과.
6. **clang-tidy 게이트**: VS 2022 번들 clang-tidy 19.1.5 로 `python tools/ci/check_clang_tidy.py preprocess build/ci-preprocess tools/ci/clang_tidy_baseline_preprocess.txt`(VS 환경에서 실행, CI 의 명령과 같음).
   - **대조군(수정 전 소스)**: `31 findings over 36 files; baseline 63`, 종료 1, `NEW: bugprone-incorrect-roundings … xpe_verify_metrics.cpp … (x + 0.5f) … 2 (baseline 0)` 와 `… (c + 0 … 1 (baseline 0)` → 신규 3건, CI 와 같다 (`evidence/52_tidy_gate_prefix_source.txt`).
   - **수정본**: `28 findings over 36 files; baseline 63`, 종료 0, 신규 없음 (`evidence/31_tidy_gate_preprocess.txt`).
   - 기준선보다 낮다는 줄 35개는 대부분 헤더(`xpe_error.h`, `xpe_types.h`, `preprocess_api.h`, `xpe_preprocess_internal.h`)의 기존 항목으로, 이 PC 의 clang-tidy 버전·헤더 처리가 CI 와 달라 로컬에서는 나오지 않는 것이다. 이 카드와 무관하고, 기준선은 바꾸지 않았다.
   - 첫 시도(소스 한 파일에 clang-tidy 직접 실행, `30_tidy_one_file.txt`)는 컴파일 데이터베이스가 헤더 경로를 못 풀어 `file not found` 로 쓸모가 없었다(게이트 스크립트는 `ninja -t compdb` 를 쓴다). 결론에 쓰지 않는다.

## 3. 못 한 것 (Gaps)
- **cppcheck 와 doxygen 은 이 PC 에 없어** 로컬에서 CI 와 같은 명령으로 돌리지 못했다. cppcheck 수정은 같은 줄을 `memcpy` 로 바꾼 것이며, CI 가 같은 파일의 `publish_quality` 의 `uint8_t*` 캐스트 memset 을 지적하지 않았다는 점(`a5015dd7` 실행 결과)이 근거일 뿐이다. 다음 CI 실행에서 확인이 필요하다. 설치(choco)는 이 PC 의 환경을 바꾸는 일이라 하지 않았다.
- doxygen 의 매개변수 불일치 검사는 위의 파이썬 검사기(같은 3건을 재현)로 갈음했다. Doxygen 1.12.0 실행은 아니다.
- 로컬 clang-tidy 는 CI 와 버전이 다를 수 있다. 소스 파일(`xpe_verify_metrics.cpp`)의 3건이 정확히 재현·해소됨은 확인했다.

## 4. 관찰만 적는 것
- 97b9367b 실행(39035358341)의 gui-e2e-native 실패 1건(`R05_APressedButton_IsRefusedOnScreen_WhenTheDllChangedAfterTheVerdict_AndNothingRuns`)은 `a5015dd7` 에서 통과했다고 카드가 적었다. 이 카드에서 고치거나 조사하지 않았다. 관찰로만 기록한다.

## 5. 271 보고서 정정 (Codex #245, 사용자 결정)
`QA-A-271/report.md` 맨 위에 정정 블록을 넣고 본문의 과한 문장을 낮췄다(`evidence/40_fix_report_271.py.txt`, 각 치환은 한 번만 나오는지 단언):
- **R245-1**: "107 % 설명, 지도·기준 0" → "이 경로(기준 → 지도 → 알고리즘)의 조건부 알고리즘 효과 중앙값이 원차이 중앙값의 1.067배, 기준·지도 효과 중앙값은 작다". 비영 칸 수: 기준 20, 지도 94, 알고리즘 112 (최대 절댓값 0.0035 / 0.2326). noisy·hot 예외를 적음.
- **R245-2**: 전역 배율의 출처를 확인된 사실로: 주입 세트 gain 이 약 1.0229 % 크고 출력이 약 1.0125 % 낮다(평균 정규화 경로). Codex 가 파일로 확인했고 나는 독립 재계산하지 않았음을 적음.
- **R245-3**: "이 지표는 충실도를 못 잰다" → "세트 정규화와 보간 편향이 섞여 보간 정확도 순위를 왜곡할 수 있다". B1·B2 는 내 사후 규칙을 못 넘었을 뿐 원래 B16 규칙에서는 회귀로 기록된다.
- **R245-4**: 가짜 줄 실험은 사후 탐색. 평탄 폭 1 팬텀의 MSE 는 1판 0.298, 2판 0.493(반례).
- **R245-5**: 0.08 / 0.41 / 0.79 는 이번 합성 실험의 조건부 검출률. 0.79 의 Monte Carlo SE 는 약 0.029.
- **사용자 결정(2026-10-18)**: 보정 z 지표를 어떻게 할지는 **새 촬영 뒤에 결정**한다. 기존 118/178 불합격과 B16 기준표는 그대로다.

## 6. 증거 파일 (`evidence/`)
`10_real_darks_rerun.txt`(재측정), `20_mutation.py.txt` / `21_mutation.txt`(변이), `30_tidy_one_file.txt`(쓸모없는 첫 시도), `31_tidy_gate_preprocess.txt`(수정본 게이트), `50_tidy_control.py.txt` / `51_tidy_control.txt` / `52_tidy_gate_prefix_source.txt`(수정 전 소스 대조군), `40_fix_report_271.py.txt`(271 정정).
