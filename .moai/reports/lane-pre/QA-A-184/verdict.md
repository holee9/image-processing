# QA-A-184 — #232 결정 재료 (보고만, 제품 코드 변경 없음)

- 브랜치: dev/preprocess, 시작 HEAD `c88294cd` (`evidence/00_head.txt`)
- 변경: 프로브 시험 1개 파일(`modules/preprocess/tests/test_zz_a184_probes.cpp`, 전부 `DISABLED_`)과 그 등록 1줄.
  제품 소스·헤더 diff 0줄 (`git diff -- modules/preprocess/src modules/preprocess/include modules/common` = 0줄, 원복 뒤 재확인).
- 시험 수 (`ctest -N`): 실행 790 / 총계 830 / DISABLED 40 (이전 790/826/36, DISABLED 프로브 4개 추가).
  전체 시험 1회 = 713 통과, 0 실패 (`evidence/12_baseline_full_suite.txt`). 프리셋 점검 OK (`evidence/15_preset_check.txt`).

---

## ① REQ-P1A-021 — 치수 불일치 반환 코드

### 주장

`xpe_offset_correct` / `xpe_gain_correct` / `xpe_defect_correct` 는 치수가 어긋나면 `XPE_ERR_BUFFER_TOO_SMALL`(-8) 을 돌려준다. spec.md:629 는 `XPE_ERR_INVALID_INPUT`(-1) 을 요구한다.
치수 어긋남에는 서로 다른 두 종류가 있고, spec 본문이 가리키는 것은 앞쪽뿐이다("input and map buffer dimensions differ").

| 종류 | 사이트 | spec 본문이 가리키나 |
|---|---|---|
| 맵 ≠ 입력 | offset_correct.cpp:186, gain_correct.cpp:309·:327, defect_correct.cpp:179 | 예 |
| 출력 ≠ 입력 | offset_correct.cpp:156, gain_correct.cpp:253, defect_correct.cpp:128 | 아니오 (api-spec.md:103/:108 은 출력 버퍼 문제를 BUFFER_TOO_SMALL 로 적음) |

`dataSize` 사이트(offset:166, gain:264, defect:138)는 진짜 "버퍼가 작다"이므로 두 변형 모두에서 건드리지 않았다.

### 증거

현재 코드가 실제로 돌려주는 값 (`evidence/11_codes_baseline.txt`, 맵 4x4, 입력 5x4 / 출력만 5x4):

```
offset  맵≠입력 -> -8   출력≠입력 -> -8
gain    맵≠입력 -> -8   출력≠입력 -> -8
defect  맵≠입력 -> -8   출력≠입력 -> -8
```

임시 변형으로 코드를 바꿔 빌드하고 전체 시험을 돌린 결과 (변형이 적용됐는지는 같은 프로브가 -1 을 찍는 것으로 확인):

| 변형 | 바꾼 곳 | 프로브 | 전체 시험 |
|---|---|---|---|
| V1 | 맵≠입력 4곳 → INVALID_INPUT | 맵≠입력 3종 -1, 출력≠입력 3종 -8 (`07_codes_V1.txt`) | 713 통과, 0 실패 (`06_v1_full_suite.txt`) |
| V2 | V1 + 출력≠입력 3곳 → INVALID_INPUT | 6종 전부 -1 (`09_codes_V2.txt`) | **4건 실패**, 709 통과 (`08_v2_full_suite.txt`) |

V2 에서 빨개진 시험 4건 (이름 + 줄, 전부 출력 버퍼 치수를 어긋나게 한 시험):

1. `OffsetCorrectTest.DimensionMismatchReturnsError` — test_offset_correct.cpp:129
2. `GainCorrectTest.DimensionMismatchReturnsError` — test_gain_correct.cpp:119
3. `DefectCorrectTest.DimensionMismatchReturnsError` — test_defect_correct.cpp:248
4. `GainCorrectReciprocalFMATest.DimensionMismatchReturnsInvalidInput` — test_gain_correct_reciprocal_fma.cpp:348 (이름은 INVALID_INPUT 인데 단언은 BUFFER_TOO_SMALL)

그 외 `BUFFER_TOO_SMALL` 을 `||` 로 함께 받는 시험(test_xpe_preprocess.cpp:828, test_xpe_preprocess_correction.cpp:180·:295·:412)은 두 변형 모두에서 통과했다.

호출자 (`evidence/13_callers.txt`, 시험 제외 전수):

- modules/preprocess/src/pipeline.cpp:152·:200·:270 — 반환값을 그대로 위로 전달(`if (result != XPE_OK) return result;`). 코드 값으로 분기하지 않음.
- gui/ImageProcTest/Services/Native/GuiPreprocessRunner.cs:125·:149·:155 — `!= XpeOk` 만 비교, 코드는 메시지 문자열에 찍기만 함.
- clients/ImageProcTest/Services/NativePreprocessPreviewService.cs, Diagnostics/XpePreprocessSyntheticOracle.cs — 대리자(delegate)로 호출, -8 비교 없음 (`grep -8` 0건).
- C#/GUI 어디에도 offset/gain/defect 의 BUFFER_TOO_SMALL 에 의존하는 분기는 없음.

### 기준선 귀속

명령: `xpe_preprocess_tests.exe` 전체 1회, `--gtest_also_run_disabled_tests --gtest_filter='A184Probes.DISABLED_DimensionMismatchReturnCodes'`. 원복 뒤 기준선 재측정(`12_`, `11_`)이 713 통과, 코드 -8 로 돌아온 것을 확인.

### 결정에 필요한 것

"맵≠입력만 INVALID_INPUT 으로 바꿀지(시험 0건 변경), 출력≠입력까지 바꿀지(시험 4건 단언 변경)" — 후자는 api-spec.md:103/:108 과도 갈라진다.

---

## ② REQ-P1A-041 — 선 잡음

### 주장

현재 `xpe_validate_readout_artifact` 는 행 평균이 0.9×65535 를 넘으면 `has_nonuniform_gain` 을 켠다. 이것은 선 잡음 검사가 아니라 밝기 검사이고, 행/열에 상관된 잡음이 있는 프레임을 못 잡는다.
저장소에는 선 잡음이 든 프레임이 없다. 따라서 이 검사가 실제 선 잡음에서 무엇을 내는지는 합성 프레임으로만 관측했다.

### 증거 (`evidence/02_probe2_line_noise.txt`, 1024x1024 UINT16)

선 잡음 지수 LNI = 선 평균 차분의 퍼짐 ÷ 화소 잡음만 있을 때의 기대 퍼짐 (~1 이면 선 잡음 없음). LNI1 은 1차 차분, LNI2 는 2차 차분(완만한 기울기 제거).

| 프레임 | rc | nonuniform_gain | LNI1 행/열 | LNI2 행/열 |
|---|---|---|---|---|
| 합성 깨끗함 (20000, 화소 σ20) | 0 | 0 | 1.04 / 1.01 | 1.06 / 1.00 |
| 합성 행 선 잡음 σ300 | 0 | **0** | 521.39 / 0.06 | 521.24 / 0.06 |
| 합성 열 선 잡음 σ300 | 0 | **0** | 0.07 / 485.49 | 0.07 / 514.79 |
| 합성 행 선 잡음 σ1500 | 0 | **0** | 2302.49 / 0.01 | 2220.04 / 0.01 |
| 합성 행 잡음 σ3000, 만점 근처 | 0 | 1 | 4591.39 / 0.01 | 4732.28 / 0.01 |
| 대조: 깨끗하지만 밝음 (62000) | 0 | **1** | 1.02 / 1.04 | 1.03 / 1.05 |
| 대조: 전부 0 인 열 하나 | 0 | 0 (dropped=1) | 1.03 / 0.99 | 1.01 / 1.06 |
| 대조: 포화 행 하나 | 0 | 1 | 0.99 / 0.99 | 1.02 / 1.00 |

읽는 법: 선 잡음이 분명한 프레임(LNI 500~2300)에서 플래그는 0 이고, 선 잡음이 전혀 없는 밝은 프레임(LNI ≈1)에서 플래그는 1 이다. 즉 지금 검사는 선 잡음과 무관하게 "밝음"에 반응한다. 지수가 지수로서 작동하는 것은 깨끗한 합성 프레임이 ≈1 로 나오는 것과 대조(밝은 깨끗한 프레임 ≈1)로 확인했다.

저장소 프레임 (같은 표, 플래그는 전부 0):

| 프레임 | LNI1 행/열 | LNI2 행/열 | 비고 |
|---|---|---|---|
| gui-s0 `wrist_lat_3072x3072.raw` | 33.74 / 11.96 | 39.56 / 7.91 | 해부 구조가 있는 실영상. 선 잡음인지 해부 기울기인지 이 지수로는 못 가름 (LNI2 로도 안 줄어듦) |
| gui-s0 `synthetic_1024x1024.raw` | 0.00 / 0.00 | 0.00 / 0.00 | 화소 잡음 추정이 0 이라 지수가 정의되지 않는 값 (잡음 없는 합성) |
| `test_data/synthetic_512x512.raw` | 0.00 / 0.00 | 0.00 / 0.00 | 위와 같음 |
| QA-A-51 uniform / lines / scatter / edge / checker8 / diag | 0.36~1.45 | 0.35~2.01 | 전부 ≈1 근방, 선 잡음 없음 (`lines.raw` 는 선 *패턴*이고 선 잡음이 아님) |

선 잡음을 넣어 만든 픽스처나 생성기는 저장소에 없다 (`xpe_calib_fixture_gen` 의 옵션 목록 — `--out --width --height --seed --expiry-ms --gain-poly --poly-degree --poly-levels` — 에 선 잡음 옵션은 없고, `--seed` 는 화소 잡음용 난수 시드; modules/preprocess/tools/xpe_calib_fixture_gen.cpp:10-21 에서 읽음).

### 기준선 귀속

명령: `xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter='A184Probes.DISABLED_LineNoiseVersusTheReadoutCheck'`, 이 트리(HEAD `c88294cd` + 프로브 파일). 합성 프레임은 고정 시드.

### 미검증

- 실제 검출기의 선 잡음(실영상)은 이 저장소에 없어서 관측 못 했다. wrist_lat 의 LNI 가 높은 이유(해부 vs 선 잡음)는 모른다.
- LNI 는 이번 카드용 임시 지표이고 요구가 정한 검출 기준이 아니다.

### 결정에 필요한 것

"선 잡음 검출을 구현할지(그 기준을 누가 정할지), 아니면 REQ-P1A-041 의 선 잡음 부분을 미구현으로 spec 에 적을지" — 그리고 `has_nonuniform_gain` 이 지금처럼 밝기에 반응하는 것을 유지할지 이름을 바꿀지.

---

## ③ SRS-CALIB-NFR-003 — ghost 핸들을 두 스레드가 공유하면

### 주장

핸들별 뮤텍스가 없으므로 한 ghost 핸들을 두 스레드가 동시에 쓰면 이력(hist1/hist2)의 갱신이 유실된다. 오류 반환·비유한 값(NaN/Inf)은 관측되지 않았다.

### 방법

두 스레드가 같은 프레임(값 1.0)과 같은 취득 시각으로 `xpe_ghost_correct` 를 번갈아 부른다. 같은 입력이면 어떤 원자적 끼어들기든 직렬 재생과 같은 최종 이력이 나오므로, 최종 hist 가 직렬 재생과 비트 단위로 다르면 그것은 갱신 유실이다. 기본 시간 상수(τ1=1, τ2=20)에서는 이력이 수축해서 유실이 가려지므로(프로브 첫 판이 이 맹점을 실제로 측정했다: 기본 τ 에서 불일치 0건), 주 실험은 τ1=τ2=1e12 로 이력이 잊지 않게 해서 `hist1[i]` 가 곧 적용된 갱신 수가 되게 했다. 대조군 둘: 외부 뮤텍스로 직렬화한 공유 핸들, 스레드당 핸들 하나. 둘 다 직렬 재생과 일치해야 한다.

### 증거

`evidence/03_probe3_shared_ghost_handle.txt` (τ=1e12, 스레드 2개, 매 실행 새 핸들):

| 크기 | 스레드당 호출 | 공유·무동기 | 외부 뮤텍스 대조 | 스레드당 핸들 대조 |
|---|---|---|---|---|
| 16×16 | 20000 | 50/50 실행에서 불일치 (최악 실행 갱신 232538건 유실 / 40000×256 중, 최대 한 실행 기준) | 0/50 | 0/50 |
| 64×64 | 2000 | 50/50 불일치, 최악 22031건 유실, 불일치 실행에서 원소 ~82% 가 다름 | 0/50 | 0/50 |
| 512×512 | 200 | 30/30 불일치, 최악 3665건 유실, 원소 ~0.5% 가 다름 | 0/30 | 0/30 |

오류 반환 0건, 비유한 값 0건 (전 구성).

기본 시간 상수(`evidence/04_probe3b_default_taus.txt`, 짧은 실행):

| 크기 | 스레드당 호출 | 공유·무동기 불일치 실행 | 대조 둘 |
|---|---|---|---|
| 16×16 | 8 | 19/3000 (1%) | 0/3000, 0/3000 |
| 64×64 | 20 | 756/1000 (76%), hist2 최대 차이 0.6144 | 0/1000, 0/1000 |

기본 τ 에서는 hist1 이 거의 안 어긋나고(최대 차 1.07e-06) 느린 누적 hist2 가 어긋난다. 이력이 수축해서 장기적으로는 가려지지만 짧은 실행의 최종 상태는 76% 에서 직렬 재생과 다르다.

호출자: 저장소 안에서 `xpe_ghost_create`/`xpe_ghost_correct` 를 부르는 파일은 시험 9개뿐이고, 제품·GUI·클라이언트 코드에는 없다(제외 목록으로 grep 0건, 대조군으로 시험 포함 grep 은 10개 파일). 즉 지금은 공유 호출이 일어나는 경로가 없다.

### 기준선 귀속

명령: `xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter='A184Probes.DISABLED_SharedGhostHandleTwoThreads'` 및 `...DefaultTaus`, 이 트리, 이 머신. 재현률은 위 표의 실행 수 대비 불일치 수.

### 미검증

- 깨짐이 "읽기-갱신-쓰기 경합"인지 다른 원인인지 코드 수준 확정은 안 했다(대조군이 외부 뮤텍스에서 0 이라는 것까지만 확인).
- 스레드 2개만 시험했다. 3개 이상, 이종 입력, 서로 다른 해상도는 안 했다.
- 비유한 값이 0건이라는 것은 이 입력(상수 프레임)에서의 관측이다.

### 결정에 필요한 것

"핸들별 뮤텍스를 넣을지(NFR-003 문장대로), 아니면 NFR-003 을 '핸들은 스레드 사이에 공유하지 않는다'로 고칠지" — 이전 REQ-P1A-066 의 옛 정의(스레드 사이에 공유하지 않음)와 같은 방향이다.

---

## Gaps / Residual-risk

- 미검증: ② 실제 검출기 선 잡음, ③ 경합의 코드 수준 원인, ③ 스레드 3개 이상. CI 구성(Mock/Native 백엔드 축)에서의 재현은 안 돌렸다 — 프로브는 DISABLED_ 라 어느 CI 잡에서도 실행되지 않는다.
- 잔여 위험: ③ 의 재현률은 이 머신(코어 수·스케줄링)에 의존한다. 결론("유실이 일어난다")은 대조군 구조 때문에 머신에 덜 의존하지만 비율은 아니다.
- 이 카드가 만든 것: 프로브 파일 1개(DISABLED_ 4건)와 CMake 등록 1줄. 시험 수는 DISABLED 만 +4.
