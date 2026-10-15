# QA-B-196 — AI SPEC 작업표 실태 대조 + T-005 사이드카 스키마 설계 (보고서, 코드 변경 없음)

카드: QA-B-196 · 관련: #130 · 트리: `dev/postprocess` 7304a6f0 · 증거: 같은 디렉터리의 `evidence_greps.txt`(명령과 원문 출력), `req_coverage_script.txt`(파이썬 소스, `.py` 는 저장소 규칙상 추적되지 않아 `.txt`) + `req_coverage_out.txt`, 표 수정안 `tasks_md_draft.md`

## 0. 결론 먼저

1. **표가 틀린 곳은 `pending` 만이 아니다.** 16개 중 `done` 으로 적힌 T-003·T-004 는 `done` 이 아니라 **부분**이다(T-003: 실행 제공자는 CPU 만 등록, T-004: 사이드카의 메타데이터를 읽어 저장은 하지만 **아무도 읽지 않는다**). `pending` 으로 적힌 T-008·T-013 은 **부분**, T-010·T-012 는 **구현됨**(조건부)이다. 나머지는 표와 맞다. 요약은 §1.
2. **T-004 의 `done` 은 시험이 단언하는 대상이 상수라서 생긴 `done` 이다.** `TC-MODELCARD` 시험들(`AiModelVersioningTest.*`, `AiModelCardTest.*`)은 `xpe_ai_get_model_card` 가 돌려주는 JSON 에 `pccp_scope`·`training_data_hash`·`model_version`… 이 **있다**고 단언하는데, 그 JSON 은 `buildStubModelCard` 가 만드는 하드코딩 문자열이고(`ai.cpp:519~525`) 모델이나 사이드카와 아무 관계가 없다. 사이드카를 읽는 코드(`LoadMetadataFromText` → `OnnxSession::GetModelMetadata`)는 있지만 `GetModelMetadata` 의 호출자가 **0곳**이고 시험도 **0건**이다(`evidence_greps.txt`).
3. **T-005 는 한 작업이 아니라 서로 다른 문서 세 개다**(§2.1): ① 모델 파일 옆 사이드카 `<이름>.json`(195 가 서명 대상으로 삼은 입력), ② 모델 카드(`xpe_ai_get_model_card` 의 출력, REQ-AI-010/011), ③ 추론 사이드카(영상마다 나가는 출력, REQ-AI-004·072·082·101). REQ-AI-004 는 ③ 이고, 표가 가리키는 `model-card.schema.json`(REQ-AI-011)은 ② 다. **REQ-AI-010·011 은 어떤 작업에도 걸려 있지 않다**(46개 REQ 중 20개가 어떤 작업에도 없다, §1.2).
4. 지금 있는 사이드카에는 **REQ-AI-008 의 다섯 필드**(`model_id`·`version`·`pccp_scope`·`training_data_hash`·`validation_metrics`)를 읽는 코드가 있고, 부위 인식 모델의 `labels` 가 있다. REQ-AI-010 의 일곱 필드 중 **다섯 개**(`intended_use`·`training_data_summary`·`demographic_performance`·`limitations`·`published_date`)와 `pccp_status`(파생값)를 담을 곳이 없다. 스키마 파일도, 스키마를 두는 디렉터리(`schemas/`)도 저장소에 없다.
5. **권고는 §2.5, 리더 결정이 필요한 것은 §5**(D1~D6). 서명과의 관계는 단순하다: 서명 메시지는 사이드카의 **원시 바이트**를 덮고 스키마를 모르므로, 스키마 변경은 서명 **형식**의 변경이 아니다. 다만 사이드카 내용을 바꾸면 그 모델의 `.sig` 를 새로 만들어야 한다.

## 1. 작업표 실태 대조 (T-001 ~ T-016)

판정 기준: **구현됨** = 시험이 그 REQ 문구를 단언한다. **부분** = 일부 문구만 코드와 시험이 있다. **없음** = 코드가 없거나 스텁이 `XPE_ERR_PROCESSING_FAILED` 만 돌려준다. 시험 개수는 `^TEST` 줄 수다.

| 작업 | 표 | 실제 | 근거 | REQ 가 요구하지만 빠진 것 |
|---|---|---|---|---|
| T-001 파이프 IPC 브리지 | done | **구현됨**(브리지). REQ-AI-003 은 기본 경로에서 미충족 | `ai_ipc_bridge.cpp`; `AiIpcBridgeTest.*`(10), `IpcDeadline.*`(27: `TheBudgetIsTheConfiguredValueNotAConstant`, `AReplyThatStopsHalfWayAndStallsIsNotCountedAsSuccess` …), `WorkerSupervisor.AfterTheWorkerIsKilledTheNextCallSucceedsOnANewPid` | REQ-AI-003 "main process crash-immune": 격리는 **opt-in 워커 경로**(`use_worker: true`)에서만이고 기본값은 끔 — 기본 경로는 프로세스 안에서 추론한다(T-006 메모와 같은 한계) |
| T-002 워커 골격 | done | **구현됨** | `ai_worker_main.cpp`; `WorkerOom.*`, `WorkerExeMissing.*`, `test_worker_protocol_conformance.cpp`, `test_bone_suppress_worker_path.cpp`(26) | REQ-AI-006 의 절반은 T-003 |
| T-003 ONNX 세션 관리자 | done | **부분** | `ai_onnx_session.cpp`; `OnnxSession*`(11). 헤더 주석 "Only the CPU EP is registered"; `OnnxSessionProviders.CpuIsReportedAndTheListComesFromTheRuntime` | REQ-AI-006 "EP selection configurable: CPU, CUDA, TensorRT, DirectML": 요청한 EP 가 없으면 CPU 로 떨어지고 **CPU 외 제공자는 등록하지 않는다**. 1.20+ 쪽은 충족(1.30.0 으로 빌드, 헤더 주석) |
| T-004 버전·메타데이터 파싱 | done | **부분** | `LoadMetadataFromText`(`ai_onnx_session.cpp:152`)가 다섯 필드를 읽어 `ModelMetadata` 에 저장. `GetModelMetadata` 호출자 **0곳**, 이를 단언하는 시험 **0건**. `TC-MODELCARD` 시험은 상수 카드를 본다(위 §0.2) | REQ-AI-008 "semver" 검증 없음(사이드카의 `version` 문자열을 아무도 검사하지 않음), 필수 필드 부재 검사 없음, 읽은 메타데이터가 어디에도 쓰이지 않음 |
| T-005 사이드카 스키마 | pending | **없음** | `ai_sidecar.*`·`model-card.schema.json`·`schemas/` 모두 없음 | §2 |
| T-006 `xpe_bodypart_recognize` | partial | **부분**(표와 같음) | 표의 메모 그대로. `BodyPart*`, `ModelRefusalBehavior.*`, `ModelLoadingTrust.*` | 실제 모델·정확도 없음(장난감 모델), 기본 경로 미격리 |
| T-007 `xpe_stitch_images` | pending | **없음**(입력 검증만) | 본문이 검증 뒤 `return XPE_ERR_PROCESSING_FAILED` 로 끝남(`// --- Stub implementation ---`). `xpe_stitch_estimate_size` 는 `width = max(width) × (1 + 0.7 × (n−1))` 휴리스틱 | REQ-AI-002 중 스티칭 알고리즘·결정론적 대체 경로 전체 |
| T-008 `xpe_bone_suppress` | pending | **부분** | 추론 경로(`OnnxSession::Run`)·opt-in 워커 경로·서명·입력 검증·결정론적 fallback 이 있다. `BoneSuppressAbi.*`(12), `WorkerPathFixture.*`/`BoneSuppressNonFinite.*`, `AiProcessedAlert.*` | REQ-AI-050(U-Net, DES 쌍으로 학습)·051(결절 민감도 +16.8%)은 실모델이 없어 **검증 불가**; 시험 모델은 ×2 장난감. REQ-AI-052(DICOM SR) 없음 |
| T-009 `xpe_dl_denoise` | pending | **없음**(입력 검증만) | 본문이 검증 뒤 `return XPE_ERR_PROCESSING_FAILED`(`// --- Stub implementation ---`) | REQ-AI-020·022 전부 |
| T-010 모델 서명·검증 | pending | **구현됨**(조건부) | `ai_model_signer.cpp/h`; `ModelSigner.*`(15), `ModelAssets.*`(3), `ModelLoadingTrust.*`(15), `ModelRefusalBehavior.*`(6); ECDSA P-256 over CNG, 독립 구현(Python/OpenSSL)이 만든 서명으로 시험 | **운영 서명 키가 없다**: 운영 신뢰 목록은 비어 있고 비어 있으면 모든 모델을 거부한다(#243, 실모델 입고 때). 키 보관·서명 절차 정의 없음. 한계(문서화됨): 실행 파일 교체는 못 막음, 롤백 방어 없음(D7) |
| T-011 PCCP 경계 | pending | **없음** | `pccp_scope` 를 읽는 한 줄(`ai_onnx_session.cpp:163`)뿐, 소비처 없음. `XPE_ERR_PCCP_EXCEEDED` 는 `xpe_error.h` 에 **없다**(코드 목록은 -17 까지) | REQ-AI-110 "배포된 AI-DSF 모듈의 승인된 PCCP 와 대조": 승인된 PCCP 의 **출처가 없다**(AI-DSF 모듈 부재). REQ-AI-111 의 오류 코드는 `xpe_common`(Lane A 소유)에 새로 필요. REQ-AI-112 감사 이벤트(REQ-REG-007)는 REG SPEC 에서 정의가 "PCCP 승인 수정 실행 시"여서 모델 적재 이벤트와 맞는지 확인 필요 |
| T-012 입력 검증 | pending | **구현됨**(QA-B-194 설계 해석대로) | `AiInputValidation.*`(28)·`AiInitHardening.*`·`AiOom.*`; 크기 경계(`TheDeclaredSizeOfEveryFormatIsBoundedByTheModuleMaximum`), 화소값 경계, 메타데이터 검사(`ADoseOrGeometryThatIsNotAFiniteNonNegativeNumberIsRefused…`, `ABodyPartThatIsNotATerminatedStringIsRefused`) | 해석의 한계: "화소값 경계" 는 **float 의 유한성**이고 정수형은 형식 자체가 경계(`IntegerFramesAreNeverScanned`) — `bitsStored` 초과 같은 범위 검사 없음. "DICOM 메타데이터 스키마 검사"는 `xpe_dl_denoise` 의 `XpeImageMetadata` 에만 있다(뼈 억제는 메타를 받지 않고, 부위 인식에는 없다) |
| T-013 시간 예산 | pending | **부분**(표 메모와 같음) | `IpcDeadline.*`(27), `BodyPartWorkerPath.*`; 뼈 억제·부위 인식의 opt-in 워커 경로에서 예산 초과 → 입력/UNKNOWN + 알림 | 기본 경로(워커 끔)와 `xpe_stitch_images`·`xpe_dl_denoise` 에는 예산 없음 |
| T-014 XAI 사이드카 | pending | **없음** | `xai`·`shap`·`gradcam`·`saliency` 단어 검색 0건(대조군 포함, `evidence_greps.txt`) | REQ-AI-070~073 전부 |
| T-015 적합 예측 | pending | **없음** | `conformal` 0건 | REQ-AI-080~083 전부 |
| T-016 드리프트 지문 | pending | **없음** | `fingerprint`·`drift_flagged` 0건 | REQ-AI-100·101 전부 |

### 1.1 표 상태 요약

| 표 상태 → 실제 | 작업 |
|---|---|
| 표와 맞음 | T-001(주석 필요)·T-002·T-005·T-006·T-007·T-009·T-011·T-014·T-015·T-016 |
| `done` → **부분** | **T-003, T-004** |
| `pending` → **부분** | **T-008, T-013** |
| `pending` → **구현됨(조건부)** | **T-010**(운영 키 전까지 출하 불가), **T-012**(해석 명시) |

### 1.2 어떤 작업에도 걸리지 않은 REQ

정의된 REQ-AI 46개 중 26개만 작업 행에 있다(`req_coverage_out.txt`, 대조군 REQ-AI-090 → T-012 로 파서 확인). 작업이 없는 20개: `001 005 010 011 021 023 024 030 031 032 033 040 041 042 051 052 060 061 062 093`. 이 중 이 카드와 직접 관련된 것:

- **REQ-AI-010·011(모델 카드와 스키마)**: 작업이 없다. T-005 의 "계획된 파일" 칸에만 `model-card.schema.json` 이 있고 REQ 칸은 004 뿐이다. 그래서 카드가 상수인 채로 `done`(T-004)에 가려졌다.
- **REQ-AI-093(최소 권한: 네트워크 없음, 쓰기는 사이드카 스크래치만)**: 작업이 없다. 워커는 kill-on-close 작업 객체에만 넣고(`ai_worker_supervisor.cpp:201~205`, `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` 하나) 네트워크·파일 쓰기를 제한하는 코드는 읽은 범위에서 없다(코드 읽기, 시험 없음).
- 나머지는 구현되었거나(001 계층 구조, 005 opt-in) 대상 함수가 없는 것(030~033·040~042·060~062 는 스티칭·조리개 검출 같은 아직 없는 기능, 051·052 는 T-008 의 실모델 의존)이다. 이것도 "작업 없음"이라는 사실 자체가 표의 빈 칸이다.

## 2. T-005 사이드카 메타데이터 스키마 (REQ-AI-004 / 008 / 010 / 011)

### 2.1 "사이드카"가 가리키는 것이 셋이다

| | ① 모델 사이드카 | ② 모델 카드 | ③ 추론 사이드카 |
|---|---|---|---|
| 정체 | 모델 파일 옆 `<이름>.json`, **입력** | `xpe_ai_get_model_card` 의 **출력** | 추론 한 번마다 나가는 **출력** |
| 요구 | REQ-AI-008(메타데이터 필드), 부위 인식 `labels` | REQ-AI-010(내용), 011(`schemas/model-card.schema.json` 적합) | REQ-AI-004(사이드카로 전달, `XpeImageMetadata` 변경 금지), 072·082·101 이 필드를 더함 |
| 지금 | 읽는 코드 있음(다섯 필드, `labels`), 서명 대상(195), **쓰는 곳 없음** | 하드코딩 상수 카드 4종, 모델·사이드카와 무관 | **없음** — 어떤 진입점도 사이드카를 내보내지 않는다(부위 인식은 라벨+신뢰도를 출력 인자로, 뼈 억제는 영상만) |
| 스키마 | 없음 | 없음(`schemas/` 디렉터리 자체가 없음) | 없음. 최소 필드는 `xpe-algorithm-spec-deepsync.md` §8.2 "AI confidence sidecar"(`model_id`, `model_version`, `confidence`, `failure_code`, `fallback_used`)에만 있고, 그 문서가 구체 구조체를 두겠다고 한 `xpe-implementation-reference.md` 에는 sidecar 언급이 **0건** |

REQ-AI-004 가 요구하는 ③ 은 지금 **없음**이다. "REQ-AI-004 가 요구하는 필드와 지금 사이드카가 가진 필드를 나란히" 에 대한 답은 아래 두 표다(②의 일곱 필드는 ①에서 와야 하고, ③ 은 별개).

### 2.2 모델 카드(②)가 요구하는 필드 대 ①이 가진 필드

| 필드(REQ) | ① 사이드카가 지금 가짐? | 카드가 지금 값을 얻는 곳 |
|---|---|---|
| `model_id`(008) | 읽음(`model_id`; 부위 인식 픽스처는 가짐) | 하드코딩 4개 ID 중 요청한 ID |
| `version` = `model_version`(008·010) | 읽음(`version`, semver 검증 없음; 부위 인식 픽스처는 가짐) | 하드코딩 `0.1.0-stub` |
| `pccp_scope`(008) → `pccp_status`(010) | 읽음(`pccp_scope`), 소비처 없음; 어떤 픽스처에도 없음 | 하드코딩 `not_applicable` |
| `training_data_hash`(008) | 읽음 | 하드코딩 `N/A` |
| `validation_metrics`(008) | 읽음(JSON 덤프 문자열) | 하드코딩 `{psnr:0.0, ssim:0.0}` |
| `intended_use`(010) | **없음** | 하드코딩 "XPE AI inference (stub -- ONNX Runtime not linked)" |
| `training_data_summary`(010) | **없음** | 하드코딩 "N/A (stub)" |
| `demographic_performance`(010) | **없음** | 하드코딩 `{}` |
| `limitations`(010) | **없음** | 하드코딩 "This is a stub build…" |
| `published_date`(010) | **없음** | 하드코딩 `2026-04-22` |
| `labels`(부위 인식) | 읽음(부위 인식 전용) | 카드에 없음 |

**발견(별도 처리 필요)**: 풀 빌드에서 실제 모델을 적재해도 카드는 "stub build, ONNX Runtime is not linked", `published_date: 2026-04-22` 를 돌려준다. 모델에 대한 **사실이 아닌 진술**이고 REQ-REG-020/021(GUI 의 Device Information 화면이 모델 버전·PCCP 상태를 보여 줌)이 이 카드를 소비하게 되면 그대로 사용자에게 간다. 실모델이 없는 지금은 드러나지 않는다.

### 2.3 ③ 추론 사이드카가 담아야 하는 것(요구들의 합집합)

| 필드 | 출처 |
|---|---|
| `schema_version`, `model_id`, `model_version`, `confidence`, `failure_code`, `fallback_used` | deepsync §8.2 "AI confidence sidecar" 최소 필드 |
| XAI: `method`, `saliency_map_reference`, `disclaimer`(REQ-AI-073 의 고정 문구) | REQ-AI-072·073 |
| 적합 예측: 예측 집합 크기·커버리지(+ α) | REQ-AI-082 |
| `drift_flagged: true` | REQ-AI-101 |
| 입력 지문 | REQ-AI-100(REQ-OPS-020) |

내보내는 **방법**(API)이 열려 있다: (a) 기존 진입점 네 개에 출력 인자 추가 — ABI 파괴, (b) 마지막 호출의 사이드카를 돌려주는 조회 함수 `xpe_ai_get_last_inference_sidecar(buf, size)` 추가 — 호출이 모듈 뮤텍스로 직렬화되지만 "마지막"은 다중 스레드에서 호출 사이의 순서를 호출자가 보장해야 한다, (c) 설정으로 파일 경로를 주어 모듈이 파일을 씀 — SRS 의 "alongside the image" 에는 가깝지만 REQ-AI-093(쓰기는 사이드카 스크래치만)과 모듈이 지금 파일을 쓰지 않는 사실과 부딪힌다.

### 2.4 검증을 어디서 하는가, 서명과의 관계

- **순서는 195 가 이미 정했다**: 서명 검증이 먼저, **검증된 텍스트만** 파싱한다(`OnnxSession::VerifiedSidecar`). 스키마 검증은 그 뒤, 같은 `Create` 안에서 하는 것이 일관된다. 서명 전의 바이트는 어떤 파서도 보지 않는다는 규칙을 스키마 검사가 깨면 안 된다.
- **거부 대 경고**: REQ-AI-008 은 "shall include"다. 거부하면 필수 필드가 빠진 모델은 적재되지 않는다(서명 거부와 같은 "사용 불가" 경로: 뼈 억제 -4, 부위 인식 UNKNOWN, 워커 계수 제외, Error 알림 1회에 `사이드카 스키마`와 필드 이름). 경고만 하면 읽을 수 없는 카드가 조용히 흘러간다.
- **서명과의 관계**: 서명 메시지는 `역할 ‖ 모델 길이·바이트 ‖ 사이드카 유무 ‖ 사이드카 길이·바이트` 이고 사이드카는 **원시 바이트**로 들어간다(`XPE-MODEL-SIG-1`). 스키마를 모르므로 **스키마 변경은 서명 형식 변경이 아니다**(서명 파일 형식 `XSIG` 1, 메시지 태그 모두 그대로). 그러나 ① 사이드카 내용을 한 글자라도 바꾸면 그 모델의 `.sig` 를 다시 만들어야 하고(카드의 `published_date` 정정도 재서명) ② 필수 필드를 새로 강제하면 이미 서명된 시험 모델 19개를 다시 만들고 다시 서명해야 한다(부위 인식 픽스처 12개는 사이드카에 `model_id`·`version`·`note`·`labels` 가 있고 `pccp_scope`·`training_data_hash`·`validation_metrics` 가 없으며, 뼈 억제 계열 픽스처는 사이드카가 아예 없다)(`tools/ai/xpe_model_signing.py sign-test-assets` 가 있어 기계적). ③ 줄바꿈이 서명 대상이라 사이드카는 `.gitattributes` 의 `eol=lf` 보호 아래에 있어야 한다(M3 시험이 CRLF 로 서명이 깨지는 것을 확인함).

### 2.5 선택지와 권고

| | 선택지 | 권고 |
|---|---|---|
| A. 문서 구조 | (1) 한 스키마가 ①②③ 모두를 겸함 (2) **셋을 따로** 두고 ② 는 ① 에서 파생 | **(2)**. ① 은 입력(저자가 쓰고 서명), ② 는 출력(모듈이 만들어 줌), ③ 은 영상마다. 합치면 REQ-AI-011("카드 JSON 이 `model-card.schema.json` 에 적합")이 입력 파일 형식과 묶인다 |
| B. 작업 분할 | T-005 하나 vs 분할 | **T-005a**(① 스키마 + 적재 시 검증, REQ-AI-008), **T-005b**(② 카드를 ① 에서 생성 + `model-card.schema.json`, REQ-AI-010·011), **T-005c**(③ 추론 사이드카, REQ-AI-004). T-014~016 은 **T-005c** 에만 의존 |
| C. 필수 필드 강제 | (1) 경고 (2) **거부**(REQ-AI-008 다섯 필드 + semver) | **(2)**. 시험 자산 19개 재생성·재서명 필요(기계적). 토이 부위 인식 모델은 `labels` 가 이미 있어 필드만 더한다 |
| D. 스키마 검증 구현 | (1) 손으로 쓴 C++ 구조 검사 (2) JSON Schema 라이브러리 추가 | **(1)** + 스키마 파일은 계약 문서이자 시험의 대조 기준. 새 의존성(공급망)을 필드 열몇 개 때문에 들이지 않는다. 스키마 파일과 C++ 검사가 같은 것을 받아들이는지는 시험 코퍼스로 확인(스키마 쪽 도구가 CI 에 있는지는 확인하지 못함) |
| E. 카드의 출처 | (1) 지금처럼 하드코딩 4 ID (2) **적재된 모델의 검증된 ① 에서 생성**, ID→모델은 사이드카의 `model_id` 로 찾음 | **(2)**, 그리고 실모델이 없는 구간의 카드는 값을 지어내지 않는다: 사이드카에 없는 필드는 `"not_provided"`(또는 키 생략)로, `published_date` 같은 상수는 쓰지 않는다. `TC-MODELCARD` 시험은 상수를 단언하므로 바뀐다(리더 문서 소관) |
| F. ③ 의 API | §2.3 (a)(b)(c) | **(b)** — ABI 파괴 없음, 파일 쓰기 없음. 단 "마지막 호출" 의미와 스레드 규칙을 헤더에 명시. 결정 필요 |
| G. 스키마 파일 위치 | REQ-AI-011 은 `schemas/model-card.schema.json` 이라 적었으나 `schemas/` 는 저장소에 없다 | 루트 `schemas/` 는 리더 소유 경로 — **패치 초안(.txt)**으로 보낸다(M3 때 받은 규칙) |

## 3. T-014·T-015·T-016 은 실모델 없이 의미 있게 구현·시험될 수 있는가

1. **T-014 XAI(REQ-AI-070~073)**: 껍데기(옵트인, 사이드카 JSON 의 필드, REQ-AI-073 의 **고정 문구** 일치)는 시험할 수 있지만 **설명 자체는 아니다**. Grad-CAM 은 기울기나 중간 활성화가 필요한데 ONNX Runtime 세션은 추론 전용이고 장난감 모델은 그것을 내놓지 않는다. SHAP 은 "tabular-features AI" 용인데 이 모듈에 그런 모델이 없다. 모델 쪽에서 중간 텐서를 출력으로 내보내는 규약을 정해야 의미가 생긴다.
2. **T-015 적합 예측(REQ-AI-080~083)**: 알고리즘(분할 적합 예측, 순서형 변형)은 모델과 무관한 수식이라 **합성 점수로 커버리지 보장(≥ 1−α)을 통계 시험으로 확인할 수 있다** — 이쪽은 의미가 있다. 단 합성 점수가 균일하면 결함을 가린다(과거 교훈): 교환가능성이 깨진 입력과 치우친 점수 분포를 일부러 넣어야 한다. 반면 REQ-AI-081(보정 집합을 학습·독립 시험 집합과 분리해 관리)과 실제 α 보장 주장은 **실제 보정 데이터**가 있어야 하므로 코드가 아니라 데이터 절차다.
3. **T-016 드리프트(REQ-AI-100·101)**: REQ-AI-100 의 입력 분포 지문(화소 통계·히스토그램)은 **모델 없이 계산·시험 가능**하고 의미도 있다. 형식은 REQ-OPS-020(OPS SPEC)이 정하는데 그 문장은 "input-distribution fingerprints" 뿐이라 필드가 정해져 있지 않다. REQ-AI-101 은 OPS 의 드리프트 경보(REQ-OPS-022)를 **받는 입구**가 이 모듈에 없어(그 상태를 넣어 줄 API 가 없다) 새 API 와 OPS 쪽 정의가 먼저다.

## 4. 표 수정안

`tasks_md_draft.md` 에 전체 표와 메모를 초안으로 두었다(`.moai/specs/` 는 리더 소유라 직접 고치지 않았다).

## 5. 리더 결정이 필요한 것

- **D1** 문서 세 개를 따로 둘지(권고 §2.5 A).
- **D2** T-005 를 a/b/c 로 나누고 REQ-AI-010·011 을 T-005b 에 거는지, REQ-AI-093(최소 권한)과 나머지 "작업 없음" REQ 를 새 작업 또는 "범위 밖" 표시로 처리할지.
- **D3** 필수 필드를 거부로 강제할지(경고가 아니라), 그 결과 시험 자산 19개를 재서명하는 데 동의하는지.
- **D4** 모델 카드의 출처를 적재된 모델의 검증된 사이드카로 바꾸고, 그 전까지 상수 카드가 모델에 대한 사실처럼 읽히는 문제(풀 빌드에서도 "stub", `2026-04-22`)를 어떻게 다룰지 — 지금 고칠지, T-005b 까지 둘지.
- **D5** ③ 의 API(§2.3 (a)/(b)/(c)). ABI 와 `api-spec.md` 가 걸린다.
- **D6** `schemas/` 디렉터리와 `model-card.schema.json` 의 위치(패치 초안으로 보내면 되는지).
- 별도: REQ-AI-111 의 `XPE_ERR_PCCP_EXCEEDED` 는 `xpe_common`(Lane A) 에 새 코드가 필요하고 REQ-AI-110 의 "승인된 PCCP" 출처가 없다 — T-011 은 이 둘이 정해지기 전에는 시작할 수 없다.

## 6. 미검증 (Gaps)

- **구현됨 판정의 근거는 시험 이름과 코드 읽기다.** 시험을 이번 실행에서 다시 돌리지는 않았다(직전 `QA-B-194b` 실행에서 ci-ai 474건 통과). T-001·T-002 의 REQ-AI-003 "main process crash-immune" 를 단언하는 시험은 `WorkerSupervisor.AfterTheWorkerIsKilledTheNextCallSucceedsOnANewPid` 등으로 확인했을 뿐 문구 대 단언을 한 줄씩 대조하지는 않았다.
- T-003 의 "CPU 외 제공자 미등록"은 코드 주석과 `CpuIsReportedAndTheListComesFromTheRuntime` 시험으로 확인했다. 이 기계의 ONNX Runtime 이 다른 제공자를 보고하는지는 보지 않았다.
- REQ-AI-093 의 "제한 없음"은 감독자 코드를 읽은 결과다(작업 객체의 한 플래그). 워커가 네트워크를 열 수 있음을 실행으로 보이지는 않았다.
- 스키마 대조(§2.2)의 "지금 사이드카가 가진 필드"는 파서 코드(`LoadMetadataFromText`)가 읽는 키다. 시험 자산의 사이드카 12개(`bodypart.json`)는 JSON 으로 열어 키를 확인했다: 모두 `labels`·`model_id`·`note`·`version` 이고 REQ-AI-008 의 다섯 필드 중 `pccp_scope`·`training_data_hash`·`validation_metrics` 를 가진 파일은 하나도 없다. `model_id`·`version` 의 값이 semver 인지는 확인하지 않았다.
- REQ-OPS-020·022 와 REQ-REG-007 은 문장 한 줄만 읽었다. 각 SPEC 의 나머지 정의(지문 형식, 드리프트 경보 전달 방식)는 읽지 않았다.
- `schemas/model-card.schema.json` 을 검증할 도구(예: Python `jsonschema`)가 CI 에 있는지는 확인하지 않았다.

## 7. 잔여 위험

- 표를 이 보고서대로 고치지 않으면 T-004 의 `done` 이 이후에도 REQ-AI-008·010 의 근거로 인용된다.
- 카드가 상수라는 사실이 문서화되지 않은 채 GUI 의 모델 정보 화면(REQ-REG-021)이 이 카드를 읽게 되면 실모델 입고 때 거짓 정보가 표시된다.
