# QA-B-76 게이트 보고서 — `.57` 픽스처 헬퍼 한 곳을 고쳐 사각 4건을 닫음

**카드**: QA-B-76 · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋**: `fbd20cc` (시험 파일 하나, `Refs #174`) · 이슈 코멘트 없음(카드에 요구 없음) · push 없음
**BUILD_EXIT**: 세 프리셋 모두 `BUILD=0`. `DICOM_EXIT=0` / `AI_EXIT=0` / `POST_EXIT=8` — `POST_EXIT` 는 §6 참고. ctest 결과는 533 / 224 / 192.

---

## 1. 주장

| # | 항목 | 결과 |
|---|---|---|
| 1 | 원인 수정 — 시험 넷이 아니라 헬퍼 하나 | `WriteInTransferSyntax`·`WriteDatasetOnly`·`WriteWithMeta` 가 모두 `ChooseDistinctP14Representation` 를 거친다(predictor 2) |
| 2 | 헬퍼 자체 단언 | 인코딩된 첫 조각의 SOS `Ss == 1` 이면 `ADD_FAILURE` 를 내고 `false` 반환 |
| 3 | 4건 모두 이제 다른 입력을 받음 | **확인** (§3) |
| 4 | 반증 — predictor 1 로 되돌리기 | **빨강** — 헬퍼 메시지 4회, 시험 4건 FAILED |
| 덤 | T501 `expectedEITarget` | 단언을 넣어도 **통과** (9/9) |
| 덤 | T504 `expectAlert` | 단언을 넣으면 **실패** (5행 중 3행) |

## 2. 수정 내용 (`modules/dicom/tests/test_dicom_reader.cpp`)

- `const int kP14FixturePredictor = 2;` 를 추가했다. 주석에 QA-B-73·75 경위를 적었다.
- `ChooseDistinctP14Representation(DcmDataset*)` 를 추가했다.
  - `DJ_RPLossless(kP14FixturePredictor, 0)` 로 `.57` 표현을 고른다.
  - 같은 파라미터를 키로 첫 조각을 꺼낸다.
  - 그 조각을 `ParseSofSos` 로 읽는다.
  - `Ss` 를 찾지 못했거나 `Ss == 1` 이면 `ADD_FAILURE` 를 낸다.
- 위 함수는 스트림 파서 옆에 정의했다. `WriteInTransferSyntax` 가 그보다 앞에 있어서 전방 선언을 두었다.
- 세 헬퍼에서 `.57` 을 쓰는 부분만 바꿨다. 다른 구문은 이전과 같다.

**첫 시도는 빨강이었다**(`_b76_first.log`, `BUILD=0`, 4건 FAILED, "no encoded fragment to inspect").
- 원인: 첫 조각을 꺼낼 때 파라미터를 `nullptr` 로 넘겼다. `getEncapsulatedRepresentation` 은 이 파라미터를 **조회 키**로 쓰기 때문에, 기본값이 아닌 파라미터로 저장된 표현을 찾지 못했다.
- 수정: 같은 `&params` 를 넘기게 고쳤다 → 6/6 통과(`_b76_second.log`).
- 이 빨강은 헬퍼 단언이 "조각을 못 읽음"도 실패로 처리한다는 것을 우연히 보여 주었다. 이 부분은 조용한 통과가 아니다.

## 3. 4건이 이제 서로 다른 입력을 받는가 (`_hash.log`, B-75 계측을 다시 붙였다가 뗌)

같은 실행 안에서 비교한 값이다. 파일 해시는 실행마다 달라진다. 생성된 UID 때문으로 보이며, 이 원인은 측정하지 않았다.

| 시험 | `.70` | `.57` |
|---|---|---|
| `EverySupportedTransferSyntaxActuallyReads` | 파일 `ab1762ea…`(17902) / 조각 `7d40f2a3…`(16740) | 파일 `c68c1f59…`(82926) / 조각 `3f806de1…`(81766) |
| `KnownDivergence_MetaLessBranchAndDetectedSyntaxAreMeasured` | 파일 `6e86c9cc…`(17556) | 파일 `a5a090d7…`(82580) |
| `KnownDivergence_EncapsulationSignalsAndMetaNullReachability` | 파일 `6e86c9cc…`(17556) | 파일 `a5a090d7…`(82580) |
| `TsLessPathIsDecidedByChecksNotByStructure` | TS-less `6e86c9cc…` / labelled `ab1762ea…` | TS-less `a5a090d7…` / labelled `c68c1f59…` |

`TsLessPath…` 의 행 7개 → 서로 다른 파일 7개(수정 전에는 6개). 계측을 뗀 뒤 소스가 `reader_b76` 복사본과 같음을 확인했고, `grep -c TMPB` 는 0이다.

### `TsLessPathIsDecidedByChecksNotByStructure` 가 이제 무엇을 구별하나 (카드 §2)

```
TS-less .70:                 open=-7 read=-12 pixels=0
TS-less .57:                 open=-7 read=-12 pixels=0
labelled .70 (normal path):  open=0  read=0   pixels=1 256x256
labelled .57 (normal path):  open=0  read=0   pixels=1 256x256
```

- **TS-less 두 행**은 기대값이 여전히 같다(`-7`). 이 두 행이 보여 주는 것은 하나다. **압축 내용(predictor 1 스트림이든 predictor 2 스트림이든)과 상관없이, 메타 없는 캡슐화 파일은 거부된다.** 두 행은 `.70` 과 `.57` 을 서로 구별하지 않는다.
  - 어느 검사(①/②)가 거부했는지는 이 시험이 기록하지 않는다. `-7` 만으로는 어느 분기인지 알 수 없고, 이번 카드에서도 재지 않았다.
- **labelled `.57` 행**은 이제 predictor 2 스트림을 정상 경로로 읽어 화소를 낸다. 수정 전에는 `.70` 과 같은 스트림이었으므로, **이 행이 `.57` 고유 스트림을 여는 관측이 된 것은 이번이 처음**이다.

## 4. 반증 (`_falsify.log`)

`kP14FixturePredictor = 1` 로 바꾼 결과, `BUILD=0`:

```
4 × ".57 fixture carries predictor 1 -- predictor 1 makes it the .70 stream again (QA-B-75)"
[  FAILED  ] EverySupportedTransferSyntaxActuallyReads
[  FAILED  ] KnownDivergence_EncapsulationSignalsAndMetaNullReachability
[  FAILED  ] KnownDivergence_MetaLessBranchAndDetectedSyntaxAreMeasured
[  FAILED  ] TsLessPathIsDecidedByChecksNotByStructure
[  PASSED  ] 2 tests.   ===EXIT=1===
```

측정 전용 시험 두 개(`SUCCEED()` 만 있는 것)도 헬퍼를 통해 빨강이 됐다. 원복했고, 원복 후 소스에 `kP14FixturePredictor = 2` 가 1곳 있음을 확인했다.

## 5. 덤 — T501 / T504 (임시 단언 → 원복, `_ei_assert.log`)

**첫 실행은 버렸다.**
- 첫 번째: 패치가 적용되지 않은 소스로 돌았다.
- 두 번째: 문자열 안에 줄바꿈이 들어가 `C2001` 이 났고 `BUILD=1` 이었다. 이때 찍힌 "PASSED 2" 는 **옛 바이너리의 결과**다.
- 세 번째 실행이 `BUILD=0` 이었고, 아래 수치는 이 실행의 것이다.

### T501 — 통과 (단언이 빠져 있었을 뿐)

API 는 목표값을 따로 돌려주지 않는다. 그래서 파일 머리말의 식 `DI = 10*log10(EI/EI_target)` 으로 거꾸로 계산했다: `implied = EI / 10^(DI/10)`. 허용 오차는 1% 로 잡았다.

```
CHEST 250/250  CHEST LAT 200/200  ABDOMEN 400/400  PELVIS 350/350  SKULL 500/500
EXTREMITY 100/100  SPINE 300/300  chest 250/250  AbDoMeN 400/400
```

9/9 일치. 대소문자 행 두 개도 일치했다. 이 결과는 위 식이 코드의 식과 같다는 **가정** 위에 있다. 코드의 식은 이번에 읽지 않았다.

### T504 — 실패 (표와 코드가 다름)

판정식은 시험 안에 이미 있는 `qcAlert = |DI| > 3` 을 그대로 썼다.

| 행 | fill | kVp/mAs | DI | alert | 표의 `expectAlert` | |
|---|---|---|---|---|---|---|
| Very dark | 0.01 | 60/5 | -29.49 | 1 | true | 일치 |
| Dark | 0.1 | 70/8 | -16.11 | 1 | **false** | 불일치 |
| **Normal** | 1.0 | 80/10 | **-3.98** | **1** | **false** | **불일치** |
| Bright | 10 | 90/12 | 7.84 | 1 | **false** | 불일치 |
| Very bright | 100 | 120/20 | 22.55 | 1 | true | 일치 |

- 표의 주석에는 Dark·Bright 가 "May or may not trigger" 로 적혀 있다.
- **"Normal image" 행에는 그런 주석이 없는데 `|DI| = 3.98` 로 경보가 켜진다.**
- 다섯 행 모두 경보가 켜진다. 즉 이 픽스처 범위에서 경보는 입력을 가르지 못한다.
- **표가 틀렸는지 코드가 틀렸는지는 판정하지 않았다.**
  - T501 의 fill=1.0 행(kVp 80, mAs 10, CHEST)도 DI 를 찍지만, 그 값은 이번 로그에서 따로 대조하지 않았다.
  - `#154`(EIT 상쇄)와의 관계는 확인하지 않았다.

## 6. 전체 검증 (`_verify.log`, `_post_rerun.log`, `_t308_isolated.log`)

| 프리셋 | BUILD | 결과 |
|---|---|---|
| ci-post | 0 | 533건 중 **1건 실패** — `EdgeEnhancementTest.T308_PerformanceBudget` (`POST_EXIT=8`) |
| ci-ai-b20 | 0 | 224/224 |
| ci-dicom | 0 | **192/192** |

- **T308 따로 3회 실행**: 92 / 92 / 93 ms, 모두 OK.
- **ctest ci-post 재실행**: 533/533 통과(`EXIT=0`).
- 실패한 실행의 측정값은 ctest 로그에 출력되지 않아 남아 있지 않다. 이 카드는 enhance_advanced 코드를 바꾸지 않았다. 임시 단언을 넣었던 `test_exposure_index.cpp` 는 원복한 뒤 빌드된 상태다.

## 7. 미검증 (Gaps)

- **`.57` 을 인코더 기본값으로 쓰는 다른 호출은 고치지 않았다.** `chooseRepresentation(EXS_JPEGProcess14, nullptr)` 가 약 1850행과 2057행에 남아 있다. 약 2081행의 주석은 이를 "predictor 1" 로 명시하고 있다. 이 호출들은 루프가 아닌 단일 호출이고 이 카드의 범위(4건) 밖이다.
- predictor 2 한 가지만 썼다. 다른 값으로 바꿔도 네 시험이 통과하는지는 재지 않았다.
- T504 에서 어느 쪽(표/코드)이 맞는지, T501 에서 거꾸로 계산한 식이 코드와 같은지는 확인하지 않았다.
- TS-less 거부가 어느 검사에서 났는지는 측정하지 않았다.

## 8. 잔여 위험

- 헬퍼 단언은 **이 세 헬퍼를 거치는 경로만** 지킨다. 새 시험이 `chooseRepresentation` 을 직접 부르면 같은 사각이 다시 생긴다.
- T308 은 부하에 민감한 성능 게이트다. 전체 실행에서 한 번 실패했다.
- 커밋은 push 전까지 이 로컬 브랜치에만 있는 유일본이다.

## 부록 — 증거

`_b76.bat`, `_env.bat`, `_verify.bat`, `_ei.bat`, `_b76_first.log`, `_b76_second.log`, `_hash.log`, `_falsify.log`, `_ei_assert.log`, `_verify.log`, `_t308_isolated.log`, `_post_rerun.log`
