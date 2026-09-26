# QA-B-34 게이트 보고서 — dicom 비네트워크 3파일 분류·보강

**카드**: QA-B-34 (#120)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-34/`
**커밋**: `5e4a4df` (1/4 Validator) · `76a04a6` (2/4 dicom.cpp 분류) · `2ad73c1` (3/4 Reader) · `20a58ce` (4/4 SCU)
**4항은 7번째 dispatch(34486857040) 통보를 받고 착수했다** — §6 에 별도로 적는다.

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | 3파일 미커버를 **계측 줄 단위**로 분류했다 | PASS |
| C2 | **`dicom.cpp` 37줄에는 도달 가능 분기가 0** — 전부 `catch (...)` | PASS |
| C3 | **`DicomValidator.cpp` 30줄 도달 불가를 시험으로 판정**했다 (B-27 은 판독만) | PASS |
| C4 | `DicomReader.cpp` 도달 가능 분기 3건을 커버했다 | PASS |
| C5 | ci-dicom 131 → **135** (+4), 무회귀 445 / 202 | PASS |
| C6 | 카드가 기대한 만큼의 커버 증가는 **얻지 못했다** | **미달** (§4) |

---

## 2. 증거 (Evidence)

### 2.1 기준 아티팩트

`gh run download 34479945843 -n coverage-coverage-dicom -D build/cov-dicom-6`
→ overall line-rate **0.76161790017211706** (카드의 0.762 와 일치).

미커버 줄 범위는 `_uncovered.txt`.

### 2.2 `dicom.cpp` — 37줄 전부 `catch (...)` (C2)

카드 2항은 "공개 진입점의 인자 검증·미초기화·오류 전파 분기" 를 채우라고 지시했다.
**계측이 그런 분기가 없다고 말한다.** 37줄을 한 줄도 빠뜨리지 않고 출력했다
(`_dicom_cpp_classification.txt`):

| 범위 | 내용 |
|---|---|
| 48-51 | `xpe_dicom_open` 의 `catch (...)` + 로그 + `return PROCESSING_FAILED` |
| 59-62 | `xpe_dicom_read_image` 동일 |
| 70-73 | `xpe_dicom_get_metadata` 동일 |
| 80-82 | `xpe_dicom_close` 동일(반환 없음) |
| 120-123 | `xpe_dicom_write` 동일 |
| 134-137 | `xpe_dicom_write_j2k` 동일 |
| 151-154 | `xpe_dicom_validate` 동일 |
| 170-173 | `xpe_dicom_cstore` → `NETWORK_FAILED` |
| 188-191 | `xpe_dicom_cfind_mwl` → `NETWORK_FAILED` |
| 198-200 | `xpe_dicom_cancel` 의 `// cancel must never throw` |

**열 개 `catch (...)` 핸들러와 그 본문이 전부다.** 인자 검증 분기는 이미 덮여 있다
(B-21 의 널 검사 재정렬, B-27 의 크기 가드 케이스). 남은 것은 폴트 주입 없이 도달할 수
없고, 폴트 주입은 B-16 판정 이후 이 레인의 방식이 아니다.

**테스트를 추가하지 않았다.** 분류만 증거로 남겼다 — 없는 분기를 채운 척하지 않는다.

### 2.3 `DicomValidator.cpp` — 38줄, 도달 불가를 **시험으로** 판정 (C3)

| 범위 | 줄 | 내용 | 판정 |
|---|---:|---|---|
| 77-96 | 15 | `getMetaInfo()` 널 가드 | 도달 불가 (아래 시험) |
| 101-117 | 15 | `getDataset()` 널 가드 | 도달 불가 (같은 이유) |
| 184-201 | 8 | `buildReport` 의 빈 문자열 분기 + `parse` 실패 `catch` | 도달 불가 |

**B-27 은 이 30줄을 소스 판독만으로 "진입로 없음" 이라 적고 Gap 에 "반증하지 않았다" 고
달았다. 이번에 시험했다.**

`EWM_dataset` 으로 Part 10 meta 헤더가 없는 파일을 만들어 넘겼다
(`ValidateDatasetWithoutMetaHeader_IsHandled`). 결과:

- `loadFile` 이 **성공**한다 (파싱 실패 분기로 가지 않는다)
- `getMetaInfo()` 가 **널이 아니다** — DCMTK 가 meta 객체를 합성한다
- 따라서 `:78` 가드는 진입하지 않는다

184-201 은 `errorsJson`/`warningsJson` 이 `dump()` 결과라 최소 `"[]"` 이므로
`.empty()` 가 참이 되지 않고, `parse` 는 자기가 만든 문자열을 읽으므로 `catch` 로 가지 않는다.

**공개 API 로는 38줄 전부 도달 불가다 — 이제 근거가 판독이 아니라 관측이다.**

#### 2.3.1 부수 관측 (발견이지 판단이 아니다)

데이터셋 전용 파일(Part 10 meta 헤더 없음)이 **`valid=true` 로 통과한다.**
검증기가 포인터의 널 여부만 보고 meta 그룹의 **내용**은 보지 않기 때문이다.

이것이 결함인지 아닌지는 계약 문제다. **처음에는 테스트가 `valid == false` 를 단언하도록
썼다가 철회했다** — api-spec 에 "meta 헤더가 없으면 비적합" 이라는 문장이 없고, 없는 계약을
테스트가 단언하면 한 사람의 의견이 규범으로 굳는다. 단언을 "답을 내고 리포트가 파싱된다"
까지로 좁히고, 동작은 여기 관측으로 남긴다.

### 2.4 `DicomReader.cpp` — 도달 가능 분기 3건 커버 (C4)

| 테스트 | 표적 줄 | 픽스처 |
|---|---|---|
| `OpenDatasetWithoutMetaHeader_TreatedAsExplicitLE` | `:105-107` "no TS in meta → Explicit LE" | `EWM_dataset` 로 meta 제거 |
| `ReadImage_NoPixelData_ReturnsDicomInvalid` | `:164-168` — 특히 `xpe_free_image(outImg)` | 비압축 파일에서 `PixelData` 만 삭제 |
| `OpenEmptyPath_ReturnsIoFailed` | `:61-62` `EC_InvalidFilename/EC_IllegalParameter` | 빈 경로 |

두 번째가 값이 있다. `:166` 은 **이미 할당한 출력 버퍼를 반환하는 줄**이라, 반환 코드만
보는 테스트에는 누수가 보이지 않는다. 세 픽스처 모두 정상 파일에서 한 가지만 바꿔 파생시켰다.

```
93/135 DicomReaderTest.OpenDatasetWithoutMetaHeader_TreatedAsExplicitLE ... Passed
94/135 DicomReaderTest.ReadImage_NoPixelData_ReturnsDicomInvalid .......... Passed
95/135 DicomReaderTest.OpenEmptyPath_ReturnsIoFailed ..................... Passed
```

### 2.5 재실측 (C5) — `_verify.log`, 필터 없음

```
===CI_POST===    100% tests passed, 0 tests failed out of 445   ===POST_EXIT=0===
===CI_AI===      100% tests passed, 0 tests failed out of 202   ===AI_EXIT=0===
===CI_DICOM===   100% tests passed, 0 tests failed out of 135   ===DICOM_EXIT=0===
```

| | B-33 | B-34 | 차 |
|---|---:|---:|---:|
| ci-dicom | 131 | **135** | +4 (Validator 1 + Reader 3) |
| ci-post / ci-ai | 445 / 202 | 445 / 202 | 0 |

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 기준 line-rate | 0.7616 | CI run `34479945843` 아티팩트 (§2.1) |
| 목표까지 | +103줄 | 카드 (0.85 × 1162 = 988, 현재 885) |
| 이번에 덮은 것 | **~4~8줄 추정** | §4 — **측정하지 않았다** |
| ci-dicom | 131 → 135 | `_verify.log` (이번 실행) |

---

## 4. 미검증 (Gaps) — **목표 미달을 먼저 적는다**

- **커버 증가량을 측정하지 않았다.** 로컬 OpenCppCoverage 부재는 B-27 이후 그대로다.
  추가한 4 케이스가 덮는 줄은 Reader 3분기(대략 `:61-62`, `:105-107`, `:164-168` = 7줄)와
  Validator 0줄이다. **+103 이 목표인데 한 자릿수다.**
- **목표에 닿지 못하는 구조적 이유** — 남은 미커버를 계측 줄로 세면:

  | 파일 | 미커버 | 도달 가능 | 성격 |
  |---|---:|---:|---|
  | `DicomNetworkSCU.cpp` | 98 | **미정** | 4항 보류(7번째 dispatch 대기). B-32 로 상당수 해소 예상 |
  | `DicomReader.cpp` | 44 | ~7 커버, 잔여 ~37 | 대부분 openjpeg 콜백·실패 핸들러(`:356-447`) |
  | `DicomValidator.cpp` | 38 | **0** | §2.3 |
  | `dicom.cpp` | 37 | **0** | §2.2 |
  | `DicomWriter.cpp` | 27 | 소수 | openjpeg 실패 핸들러 |

  **비네트워크 3파일에서 얻을 수 있는 것은 한 자릿수다.** +103 은 SCU 98줄이 B-32 로
  얼마나 해소됐는지에 사실상 달려 있다 — 7번째 dispatch 가 답을 준다.
- **`DicomReader.cpp` 잔여 ~37줄을 시도하지 않았다.** `:401-413`(openjpeg 스트림 콜백),
  `:427-431`/`:446-447`(디코드 실패 정리)은 B-29 의 손상 픽스처로 일부 닿았을 수 있으나
  이번 아티팩트는 B-29 **이전** 시점이라 확인되지 않는다.
- **이번 아티팩트는 B-32(MWL 실행분) 미반영이다** — 카드가 명시한 대로다.
- **§2.3.1 의 meta-헤더 관측은 계약 판단을 하지 않았다.** leader 몫이다.

---

## 5. 잔여 위험 (Residual-risk)

- **분류가 "도달 불가" 로 수렴할수록 목표는 산술적으로 멀어진다.** Validator 38 + dicom.cpp 37
  = 75줄이 확정적으로 닿지 않는데, 목표는 +103 이다. SCU 를 빼면 남은 여지가 없다 —
  0.85 를 비네트워크 파일로 달성하는 경로는 존재하지 않는다는 것이 이번 분류의 결론이다.
- **`EWM_dataset` 픽스처가 두 파일에서 쓰인다**(validator·reader). DCMTK 가 이 쓰기 모드의
  동작을 바꾸면 두 테스트가 함께 흔들린다.
- **`ValidateDatasetWithoutMetaHeader_IsHandled` 는 약한 단언이다** — 의도적이다(§2.3.1).
  계약이 정해지면 단언을 조일 수 있다.
- **빈 경로 테스트는 DCMTK 의 오류 코드 매핑에 의존한다.** DCMTK 가 빈 경로에 다른 코드를
  돌려주면 `:61-62` 대신 다른 분기로 가고, 테스트는 여전히 통과하지만 표적을 잃는다.
- 커밋 3건은 origin/main 병합 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_dicom.bat` | ci-dicom 빌드 + 전체 ctest |
| `_uncovered.txt` | 6번째 dispatch 아티팩트의 파일별 미커버 줄 범위 |
| `_dicom_cpp_classification.txt` | `dicom.cpp` 37줄 전문 — 전부 `catch (...)` |
| `_validator.log` | Validator 케이스 첫 실행(단언 철회 전) |
| `_verify.log` | 재실측 445 / 202 / 135 |

---

## 6. 4항 — `DicomNetworkSCU.cpp` (7번째 dispatch 뒤 착수)

**커밋**: `20a58ce` · **증거**: `_uncovered7.txt`, `_scu_classification.txt`, `_scu_run1.log`, `_verify4.log`

### 6.1 주장

| # | 주장 | 상태 |
|---|------|------|
| C7 | 54줄을 계측 줄 기준 4분류했다 (A 도달가능 / B 죽은 코드 / C 취소 경쟁 / D 도달 불가) | PASS |
| C8 | 도달 가능한 **14줄만** 테스트로 덮었다 — 나머지는 채우지 않고 사유를 적었다 | PASS |
| C9 | `responseToJson` 19줄이 **호출자 0인 죽은 코드**임을 특정했다 (삭제하지 않음) | PASS |
| C10 | `:236-238`(#137 실패 경로)이 **이 API 형태로는 도달 불가**임을 논증했다 | PASS |
| C11 | ci-dicom 135 → **140**(+5), ci-ai 202 무회귀, ci-post **455** | PASS |
| C12 | 0.85 는 이번에도 **닿지 못한다** — 남는 40줄의 성격을 §6.5 에 | **미달** |

### 6.2 분류 (전문 `_scu_classification.txt`)

| 군 | 줄 수 | 내용 |
|---|---:|---|
| **A 도달 가능** | **14** | UID 폴백 3(`:69 :72 :76`) · `CALLED_AE@host` 파싱 3(`:300-302`) · buildFindRequest 실패 5(`:213-214 :337-339`) · query 키 2(`:328 :334`) · 버퍼 부족 1(`:281`) |
| **B 죽은 코드** | **19** | `responseToJson` 전체(`:344-368`) |
| **C 취소 경쟁** | 6 | `:102 :120-121 :194 :219-220` |
| **D 도달 불가** | 15 | `:59 :108-109 :137-138 :143-144 :200-201 :236-238 :247 :249-250` |

### 6.3 두 가지 판정 — 판독이 아니라 논증이다

**`responseToJson` 는 죽은 코드다 (19줄).** `grep -rn responseToJson modules/ tests/` 가
선언(`DicomNetworkSCU.h:69`)과 정의(`.cpp:344`) 2건만 낸다. 호출자가 없다.
`cfindMwl` 은 `:254-287` 에서 같은 직렬화를 **인라인으로 중복 구현**한다.
카드의 "죽은 코드 삭제(목록만)" 지시대로 **지우지 않았다** — 목록만 남긴다.

**`:236-238`(findPresID == 0)은 이 API 형태로 진입로가 없다.** B-32 에서 내가 넣은
#137 방어 코드다. "협상은 됐는데 MWL 컨텍스트만 없는" 상태여야 참이 되는데,
`cfindMwl` 은 MWL 컨텍스트를 **하나만** 제안한다(`:187-190`). SCP 가 그것을 거부하면
수락 컨텍스트가 0개가 되어 `negotiateAssociation` 이 `:205-207` 에서 먼저 실패한다.
B-34 보고서가 "컴파일은 됐으나 한 번도 발화하지 않았다" 고 적은 것의 이유가 이것이다.
**방어 코드로는 유효하므로 제거 대상이 아니다.**

**취소 6줄은 단일 스레드로 참이 될 수 없다.** `cstore`/`cfindMwl` 이 진입 즉시
`s_cancelRequested.store(false)`(`:43`, `:163`)를 하므로, 호출 전에 `cancel()` 을 해도
플래그가 지워진다. `:43` 과 `:101` 사이에 다른 스레드가 끼어야 한다.
**Cancel 스킵 결정을 유지한다** — 경쟁을 단언하는 테스트는 만들지 않았다.

### 6.4 추가한 테스트 5건 (`_scu_run1.log`)

```
136/140 DicomNetworkTest.CStoreFileWithoutSopUids_FallsBackToDxDefault ... Passed
137/140 DicomNetworkTest.CStoreCalledAeInHost_NegotiatesAndStores ........ Passed
138/140 DicomNetworkTest.CFindMalformedQueryJson_ReturnsProcessingFailed . Passed
139/140 DicomNetworkTest.CFindQueryWithNameAndAccession_ReturnsJsonArray .. Passed
140/140 DicomNetworkTest.CFindOutBufferTooSmall_ReturnsBufferTooSmall ..... Passed
```

`CStoreCalledAeInHost` 는 분기 실행만 보지 않고 **SCP 의 `storeRequests` 증가**를
단언한다 — 파싱이 실제로 쓸 수 있는 association 을 만들었다는 뜻이다.

### 6.5 산술 — 0.85 는 이번에도 닿지 않는다

| | 줄 |
|---|---:|
| 7번째 dispatch 커버 | 938 / 1167 = **0.804** |
| 이번에 덮은 SCU | **+14** |
| 예상 | 952 / 1167 ≈ **0.816** |
| 0.85 필요치 | 992 — **40줄 부족** |

**남는 40줄의 성격이 결론이다**: SCU 40 + Validator 38 + `dicom.cpp` 37 + Reader 잔여 33
+ Writer 27. 이 중 **Validator 38 · dicom.cpp 37 · SCU 의 B/C/D 40 = 115줄이 확정적으로
도달 불가하거나 죽은 코드**다. 남은 후보는 Reader·Writer 의 openjpeg 실패 핸들러뿐인데,
그것도 폴트 주입 경로다. **0.85 는 폴트 주입을 도입하거나 죽은 코드를 제거하지 않는 한
달성 경로가 없다** — 이것이 B-27 부터 세 카드에 걸쳐 수렴한 결론이다.

### 6.6 미검증 (Gaps) — 4항

- **커버 증가량을 이번에도 측정하지 않았다.** `+14` 는 계측 줄 번호와 테스트가 지나는
  분기를 대조해 센 값이고, 다음 dispatch 로만 확인된다.
  다만 **B-34 1~3항의 추정은 이번에 확인됐다** — 6번째 dispatch 의 Reader 미커버 44가
  7번째에서 40이 됐고, 사라진 4줄(`:62 :107 :166-167`)이 정확히 그때 만든 3테스트의
  표적이다. 그때 "한 자릿수" 라고 쓴 추정이 관측으로 확인된 셈이다.
- **D 군 6줄(`:137-138 :143-144 :247 :249-250`)은 거부하는 mock 을 넣으면 도달 가능하다.**
  시도하지 않았다 — mock 확장은 카드 범위 밖이고, leader 판단 사항이다.
- **`:59`(dataset 널)를 시험으로 반증하지 않았다.** Validator 에서 관측한 성질
  (loadFile 성공 시 객체가 존재)에서 유추한 것이다. B-27→B-34 에서 한 것과 같은 승격을
  하지 않았다는 뜻이므로 판독으로 남긴다.

### 6.7 ci-post 기준선 정정 — 445 가 아니라 **455**

재실측 중 `build/ci-post` 가 **재빌드에 실패**했다(`0xc0000139`, entry point not found).
원인은 그 디렉터리의 stale 상태였다 — 새 디렉터리(`build/ci-post-b34`)로
`cmake --preset ci-post` 를 다시 돌리자 구성·빌드·테스트가 모두 통과했고,
케이스가 **455** 로 나왔다.

**445 는 stale 기준선이었다.** ci-post 는 `modules/dicom` 을 아예 빌드하지 않으므로
(`build/ci-post/modules/` = common·display·enhance_advanced·enhance_basic·gsvg)
이번 커밋과 무관하고, 차이 10건은 병합으로 들어온 Lane A 테스트다.
**B-33 의 "192 vs 202" 와 같은 유형이 ci-post 에서 반복됐다.**

```
===CI_POST===   100% tests passed, 0 tests failed out of 455   ===POST_EXIT=0===
===CI_AI===     100% tests passed, 0 tests failed out of 202   ===AI_EXIT=0===
===CI_DICOM===  100% tests passed, 0 tests failed out of 140   ===DICOM_EXIT=0===
```

### 6.8 잔여 위험 — 4항

- **`build/ci-post` 는 지금도 깨진 상태다.** 새 디렉터리로 우회했을 뿐 고치지 않았다.
  다음에 그 디렉터리를 쓰는 사람은 같은 실패를 본다. 지우면 되지만 **빌드 디렉터리
  처분은 이 카드의 범위 밖**이라 손대지 않았다.
- **`CFindOutBufferTooSmall` 은 mock 이 3건을 돌려준다는 전제에 기댄다.** worklist 가
  줄어들면 8바이트를 넘지 않을 수 있고, 그러면 테스트는 통과하면서 표적을 잃는다.
- **`CStoreFileWithoutSopUids` 는 DCMTK 가 meta 를 합성해도 `MediaStorage*` 는 비운다는
  관측에 기댄다.** DCMTK 가 이를 채우도록 바뀌면 `:69 :72` 는 지나지 않는다.
- 커밋 4건은 push 전까지 미푸시다.

### 6.9 부록 — 4항 증거 파일

| 파일 | 내용 |
|---|---|
| `_uncovered7.txt` | 7번째 dispatch 파일별 미커버 줄 (0.80377) |
| `_scu_classification.txt` | SCU 54줄 A/B/C/D 분류 전문 |
| `_scu_run1.log` | ci-dicom 140/140, 경고 0 |
| `_post_fresh.log` | `build/ci-post-b34` 신규 구성·빌드 (CFG=0 BUILD=0) |
| `_verify4.bat` / `_verify4.log` | 재실측 455 / 202 / 140 |
