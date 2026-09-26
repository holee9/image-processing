# QA-B-27 게이트 보고서 — xpe_dicom 커버리지 0.718 → 0.85 시도와 그 한계

**카드**: QA-B-27 (#120)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-27/`
**선행 병합**: `git merge origin/main` → `95f36a7`

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | CI 아티팩트로 파일별 line-rate 표를 만들었다 (추정 아님) | PASS |
| C2 | 미커버 326줄을 도달 가능/불가로 분류했다 | PASS |
| C3 | **0.85 는 테스트 추가만으로 도달 불가다** — 최대 0.772 | PASS (수치 근거 §2.4) |
| C4 | 도달 가능 분기에 테스트 8건을 추가했다. ci-dicom 118/118 | PASS |
| C5 | 3항(SCP) 판단: **(b) 스킵 유지** — 근거 §2.5 | PASS |
| C6 | `DicomValidator::checkConformance` 36줄이 **호출자 없는 죽은 코드**임을 확인 | PASS |
| C7 | 후 수치는 측정하지 못했다 — 로컬 OpenCppCoverage 부재 | **GAP** (§4) |

---

## 2. 증거 (Evidence)

### 2.1 파일별 표 (전) — CI run `34447732961`, `coverage-coverage-dicom`

`gh run download 34447732961 -n coverage-coverage-dicom -D build/cov-dicom-ci` 로 받은
Cobertura 를 파싱했다 (`_cov_before.txt`). **overall line-rate 0.7179930795847751.**

| 파일 | 줄 | 커버 | rate | 미커버 |
|---|---:|---:|---:|---:|
| `dicom/src/DicomNetworkSCU.cpp` | 184 | 77 | 0.418 | **107** |
| `dicom/src/DicomValidator.cpp` | 149 | 77 | 0.517 | **72** |
| `dicom/src/DicomReader.cpp` | 242 | 193 | 0.798 | 49 |
| `dicom/src/dicom.cpp` | 102 | 64 | 0.627 | 38 |
| `dicom/src/DicomWriter.cpp` | 211 | 180 | 0.853 | 31 |
| `common/src/xpe_common.cpp` | 134 | 121 | 0.903 | 13 |
| `common/src/xpe_logging.cpp` | 73 | 61 | 0.836 | 12 |
| `common/src/xpe_memory.cpp` | 60 | 56 | 0.933 | 4 |
| `dicom/src/DicomReader.h` | 1 | 1 | 1.000 | 0 |
| **합계** | **1156** | **830** | **0.718** | **326** |

보고서에 `modules/common` 3파일(268줄)이 함께 들어간다 — dicom 테스트가 `xpe_common` 을
링크하기 때문이다. 이 3파일은 **Lane A 소유**이며 이 카드에서 손대지 않았다.

### 2.2 미커버 위치

`_uncovered.txt` 에 파일별 줄 범위를 남겼다. 예:
```
=== dicom/src/DicomValidator.cpp  uncovered=72 ===
  68-70, 80-86, 88-92, 94-96, 101-107, 109-113, 115-117, 173-174, 176-186,
  189-192, 195-208, 222-223, 226-228, 236-238
```

### 2.3 분류 (소스 판독)

| 파일 | 미커버 | 라이브 SCP 필요 | 죽은 코드 | `catch(...)`·라이브러리 실패 | **도달 가능** |
|---|---:|---:|---:|---:|---:|
| `DicomNetworkSCU.cpp` | 107 | ~95 | 0 | 0 | **~12** |
| `DicomValidator.cpp` | 72 | 0 | **36** | 0 | **~36 중 6** (나머지 30은 아래) |
| `DicomReader.cpp` | 49 | 0 | 0 | ~14 | **~35** |
| `dicom.cpp` | 38 | 0 | 0 | ~34 | **~4** |
| `DicomWriter.cpp` | 31 | 0 | 0 | ~26 | **~5** |
| `common/*` (Lane A) | 29 | — | — | — | (범위 밖) |
| **합계** | **326** | **~95** | **36** | **~74** | **~62** |

Validator 의 30줄(80-117)은 "meta info 없음"·"dataset 없음" 조기 반환인데, DCMTK 의
`loadFile` 이 성공하면 `getMetaInfo()`/`getDataset()` 이 널을 주지 않아 **일반 입력으로는
진입로가 없다고 판단**했다. 이 판단은 소스 판독 근거이며 실험으로 반증하지 않았다(§4).

### 2.4 도달 가능성 산술 — **0.85 는 불가능하다**

- 임계 0.85 · 총 1156줄 → 필요 커버 **983**. 현재 830. **+153 필요.**
- 도달 가능 미커버 **~62**. 전부 덮어도 892/1156 = **0.772**.
- `common` 29줄까지 전부 덮어도 921/1156 = **0.797**.

**남은 191줄(SCP 95 + 죽은 코드 36 + catch/라이브러리 실패 60)은 테스트로 닿지 않는다.**
따라서 이 카드의 목표는 **테스트 추가만으로는 달성 불가**다. 닫으려면 셋 중 하나가 필요하다:

1. 인프로세스 mock SCP (매니페스트·의존성 변경 — 레인 소유 아님, §2.5)
2. 죽은 코드 `checkConformance` 제거 (36줄이 분모에서 빠짐 — 소유자 판단 필요, §2.6)
3. 임계값 또는 분모 정책 조정 (카드가 금지)

### 2.5 3항 판단 — (b) 스킵 유지

**(a) in-process `DcmSCP` 를 택하지 않았다.** 근거 순서대로:
- QA-B-25 실측: vcpkg 트리에 `storescp.cfg` 만 있고 실행 파일이 없다.
- `DcmSCP` 클래스를 테스트에서 직접 쓰려면 `dcmnet` 링크가 필요하고, 이는
  `test_dicom_network_scu` 타깃의 의존성 확대다. 가능은 하다.
- **그러나** 단위 테스트 프로세스 안에서 리스너를 띄우는 것은 레인 규약이 금지하는
  백그라운드 프로세스이며(포트 점유·정리 미보장), 카드 4항의 "시간 예산 단언 금지" 와도
  같은 계열의 위험이다.
- 그리고 §2.4 가 보이듯 **4건을 살려도 0.85 에 닿지 않는다** — 비용 대비 효과가 목표를
  해결하지 못한다.

따라서 스킵을 유지하고, **연결 전 오류 분기만 채웠다**(§2.6 추가 목록).
스킵 메시지는 #124 에서 이미 사실로 정정해 두었다.

### 2.6 추가한 테스트 8건 (C4)

| 파일 | 테스트 | 표적 줄 |
|---|---|---|
| `test_dicom_validator.cpp` | `ValidateNotDicom_SmallBuffer_ReturnsBufferTooSmall` | `DicomValidator.cpp:66-74` (DICOM_INVALID 경로의 버퍼 검사) |
| | `ValidateConformant_SmallBuffer_ReturnsBufferTooSmall` | 성공 경로 버퍼 검사 |
| | `ValidateStrippedTags_ReportsAllMissing` | 필수 태그 4종 누락 루프 + `buildReport` warnings-empty 분기 |
| `test_dicom_writer.cpp` | `WriteEmptyBodyPart_StillWritesFile` | `DicomWriter.cpp:169` else 분기 |
| | `WriteJ2KNullPixelData_ReturnsProcessingFailed` | `:232` compressJ2K 널 가드 → `:80-81` |
| `test_dicom_network_scu.cpp` | `CStoreNonDicomFile_ReturnsIoFailed` | `DicomNetworkSCU.cpp:53-54` loadFile 실패 |
| | `CStoreMissingFile_ReturnsIoFailed` | 같은 분기, 다른 입력 |
| `test_dicom_reader.cpp` | `ReadJ2K_NoPixelData_ReturnsDicomInvalid` | `DicomReader.cpp:294-297` |

**결과** (`_after.log`):
```
===BUILD=0===
100% tests passed, 0 tests failed out of 118
===CTEST=0===
```
전 110 → 후 118 (전체 ctest 기준, `-R` 필터 없음).

### 2.7 죽은 코드 발견 (C6)

```
$ grep -rn "checkConformance" modules/ tests/
modules/dicom/src/DicomValidator.h:39:    static ValidationResult checkConformance(const std::string& filePath);
```
**정의(`DicomValidator.cpp:173-208`, 36줄)와 헤더 선언만 있고 호출자가 없다.**
`DicomValidator` 는 `XPE_API` 가 없는 내부 클래스라 DLL 밖에서도 부를 수 없다.
즉 이 36줄은 어떤 테스트로도 실행할 수 없으면서 분모에 남아 있다.

**제거하지 않았다.** 죽은 코드를 지워 비율을 올리는 것은 카드가 금지한
"스킵을 지우기만 하는 것" 과 같은 계열이고, 내부 헤더의 공개 선언을 지우는 판단은
소유자(leader) 몫이다. 보고만 한다.

### 2.8 시도했다가 철회한 것 (거짓 단언 방지)

`ReadJ2K_NotEncapsulated_ReturnsDicomInvalid` 를 썼다가 **제거했다.** Explicit LE 파일의
meta TransferSyntaxUID 만 J2K 로 바꿔 저장하면 `xpe_dicom_read_image` 가 `XPE_OK` 를
반환한다(실측: 첫 실행에서 이 단언이 실패). DCMTK 가 저장 시 실제 인코딩에 맞춰 meta TS 를
다시 쓰는 것으로 보인다. **틀린 기대를 단언하는 테스트를 남기느니 제거하고** 사유를
`test_dicom_reader.cpp` 주석에 기록했다. `DicomReader.cpp:311-319` 는 미커버로 남는다.

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 전 | 후 | 근거 |
|---|---|---|---|
| xpe_dicom line-rate | **0.718** | **미측정** | 전: CI run `34447732961` 아티팩트 (§2.1). 후: §4 |
| ci-dicom 전체 ctest | 110 | **118** | `_after.log`, `-R` 필터 없음 |
| ci-dicom skip | 9 | 9 | DegradedMode 5 + Network 4, 변화 없음 |

전 수치는 leader 가 카드에 적은 0.718 과 내가 아티팩트에서 재계산한 0.71799…가 일치한다.

---

## 4. 미검증 (Gaps)

- **후 커버리지 수치를 측정하지 못했다.** 로컬에 OpenCppCoverage 가 없다
  (`/c/Program Files/OpenCppCoverage/` 부재, `cmake/XpeCoverage.cmake:36-37` 이 찾는 경로).
  카드 4항의 대체 경로대로 ci-dicom 전체 ctest 결과와 표만 제출한다.
  **추가 8건이 몇 줄을 올렸는지 이 보고서는 주장하지 않는다.** leader 의 CI dispatch 가 정본이다.
- **§2.3 의 도달 가능/불가 분류는 소스 판독 판단이다.** 특히 Validator 80-117 의 30줄을
  "진입로 없음" 으로 분류한 것은 DCMTK 동작에 대한 추론이며, 반례를 만들어 보지 않았다.
  분류가 틀렸다면 §2.4 의 상한 0.772 도 그만큼 올라간다.
- **`DicomReader` 도달 가능 ~35줄 중 1건만 덮었다.** J2K 비트스트림 손상 경로
  (`DicomReader.cpp:401-431` 등 ~30줄)는 파일 바이트를 직접 조작해야 하고, 이번에 하지 않았다.
  **남은 최대 군집이며 후속 카드 후보다.**
- **`checkConformance` 를 "죽은 코드" 로 판정한 근거는 저장소 전수 grep 이다.** 저장소 밖
  소비자(다른 리포지터리·향후 계획)는 확인할 수 없다.
- **in-process `DcmSCP` 가 실제로 가능한지 빌드로 시험하지 않았다.** §2.5 는 비용·규약·효과
  근거로 (b) 를 택한 것이지, "기술적으로 불가능하다" 는 실측 주장이 아니다.

---

## 5. 잔여 위험 (Residual-risk)

- **이 카드는 목표 수치를 달성하지 못한다.** §2.4 가 그 이유를 수치로 보인다. 합격 여부는
  leader 판단이며, 목표를 닫으려면 §2.4 의 세 선택지 중 하나에 대한 결정이 선행이다.
- **`common` 3파일이 dicom 커버리지 분모에 들어간다.** 268줄(23%)이 Lane A 소유 코드다.
  dicom DLL 의 품질과 무관한 변동이 이 수치를 흔들 수 있다 — 분모 정책 자체가 검토 대상일 수 있다.
- **추가한 테스트는 커버리지를 위해 쓴 것이지 결함을 찾기 위해 쓴 것이 아니다.**
  8건 모두 현재 동작을 그대로 단언한다. 동작이 바뀌면 실패하지만, 지금 동작이 옳은지는
  이 카드가 검증하지 않았다.
- **`WriteJ2KNullPixelData` 는 `data=nullptr`, `dataSize=0` 으로 #123 가드를 통과시킨다.**
  가드 정책이 "널 데이터도 거부" 로 바뀌면 이 테스트의 기대값이 바뀐다.
- 커밋은 origin/main 병합 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_cov_before.txt` | CI 아티팩트 파싱 — 파일별 line-rate 표, overall 0.71799 |
| `_uncovered.txt` | 파일별 미커버 줄 범위 |
| `_dicom.bat` / `_after.log` | ci-dicom 빌드 + 전체 ctest 118/118 |
| `build/cov-dicom-ci/coverage.xml` | 원본 Cobertura (다운로드본) |
