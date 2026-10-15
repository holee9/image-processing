# QA-B-194 — T-012 입력 검증 강화 (REQ-AI-090): 현재 상태와 설계 메모

카드: QA-B-194 · 관련: #130 · 코드 변경 없음(설계 메모). 측정은 임시 프로브(`probe_source.txt`, `scan_probe_source.txt`)로 했고 시험 파일은 바이트 동일하게 복원했다(커밋하지 않음).
장난감 모델·스텁 기준이다. 아래 어떤 문장도 실제 모델의 동작을 말하지 않는다.

각 항목에 **측정**(프로브 출력, `probe_out.txt`)과 **읽음**(코드를 읽었을 뿐 실행하지 않음)을 구분해 적었다.

## 0. 결론 먼저

1. REQ-AI-090 은 세 가지를 요구한다 — 영상 크기 경계, **화소값 경계**, DICOM 메타데이터 스키마 검사. 지금 코드는 **첫째만 부분적으로** 한다(null·크기). 화소값과 메타데이터는 검증이 없다. SRS 도 VVP 도 "부분: null·크기만" 이라고 적고 있다(§1).
2. 실측으로 확인한 대표 결함 일곱:
   - **비유한(NaN/±Inf) 입력이 입구에서 걸리지 않는다**: 부위 인식·뼈 억제 모두 모델까지 가서 `XPE_ERR_PROCESSING_FAILED` 로 끝나고, 알림은 "AI model output was non-finite" — **원인이 호출자의 입력인데 모델의 출력 탓으로 보고**한다(측정).
   - `xpe_dl_denoise` 에 `UINT8` 4000×4000 이라 선언하고 데이터 1바이트를 준 입력이 **입력 검증을 통과**한다(`INVALID_INPUT` 아님, 측정). 원인: `validateImageBuffer` 가 화소 크기를 아는 두 형식(UINT16·FLOAT32)에만 크기를 검사한다(읽음).
   - 같은 함수에 NaN·음수·0·무한대 메타데이터와 **NUL 로 끝나지 않는 `bodyPart`** 가 통과한다(측정).
   - `xpe_stitch_images`·`xpe_stitch_estimate_size` 는 형식·크기가 제각각인 파트를 받아들이고(측정: 4×4 float + 8×8 float + 4×4 uint16 → OK, 19×8), **`partCount` 에 상한이 없다**(읽음: 검증 루프가 `parts[i]` 를 `partCount` 번 읽는다).
   - `xpe_ai_init("")` 가 성공하고 모델을 **작업 디렉터리에서** 읽는다(측정: 임시 작업 디렉터리의 `bodypart.onnx` 로 부위 인식 성공). 같은 모듈의 워커는 "DLL 옆에서만" 찾는 것과 반대다.
   - 설정 JSON 이 깨졌거나 객체가 아니면(`{bad`, `[]`, `5`, `"x"`) 알림 없이 OK 이고, 범위 밖 숫자(`timeout_ms` -1·10000000000, `confidence_threshold` -5·1e999)도 알림 없이 받아들여진다(측정). `timeout_ms` -1 은 `uint32_t` 로 변환되면 4294967295(읽음).
   - `xpe_ai_get_model_card` 가 `"` 가 든 모델 ID 로 **깨진 JSON** 을 돌려준다(측정: `{"model_id":"a"b",...}`).
3. 예외: `xpe_ai_init` 만 try/catch 가 없다(읽음). `modelDirPath` 대입·`loadedModels` 대입·`nlohmann::json::parse` 가 던지면 예외가 C ABI 밖으로 나가고 `new` 한 상태가 샌다. 나머지 진입점 중 할당하는 것(`bodypart`, `bone_suppress`, `get_model_card`)은 감싸여 있지만, 감싼 것의 **증거가 할당 실패 스윕이 아니다**: `test_ai_exception_guard.cpp` 는 잠금 구역 안에서 던지는 시험 훅과 바이트 변환 불가 경로로 증명했고, `xpe_bodypart_recognize` 의 감싸기는 그 시험이 덮지 않는다(읽음). `xpe_stitch_*`·`xpe_dl_denoise`·`worker_state`·`set_fallback_mode` 는 할당이 없다(읽음).
4. 제안: **다섯 마일스톤**(§5), 반환 코드는 preprocess 쪽 원칙과 같게(비유한 입력 → `INVALID_INPUT`, 출력 불변, 오류 알림 1건; 생산자가 유한 입력에서 비유한을 내면 `PROCESSING_FAILED`; 예외는 C ABI 를 넘지 않는다). 리더 결정이 필요한 것은 §6 의 D1~D8.
5. 호출자 영향은 작다: C#·GUI 가 부르는 AI 함수는 `xpe_bone_suppress`·`xpe_ai_init`·상태 조회뿐이고, GUI 는 `INVALID_INPUT` 을 이미 "시도하지 않음" 으로 해석한다(§7).

## 1. REQ-AI-090 원문과 지금 문서가 말하는 상태

- `SPEC-XPE-P3-AI/spec.md:303`: "**REQ-AI-090** (Ubiquitous): AI input validation shall include: image dimension bounds, pixel value bounds, DICOM metadata schema check."
- `docs/project/srs_ai.md:435`: 같은 문장, Verification 칸 "**[DEFERRED: basic input validation implemented in skeleton]**", 요약표(514행) "Partial (input null checks)".
- `docs/project/vvp_ai.md:292`: "REQ-AI-090~093 (adversarial robustness — **partial**: null and size checks only)".
- `SPEC-XPE-P3-AI/tasks.md`: `| T-012 | Input validation hardening | REQ-AI-090 | - | ai.cpp | pending |`.

요구 문장을 이 코드에 놓으면(해석 — 요구는 숫자를 주지 않는다):

| 항목 | 이 코드에서 뜻하는 것 | 지금 |
|---|---|---|
| 영상 크기 경계 | 폭·높이 1 이상, 총 바이트 ≤ 4096×4096×4(64 MB), `dataSize` 가 선언한 크기를 담는다, 파트 수 상한 | 부분(UINT8 구멍, 파트 수 상한 없음) |
| 화소값 경계 | float 입력은 유한(NaN·±Inf 아님). **[0,1] 같은 범위 검사는 하지 않는다** — 입력 척도는 호출자가 모델에 맞춰 정한다(`ai_api.h` PIXEL SCALE: "module does NOT normalise"), GUI 도 그 가정을 쓴다 | 없음 |
| DICOM 메타데이터 스키마 | `XpeImageMetadata`(96바이트 구조체)의 필드 유효성: `bodyPart` 가 NUL 로 끝남, `kVp`·`mAs`·`SID_mm`·`pixelPitch_mm` 유한, 음수 아님(0 = 모름) | 없음(`xpe_dl_denoise` 만 메타데이터를 받는다) |

"DICOM 메타데이터 스키마" 는 이 C ABI 에서 DICOM 파일이 아니라 `XpeImageMetadata` 구조체다(파일 단위 검사는 `modules/dicom`). 0 이 "모름" 이라는 약속은 `DicomWriter.cpp:166-195`(양수일 때만 쓴다)와 같다.

## 2. 지금 상태 표 (진입점 × 검증 항목)

✓ 있음 · ✗ 없음 · — 해당 없음. (측정) = 프로브로 확인, (읽음) = 코드를 읽음.

| 진입점 | 필수 포인터 | 초기화 순서 | 크기·`dataSize` | 형식 | **비유한 입력** | 메타데이터 | 예외 |
|---|---|---|---|---|---|---|---|
| `xpe_bodypart_recognize` | ✓ (img·label, bufLen 0) | ✓ 포인터 다음 | ✓ `validateImageBuffer` | ✓ FLOAT32 (모델이 있을 때만) | ✗ (측정: UNKNOWN + 모델 출력 탓 알림) | — | ✓ try/catch (스윕 증거 없음) |
| `xpe_bone_suppress` | ✓ | ✓ | ✓ 두 버퍼·`dataSize`≥바이트·폭 높이 일치 | ✓ FLOAT32 둘 다 | ✗ (측정: `-3`, 출력 불변, 모델 출력 탓 알림) | — | ✓ try/catch (훅 증거) |
| `xpe_dl_denoise` | ✓ img·meta | ✓ | ✓ 단 **UINT8 은 검사 안 됨**(측정) | ✗ 형식 검사 없음 | ✗ (측정: `-3`) | ✗ 필드 검사 없음(측정) | 할당 없음(읽음) |
| `xpe_stitch_images` | ✓ parts·out, `partCount`≥2 | ✓ 포인터 다음 | ✓ 파트마다, 출력은 `data`·`dataSize≠0` 만 | ✗ 파트끼리 형식·크기 일치 검사 없음(측정) | ✗ | — | 할당 없음(읽음) |
| `xpe_stitch_estimate_size` | ✓ | **없음(의도)**: 결정론적 | ✓ 파트마다 | ✗ 혼합 허용(측정: 19×8) | — (화소를 안 읽음) | — | 할당 없음 |
| `xpe_ai_init` | ✓ `modelDirPath` NULL | — | — | — | — | — (설정 JSON: 아래) | **✗ try/catch 없음**(읽음) |
| `xpe_ai_get_model_card` | ✓ id·buf, bufSize 0 | ✓ | — | — | — | id: ✗ 길이·문자 | ✓ try/catch (훅 증거) |
| `xpe_ai_set_fallback_mode` | — | ✓ | — | — | — | 아무 정수나 bool 로 | 할당 없음 |
| `xpe_ai_worker_state` | ✓ stateOut | ✓ | — | — | — | — | 할당 없음 |
| `xpe_ai_shutdown`·`xpe_ai_version` | — | — | — | — | — | — | 할당 없음 |

설정·수명(`xpe_ai_init`) 항목:

| 항목 | 지금 |
|---|---|
| `modelDirPath` 비어 있음 | ✗ 통과, 모델을 작업 디렉터리에서 읽음(측정) |
| `modelDirPath` 가 없는 디렉터리 | 통과(첫 호출에서 "사용 불가" 경고) — **지연 적재 설계**, 문제 아님 |
| 설정 JSON 이 깨짐·객체 아님 | ✗ 알림 없는 기본값(측정) |
| 키 모름·형식 틀림 | ✓ Warning (QA-B-60, #145: **반환 코드는 OK 로 유지**하기로 한 결정) |
| `timeout_ms` 음수·`int` 범위 밖 | ✗ 알림 없이 변환(읽음: `static_cast<uint32_t>(get<int>())`) |
| `confidence_threshold` 범위 | 사용자가 정한 값 그대로 쓴다 — `ai_api.h` 에 "range check 없음" 으로 문서화됨, 의도 |
| 이미 초기화된 뒤 다시 `init` | OK 반환·새 설정 무시(측정: 알림 없음). 클라이언트 시험(`AiBoneSuppressionStageTests.cs:169`)이 이 동작을 계약으로 적어 두었다 |

### 진입점끼리 다른 점 (같은 위반에 다른 대답)

1. 형식 거절 코드: `xpe_bone_suppress`·`xpe_bodypart_recognize` 는 `UNSUPPORTED_FORMAT`, `xpe_dl_denoise`·`xpe_stitch_*` 는 형식을 보지 않는다.
2. 실패했을 때 출력: 부위 인식은 라벨 `UNKNOWN` 을 쓴다, 뼈 억제 프로세스 안 경로는 출력을 건드리지 않고(측정: -777 그대로), 워커 경로는 입력을 복사한다. 각각 의도가 문서에 있다(`ai_api.h`). 입구 거부는 어느 쪽이든 **출력 불변** 이어야 하는데 지금 입구 거부가 없는 항목(비유한)에서는 이 차이가 드러나지 않는다.
3. 포인터와 초기화의 순서: 대부분 "필수 포인터 → 초기화" 인데 `xpe_stitch_estimate_size` 만 초기화를 요구하지 않는다(의도: 결정론적 추정은 모델이 필요 없다).

## 3. 실측 (`probe_out.txt`, 장난감 모델·스텁 아닌 풀 빌드)

핵심 줄(전체는 `probe_out.txt`):

```
bodypart NaN  model=models_bodypart_a   rc=-3 label=UNKNOWN conf=0 | alert: AI model output was non-finite (inf/NaN); this image was not AI-processed
bodypart NaN  model=models_bodypart_dep rc=-3 ... 같은 알림        (±Inf 도 같다)
bone NaN                                rc=-3 out[4]=-777 out[0]=-777 | 같은 알림
denoise NaN/+Inf/-Inf                   rc=-3                                (스텁이 늘 -3)
denoise uint8 4000x4000 dataSize=1      rc=-3  (INVALID_INPUT=-1 이어야 한다)
denoise garbage metadata                rc=-3  (NaN kVp, 음수 mAs, SID 0, 무한대 pitch, 끝이 없는 라벨)
stitch_estimate_size 혼합 형식·크기     rc=0 w=19 h=8
stitch_images 혼합 파트·작은 출력       rc=-3  (스텁; 출력이 작다는 이유로 INVALID_INPUT 이 아님)
init config={bad / [] / 5 / "x" / timeout_ms -1 / 10000000000 / threshold -5 / 1e999   rc=0 alerts=0
init("") 뒤 recognize                   rc=0 label=CHEST conf=0.6 (모델을 작업 디렉터리에서 읽음)
둘째 init (다른 디렉터리·0.99)          rc=0 alerts=0, 이어서 recognize 는 첫 세션의 모델
model_card id=a"b                       rc=-9 card={"model_id":"a"b","error":"model_not_loaded",...
```

- **프로브 한계**: ① 비유한 입력의 `denoise`·`stitch` 는 스텁이라 늘 `-3` 이어서 "입구가 안 막는다" 는 사실이 아니라 "아무것도 안 한다" 는 사실만 보인다. 그래도 `validateImageBuffer` 는 화소를 읽지 않으므로(읽음) 검증이 없는 것은 확실하다. ② `timeout_ms` 의 효과는 관찰할 수단이 없어 변환식을 읽은 것이다. ③ `partCount` 가 배열보다 큰 경우는 범위 밖 읽기라 안전하게 프로브할 수 없다 — 읽음으로만 확인.
- 프로브 자체의 버그 하나: `printf` 의 인자 평가 순서 때문에 `stitch_estimate_size` 의 `w`·`h` 가 호출 전 값(0, 0)으로 찍혀 처음엔 "OK 인데 0×0" 으로 보였다. 호출을 별도 문장으로 분리해 다시 쟀다(19×8 과 13×8, `probe_out.txt` 의 값).
- 입구 비유한 검사의 비용(`scan_probe_out.txt`, 기존 `xpe::ai::AllFinite`): 1024² 0.082 ms, 3072² 1.443 ms, 4096² 2.902 ms (15회 최솟값, **캐시에 올라와 있는 데이터** 기준 — 실제 호출에서 영상이 캐시에 없으면 더 걸린다). 뼈 억제의 워커 왕복(3072² 약 68 ms, QA-B-191)의 약 2% 이다. 오류 위치(첫 비유한 화소의 인덱스·x·y)를 알림에 넣으려면 실패한 경우에만 두 번째로 훑으면 된다(정상 경로에는 영향 없음).

## 4. preprocess 쪽 원칙과 맞춘다

기준 문서: `docs/project/api-spec.md` §(3)·(4)(QA-A-214b·215·217)와 QA-A-200·204(`#233`)의 할당 실패 스윕.

- **소비자는 입구에서 비유한 입력을 거부한다**: NaN 또는 ±Inf 화소가 하나라도 있으면 **아무것도 쓰기 전에** `XPE_ERR_INVALID_INPUT`, 기존 인자 검사는 같은 우선순위로 먼저, 알림은 오류 심각도·`<N> pixel(s) of the input frame are NaN or infinite (first: index <I>, x=<X>, y=<Y>); ` 뒤에 함수별 맺음말(`api-spec.md:872`).
- **생산자가 유한 입력에서 비유한을 내면 `XPE_ERR_PROCESSING_FAILED`**, 출력 불변: 이미 모듈 출력 쪽에 있다(`AllFinite`, 뼈 억제·부위 인식, `non_finite` 응답).
- **C ABI 밖으로 예외가 나가지 않는다**, 할당 실패는 `XPE_ERR_OUT_OF_MEMORY`, 실패하면 상태는 호출 전과 같다. 증거 방법: **K번째 할당만 실패시키는 스윕**(K = 1, 2, … 한 호출이 K 번보다 적은 할당으로 끝날 때까지). 라이브러리가 공유 라이브러리라 시험 실행 파일의 `operator new` 교체가 닿지 않으므로 **라이브러리 소스를 시험 실행 파일에 직접 컴파일**한다(`modules/common/tests/test_common_oom_injection.cpp`, `xpe_preprocess_oom_tests`). 스윕 중에는 gtest 매크로를 쓰지 않는다(gtest 도 할당한다).

AI 모듈에서 달라지는 점:
- 입력 "써진 것" 은 출력 버퍼다. 뼈 억제는 이미 "프로세스 안 경로 실패 = 출력 불변" 이고 부위 인식은 `UNKNOWN` 라벨을 쓴다. 입구 거부는 **두 함수 모두 출력 불변** 으로 하는 것이 원칙에 맞다: 부위 인식의 `UNKNOWN` 은 "쓸 수 있는 답이 없음" 이라는 documented fallback 이고, 입구 거부는 호출자 오류라 다르다(호출자는 `INVALID_INPUT` 을 보고 입력을 고친다). — **결정 D2**.
- 비유한 알림의 맺음말(함수별): 예 `the frame was not processed and the output buffer was not changed`.

## 5. 고칠 범위와 마일스톤 제안

각 마일스톤을 따로 커밋한다. 파일 수는 읽은 코드에서 센 어림이고 약속이 아니다.

| M | 내용 | 제품 파일 | 시험 |
|---|---|---|---|
| **M1** | **공통 검증기 + 비유한 입구**: `validateImageBuffer` 가 형식에서 화소 크기를 얻도록 정리(UINT8 구멍, 알 수 없는 형식), 비유한 스캔 함수(`AllFinite` 재사용)와 알림 문구를 한 곳에. `xpe_bone_suppress`·`xpe_bodypart_recognize`·`xpe_dl_denoise`·`xpe_stitch_images` 입구에서 호출 | `ai.cpp`, `ai_finite.h`(필요 시 인덱스 보고), 새 내부 헤더 1 | 함수 × (NaN·+Inf·-Inf, 맨 처음·맨 끝 화소), 출력 불변, 알림 문구 전체 일치, UINT8 크기, 반증 |
| **M2** | **스티치 입력**: `partCount` 상한(D5), 형식·크기 일치(D6), 출력 버퍼 용량 검사(`xpe_stitch_images`: 추정 크기보다 작으면 `BUFFER_TOO_SMALL`/`INVALID_INPUT`: D6) | `ai.cpp` | 상한 경계, 혼합 거절, 용량 경계 |
| **M3** | **메타데이터와 ID**: `xpe_dl_denoise` 의 `XpeImageMetadata` 필드 검사(D3), `xpe_ai_get_model_card` 의 ID 문법(D4) | `ai.cpp` | 필드별 경계(NaN·음수·0·끝 없는 라벨), 정상 메타데이터 대조, ID 길이·문자 |
| **M4** | **초기화·설정 강화**: `xpe_ai_init` 의 try/catch·롤백(할당 실패 때 `g_aiState` 불변, 누수 없음), 빈 디렉터리 문자열 거절(D1), 깨진·객체 아닌 설정과 범위 밖 숫자를 **알림으로**(반환 코드는 #145 결정대로 OK 유지: D7), 둘째 `init` 의 무시를 알림으로 말할지(D8) | `ai.cpp` | 설정 표(깨짐·형식·범위), 롤백, 빈 문자열 |
| **M5** | **할당 실패 스윕 대상**: `xpe_ai_oom_tests`(새 실행 파일) — `ai.cpp`·`ai_onnx_session.cpp`·`ai_ipc_bridge.cpp`·`ai_worker_supervisor.cpp` 를 직접 컴파일하고 `operator new` 를 교체. 스윕 대상: `xpe_ai_init`(M4 의 롤백 증명), `xpe_bone_suppress`·`xpe_bodypart_recognize`(프로세스 안 경로), `xpe_ai_get_model_card`. 질문 셋(예외가 나갔나, 오류 반환 시 상태가 호출 전과 같은가, 코드가 `OUT_OF_MEMORY` 인가) | `CMakeLists.txt`, 새 시험 파일 1 | K 스윕 4개 |
| M6 | 문서(리더): `srs_ai.md` REQ-AI-090 칸, `vvp_ai.md`, `rtm_ai.md`, `api-spec.md` §9 의 반환 코드 변경, `tasks.md` T-012 | 0 | — |

M5 의 어려움(미리 적는다): ONNX Runtime 의 할당은 `operator new` 가 아니라 ORT 할당기를 거치므로 스윕이 세는 것은 우리 쪽 표준 컨테이너·`std::string`·`nlohmann` 뿐이다. 모델 세션 안의 할당 실패는 이 방식으로 증명되지 않는다 — 그 부분은 "세션 생성 실패를 에러 코드로 매핑" 이라는 기존 시험(`AiExceptionGuard.*`)에 남는다. 또 `xpe_alert_push` 는 `xpe_common` 공유 라이브러리 안에서 할당하므로 스윕에 안 잡힌다(`test_common_oom_injection.cpp` 가 따로 한다).

## 6. 리더 결정이 필요한 것

| # | 결정 | 내 제안 | 근거·위험 |
|---|---|---|---|
| **D1** | `xpe_ai_init("")` | `INVALID_INPUT` (빈 문자열은 "없는 인자", #142 의 `bufLen 0` 과 같은 취급) | 작업 디렉터리에서 모델을 읽는 것은 워커의 "DLL 옆에서만" 원칙과 반대이고 배포 환경에서 임의 파일을 적재할 수 있다. GUI 는 항상 절대 경로를 넘긴다(`GuiAiRunner.cs`) — 영향 없음. 이 변경은 "동작 변경" |
| **D2** | 비유한 입력 거부 시 부위 인식의 출력 | **출력 불변**(라벨 안 씀), `INVALID_INPUT` | 호출자 오류와 documented fallback(`UNKNOWN`)을 구분. 반대 선택(`UNKNOWN` 도 씀)은 호출자가 이 둘을 코드로 구분 못 한다 |
| **D3** | `XpeImageMetadata` 규칙 | `bodyPart` NUL 종료 필수, 네 실수는 유한이고 0 이상(0 = 모름), 위반은 `INVALID_INPUT`. `acquisitionTime`·`flags` 는 검사하지 않음 | "DICOM 메타데이터 스키마" 가 구조체 필드 유효성이라는 해석(§1)에 의존. 해석이 다르면(예: 필수 필드 목록) 요구 문장이 숫자를 주지 않으므로 사용자 확인 필요 |
| **D4** | 모델 ID 문법 | `[A-Za-z0-9._-]{1,64}`, 위반은 `INVALID_INPUT` (이스케이프 대신 거절) | 알려진 ID 4개가 모두 이 문법이다(읽음: `bodypart_cnn_v1` 등). 거절하면 JSON 을 만들 때 이스케이프 코드가 필요 없다 |
| **D5** | `xpe_stitch_*` 의 `partCount` 상한 | **16** (제안값) | SRS·SPEC·PRD 에 최대 파트 수가 없다(검색 확인). 임의의 숫자이므로 **제품 쪽 값이 있으면 그것**. 상한이 없을 때의 위험은 `partCount` 가 호출자 배열을 넘어 읽는 것 |
| **D6** | 스티치 파트 일치 규칙 | 형식·(폭 또는 높이?) — **같은 형식 필수**(`UNSUPPORTED_FORMAT`/`INVALID_INPUT` 중 선택), 크기는 겹침 방향의 한 축만 같으면 되는지 제품 정의 필요 | SRS 는 "overlapping partial images" 라고만 한다. 크기 규칙은 알고리즘이 없어 정의할 수 없으므로 M2 에서 형식 일치만 하고 크기 규칙은 보류하는 것이 안전하다 |
| **D7** | 깨진·범위 밖 설정 | 반환 코드는 **OK 유지**(#145 가 "키 모름도 OK" 로 정한 선과 같다), **Warning 을 낸다**(깨진 JSON, 객체 아님, `timeout_ms` 가 [0, 2^31) 밖, 정수 아님) | 코드를 바꾸면 #145 결정을 뒤집는다. 알림은 "조용한 무시" 만 없앤다 |
| **D8** | 이미 초기화된 뒤 둘째 `init` | 지금처럼 OK·무시를 유지하되 **설정이나 디렉터리가 다르면 Warning** | GUI 시험이 "무시" 를 계약으로 적어 두었다. 알림만 더하면 계약은 안 깨진다 |

D5·D6 은 요구가 숫자를 주지 않는 부분이라 임의 값을 코드에 박기 전에 확인이 필요하다.

## 7. 바뀌는 반환 코드가 C#·GUI 호출자에 주는 영향

검색: `gui/`, `clients/`, `tools/` 의 `.cs`·`.xaml`·`.cpp`·`.h`·`.py`·`.ps1` 에서 AI 함수 이름.
- **C# 이 직접 부르는 AI 수출은 네 개뿐이다**: `GuiAiRunner.cs` 의 P/Invoke 선언이 `xpe_ai_init`, `xpe_ai_shutdown`, `xpe_ai_worker_state`, `xpe_bone_suppress` 이다. `xpe_bodypart_recognize`·`xpe_dl_denoise`·`xpe_stitch_images`·`xpe_stitch_estimate_size`·`xpe_ai_get_model_card` 이름을 `gui/`·`clients/` 의 `.cs`·`.xaml` 에서 검색한 결과는 0건이다 — 그 함수들의 반환 코드 변경은 C#·GUI 에 영향이 없다(`xpe_ai_set_fallback_mode` 도 선언이 없다).
- `xpe_bone_suppress` 반환 코드는 GUI 의 `AiBoneSuppressionStage.Classify` 가 읽는다: `0` 성공, `InvalidInput(-1)`·`NotInitialized`·`UnsupportedFormat` → `NotAttempted`(출력 안 읽음, 워커 실패 카운트 안 건드림), 그 밖(-3 등) → `Failed`. M1 이 비유한 입력에 돌려주는 코드를 `-3` → `-1` 로 바꾸면 GUI 는 이를 "시도하지 않음 — 입력이 거부됨"(`RefusalMeaning`)으로 보여 준다. 화면 문구가 바뀌고 더 정확해진다(지금은 "실패" 로 보인다). GUI 의 입력은 16비트 정수를 65535 로 나눈 float(`AiBoneSuppressionStage.ToFloat`)이라 비유한이 생길 수 없으므로 이 입구 거부는 GUI 경로에서는 발동하지 않는다(`TryToUInt16` 은 출력 쪽의 비유한을 막는 것이다).
- `xpe_ai_init`: GUI 는 설정을 JSON 직렬화기로 만들어 항상 유효한 객체를 넘기고(`GuiAiRunner.cs:41`), 모델 디렉터리는 절대 경로로 넘긴다. D1(빈 문자열 거절)·D7(알림만 추가)·D8(알림만 추가)은 GUI 를 깨지 않는다. `init` 반환 코드가 비 0 이면 GUI 는 `InterpretInit` 으로 처리한다.
- 클라이언트 시험은 `AiBoneSuppressionStageTests.cs:169` 가 "초기화된 뒤 둘째 `init` 은 OK 이고 새 설정을 무시한다" 를 계약으로 가진다 → D8 이 반환 코드를 바꾸지 않는 이유.
- 미검증: C# 시험 전부를 열어 본 것은 아니다(키워드 검색). M1~M4 를 구현할 때 `clients/` 시험을 같은 검색으로 다시 확인한다.

## 8. 미검증 (Gaps)

- 실제 모델이 없어서 "비유한 입력이 실제 모델을 어떻게 흔드는가" 는 모른다. 측정한 것은 장난감 모델의 행동뿐이다.
- 스텁 빌드의 `denoise`·`stitch` 는 입력을 처리하지 않아 "입구가 안 막는다" 의 증거가 **검증기를 읽은 것** 이다.
- `xpe_ai_init` 의 예외 노출은 읽음이다(할당 실패 스윕은 M5 의 일).
- `partCount` 가 배열을 넘는 경우는 안전하게 측정할 수 없었다.
- 입구 스캔 비용은 **데이터가 캐시에 있을 때** 의 최솟값이다.
- 워커 경로에서의 입구 검증(부모가 하는 것과 워커가 하는 것의 분담)은 설계하지 않았다: 부모가 입구에서 막으면 워커는 유한한 입력만 받는다 — 워커 쪽 `ParseImageRequest` 에 같은 검사를 더할지는 M1 에서 정한다.

## 9. 잔여 위험

- D5(16)와 D3·D4 의 값은 요구가 주지 않은 숫자·문법이라 제품 쪽 정의가 나오면 바뀔 수 있다. 코드에 상수 한 곳으로 두면 바꾸기 쉽다.
- 입구 검사를 늘리면 "이전에 통과하던 입력이 `INVALID_INPUT`" 이 된다. 이 모듈의 소비자는 현재 GUI 하나뿐(그것도 뼈 억제만)이고 §7 에서 영향을 확인했지만, 다른 소비자가 생기면 그쪽의 입력 가정은 확인된 적이 없다.
