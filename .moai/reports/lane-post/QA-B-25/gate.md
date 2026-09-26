# QA-B-25 게이트 보고서 — dicom coverage 0.696: Skip 사유 규명 + 되살리기

**카드**: QA-B-25 (#120 #124)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-25/`
**선행 병합**: `git merge origin/main` → `7275916`

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | **Skipped 는 24건이 아니라 12건이다** — 24는 한 잡 안 두 ctest 스텝의 중복 계수 | PASS (카드 전제 정정) |
| C2 | **CI 환경 문제가 아니다** — 로컬에서도 같은 12건이 스킵된다 | PASS (카드 전제 정정) |
| C3 | 12건 각각의 skip 조건·사유를 소스에서 확정했다 | PASS |
| C4 | 되살릴 수 있는 3건(reader 1 / validator 2)을 합성 픽스처로 복구했다 | PASS |
| C5 | network 4건은 복구 불가 — skip 유지하되 **거짓 메시지를 사실로 정정**했다 | PASS |
| C6 | DegradedMode 5건은 Lane A 소유이며 설계상 스킵이다 — 손대지 않았다 | PASS |
| C7 | 재실측 ci-dicom 전체 110/110, skip 12 → 9 | PASS |
| C8 | 프리셋·CI yml·매니페스트 미수정 | PASS |

---

## 2. 증거 (Evidence)

### 2.1 "24건" 은 12건의 중복 계수다 (C1)

CI 로그(`dispatch-34444576614.log`)에서 `coverage (coverage-dicom)` 잡만 추려 세었다.

```
$ grep "coverage (coverage-dicom)" LOG | grep -cE "\(Skipped\)"
24
$ grep "coverage (coverage-dicom)" LOG | grep "Run coverage"     | grep -cE "\(Skipped\)"
12
$ grep "coverage (coverage-dicom)" LOG | grep "Enforce coverage" | grep -cE "\(Skipped\)"
12
$ grep "coverage (coverage-dicom)" LOG | grep -E "\(Skipped\)" | sed 's/.*\t//' | sort -u | wc -l
12
```

**같은 잡에서 ctest 가 두 번 돈다** (`Run coverage (OpenCppCoverage over ctest)` 와
`Enforce coverage threshold (85%)`). 두 실행이 각각 12건을 스킵해 로그에 24줄이 남았다.
카드의 내역(DegradedMode 10 / Network 8 / Validator 4 / Reader 2)도 정확히 실제의 2배다
(5 / 4 / 2 / 1). **고유 스킵은 12건이다.**

### 2.2 CI 러너의 문제가 아니다 (C2)

카드는 "로컬 ci-dicom 47/47 통과와 다르다 — CI 러너에는 없는 무언가" 라고 적었다.
로컬에서 **전체** ci-dicom 을 돌려 확인했다 (`_base.log`):

```
59/110  DegradedMode.BP06_GsvgMissingReportsR0 ...........***Skipped
60/110  DegradedMode.BP07_CollimationMissingEnhanceAdvancedReportsR0 ***Skipped
61/110  DegradedMode.BP08_EiMissingEnhanceBasicReportsR0 .***Skipped
62/110  DegradedMode.BP09_DicomMissingReportsR0 ..........***Skipped
63/110  DegradedMode.BP10_DisplayMissingReportsR0 ........***Skipped
69/110  DicomReaderTest.UnsupportedTS_ReturnsUnsupportedFormat ***Skipped
96/110  DicomValidatorTest.ValidateMissingPatientID_ReportsError ***Skipped
101/110 DicomValidatorTest.ValidateBadUID_ReportsWarning .***Skipped
102/110 DicomNetworkTest.CStoreSuccess_ReturnsOK .........***Skipped
104/110 DicomNetworkTest.CFindResults_ReturnsJsonArray ...***Skipped
105/110 DicomNetworkTest.CFindEmpty_ReturnsEmptyArray ....***Skipped
107/110 DicomNetworkTest.CancelCStore_TerminatesOperation ***Skipped
100% tests passed, 0 tests failed out of 110
```

**로컬에서도 정확히 같은 12건이 스킵된다.** 앞 카드들의 "47/47" 은 내가 `-R "Dicom"` 으로
거른 부분집합이었고 전체 스위트가 아니었다 — 그 수치를 전체와 비교한 것이 전제 오류의 출처다.
스킵 조건은 전부 **소스 안에 있고 환경과 무관하다.**

### 2.3 12건 사유 표 (C3)

| # | 테스트 | 위치 | 조건 | 실제 사유 | 분류 |
|---|---|---|---|---|---|
| 1-5 | `DegradedMode.BP06~BP10_*` | `modules/common/tests/test_post_degraded_mode.cpp:46` | `XPE_DEGRADED_ABSENT_DLL` 미설정 | DLL 을 일부러 제거한 **스테이징 환경**에서만 의미가 있는 시나리오 스위트 | (c) 환경 — **Lane A 소유** |
| 6 | `DicomReaderTest.UnsupportedTS_ReturnsUnsupportedFormat` | `test_dicom_reader.cpp:143` | `!fs::exists(s_implicitLEDcm)` | 73행이 **경로만 대입**하고 파일을 쓰지 않는다 (`// TODO`) | (a) 픽스처 부재 |
| 7 | `DicomValidatorTest.ValidateMissingPatientID_ReportsError` | `test_dicom_validator.cpp:76` | `!fs::exists(s_missingTagDcm)` | 같음 — 경로만 대입 (`// TODO`) | (a) 픽스처 부재 |
| 8 | `DicomValidatorTest.ValidateBadUID_ReportsWarning` | `test_dicom_validator.cpp:132` | **무조건** | 픽스처 경로조차 없다. `GTEST_SKIP()` 한 줄이 본문 전부 | (a) 픽스처 부재 |
| 9-12 | `DicomNetworkTest.CStore/CFind×2/CancelCStore` | `test_dicom_network_scu.cpp:68,88,103,127` | `!s_serverAvailable` | `s_serverAvailable = false;` **하드코딩**. 탐지 코드가 없다 | (b) 네트워크 SCP |

**표 작성 중 관측 1건.** network 의 skip 메시지는 `"DCMTK storescp not available"` 이었다.
탐지를 한 적이 없으므로 **"확인해 보니 없더라"가 아니라 "확인한 적 없음"** 이다. 사실과 다른
메시지가 로그에 남아 있었고, 이것이 카드가 "CI 러너에 없는 무언가" 를 의심하게 만든 원인 중 하나다.

### 2.4 되살린 3건 (C4)

검증기 구현을 먼저 읽어 **그 결함을 실제로 잡는지** 확인한 뒤 픽스처를 만들었다
(`DicomValidator.cpp:124-153`: 필수 Type-1 태그 4종 검사 + `isValidUID` 정규식 검사가 이미 있다).

| 테스트 | 픽스처 생성 방식 |
|---|---|
| `UnsupportedTS_ReturnsUnsupportedFormat` | 기록된 Explicit LE 파일을 `DcmFileFormat::loadFile` → `saveFile(..., EXS_LittleEndianImplicit)` 로 재인코딩 |
| `ValidateMissingPatientID_ReportsError` | conformant 파일에서 `findAndDeleteElement(DCM_PatientID)` 후 저장 |
| `ValidateBadUID_ReportsWarning` | conformant 파일의 `DCM_SOPInstanceUID` 를 `"not.a.valid.uid"` 로 교체 후 저장 (`isValidUID` 의 `^[0-9]+(\.[0-9]+)*$` 에 걸린다) |

셋 다 **conformant 파일에서 파생**했다. 손으로 만든 바이트가 아니라 "시험 대상 태그 하나만
다른" 파일이라, 실패했을 때 원인이 그 태그임이 확정된다.

`GTEST_SKIP` 가드는 `ASSERT_TRUE(fs::exists(...))` 로 바꿨다 — 픽스처가 안 만들어지면
**조용히 넘어가는 대신 실패**해야 한다. 스킵으로 되돌아가는 퇴행을 막는 장치다.

`test_dicom_reader` / `test_dicom_validator` 타깃에 DCMTK 링크를 추가했다
(`xpe_dicom` 과 같은 `if(dcmtk_FOUND)` 조건부 그대로).

**결과** (`_after.log`):
```
 69/110 Test  #69: DicomReaderTest.UnsupportedTS_ReturnsUnsupportedFormat .....   Passed  0.03 sec
 96/110 Test  #96: DicomValidatorTest.ValidateMissingPatientID_ReportsError ...   Passed  0.03 sec
101/110 Test #101: DicomValidatorTest.ValidateBadUID_ReportsWarning ...........   Passed  0.03 sec
```
셋 다 **스킵이 아니라 통과**한다 — 이전에 한 번도 실행된 적 없는 분기 3개가 이제 실행된다.

### 2.5 network 4건 — 복구 불가, 메시지 정정 (C5)

의존성 트리를 실제로 뒤졌다:
```
$ find <vcpkg>/x64-windows -iname "storescp*" -o -iname "wlmscpfs*"
.../x64-windows/debug/etc/storescp.cfg
.../x64-windows/etc/storescp.cfg
```
**설정 파일만 있고 실행 파일이 없다.** 바이너리를 들이려면 매니페스트를 고쳐야 하는데
카드가 금지했고 레인 소유도 아니다. 단위 테스트에서 리스너를 띄우는 것은 레인 규약이 금지하는
백그라운드 프로세스 위험이기도 하다.

카드 (b)안의 두 번째 갈래대로 **skip 을 유지**하되, 거짓 메시지를 사실로 고쳤다:

```diff
-    s_serverAvailable = false; // will be set to true after server integration
+    // ... 하드코딩된 false 이며 탐지가 아니다. storescp / wlmscpfs 실행 파일이
+    // 의존성에 없고(vcpkg 트리에 storescp.cfg 만 존재), 이를 추가하는 것은
+    // 이 레인이 소유하지 않은 매니페스트 변경이다.
+    s_serverAvailable = false;
-    GTEST_SKIP() << "DCMTK storescp not available";
+    GTEST_SKIP() << "no mock C-STORE SCP is started by this suite (#124)";
-    GTEST_SKIP() << "DCMTK wlmscpfs not available";
+    GTEST_SKIP() << "no mock C-FIND SCP is started by this suite (#124)";
```

시간 예산 단언은 넣지 않았다 (카드 4번 · B-19 원칙).

### 2.6 DegradedMode 5건 — 손대지 않았다 (C6)

`modules/common/tests/test_post_degraded_mode.cpp` 는 **Lane A 소유**다. 조건을 읽어 보면
`XPE_DEGRADED_ABSENT_DLL` + `XPE_DEGRADED_STAGE_DIR` 로 구동되는, DLL 이 일부러 빠진
스테이징 환경 전용 시나리오다. 그리고:

```
$ grep -rn "XPE_DEGRADED_ABSENT_DLL" --include=*.yml --include=*.yaml \
       --include=*.cmake --include=CMakeLists.txt .
(출력 없음)
```
**저장소 어디에서도 이 변수를 설정하지 않는다.** 즉 이 5건은 일반 실행에서 항상 스킵되는
설계이며, coverage-dicom 잡의 결함이 아니다. 되살리려면 별도 CI 잡이 스테이징을 구성해야 하고
그 판단은 leader·Lane A 몫이다.

### 2.7 재실측 (C7)

`_after.log` (ci-dicom 전체):
```
100% tests passed, 0 tests failed out of 110
Skipped: 9  (DegradedMode 5 + DicomNetworkTest 4)
```
`_verify.log` (3개 config):
```
===CI_POST===    100% tests passed, 0 tests failed out of 444   ===POST_EXIT=0===
===CI_AI===      100% tests passed, 0 tests failed out of 129   ===AI_EXIT=0===
===CI_DICOM===   100% tests passed, 0 tests failed out of 48    ===DICOM_EXIT=0===
```

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 이전 | 이번 | 차 |
|---|---|---|---|
| ci-dicom 전체 skip | 12 (`_base.log`, 이번 실행) | 9 (`_after.log`) | **-3** |
| ci-dicom 전체 통과 | 110/110 | 110/110 | 0 |
| ci-post / ci-ai / ci-dicom(-R Dicom) | 444 / 129 / 48 (B-24 `_verify2.log`) | 444 / 129 / 48 | 0 |

**테스트 수는 늘지 않았다** — 이 카드는 케이스를 더한 게 아니라 **이미 있던 3건을 실행시킨** 것이다.
`_base.log` 는 이번 세션에서 변경 전 트리로 직접 측정한 값이며, CI 의 0.696 과 비교한 값이 아니다.

**커버리지 수치는 재지 않았다.** 카드 3번대로 leader 가 CI dispatch 로 잰다.
0.696 → 얼마가 될지는 이 보고서가 주장하지 않는다.

---

## 4. 미검증 (Gaps)

- **coverage 수치 변화를 측정하지 않았다.** 되살린 3건이 실행하는 분기가 line-rate 를
  얼마나 올리는지는 모른다. 로컬 OpenCppCoverage 문제로 카드가 leader 에게 맡긴 항목이다.
  **"0.85 를 넘는다" 고 주장할 근거가 이 보고서에는 없다.**
- **CI 에서 3건이 실제로 통과하는지 확인하지 않았다.** 로컬 통과만 관측했다. 픽스처는
  DCMTK 호출로 생성하므로 CI 러너의 DCMTK 가 같은 동작을 하는지는 dispatch 결과로만 알 수 있다.
- **network 4건의 `xpe_dicom_cstore`/`cfind` 경로는 여전히 한 번도 실행되지 않는다.**
  메시지를 사실로 고쳤을 뿐 커버리지에는 기여가 없다. 이 4건이 덮어야 할 코드는 미검증 상태다.
- **DegradedMode 5건이 통과할지 실패할지 모른다.** 환경을 구성해 본 적이 없다.
  "스킵 사유가 설계다" 까지가 관측이고, 스테이징을 만들면 통과한다는 것은 확인하지 않았다.
- **`ValidateBadUID_ReportsWarning` 은 `warnings` 가 비어 있지 않은 것만 단언한다.**
  경고 개수·태그·메시지 문자열은 검사하지 않았다 — 과단언을 피했다.
- **Explicit LE → Implicit LE 재인코딩이 픽셀 데이터를 보존하는지 확인하지 않았다.**
  이 테스트는 `xpe_dicom_open` 이 거부하는지만 보므로 문제되지 않지만, 파일 자체의
  내용 동등성은 관측 범위 밖이다.

---

## 5. 잔여 위험 (Residual-risk)

- **픽스처 생성이 조용히 실패할 수 있다.** `loadFile(...).good()` 이 거짓이면 파일이 안 만들어지는데,
  그 경우 `ASSERT_TRUE(fs::exists(...))` 가 **실패**하도록 해 뒀다(스킵으로 되돌아가지 않는다).
  다만 실패 메시지는 "픽스처가 안 써졌다" 까지만 말하고 DCMTK 쪽 원인은 알려주지 않는다.
- **테스트 타깃이 DCMTK 에 직접 의존하게 됐다.** `xpe_dicom` 과 같은 조건부를 복제했으므로
  빌드 구성이 갈리면 두 곳을 같이 고쳐야 한다.
- **skip 메시지 정정은 동작을 바꾸지 않는다.** 여전히 4건은 실행되지 않는다. 메시지가 사실이 된
  것뿐이며, 이것을 "네트워크 경로가 검증됐다" 로 읽으면 안 된다.
- **`_base.log` 의 "변경 전" 측정은 이번 세션에서 한 번만 했다.** 반복 측정하지 않았다.
- 커밋은 origin/main 에 병합되기 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_base.bat` / `_base.log` | 변경 전 ci-dicom 전체 — 110건 중 12 skip |
| `_after.log` | 변경 후 ci-dicom 전체 — 110/110, 9 skip, 되살린 3건 Passed |
| `_verify.bat` / `_verify.log` | 3개 config 재실측 444 / 129 / 48 |
