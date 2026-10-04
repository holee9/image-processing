# QA-A-237d — 고스트 제자리 안전 판정을 tier 의 실제 float 계산과 맞췄다 (Codex #142 보류 2건)

카드: QA-A-237d · Refs #245 · 기준 커밋 `95b41aa8`(237c 에서 먼저 고친 clang-tidy 정리) · 회신 `.moai/state/codex-archive/142.md`.
단위: MiB(2^20 B), 괄호 안은 십진 MB. 모든 수치는 `evidence/` 의 파일에서 가져왔다.

## 0. 결론 먼저

1. **결함이 맞다.** 237b ③의 제자리 안전 판정은 `a1 × h1` 의 곱만 double 로 상한을 잡았고, 실제 계산이 먼저 float 로 만드는 계수 `a1 = a1_base × exposureWeight × signalDependence` 자체가 `+Inf` 로 넘치는 경우를 보지 못했다. 작은 이력 뒤에 큰 프레임이 오면 판정은 "안전"(`worst ≈ 9e25`)인데 실제로는 `a1 = +Inf` 라 프레임이 실패하고, 제자리 실패 분기가 **이전 이력을 0 으로 지웠다**(REQ-P1A-032 위반).
2. **고쳤다.** 판정이 tier 의 식을 같은 순서, 같은 float 형식으로 최악 입력에서 평가하고, 중간 값 각각이 유한하고 1e37 이하인지 본다(§2 대조표).
3. **시험을 먼저 빨갛게 만들었다.** Codex 재현과 변형 3건이 옛 판정에서 실패하고(`evidence/10`, `11`), 고친 뒤 통과한다. 옛 판정으로 되돌리면 시험 3개가 다시 빨갛다(`evidence/60`).
4. **느린 경로의 세 할당 실패**를 6개 구성(티어 3 × 이력 유무 2)에서 주입해 OOM 코드·픽셀·이력·통계 불변·누수 없음을 단언한다(§3). 손상 반증도 있다(`evidence/61`).
5. **237b 의 보존 결과는 그대로다**: 다이제스트 32개 통과, 메모리 207.5 MiB 유지(정상 프레임은 제자리 경로에 남음), 시간은 잡음 범위(§4).
6. **게이트**: main 의 clang-tidy 기준선 게이트 새 지적 0(종료 0), doxygen 종료 0 / 경고 0(양성 대조군 포함). 237b 에서는 이 게이트를 돌리지 않았고, 돌려 보니 새 지적 6건이 있어서 `95b41aa8` 에서 먼저 정리했다(§5).

## 1. 결함의 구조

실제 계산(tier 3, `ghost_correct.cpp` `ghost_tier3`)은 화소마다 이 순서로 float 를 만든다.

```
signalDependence = 1.0f + beta * (raw / 32768.0f)
a1 = a1_base * exposureWeight * signalDependence        // float 곱 둘
corrected = raw - a1 * h1[i] - a2 * h2[i]
```

옛 판정은 이것을 `c1 = |a1_base| × weight × dependence`(double)로 계산하고 `worst = M + c1 × h1 + c2 × h2` 만 한계와 비교했다. `a1` 이 float 로는 `+Inf`(≥ 3.4e38)지만 double 로는 멀쩡하고, 이력 `h1` 이 작으면(1e-15) 곱 `c1 × h1` 도 작아서(7e24) 통과한다. 실제 계산에서는 `+Inf × 1e-15 = +Inf` 라 보정값이 `-Inf` 가 되어 프레임이 실패하고, 제자리 경로는 이미 이력 평면을 쓰고 있어서(되돌릴 수 없어서) 실패 분기가 이력을 비운다.

재현(Codex, `evidence/10`): 3×3, tier 3, `{"tier":"3","alpha1":0.1,"tau1":1,"alpha2":0,"tau2":1,"nlcscBeta":1}`, 전 화소 `1e-15f` 프레임 성공 → 전 화소 `1e25f` 프레임. 이 프레임에서 `exposureWeight ≈ 1.5e20`, `signalDependence ≈ 3e20`, `a1 = 0.1 × 1.5e20 × 3e20 ≈ 4.6e39 > FLT_MAX`.

왜 237b 의 시험이 놓쳤는가: 차등 시험의 프레임 크기 목록이 `1.0` 이상만이었다(작은 이력을 만드는 `1e-15` 급 프레임이 없었다). 그래서 "작은 이력 → 큰 프레임" 조합이 나오지 않았다. 이번에 크기 목록에 `1e-30 … 1e-5` 와 `1e20 … 1e28` 을 더했고, 같은 시험이 옛 판정에서 빨갛다(`evidence/11`: `huge_beta tier 3 2x5 frame 1: pixels differ`).

## 2. 수정 — 판정과 실행이 같은 식을 쓴다

`frame_cannot_leave_float_range` 를 다시 썼다. M(프레임의 최대 |값|)을 화소와(|평균| ≤ M 이므로) 평균에 대입하고, 핸들이 들고 있는 이력 상한(재귀식, 평면을 읽지 않음)을 `h1`, `h2` 에 대입하고, 계수는 크기로 넣어서 **tier 와 같은 식을 같은 순서의 float 로** 평가한다. 평가한 값 각각이 유한하고 `1e37`(FLT_MAX 의 1/34) 이하여야 제자리로 간다. 하나라도 아니면(NaN·Inf 포함) 느린 길이다.

| tier 코드의 식(실제 계산) | 판정이 평가하는 식 | 해당 tier |
|---|---|---|
| `exposureWeight = 1 + (mean / 32768) × 0.5` | `ew = 1 + (M / 32768) × 0.5` (|mean| ≤ M) | 2, 3 |
| `signalDependence = 1 + beta × (raw / 32768)` | `sd = 1 + |beta| × (M / 32768)` | 3 |
| `a1 = a1_base × exposureWeight × signalDependence` (float 곱 둘, 이 순서) | `a1w = |a1_base| × ew × sd` (같은 순서) | 1~3 (tier 1 은 ew = sd = 1) |
| `a2 = a2_base × exposureWeight × signalDependence` | `a2w = |a2_base| × ew × sd` | 1~3 |
| `a1 × h1[i]`, `a2 × h2[i]` | `t1 = a1w × H1`, `t2 = a2w × H2` | 1~3 |
| `corrected = raw − a1 × h1 − a2 × h2` | `corr = M + t1 + t2` | 1~3 |
| `localMean`: 이웃 9개의 float 누적합 | `sum9 = 9 × M` | 3 |
| `corrected = keep × corrected + local × localMean` | `blend = |keep| × corr + |local| × M` | 3 |
| `n1 = decay1 × h1 + raw`, `n2 = decay2 × h2 + raw` | `n1w = decay1 × H1 + M`, `n2w = decay2 × H2 + M` | 1~3 |

차이점 하나: 옛 판정은 double 이었고 중간 계수 `a1`, `a2`, `ew`, `sd` 를 검사하지 않았다. 새 판정은 전부 float 이고 `ew`, `sd`, `a1w`, `a2w`, `t1`, `t2`, `corr`, `n1w`, `n2w`(tier 3 은 `sum9`, `blend` 도) 각각을 검사한다. 판정이 표의 어느 식과 갈라지는지는 함수 위 주석에 같은 표를 두어서, tier 식을 고치는 사람이 판정도 고치게 했다.

수정이 지키지 못하는 것(남은 위험): 제자리 경로 안의 "실패하면 이력을 비우는" 분기(`ghost_correct.cpp`)는 판정이 정답이어야만 도달하지 않는다. 지금은 시험과 표본으로만 도달 불가를 보인다(§3).

## 3. 시험

| 시험 | 덮는 것 | 결과 |
|---|---|---|
| `GhostCoefficientOverflowIsAFailedFrameNotAnErasedHistory` | Codex 재현(3×3, tier 3) + 변형 6(크기 `40×30`, `37×29`, 첫 프레임 `1e-30`~`1e-5`, 둘째 `1e20`~`3e37`, `nlcscBeta` 0.1 / 1 / 1000, tier 2). 실패하면 픽셀·이력·통계가 비트 단위로 그대로 | 옛 판정에서 **빨강**, 새 판정 통과 |
| `GhostInPlaceVerdictAroundTheCoefficientOverflow` (훅 구성) | 계수 넘침 **직전/직후**: `nlcscBeta` 1e-3 ~ 1e6 을 1.25 배씩, 둘째 프레임 `1e18` ~ 3.4e38 을 3 배씩, 첫 프레임 `0`(이력 0) / `1e-30` / `1e-15` / `1` / `1e6`(비영 이력), tier 1·2·3. 같은 프레임을 정상 핸들과 느린 길 강제 핸들에 먹여 일치, 느린 길이 실패하는 프레임에서는 제자리 길을 타지 않음 | 122,760 프레임: 제자리 102,934, 느린 길 실패 15,356. 통과(옛 판정에서 빨강, `evidence/60`) |
| `GhostInPlaceRouteIsNeverTakenForAFrameTheScratchRouteFails` (237b 의 차등 시험, 확장) | 크기 목록에 작은 값과 `1e20~1e28` 추가, `nlcscBeta` 1 / 1000 구성 추가 | 3,780 프레임: 제자리 686, 실패 1,824. 통과(옛 판정에서 빨강) |
| `GhostNonFiniteFramesAreRefusedAndChangeNothing` | NaN, +Inf, −Inf 한 화소, 이력 0 / 비영, tier 1~3: `XPE_ERR_INVALID_INPUT`, 픽셀·이력·통계 불변 | 18 구성 통과 |
| `OomInjection.AGhostFrameOnTheScratchRouteWhoseAllocationFailsChangesNothing` | 느린 길을 강제하고 k = 1, 2, 3 번째 할당을 실패시킴(`next1`, `next2`, `backup`). tier 1~3 × 이력 0 / 비영 = 6 구성 | 각 k: `XPE_ERR_OUT_OF_MEMORY`, 픽셀·이력·통계 불변, 누수 없음(live 블록 수 동일). 첫 성공 호출의 할당 수 = 3, 그 결과가 주입 없는 기준과 같음 |

### 반증

| 손상 | 결과 | 증거 |
|---|---|---|
| 판정을 옛 식(double, 곱 `a1 × h1` 만)으로 되돌림 | 위 시험 3개 빨강(Codex 재현, 경계 훑기, 차등 시험) | `evidence/60` |
| 느린 길에서 할당 **전에** 통계(`lastFrameMean`)를 건드림 | OOM 시험 빨강(`statistics changed, failing allocation 1`) | `evidence/61` |

## 4. 237b 의 보존 결과가 유지되는가

| 항목 | 결과 | 증거 |
|---|---|---|
| 출력 다이제스트 32개 | 통과(수정 뒤 `OutputUnchanged237b.*` 통과: DLL 구성 7개, 훅 구성 11개) | `20`, `22` |
| 실영상 `bright01~06`, tier 3 | 237b 결과(`c3`)와 새 결과의 출력 다이제스트가 모든 실행에서 같음 | `42` |
| 메모리(정상 프레임은 제자리 경로에 남음) | 정상 상태 153.3 MiB, 최대 207.5 MiB, 237b 와 같음(번갈아 10쌍, 티어 1·티어 3) | `40`, `41` |
| 시간 | 티어 1: 최솟값 96.0 → 97.5 ms(비 1.016), 중앙값 99.7 → 100.3 ms(1.007). 같은 빌드끼리 대조(`c3` 대 `c3`)는 최솟값 비 0.977 이었으므로 이 차이는 잡음 범위다. 티어 3: 비 0.999 | `40`, `41`, `43` |
| 전체 스위트(세 수: 실행 / 총계 / DISABLED) | `xpe_preprocess_tests` 1028 / 1071 / 43(통과 1020, 건너뜀 8), `xpe_preprocess_oom_tests` 85 / 86 / 1, 종료 0 | `50`, `51` |

## 5. 게이트 (로컬, 커밋 전)

- **clang-tidy**: main 의 `tools/ci/check_clang_tidy.py` 와 기준선(`origin/main` `225a0e44`)을 임시 폴더에 꺼내 이 브랜치의 트리에 돌렸다(`modules/preprocess`). 최종: 종료 0, 새 지적 0, 29건(기준선 35), 기준선보다 줄어든 항목 6건(`ghost_correct.cpp` 의 옛 시그니처 항목, 기준선 갱신 대상, 리더 소유 `tools/ci`). `evidence/72`.
  - 237b 의 코드에서는 새 지적 6건(`bugprone-easily-swappable-parameters`: 함수 `frame_cannot_leave_float_range`, `ghost_tier1`~`3` 의 인접 float 매개변수)이 있었다. `95b41aa8` 이 계수를 `TierCoeffs` 로 묶어 정리했다(산술은 그대로, 다이제스트 시험 통과). `evidence/../QA-A-237c/evidence/10`.
  - 이 브랜치는 main 보다 307 커밋 뒤라 게이트는 아직 이 트리에 없다. main 의 파일로 돌린 것이고, 병합 뒤 결과가 같은지는 병합 쪽에서 확인된다. `modules/common` 은 이번에 바뀌지 않아 돌리지 않았다.
- **doxygen**: 1.12.0(공식 릴리스 zip, 이 워크트리에는 없어서 내려받음, SHA-256 기록: `evidence/73`)을 CI 와 같은 명령(`docs/help/doxygen` 에서 `doxygen Doxyfile`, `WARN_AS_ERROR=FAIL_ON_WARNINGS`)으로 돌렸다. 종료 0, 경고 0(`evidence/70`). `*internal*` 헤더는 `EXCLUDE_PATTERNS` 로 대상 밖이다. **양성 대조군**: 틀린 `@param nope` 가 있는 헤더에서 같은 바이너리가 종료 1 과 `argument 'nope' of command @param is not found` 오류를 냈다(`evidence/71`).

## 6. 이 작업이 보지 않은 것 (Gaps) / 잔여 위험

- 판정의 정당성은 **시험과 표본**(경계 훑기 122,760프레임, 차등 3,780프레임)으로만 보였다. 형식적 증명은 아니다. 판정이 한 번 틀리면 제자리 경로가 실패했을 때 이력을 비우는 구조는 그대로다(되돌릴 사본이 없다). 판정이 정답인 한 도달하지 않는다.
- 경계 훑기는 계수 `alpha1 0.1`, `tau1 1`, `alpha2 0.01`, `tau2 20` 한 쌍과 `nlcscBeta` 변화로 했다. `alpha`·`tau` 를 같이 훑지는 않았다(`alpha` 는 안정성 조건 `S < 1` 로 상한이 있다). 차등 시험은 계수 4종으로 했다.
- tier 2 에서 계수 넘침은 `ew` 가 `1 + 0.5 × M / 32768` 이라 `M` 이 FLT_MAX 근처가 아니면 일어나지 않는다. tier 2 의 변형은 1건뿐이다.
- 느린 길의 일시 최대(계산 297.3 MiB)는 **여전히 측정하지 않았다**. 이 카드는 판정의 정확성이 대상이었고, 측정은 QA-A-237c M1 이다.
- clang-tidy 는 `modules/preprocess` 만 돌렸다. 전체 CI 를 돌리지 않았다. 로컬 MSVC 한 구성.
- 이 브랜치가 main 보다 뒤라 게이트·기준선 파일은 main 의 것을 임시로 썼다.

## 증거

`evidence/`: `10`, `11`(옛 판정에서 빨강), `20`~`22`(수정 뒤 통과, 경계 훑기 포함), `30`(OOM 주입), `40`~`43`(전/후 번갈아, 같은 빌드 대조), `50`, `51`(전체 스위트), `60`, `61`(반증), `70`(doxygen 실행), `71`(doxygen 양성 대조군), `72`(clang-tidy 게이트), `73`(doxygen 바이너리 출처).

## Card Cross-Check

| milestone | card |
|---|---|
| 결함 재현·수정·시험 | QA-A-237d |
