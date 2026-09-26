# QA-B-65 게이트 보고서 — 이름을 동작에 맞춘다, 그리고 D1

**카드**: QA-B-65 (#164 #142) · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋 1건**: `6d74a91` · **증거**: `.moai/reports/lane-post/QA-B-65/`

---

## 1. 주장 (Claim)

| # | 항목 | 결론 |
|---|---|---|
| 1 | 키 이름 | `sensitivity` → **`confidence_strictness`**. 이유는 §2 |
| 2 | 산술식 | **한 글자도 바꾸지 않았다.** `0.7 + 0.3 * 키` 그대로 |
| 3 | 동작 | **불변.** B-63 방향 단언이 그대로 참이고, 스윕 결과가 개명 전과 동일 |
| 4 | 옛 이름 | **조용히 받지 않는다.** 미지 키 경고가 이름으로 말하고, 결과는 기본값 |
| 5 | #142 D1 | **이미 해소돼 있었다** — 이로써 #142 다섯 건이 모두 정리 |

---

## 2. 이름을 `confidence_strictness` 로 정한 이유

### 2.1 이 키가 실제로 하는 두 가지

```cpp
// (1) collimation_detect.cpp:134 — 올리면 각도 탐색이 촘촘해진다 (step 3 → 1)
int thetaStep = std::max(1, static_cast<int>(2.0f * (1.0f - 키) + 1.0f));

// (2) collimation_detect.cpp:181 — 올리면 요구 신뢰도가 오른다 (0.70 → 1.00)
float confidenceThreshold = 0.7f + 0.3f * 키;
```

**둘은 반대로 당긴다.** (1)은 더 찾게 하고 (2)는 더 거절하게 한다.
**B-63 이 검출 문턱에서 어느 쪽이 결정하는지 쟀고, (2)가 이겼다.**

### 2.2 기각한 후보 — `confidence_threshold` 계열

자연스러워 보이지만 **두 번째 거짓말을 만든다.** 값은 임계 자체가 아니라 임계로
**보간되는 계수**다 — `0.5` 를 주면 실제 임계는 **0.85** 다. 방향은 맞히지만
크기에서 틀린다. 옛 이름의 실패(방향이 반대)를 크기 실패로 바꾸는 것뿐이다.

### 2.3 고른 것

**`confidence_strictness`** — 잰 방향(**올리면 엄격해진다**)을 말하면서 **임계 값 자체라고
주장하지 않는다.** 범위 `[0, 1]` 이 "얼마나 엄격한가" 로 자연스럽게 읽힌다.
(1)의 theta 효과는 이름이 담지 못하므로 **주석에 적었다** — 반대로 당기지만 문턱에서는
결정하지 못한다는 측정과 함께.

### 2.4 개명 범위

| 파일 | 바뀐 것 |
|---|---|
| `internal.h` | `XPE_COL_DEFAULT_SENSITIVITY` → `XPE_COL_DEFAULT_CONF_STRICTNESS`, 선언 인자명, 개명 사유 문서 |
| `enhance_advanced_helpers.cpp` | 읽는 키 문자열, 인자명, 기본값 |
| `collimation_detect.cpp` | 지역 변수, **알려진 키 배열**, 주석 2곳 |
| 테스트 2파일 | JSON 키, 주석, 테스트 이름 |

---

## 3. 증거 (Evidence)

### 3.1 GREEN — 8/8, BUILD=0 (`_green.log`)

```
===BUILD=0===
[  INFO ] strictness sweep: 1 of 12 edge strengths split 0.0 from 1.0
[       OK ] ConfigValueDependency.CollimationConfidenceStrictnessRejectsMoreAsItRises
[  INFO ] old name: [48,47,207,207]  default: [48,47,207,207]  new name @1.0: [48,47,207,207]
[       OK ] ConfigValueDependency.OldSensitivityNameIsReportedAsUnknownAndHasNoEffect
8 tests ran. ===EXIT=0===
```

### 3.2 동작 불변 — 스윕 결과가 개명 전과 같다

| 항목 | QA-B-63 (개명 전) | QA-B-65 (개명 후) |
|---|---|---|
| split 수 | **1 of 12** | **1 of 12** |
| split 지점 | fg=21.5 | fg=21.5 |
| 방향 | 낮은 값 검출 / 높은 값 폴백 | 동일 |

**B-63 이 걸어 둔 방향 단언이 그대로 참이다.** 카드가 요구한 "화소값이 하나도 안 바뀌어야
한다" 의 증거가 이것이다 — 단언 내용은 그대로 두고 키 이름만 바꿨으므로, 통과한다는 것이
곧 동작이 안 바뀌었다는 뜻이다.

### 3.3 옛 이름의 경로 — 가정하지 않고 확인했다

카드가 "**그 경로가 실제로 그렇게 동작하는지 확인**" 하라고 했다. 세 가지를 단언했다:

1. **경고가 이름으로 말한다** — 알림에 `sensitivity` 가 등장한다(알려진 키 목록에서
   빠졌으므로 B-60/B-61 경로로 떨어진다).
2. **효과가 없다** — 결과가 요청한 `1.0` 이 아니라 **기본값**과 같다.
3. **우연히 통과하지 않는다** — 기본값과 새 이름 `1.0` 을 **둘 다** 찍어 비교한다.

유예 기간은 두지 않았다 — 리더의 호출자 감사에서 C#·XAML·배포 JSON 어디에도 이 키를
설정하는 곳이 없었다.

### 3.4 반증 — "이름만 바꿨다" 가 실제로 감시되는가 (`_falsify_arith.log`)

산술식을 `0.7f - 0.3f * 키` 로 뒤집고 재실행:

```
===BUILD=0===
[  FAILED  ] ConfigValueDependency.CollimationConfidenceStrictnessRejectsMoreAsItRises
[       OK ] 나머지 7건 (옛 이름 케이스 포함)
1 FAILED TEST
```

**방향 단언만 실패한다.** 동작을 건드리면 잡히고, 이름만 바꾸면 안 잡힌다 — 그 구분이
실제로 일어나고 있다. 원복했다.

---

## 4. #142 D1 — 이미 해소돼 있었다

카드가 "**D1 이 아직 있는지 확인이 먼저**" 라고 했다. 있었던 것은 테스트다:

`modules/enhance_basic/tests/test_empty_image_contract.cpp` 가 **7함수 전부**
(`xpe_log_transform` · `xpe_log_inverse` · `xpe_noise_reduce` · `xpe_noise_estimate_sigma` ·
`xpe_contrast_enhance` · `xpe_edge_enhance` · `xpe_calc_exposure_index`)에 대해
`ZeroWidth` · `ZeroHeight` · `NullData` 를 **`XPE_ERR_INVALID_INPUT` 으로 통일**해 단언하고,
`ValidImageStillAccepted` 로 **양성 대조**까지 둔다. 파일 머리말에 QA-B-39 가 발견한 원래
불일치(세 함수는 OK, 둘은 INVALID_INPUT, 로그 변환 둘은 0회 루프로 OK)가 기록돼 있다.

**이로써 #142 다섯 건이 모두 정리됐다.**

| # | 상태 | 처리 |
|---|---|---|
| D1 빈 이미지 계약 | **해소** | `test_empty_image_contract.cpp` (QA-B-40 계열) |
| D2 include guard 중복 | **해소** | 가드 분리 |
| D3 미등록 중복 소스 | **삭제** | `723d076` |
| D4 fallback 무효과 | **단언됨** | QA-B-64 (고치지 않음, 결정 대기) |
| D5 cross-DLL free | **전제 단언됨** | QA-B-64 |

---

## 5. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 개명 범위 | 소스 3파일 + 테스트 2파일 | `6d74a91` |
| 산술식 변경 | **0** | `collimation_detect.cpp:186` |
| 스윕 결과 (개명 전) | 1 of 12, fg=21.5 | QA-B-63 `_green_adv.log` |
| 스윕 결과 (개명 후) | **1 of 12, fg=21.5** | `_green.log` |
| 옛 이름 결과 | `[48,47,207,207]` = 기본값 | `_green.log` |
| 반증 (산술식 뒤집기) | BUILD=0, **방향 단언만 FAILED** | `_falsify_arith.log` |
| 이전 ctest | 532 / 224 / 177 | QA-B-64 `_verify.log` |
| 현재 ctest | **533 / 224 / 177** | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 6. 미검증 (Gaps)

- **화소값 불변을 화소 단위로 재지 않았다.** collimation 은 정수 ROI 를 반환하므로 비교
  대상이 화소가 아니라 좌표다. **다른 함수의 화소는 이 개명이 닿지 않는다**(키가
  collimation 전용) — 그 점은 확인했지만, 전체 파이프라인 출력을 개명 전후로 비교하는
  측정은 하지 않았다. 근거는 ctest 전량 통과다.
- **theta 효과와 임계 효과의 분리 측정은 여전히 안 했다** — 카드가 범위 밖으로 명시했다.
  주석은 "문턱에서는 임계가 결정한다" 고 적었고 그것은 B-63 의 측정이지만, **theta 단독
  효과는 재지 않았다.**
- **옛 이름 경고의 중복 억제 상호작용은 재지 않았다.** B-61 의 설정당 한 번 규칙이
  적용되므로, 같은 오타를 반복하는 호출자는 한 번만 듣는다 — 의도이지만 이 카드에서
  확인하지 않았다.
- **문서는 손대지 않았다** — 리더가 처리한다(설계 문서 2개, `sdd_adv.md`).
- **D1 의 계약이 `enhance_basic` 밖에서도 일관한지는 보지 않았다.** `#142` 는 그 모듈만
  가리켰고 다른 모듈의 빈 이미지 처리는 이 카드 범위가 아니다.

---

## 7. 잔여 위험 (Residual-risk)

- **이름이 고쳐졌다고 동작이 안전해진 것은 아니다.** 값을 올리면 여전히 덜 검출하고,
  **경계에서만 그렇다** — 대개는 아무 일도 안 일어나고 어려운 영상에서만 검출을 잃는다.
  #164 에 남는 위험으로 기록돼 있다.
- **폴백은 여전히 `XPE_OK` 를 반환한다**(REQ-ADV-041). 검출을 잃어도 호출자는 성공을
  받는다. 이 카드 범위 밖이다.
- **개명은 ABI 가 아니라 config 문자열 계약을 바꾼다.** 컴파일러가 잡아 주지 않으므로,
  잡는 것은 런타임 경고뿐이다 — 그 경고를 읽지 않는 호출자에게는 조용하다.
- **`confidence_strictness` 도 완전한 이름은 아니다.** theta 효과를 담지 못하고, 주석에
  의존한다. 한 이름이 서로 반대로 당기는 두 효과를 다 말할 수는 없다는 것이 이 키의
  구조적 성질이다 — 그것을 쪼개는 것은 출력이 바뀌는 변경이라 하지 않았다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` / `_verify.log` | 세 프리셋, 533 / 224 / 177, 경고 0 |
| `_b65.bat` / `_green.log` | 8건 BUILD=0, 스윕 1 of 12, 옛 이름 케이스 |
| `_falsify_arith.log` | 산술식 뒤집기 → **방향 단언만 FAILED** |
