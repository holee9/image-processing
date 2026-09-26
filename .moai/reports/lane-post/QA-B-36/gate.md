# QA-B-36 게이트 보고서 — 검증기의 Part 10 meta 그룹(0002) 검사

**카드**: QA-B-36 (#139, leader 결정 (a))
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-36/`
**커밋**: `49f5709` (1/2 RED 테스트) · `2ad5ce8` (2/2 GREEN 구현)

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | B-34 가 철회한 케이스를 **RED 로 되살렸다** — 실패하는 것을 관측했다 | PASS |
| C2 | meta 부재 판별은 **널 검사가 아니라 `card()`** 다 — 근거는 §2.3 | PASS |
| C3 | meta 내용(TS UID · MediaStorage SOP Class/Instance UID) 불일치를 검출한다 | PASS |
| C4 | **writer 산출물이 새 검사를 통과한다** (왕복) | PASS |
| C5 | ci-dicom 136 → **138**(+2), ci-post 451 / ci-ai 198 무회귀, 경고 0 | PASS |
| C6 | api-spec 문구 초안을 §5 에 냈다 (반영은 leader) | PASS |

---

## 2. 증거 (Evidence)

### 2.1 RED (C1) — `_red.log`

```
11/13 DicomValidatorTest.ValidateDatasetWithoutMetaHeader_ReportsMissingMeta ***Failed
12/13 DicomValidatorTest.ValidateMetaSopClassMismatch_ReportsInconsistency ...***Failed
13/13 DicomValidatorTest.ValidateWriterOutput_IsConformant ................... Passed
85% tests passed, 2 tests failed out of 13
```

**실패한 것이 정확히 새 계약 2건이고, 나머지 11건은 그대로 통과한다.** 왕복 케이스가
RED 단계에서도 GREEN 인 것이 중요하다 — 이 프로젝트 writer 는 처음부터 제대로 된 meta 를
쓰고 있었다는 뜻이고, 따라서 새 규칙이 자기 산출물을 깨뜨리지 않는다는 것을 구현 **전에**
확인한 셈이다.

### 2.2 B-34 의 철회를 왜 지금 되살리는가

B-34 는 같은 케이스를 쓰다가 `EXPECT_FALSE(j["valid"])` 를 **철회**했다. 이유는
"api-spec 에 그런 문장이 없고, 없는 계약을 테스트가 단언하면 한 사람의 의견이 규범으로
굳는다" 였다. 그 판단은 지금도 옳다 — 바뀐 것은 테스트가 아니라 **계약**이다.
#139 에서 leader 가 (a) 를 결정했으므로 단언에 근거가 생겼다. 되살린 단언은
그때 쓰려던 것보다 강하다: `valid=false` 뿐 아니라 **사유가 meta 를 지목**할 것까지 본다.

### 2.3 판별 방법 (C2) — 이것이 이 카드의 실질

```cpp
if (meta->card() == 0) { ... }
```

**널 검사로는 잡히지 않는다.** `DicomValidator.cpp:78` 의 `if (!meta)` 가 이미 있는데도
dataset 전용 파일이 `valid=true` 로 통과했던 이유가 이것이다 — DCMTK 는 dataset 전용
파일을 `loadFile` 할 때도 `DcmMetaInfo` 객체를 **합성**하므로 `getMetaInfo()` 가 널이
아니다(B-34 §2.3 관측). 두 경우를 가르는 것은 포인터가 아니라 **합성된 객체에 원소가
없다**는 사실이고, `card()` 가 그 관측이다.

**이 판별은 가설이 아니라 시험을 통과했다.** `card()==0` 이 참이 아니었다면 RED 2건 중
첫 번째가 GREEN 으로 바뀌지 않았을 것이다. `_green.log` 의 13/13 이 그 시험 결과다.

대안으로 `getPreambleUsed()` 를 쓸 수도 있으나 채택하지 않았다 — preamble 이 없으면서
meta 그룹은 있는 파일을 잘못 탈락시킨다. 계약이 요구하는 것은 preamble 이 아니라
meta 그룹이다.

### 2.4 검사 내용 (C3) — `DicomValidator.cpp` +65줄

| 조건 | tag | 판정 |
|---|---|---|
| meta 그룹 부재 (`card()==0`) | `0002,0000` | `valid=false` |
| `TransferSyntaxUID` 부재 | `0002,0010` | `valid=false` |
| `TransferSyntaxUID` UID 형식 위반 | `0002,0010` | `valid=false` |
| `MediaStorageSOPClassUID` 부재 / 데이터셋 `SOPClassUID` 와 불일치 | `0002,0002` | `valid=false` |
| `MediaStorageSOPInstanceUID` 부재 / 데이터셋 `SOPInstanceUID` 와 불일치 | `0002,0003` | `valid=false` |

데이터셋 쪽 태그가 **아예 없는** 경우는 판정하지 않는다 — 기존 Type 1 루프가 이미
`Missing required Type 1 tag` 로 보고하므로, 같은 사실을 두 번 적으면 보고서만 부풀고
원인은 그대로다.

### 2.5 카드 문구와 다른 점 — TS UID "인코딩 불일치" 는 검사하지 않는다

카드 1항은 "meta 있으나 **TS UID 가 데이터셋 인코딩과 불일치**" 케이스를 요구했다.
**이것은 DCMTK 를 통해서는 검출할 수 없다.** 파서가 meta 의 TransferSyntaxUID 를 **신뢰해서**
데이터셋을 읽기 때문에, `ds->getOriginalXfer()` 는 언제나 meta 가 적은 값과 같다.
둘을 비교하면 항상 일치하므로 검사로 성립하지 않는다.

그래서 같은 의도("meta 내용이 데이터셋과 불일치")를 **검출 가능한 축**으로 옮겼다 —
`MediaStorageSOPClassUID` 대 데이터셋 `SOPClassUID`. 테스트
`ValidateMetaSopClassMismatch_ReportsInconsistency` 가 meta 만 SecondaryCapture 로
바꾸고 데이터셋은 DX 로 두어 이 축을 시험한다. TS 쪽은 **존재와 형식**까지만 본다.
카드 문구를 그대로 구현한 척하지 않는다.

### 2.6 재실측 (C4, C5) — `_verify.log`, 필터 없음

```
===CI_POST===   100% tests passed, 0 tests failed out of 451   ===POST_EXIT=0===
===CI_AI===     100% tests passed, 0 tests failed out of 198   ===AI_EXIT=0===
===CI_DICOM===  100% tests passed, 0 tests failed out of 138   ===DICOM_EXIT=0===
```

| | B-35 | B-36 | 차 |
|---|---:|---:|---:|
| ci-dicom | 136 | **138** | +2 (신규 3 − 철회 케이스 대체 1) |
| ci-post / ci-ai | 451 / 198 | 451 / 198 | 0 |

빌드 경고 0 (`XPE_WARNINGS_AS_ERRORS=ON`).

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| RED | 13건 중 2건 실패 | `_red.log` (구현 전, 이번 실행) |
| GREEN | 13건 전부 통과 | `_green.log` (구현 후, 이번 실행) |
| ci-dicom 이전 | 136 | QA-B-35 `_dicom.log` |
| ci-dicom 현재 | 138 | `_verify.log` (이번 실행) |
| ci-post / ci-ai | 451 / 198 | `_verify.log` (이번 실행) |

---

## 4. 미검증 (Gaps)

- **커버리지를 재지 않았다.** 로컬 OpenCppCoverage 부재는 그대로다. 다만 이번 변경은
  `DicomValidator.cpp` 에 **커버되는 코드 65줄을 더한다**(테스트 3건이 지난다).
  임계 0.80 대비 어떻게 움직이는지는 다음 dispatch 로만 확인된다.
- **`0002,0003`(SOPInstanceUID) 불일치는 테스트하지 않았다.** 구현은 SOP Class 와 같은
  루프로 처리하지만, 시험한 것은 Class 축 하나다. 두 축이 같은 코드 경로라는 것이
  근거이지 관측은 아니다.
- **TS UID 형식 위반 분기를 시험하지 않았다.** meta 에 형식이 깨진 TS UID 를 넣는
  픽스처는 만들지 않았다.
- **다른 SOP Class(비-DX) 파일로는 확인하지 않았다.** 왕복 확인은 writer 가 쓰는
  DX Presentation 한 종류다.
- **api-spec 반영 여부는 이 보고서 밖이다.** §5 는 초안이고, `docs/` 는 main 소유다.

---

## 5. api-spec §11 문구 초안 (반영은 leader)

> **File meta information (PS3.10 group 0002).** `xpe_dicom_validate` requires the
> file to carry a Part 10 file meta information group, and requires its content to
> agree with the dataset. A file is reported `valid: false` when any of the
> following holds:
>
> - the file has no meta information group (a bare dataset written without a
>   Part 10 header);
> - `TransferSyntaxUID (0002,0010)` is absent, or is not a well-formed UID;
> - `MediaStorageSOPClassUID (0002,0002)` is absent, or differs from the
>   dataset's `SOPClassUID (0008,0016)`;
> - `MediaStorageSOPInstanceUID (0002,0003)` is absent, or differs from the
>   dataset's `SOPInstanceUID (0008,0018)`.
>
> Each failing condition contributes one entry to `errors`, tagged with the
> group-0002 element it concerns. When a dataset-side tag is itself missing, the
> missing-Type-1 report covers it and no separate meta mismatch is reported.
>
> Note: the meta `TransferSyntaxUID` is checked for presence and format only.
> Whether it describes the dataset's actual encoding cannot be verified through
> the parser, which trusts that element when reading the dataset.

---

## 6. 잔여 위험 (Residual-risk)

- **기존 파일 중 이 규칙에 새로 걸리는 것이 있을 수 있다.** 이 프로젝트 writer 산출물은
  통과하지만(§2.1), 외부 PACS·스캐너가 만든 파일 중 meta 와 데이터셋의 SOP Instance UID 가
  어긋난 것은 이제 `valid=false` 가 된다. 규격상 옳은 판정이지만 **동작 변경**이므로
  기존 통과 파일이 탈락할 수 있다는 뜻이다.
- **`card()` 는 DCMTK 의 합성 동작에 기댄 관측이다.** DCMTK 가 dataset 전용 파일에서
  meta 를 채우도록 바뀌면 첫 번째 검사가 조용히 무력해진다. 그때 실패하는 것은
  `ValidateDatasetWithoutMetaHeader_ReportsMissingMeta` 이므로 침묵하지는 않는다.
- **`EWM_dontUpdateMeta` 픽스처에 의존한다.** DCMTK 가 저장 시 meta 를 강제로 재생성하도록
  바뀌면 불일치 픽스처를 만들 수 없고, 그 테스트는 통과하면서 표적을 잃는다.
- 커밋 2건은 push 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_dicom.bat` | ci-dicom 빌드 + `-R DicomValidator` |
| `_red.log` | 구현 전 — 13건 중 2건 실패 |
| `_green.log` | 구현 후 — 13/13, 경고 0 |
| `_verify.bat` / `_verify.log` | 재실측 451 / 198 / 138 |
