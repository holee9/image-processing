# QA-B-28 게이트 보고서 — 죽은 코드 `DicomValidator::checkConformance` 제거

**카드**: QA-B-28 (#120, B-27 후속)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-28/`
**선행 병합**: `git merge origin/main` → `3f9b1fc`

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | 제거 전 "호출자 0" 전제를 레인이 직접 재확인했다 | PASS |
| C2 | 정의·선언만 제거했고 `ValidationResult`/`buildReport` 는 보존했다 | PASS |
| C3 | ci-dicom 118/118 유지, 경고 0 | PASS |
| C4 | `docs/` 트레이스 grep 결과: **0건** | PASS |
| C5 | **B-27 의 "36줄" 은 소스 구간이었다 — 계측 미커버는 31줄이다** | PASS (자기 정정) |

---

## 2. 증거 (Evidence)

### 2.1 제거 전 전제 재확인 (C1)

삭제는 되돌리기 어려운 쪽이라 leader 의 grep 결과를 그대로 믿지 않고 다시 쟀다.

```
$ grep -rn "checkConformance" --include=*.cpp --include=*.h --include=*.hpp \
       --include=*.cs --include=*.cmake --include=CMakeLists.txt .
./modules/dicom/src/DicomValidator.cpp:173:DicomValidator::ValidationResult DicomValidator::checkConformance(...)
./modules/dicom/src/DicomValidator.h:39:    static ValidationResult checkConformance(const std::string& filePath);
```
**정의 1 + 선언 1. 호출자 0.** `DicomValidator` 는 `XPE_API` 가 없는 내부 클래스라
DLL 경계 밖에서도 부를 수 없다(B-27 §2.7).

### 2.2 `docs/` 트레이스 (C4)

```
$ grep -rn "checkConformance" docs/ .moai/
.moai/reports/lane-post/QA-B-27/gate.md:19   ← 내 B-27 보고서
.moai/reports/lane-post/QA-B-27/gate.md:82
.moai/reports/lane-post/QA-B-27/gate.md:124-125
.moai/reports/lane-post/QA-B-27/gate.md:169
```
**SDD/SRS/api-spec 어디에도 없다.** 나오는 것은 전부 내가 어제 쓴 B-27 보고서 자신이다.
문서 정리 대상 0건 — leader 가 손댈 것이 없다.

### 2.3 변경 (C2)

- `modules/dicom/src/DicomValidator.cpp` — 정의 37행(빈 줄 포함) 제거
- `modules/dicom/src/DicomValidator.h:39` — 선언 1행 제거

**함께 지우지 않은 것:**
- `ValidationResult` (헤더 33행) — `validate()` 가 45행에서 쓴다
- `buildReport` (헤더 40행) — `validate()` 가 64·88·109·158행에서 쓴다
- `s_requiredTags` — `validate()` 에도 남아 있다 (`grep -c` → 2)
  `/WX` 빌드에서 미사용 경고가 나지 않는 이유다.

카드 1항의 "그 함수만 쓰던 include/헬퍼" 는 **없었다.** 그 외 정리도 하지 않았다.

### 2.4 재실측 (C3)

```
===BUILD=0===
100% tests passed, 0 tests failed out of 118
===CTEST=0===
```
전체 ctest 기준 118/118 — B-27 과 동일. 빌드 경고 0(`XPE_WARNINGS_AS_ERRORS=ON`).

### 2.5 자기 정정 — 31줄이지 36줄이 아니다 (C5)

B-27 보고서와 완료 메시지에 "죽은 코드 **36줄**" 이라고 적었고, leader 가 카드에 그대로 옮겼다.
**틀렸다.** 36 은 소스 줄 번호 구간(173~208)의 길이이고, 커버리지가 계측한 줄 수가 아니다.
아티팩트를 다시 파싱했다:

```
DicomValidator.cpp  instrumented=149 covered=77
checkConformance span 173-208: instrumented=31 covered=0 uncovered=31
```

Cobertura 는 선언·닫는 중괄호·빈 줄을 계측하지 않는다. **분모에서 빠지는 것은 31줄이다.**

산술 갱신 (B-27 §2.4 의 표를 이 값으로 대체):

| | 전 | 제거 후(예상) |
|---|---:|---:|
| 총 계측 줄 | 1156 | **1125** |
| 커버 | 830 | 830 |
| line-rate | 0.7180 | **0.7378** |

B-27 이 낸 상한도 갱신된다: 도달 가능 ~62줄을 전부 덮으면 892/1125 = **0.793**
(B-27 은 0.772 로 적었다 — 분모가 31 줄었으므로 상한이 그만큼 올라간다).
**여전히 0.85 에 못 미친다.** 결론은 바뀌지 않지만 수치는 바로잡는다.

이 오류는 QA-B-25 에서 스스로 적어 둔 규칙("수치엔 범위를 붙인다")의 같은 계열이다 —
"36" 에 *소스 구간* 이라는 범위를 안 붙여 leader 가 *계측 줄 수* 로 읽었다.

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 제거 전 line-rate | 0.7180 | CI run `34447732961` 아티팩트 (B-27 §2.1) |
| 제거되는 계측 줄 | **31** (커버 0) | 같은 아티팩트를 173-208 범위로 재파싱 (§2.5) |
| 제거 후 line-rate | **0.7378 (산술 예상)** | 830/1125 — **측정값 아님**, §4 |
| ci-dicom 전체 ctest | 118 → 118 | `_after.log` |

---

## 4. 미검증 (Gaps)

- **제거 후 line-rate 0.7378 은 산술 예상이지 측정값이 아니다.** 로컬에 OpenCppCoverage 가
  없어 재측정할 수 없다(B-27 §4 와 같은 제약). 분모에서 31줄이 빠지고 커버 수는 그대로라는
  가정에 기댄다. 실제 값은 leader 의 CI dispatch 가 정본이다.
- **B-27 이 낸 다른 수치들도 같은 방식으로 재검산하지 않았다.** 이번에 바로잡은 것은
  `checkConformance` 한 항목뿐이다. §2.3 분류표의 "SCP 필요 ~95", "catch ~74" 등은
  줄 범위를 세어 얻은 값이라 같은 착오 가능성이 남아 있다 — 필요하면 별도로 재검산한다.
- **저장소 밖 소비자는 확인할 수 없다.** 다른 리포지터리나 향후 계획이 이 함수를
  기대하고 있었다면 이 제거로 드러날 것이다. 내부 클래스라 가능성은 낮다고 보지만
  실측한 것은 이 저장소뿐이다.
- **함수가 하던 일이 다른 곳에 필요한지 판단하지 않았다.** `checkConformance` 는
  `validate()` 와 겹치는 검사(필수 태그 확인)를 하는 별도 구현이었다. 의도적으로 남겨둔
  대안 구현이었을 가능성은 배제하지 못한다 — git 이력으로 추적하지 않았다.

---

## 5. 잔여 위험 (Residual-risk)

- **0.85 목표는 이 제거로도 닫히지 않는다.** 상한이 0.772 → 0.793 으로 올라갈 뿐이다.
  leader 가 사용자 판정 중이라고 알린 결정 2(REQ-P0-006 원문이 85% 를 `xpe_common` 에만
  요구한다는 확인)가 실제 해법일 가능성이 크다 — 이 레인은 그 판정을 기다린다.
- **제거는 되돌릴 수 있다.** 커밋 하나이고 함수는 자기완결적이었다. 되살릴 일이 생기면
  `git revert` 로 충분하다.
- **`ValidationResult`/`buildReport`/`s_requiredTags` 는 남았다.** 이들이 `validate()` 와
  공유되므로, 앞으로 `validate()` 를 손볼 때 "checkConformance 용" 이라는 오해가 없도록
  이 보고서가 근거로 남는다.
- 커밋은 origin/main 병합 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_dicom.bat` / `_after.log` | ci-dicom 빌드 + 전체 ctest 118/118 |
| (참조) `../QA-B-27/build/cov-dicom-ci/coverage.xml` | 31줄 재검산에 쓴 원본 Cobertura |
