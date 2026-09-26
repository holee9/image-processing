# QA-B-75 게이트 보고서 — 여러 입력을 돈다는 시험, 입력이 정말 다른가

**카드**: QA-B-75 · **레인**: Lane B (`xpe-post`, `dev/postprocess`, HEAD `8ef44c9`)
**코드 변경 없음, 커밋 없음, 이슈 코멘트 없음**(카드 §3). 임시 계측은 원복했고 `git status` 에 추적 파일 변경이 없다.

---

## 1. 주장 (Claim)

| 분류 | 확인 | 잠재 |
|---|---|---|
| **B** (입력 개수만 셈) | **0** | **0** |
| **C** (아무것도 안 셈) | **4** — 모두 dicom, 모두 `.70`/`.57` 쌍 | **0** |

C 확인 4건은 원인이 하나다. **predictor 1 `.57` 이 `.70` 과 같은 스트림**이라는 B-73 발견이 B-74 가 고친 시험 밖에도 남아 있었다.
그중 3건은 **파일 자체가 바이트 단위로 같다**(메타 없는 파일이라 라벨로도 구별되지 않는다).

## 2. 방법

1. **이름이 아니라 구조로 후보를 모았다.** 여섯 모듈의 `tests/*.cpp` 와 `tests/e2e_post_pipeline` 에서:
   - 루프 본문에서 API(`xpe_*` 등)나 입력 생성기를 부르는 루프 — `_scan.txt`, 47건
   - 범위 기반 for 전부 — `_rangefor.txt`, 91건
   - 시험 본문 최상위(들여쓰기 4칸)의 인덱스 루프 전부 — `_toplevel.txt`, 183건

   위 목록을 픽셀 채우기·로그 출력 루프와 나머지로 나눈 뒤, 나머지의 본문을 읽었다(`_bodies.txt`).
2. **대조군**: 스캐너가 이미 아는 사례 두 건(`KnownDivergence_Sv1EncoderIgnoresPredictorArgument` 912행, `ReadJpegLosslessProcess14AllPredictors_PixelExactAndDistinct` 2803행)을 후보로 잡았다. 스캐너가 이 모양을 볼 수 있다는 뜻이다.
3. **판정 기준 — 사각이 생기는 자리**: 루프 변수와 API가 받는 입력 사이에 **변환층(생성기·인코더·헬퍼)** 이 있고, 시험이 그 결과가 서로 다른지를 단언하지 않는 경우.
   - 루프 변수가 리터럴 그대로 API에 들어가면 입력이 같아질 경로가 없다. 이런 루프는 아래 §4 "제외" 표에 적었다.
   - 헬퍼가 인자를 그대로 복사하는 것은 **판독으로 확인**한 경우만 제외했다.
4. **실행 확인**: dicom 변환층 후보 6개 시험에 임시 계측(`TMPB75`)을 넣었다. 루프마다 두 해시를 찍었다.
   - 생성 파일 전체의 FNV-1a 해시
   - 첫 조각(네이티브면 PixelData)의 해시

   `BUILD=0`, 6/6 통과(`_hash.log`). 계측을 원복한 뒤 다시 빌드해 `BUILD=0`, 6/6 통과를 확인했다(`_restored.log`). 원복 뒤 `grep -c TMPB75` → 0.

## 3. 발견 (B/C) — 증거는 `_hash.log`

| # | 시험 (`test_dicom_reader.cpp`) | 분류 | 실행 결과 |
|---|---|---|---|
| 1 | `TsLessPathIsDecidedByChecksNotByStructure` (2653행 루프) | **C 확인** | `TS-less .70` 과 `TS-less .57` 이 **같은 파일** — `file=d2519cb8332ad9c0(17556)` 두 번. 행 7개, 서로 다른 파일 6개. 두 행 모두 기대값이 `XPE_ERR_UNSUPPORTED_FORMAT` 이라 통과한다 |
| 2 | `EverySupportedTransferSyntaxActuallyReads` (669행 루프) | **C 확인 (스트림 수준)** | `.70`/`.57` 파일은 다르다(`f9829ecb…` / `c755a009…`, 둘 다 17902B, 라벨이 다름). 첫 조각은 **같다**: `pix=7d40f2a3…(16740)`. 구문 4개, 서로 다른 압축 스트림 3개. 화소 일치만 단언한다 |
| 3 | `KnownDivergence_MetaLessBranchAndDetectedSyntaxAreMeasured` (2496행 루프) | **C 확인** | `.70`/`.57` 행이 같은 파일 `d2519cb8…(17556)`. 행 5개, 파일 4개. 측정 시험(`SUCCEED()`)이라 단언이 없다 |
| 4 | `KnownDivergence_EncapsulationSignalsAndMetaNullReachability` (2553행 첫 루프) | **C 확인** | 3번과 같은 두 파일(`d2519cb8…`). 측정 시험 |

1·3·4번은 모두 `WriteDatasetOnly(src, dst, EXS_JPEGProcess14SV1 / EXS_JPEGProcess14)` 를 인코더 기본값(predictor 1)으로 부른다. 메타 헤더가 없으니 라벨도 없어서 파일이 바이트 단위로 같다.
2번은 `WriteInTransferSyntax` 의 `.57` 분기가 `chooseRepresentation(EXS_JPEGProcess14, nullptr)`, 즉 predictor 1 을 쓴다.

**참고 — 3·4번이 기록하는 "`.57` 행"은 `.70` 행과 같은 입력의 측정이다.** 이 행들을 근거로 쓴 과거 보고(B-71·B-72)의 `.57` 수치는 `.70` 과 독립된 관측이 아니다. 이 문단은 사실만 적었고, 판정은 하지 않는다.

`TmpB75` 가 찍은 `xfer=` 값(dataset-only 파일에서 `1.2.840.10008.1.2.1`)은 이 카드의 계측 방식(`loadFile(EXS_Unknown)` 한 번)으로 얻은 값이다. B-71 측정과는 조건이 다르므로 여기서는 근거로 쓰지 않았다. 동일성 판정은 파일 해시로만 했다.

## 4. 검토했지만 B/C 가 아닌 것

### 구조는 C, 실행 결과 입력은 서로 다름 (지금은 사각 없음)

| 시험 | 결과 |
|---|---|
| `DicomJ2kFailureTest.FailedReadLeavesOutputUntouched` (1302) | 파일 4개 모두 다름. 그중 garbage·truncated 두 행은 기대값이 같다(`-3`). 생성기가 모양을 무시하게 바뀌어도 이 두 행은 통과한다 — **다름을 단언하지 않는 구조** |
| `CompressedMatchedSize_StillReadsNormally` (1598) | J2K(`c9e115e8…`, .90) / JPEG-LL(`906d77dc…`, .70) 서로 다름 |

### 구조는 C, 헬퍼가 인자를 그대로 복사함 (판독)

`createMetadata`(`test_exposure_index.cpp:68`), `MakeMeta`(`_ext.cpp:50`, `test_mfp_scalar_ext.cpp:88`, `test_coverage_ext.cpp:41`), `MakeImage`(`test_coverage_ext.cpp:27`) 이 모두 `strncpy_s`/`snprintf`/대입으로 인자를 그대로 복사한다.

- `T501_BodyPartEITargetLookup`, `T504_QCAlertForHighDI`, `DifferentBodyPartsProduceFiniteResults`, `MultipleBodyPartsSucceed`
- `MultiscaleMaxLevelsAcrossSizes` / `OnNonSquareImages` / `OnTinyImages`

**부수 관찰(판독, 이 카드의 분류와는 다른 사각)**: 입력은 달라도 **출력을 행별 기대값과 비교하지 않는다.**
- `T501` 은 표에 `expectedEITarget` 을 적어 두고 쓰지 않는다.
- `T504` 는 `expectAlert` 를 쓰지 않는다. `|DI|>3` 은 출력(`std::cout`)만 하고 단언하지 않는다.

두 시험 모두 finite 여부만 단언한다. 부위나 노출을 무시하는 구현도 통과한다.

### 리터럴이 API에 직접 들어감 (변환층 없음)

- enhance_advanced
  - `test_edge_enhancement.cpp`: T302(267), T305(458), T306(507)
  - `test_edge_enhancement_ext.cpp`: `DisableOvershootViaConfigRejected`(276)
  - `test_exposure_index.cpp`: T506(421)
  - `test_config_value_dependency.cpp`: 211, 401
  - `test_config_warning_once.cpp`: 183
- 함수 표를 도는 시험
  - `*_datasize_overread` 4개(enhance_basic 178, enhance_advanced 152, ai 143, display 148)
  - `EmptyImageContract` 표(enhance_basic 119·127·135·145, display 558·568·578·590, dicom writer 380·391·402·414)
- dicom 리터럴 목록 순회
  - `kNativeSyntaxes` 교차검사(2267)
  - raw 바이트 목록(2590)

### 같은 입력을 의도적으로 반복함 (스레드·누수·재진입)

- ai
  - `test_ai_abi.cpp`: 130
  - `test_ai_fallback.cpp`: 76, 589/592
  - `test_ai_worker_isolation.cpp`: 134, 160, 199, 261, 277, 282
- dicom
  - `test_dicom_reader.cpp`: 966/968, 1223/1231
  - `test_dicom_writer.cpp`: 276/279
- enhance_advanced
  - `test_config_warning_once.cpp`: 161, 200, 219, 228, 270
  - `test_integration.cpp`: 325, 416, 546, 839/842
  - `test_integration_ext.cpp`: 208, 230
  - `test_mfp_scalar_ext.cpp`: 387
  - `test_config_value_dependency.cpp`: 476(알림 비우기)
- enhance_basic: `test_enhance_integration.cpp` 179/217(픽셀), 242/249, 309/313
- display: `test_display_integration.cpp` 460/465/468
- gsvg
  - `test_gsvg_abi_smoke.cpp`: 219, 372/375
  - `test_gsvg_benchmark.cpp`: 14
  - `test_gsvg_degraded.cpp`: 215

### 이미 서로 다름을 다루는 시험 (A 또는 측정)

- `ReadJpegLosslessProcess14AllPredictors_PixelExactAndDistinct` (2803) — A
- `KnownDivergence_Sv1EncoderIgnoresPredictorArgument` (912) — 스트림 수를 단언(=1)
- `KnownDivergence_JpegLosslessPredictorInBitstreamIsMeasured` (2700) — 행마다 Ss 와 조각을 측정해 기록

## 5. Baseline 귀속

| 항목 | 값 | 근거 |
|---|---|---|
| 스캔 후보 | 47 / 91 / 183 | `_scan.txt` / `_rangefor.txt` / `_toplevel.txt` |
| 계측 빌드·실행 | `BUILD=0`, 6/6 PASSED | `_hash.log` |
| 원복 빌드·실행 | `BUILD=0`, 6/6 PASSED, `TMPB75` 0건 | `_restored.log` |
| `TEST_P`/`INSTANTIATE` | 0건 (여섯 모듈 + e2e) | grep |
| `tests/enhance_advanced_tests` | 이 워크트리에 없음 | `ls` |

## 6. 미검증 (Gaps)

- **스캐너는 휴리스틱이다.** 다음 경우는 놓쳤을 수 있다.
  - 들여쓰기가 4칸이 아닌 최상위 루프
  - `while`/`do` 루프(최상위 `while` 은 `mock_scp.cpp` 두 건만 보였다)
  - 헬퍼 함수·픽스처 `SetUp` 안에서 입력을 만드는 루프
  - 루프 없이 복사·붙여넣기로 여러 번 부르는 시험 — 이 카드의 탐색 방법으로는 원리상 보이지 않는다
- **실행 확인은 dicom 6개 시험만 했다.** enhance_advanced 헬퍼 쪽은 판독으로만 제외했다.
- ctest 전체는 돌리지 않았다. 코드 변경이 없고, 원복 뒤 해당 필터 6건만 확인했다.
- §4 "리터럴 직접" 표의 함수 표 시험은 **표에 같은 함수가 두 번 들어가도 통과**한다. 리터럴이 눈에 보이므로 제외했지만 단언은 없다.

## 7. 잔여 위험

- `.70`/`.57` 쌍은 **이 카드가 찾은 4건이 전부라는 보장이 없다.** 같은 생성기(`WriteDatasetOnly`, `WriteWithMeta`, `WriteInTransferSyntax`)를 쓰는 루프 밖의 단일 호출 시험은 세지 않았다.
- 순서 결정은 리더 몫이다(카드 §1).

## 부록 — 증거 파일

`_scan.txt`, `_rangefor.txt`, `_toplevel.txt`, `_bodies.txt`, `_b75.bat`, `_env.bat`, `_hash.log`, `_restored.log`
