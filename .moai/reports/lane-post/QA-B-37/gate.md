# QA-B-37 게이트 보고서 — 검증기 미시험 분기 3건 시험 + 반증

**카드**: QA-B-37 (#139 마감)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-37/`
**커밋**: `42716e6` — **테스트만. 제품 코드 변경 0.**

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | `(0002,0003)` 불일치가 `valid=false` 이고 **사유가 그 태그를 지목**한다 | PASS |
| C2 | 형식이 깨진 TS UID(`"1.2.abc"`)가 `valid=false` 이고 `(0002,0010)` 을 지목한다 | PASS |
| C3 | 비-DX(CT) SOP Class 왕복이 `valid=true` 다 — meta 규칙은 **일관성 규칙**이다 | PASS |
| C4 | **반증 1회** — `(0002,0003)` 비교를 끄면 **정확히 1건만** 실패한다 | PASS |
| C5 | ci-dicom 138 → **141**(+3), ci-post 451 / ci-ai 198 무회귀, 경고 0 | PASS |
| C6 | 제품 코드를 고치지 않았다 — B-36 구현이 이미 옳았다 | PASS |

---

## 2. 증거 (Evidence)

### 2.1 세 케이스 (C1~C3) — `_green.log`

```
100% tests passed, 0 tests failed out of 16
```

| 케이스 | 픽스처 | 단언 |
|---|---|---|
| `ValidateMetaSopInstanceMismatch_ReportsInconsistency` | meta 의 `MediaStorageSOPInstanceUID` 만 다른 UID 로 교체 (`EWM_dontUpdateMeta`) | `valid=false` **+ 오류 하나가 태그 `0002,0003`** |
| `ValidateMetaMalformedTransferSyntax_ReportsInvalidUid` | meta 의 `TransferSyntaxUID` = `"1.2.abc"` | `valid=false` **+ 태그 `0002,0010`** |
| `ValidateNonDxSopClass_RoundTripsAsConformant` | 데이터셋 `SOPClassUID` 를 CT(`1.2.840.10008.5.1.4.1.1.2`)로 바꾸고 **기본 저장 모드**로 meta 를 함께 갱신 | `valid=true` |

**태그까지 단언한 이유**: `valid=false` 만 보면 다른 이유로 실패해도 통과한다.
예컨대 UID 를 바꾸는 과정에서 Type 1 태그가 깨졌다면 "필수 태그 누락" 으로도
`valid=false` 가 되고, 테스트는 통과하면서 **표적을 잃는다.**
태그를 지목하게 하면 그 경우가 걸러진다.

**세 번째 케이스의 값**: meta 검사가 `일관성` 이 아니라 `DX 전용` 을 강제하고 있었다면
이 케이스가 실패한다. 어떤 요구사항도 DX 전용을 말하지 않으므로, 그런 강제는 조용한
결함이 된다 — 통과했으므로 그 결함은 없다.

### 2.2 반증 (C4) — `_falsify.log`, 이 카드의 실질

세 케이스가 통과한다는 사실만으로는 **그 케이스가 의도한 코드를 시험한다**는 것이
증명되지 않는다. 그래서 구현을 한 줄 지우고 다시 돌렸다:

```cpp
// DicomValidator.cpp — kPairs 에서 이 줄만 제거
- { DCM_MediaStorageSOPInstanceUID, DCM_SOPInstanceUID, "0002,0003", "SOPInstanceUID" },
```

```
14/16 DicomValidatorTest.ValidateMetaSopInstanceMismatch_ReportsInconsistency ***Failed
94% tests passed, 1 tests failed out of 16
```

**16건 중 정확히 1건만 실패했고, 그것이 이 카드가 새로 만든 케이스다.**
- 새 케이스가 그 비교를 실제로 지난다는 증거이고,
- 동시에 **다른 15건은 그 비교에 의존하지 않는다**는 증거다 — 곁가지로 통과시키고
  있던 것이 없다는 뜻이다.

원복 후 재빌드해 `git status` 가 제품 코드에 대해 깨끗함을 확인했다.

### 2.3 재실측 (C5) — `_verify.log`, 필터 없음

```
===CI_POST===   100% tests passed, 0 tests failed out of 451   ===POST_EXIT=0===
===CI_AI===     100% tests passed, 0 tests failed out of 198   ===AI_EXIT=0===
===CI_DICOM===  100% tests passed, 0 tests failed out of 141   ===DICOM_EXIT=0===
```

| | B-36 | B-37 | 차 |
|---|---:|---:|---:|
| ci-dicom | 138 | **141** | +3 |
| ci-post / ci-ai | 451 / 198 | 451 / 198 | 0 |

빌드 경고 0 (`XPE_WARNINGS_AS_ERRORS=ON`).

### 2.4 첫 재실측을 폐기한 이유 (자기 정정)

반증 직후 처음 돌린 재실측은 ci-dicom **141 중 1건 실패**로 나왔다.
원인은 코드가 아니라 **스크립트**다 — `_verify.bat` 이 ci-post·ci-ai 는 빌드하면서
ci-dicom 은 `ctest` 만 했기 때문에, 소스는 원복됐지만 **반증용 바이너리가 그대로
남아 있었다.** 스크립트에 `cmake --build build\ci-dicom` 을 추가하고 다시 측정한 것이
§2.3 이다.

이 숫자를 "간헐적 실패" 로 넘기지 않고 원인을 특정했다는 것을 기록한다 —
반증 실험은 바이너리를 오염시키므로, 뒤따르는 측정은 **반드시 재빌드를 포함해야 한다.**

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 이전 ci-dicom | 138 | QA-B-36 `_verify.log` |
| 케이스 추가 후 | 141 | `_verify.log` (이번 실행, 재빌드 포함) |
| 반증 시 | 16건 중 1건 실패 | `_falsify.log` (이번 실행) |
| ci-post / ci-ai | 451 / 198 | `_verify.log` (이번 실행) |
| 제품 코드 변경 | 0 | `git diff --stat` = `1 file changed, 89 insertions(+)` (테스트 파일 하나) |

---

## 4. 미검증 (Gaps)

- **`(0002,0002)` 축은 이번에 반증하지 않았다.** 반증은 Instance 쌍 하나만 껐다.
  Class 축은 B-36 이 케이스로 시험했으나, "그 케이스가 그 비교를 지난다" 는 반증은
  두 카드 어디에도 없다.
- **`MediaStorage*` 부재(빈 값) 분기는 시험하지 않았다.** 구현에는 "meta 에 해당
  UID 가 없음" 경로가 따로 있는데, 이번 세 케이스는 모두 **값이 있고 다른** 경우다.
- **TS UID 부재 분기도 시험하지 않았다.** 형식 위반만 봤다.
- **CT 왕복은 검증기만 통과시켰다.** 그 파일을 reader 가 읽거나 SCU 가 전송하는
  경로는 보지 않았다 — 카드 범위가 검증기다.
- **커버리지를 재지 않았다.** 로컬 도구 부재는 그대로이고, 임계 0.80 대비 이동은
  다음 dispatch 로만 확인된다.

---

## 5. 잔여 위험 (Residual-risk)

- **세 픽스처가 모두 `s_conformantDcm` 파생이다.** 그 파일이 바뀌면 세 케이스가 함께
  흔들린다. 한 가지만 바꿔 파생시키는 방식이라 표적은 좁지만, 뿌리는 하나다.
- **`EWM_dontUpdateMeta` 의존이 B-36 에서 이어진다.** DCMTK 가 저장 시 meta 를 강제
  재생성하도록 바뀌면 불일치 픽스처를 만들 수 없고, 두 케이스는 **통과하면서 표적을
  잃는다.** 그때 신호는 없다 — 이것이 이 계열 테스트의 구조적 약점이다.
- **`"1.2.abc"` 가 계속 UID 로 거부된다는 보장은 `isValidUID` 의 정규식에 있다.**
  그 규칙이 느슨해지면 케이스가 조용히 무력해진다.
- 커밋은 push 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_dicom.bat` | ci-dicom 빌드 + `-R DicomValidator` |
| `_green.log` | 세 케이스 추가 후 16/16 |
| `_falsify.log` | `(0002,0003)` 비교 제거 시 16건 중 1건 실패 |
| `_verify.bat` / `_verify.log` | 재실측 451 / 198 / 141 (ci-dicom 재빌드 포함) |
