# QA-B-38 게이트 보고서 — 검증기 meta 부재 분기 3건 + `(0002,0002)` 반증

**카드**: QA-B-38 (#139 계열 종료)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-38/`
**커밋**: `dbf6ad3` — **테스트만. 제품 코드 변경 0.**

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | `MediaStorageSOPClassUID` 빈 값 → `valid=false` + 태그 `0002,0002` | PASS |
| C2 | `MediaStorageSOPInstanceUID` 빈 값 → `valid=false` + 태그 `0002,0003` | PASS |
| C3 | `TransferSyntaxUID` **원소 삭제** → `valid=false` + 태그 `0002,0010` | PASS |
| C4 | 반증: `kPairs` 의 SOPClass 쌍 제거 시 **19건 중 정확히 2건** 실패 | PASS |
| C5 | **반증 뒤 재빌드 후** 재실측했다 (B-37 §2.4 규약) | PASS |
| C6 | ci-dicom 141 → **144**(+3), ci-post 451 / ci-ai 198 무회귀, 경고 0 | PASS |

---

## 2. 증거 (Evidence)

### 2.1 왜 이 세 분기가 남아 있었나

B-36 이 구현한 meta 검사에는 조건이 두 갈래씩 있다 — **값이 없다**와 **값이 다르다**.
B-36·B-37 이 만든 케이스는 전부 **후자**였다(다른 UID, 다른 SOP Class, 깨진 형식).
전자로 들어가는 입력은 한 번도 없었고, B-37 이 그것을 Gaps 에 적었다.
이 카드는 그 목록을 케이스로 바꾼다.

### 2.2 세 케이스 (C1~C3) — `_green.log`

```
100% tests passed, 0 tests failed out of 19
```

| 케이스 | 픽스처 편집 | 단언 |
|---|---|---|
| `ValidateMetaSopClassEmpty_ReportsMissingMetaSopClass` | `putAndInsertString(DCM_MediaStorageSOPClassUID, "")` | `valid=false` + 태그 `0002,0002` |
| `ValidateMetaSopInstanceEmpty_ReportsMissingMetaSopInstance` | `putAndInsertString(DCM_MediaStorageSOPInstanceUID, "")` | `valid=false` + 태그 `0002,0003` |
| `ValidateMetaTransferSyntaxAbsent_ReportsMissingTransferSyntax` | `findAndDeleteElement(DCM_TransferSyntaxUID)` | `valid=false` + 태그 `0002,0010` |

세 번째만 **원소를 지운다.** 빈 값이 아니라 부재를 쓴 이유는 이것이
**`card() != 0` 이면서 인코딩을 말하지 않는 유일한 경우**이기 때문이다 —
meta 그룹은 존재하므로 B-36 의 첫 검사(`card()==0`)에 걸리지 않고, TS 검사만 남는다.
빈 값으로 했다면 두 검사 중 어느 쪽이 잡았는지 구분되지 않는다.

세 케이스가 픽스처를 만드는 방식이 같아 헬퍼 셋으로 묶었다
(`writeWithMetaEdit` / `validateReport` / `hasErrorTagged`).
**케이스마다 달라지는 것은 람다 한 줄뿐**이라, 무엇이 시험 대상인지가 한눈에 보인다.

### 2.3 반증 (C4) — `_falsify.log`

B-37 의 Gaps 가 지목한 미반증 축이 `(0002,0002)` 였다. 이번에 그 축을 껐다:

```cpp
// DicomValidator.cpp — kPairs 에서 이 줄만 제거
- { DCM_MediaStorageSOPClassUID,    DCM_SOPClassUID,    "0002,0002", "SOPClassUID"    },
```

```
12/19 DicomValidatorTest.ValidateMetaSopClassMismatch_ReportsInconsistency ***Failed
17/19 DicomValidatorTest.ValidateMetaSopClassEmpty_ReportsMissingMetaSopClass ***Failed
89% tests passed, 2 tests failed out of 19
```

**19건 중 정확히 2건**이고, 그 둘은 **예측한 조합과 일치한다**:
- `ValidateMetaSopClassMismatch` (B-36 이 만든 "다르다" 축)
- `ValidateMetaSopClassEmpty` (이번 카드의 "없다" 축)

한 줄이 두 케이스를 모두 지탱한다는 뜻이고, 나머지 17건은 그 줄에 의존하지 않는다.
**건수를 미리 말하고 맞춘 것이 이 반증의 값이다** — 실패 건수가 1이나 3이었다면
케이스가 의도한 것과 다른 코드를 지나고 있다는 신호였다.

### 2.4 반증 뒤 재빌드 (C5)

B-37 §2.4 에서 스스로 만든 규약을 이번에 처음부터 적용했다 — `_verify.bat` 이
ci-dicom 을 `cmake --build` 한 뒤 `ctest` 한다. B-37 에서 이 순서를 빠뜨려
반증용 바이너리로 측정하는 사고가 있었고, 같은 실수를 반복하지 않았다.

```
===DICOM_BUILD=0===
100% tests passed, 0 tests failed out of 144
```

### 2.5 재실측 (C6) — `_verify.log`, 필터 없음

```
===CI_POST===   100% tests passed, 0 tests failed out of 451   ===POST_EXIT=0===
===CI_AI===     100% tests passed, 0 tests failed out of 198   ===AI_EXIT=0===
===CI_DICOM===  100% tests passed, 0 tests failed out of 144   ===DICOM_EXIT=0===
```

| | B-37 | B-38 | 차 |
|---|---:|---:|---:|
| ci-dicom | 141 | **144** | +3 |
| ci-post / ci-ai | 451 / 198 | 451 / 198 | 0 |

빌드 경고 0 (`XPE_WARNINGS_AS_ERRORS=ON`).

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 이전 ci-dicom | 141 | QA-B-37 `_verify.log` |
| 케이스 추가 후 | 144 | `_verify.log` (이번 실행, 재빌드 포함) |
| 반증 시 | 19건 중 2건 실패 | `_falsify.log` (이번 실행) |
| ci-post / ci-ai | 451 / 198 | `_verify.log` (이번 실행) |
| 제품 코드 변경 | 0 | `git diff --stat` = `1 file changed, 71 insertions(+)` |

---

## 4. 미검증 (Gaps)

- **`card()==0` 분기는 반증하지 않았다.** B-36 이 케이스로 시험했고 이 카드가
  세 축 중 두 축을 반증했지만, "meta 그룹 부재" 검사 자체를 껐을 때 어느 케이스가
  실패하는지는 확인하지 않았다. #139 계열에서 마지막으로 남는 미반증 축이다.
- **TS UID 형식 검사(`isValidUID`)도 반증하지 않았다.** B-37 이 케이스로 시험했다.
- **빈 값과 부재를 구현이 같은 경로로 처리한다는 것은 코드 판독이다.**
  `metaString()` 이 둘 다 빈 문자열을 돌려주므로 같은 분기로 간다 — 이 카드는
  `SOPClass`/`SOPInstance` 는 빈 값으로, `TransferSyntax` 는 부재로 시험했고,
  **각각의 반대 형태는 시험하지 않았다.**
- **커버리지를 재지 않았다.** 로컬 도구 부재는 그대로다. 이번 세 케이스가 새로
  덮는 계측 줄 수는 다음 dispatch 로만 확인된다.
- **DCMTK 가 TS UID 없는 파일을 저장해 준다는 것은 이번 관측이다.** 규격상 Part 10
  파일은 TS UID 를 요구하므로, 이 픽스처는 규격 위반 파일을 일부러 만드는 것이다.
  DCMTK 가 이후 이를 거부하면 `ASSERT_TRUE(saveFile(...).good())` 에서 멈춘다 —
  조용히 무력해지지 않고 실패로 드러난다.

---

## 5. 잔여 위험 (Residual-risk)

- **헬퍼로 묶은 대가가 있다.** `writeWithMetaEdit` 안의 `EXPECT_*` 는 실패해도
  호출부를 멈추지 않으므로, 픽스처 생성이 깨지면 세 케이스가 **엉뚱한 파일을 검증하며
  통과할** 수 있다. 그때 신호는 헬퍼 안의 실패 메시지뿐이다.
- **`EWM_dontUpdateMeta` 의존은 B-36 부터 이어지는 구조적 약점이다.** DCMTK 가 저장 시
  meta 를 강제 재생성하도록 바뀌면 부재·불일치 픽스처를 만들 수 없고, 관련 케이스는
  **통과하면서 표적을 잃는다.**
- **`s_conformantDcm` 하나에서 다섯 케이스가 파생된다**(B-36 2 + B-37 2 + B-38 3 중 일부).
  뿌리가 바뀌면 함께 흔들린다.
- 커밋은 push 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_dicom.bat` | ci-dicom 빌드 + `-R DicomValidator` |
| `_green.log` | 세 케이스 추가 후 19/19 |
| `_falsify.log` | `(0002,0002)` 비교 제거 시 19건 중 **2건** 실패 |
| `_verify.bat` / `_verify.log` | 재실측 451 / 198 / 144 (ci-dicom 재빌드 포함) |
