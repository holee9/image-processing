# QA-B-194b — Codex #85 보류 2건: OUT_OF_MEMORY 매핑, nlohmann 필수화 (+ 워커 exe 부재 사실)

카드: QA-B-194b · 관련: #130 · 근거: Codex #85 (194 M5·M4)

## 1. 주장 (Claim)

### 1.1 `kOutOfMemory` → -2 를 세 호출자에 모두 (중간)

세션 생성 중 메모리 부족(`OnnxSession::Create` 가 `kOutOfMemory`)은 모델을 여는 모든 경로에서 `XPE_ERR_OUT_OF_MEMORY`(-2)이고 "모델을 쓸 수 없음"이 아니다.

| 경로 | 이전 | 이후 | S+ 계수 |
|---|---|---|---|
| 뼈 억제, 프로세스 안 | -2 (M5 가 이미 처리) | -2 (변화 없음) | 해당 없음(워커 없음) |
| 부위 인식, 프로세스 안, 첫 호출(세션 생성) | `UNKNOWN` + -3, "unavailable" Warning 1회, 이후 같은 경고로 기억 | **-2**, 알림 없음, 라벨 미기록, 아무것도 기억하지 않음(다음 호출이 다시 시도) | 해당 없음 |
| 뼈 억제, 워커 | 워커가 -3 으로 답함(`default`) → 호스트가 -3 통과 | 워커가 **-2** 로 답함 → 호스트가 -2 통과 | **센다**(뼈 억제 워커 경로의 기존 규칙: 서명 거부만 예외) |
| 부위 인식, 워커 | 워커가 `CONFIG_INVALID` + `model_unavailable` 로 답함 → 호스트는 "모델 없음" Warning 으로 읽고 **0 으로 되돌림** | 워커가 **-2**(플래그 없음)로 답함 → 호스트는 실패로 **센다**, 알림 "AI worker failed (code -2, failure 1 of 3) …", 호출자에게는 문서화된 대체 신호(`UNKNOWN` + -3) | **센다** |

"세 호출자" = `ai_bodypart_model.h`(`BodyPartLoadFailure::kOutOfMemory` 추가, `kModelUnreadable` 로 뭉개던 것 분리), `ai.cpp`(부위 인식 프로세스 안), `ai_worker_main.cpp`(뼈 억제 switch 와 부위 인식 오류 분기). OOM 은 "모델을 쓸 수 없음"이 아니므로 S+ 에서 0 으로 되돌리지 않고, 센다 — 뼈 억제 워커 쪽의 기존 규칙과 같다. 부위 인식 워커 경로는 호출자에게 -2 를 주지 않고 문서화된 대체 신호를 주는데, 이 경로는 모든 실패에 대해 그렇게 해 왔다(전송 실패 포함). 바꾸지 않았다.

### 1.2 nlohmann 을 필수 의존성으로 (중간, 리더 결정)

- `modules/ai/CMakeLists.txt`: `nlohmann_json::nlohmann_json` 타깃이 없으면 `message(FATAL_ERROR …)` 로 configure 가 멈춘다. DLL·워커·oom 시험 파일이 무조건 링크한다.
- 최소 파서 경로 제거 세 곳: `ai.cpp` `parseConfig` 의 `#else`(문자열 스캔으로 `timeout_ms`/`use_worker` 만 읽던 것), `ai_onnx_session.cpp` `LoadMetadataFromText` 의 `#else`, `ai_bodypart_model.h` `LoadBodyPartLabels` 의 `#else`("this build cannot read the label sidecar"). 매크로 `XPE_AI_USE_NLOHMANN_JSON`·`XPE_AI_BODYPART_NLOHMANN`·`XPE_AI_BODYPART_HAS_JSON` 은 `modules/` 어디에도 남지 않았다(`grep` 확인).
- 공개 헤더에 "JSON PARSER" 문단: 같은 계약(깨진 JSON·범위 밖 값의 경고)이 모든 빌드에서 성립한다.

### 1.3 리더 질문: 워커 실행 파일이 없을 때(사실)

`xpe_ai.dll` 을 `xpe_ai_worker.exe` 가 없는 디렉터리에 복사해 `use_worker=true` 로 `xpe_bone_suppress` 를 4번 호출했다(제품 DLL 그대로, 시험 키·서명 불필요): 1~3번째 호출은 `XPE_ERR_IO_FAILED`(-9), 연속 실패 수가 1·2·3, 3번째에서 `XPE_AI_WORKER_DISABLED`, 4번째는 `XPE_ERR_PROCESSING_FAILED` 로 즉시 반환. **세어진다.** 같은 복사본에 워커와 서명된 모델을 두면 정상 처리(`XPE_OK`, 출력 ×2)되어 결과가 워커 부재에서 나온 것임이 대조된다. `xpe-gui` 에 사실로 전달했다.

## 2. 증거 (Evidence)

- ci-ai: `[  PASSED  ] 474 tests.`(M4 470 → +4: `WorkerExeMissing` 2, `WorkerOom` 2), 스킵 5, 실패 0, 컴파일 경고 0. `xpe_ai_oom_tests`: `[  PASSED  ] 10 tests.`(8 → +2: `ABodyPartSessionThatRunsOutOfMemory…`, `ABoneSessionThatRunsOutOfMemory…`). 스텁: `xpe_ai_tests` 381 통과(스킵 98), `xpe_ai_oom_tests` 6 통과 + 건너뜀 4, 경고 0. 로컬 doxygen 1.12.0 `exit=0`, 경고 0줄, `check_header_docs.py` 0 findings, 린트 0.
- 새 시험:
  - `AiOom.ABodyPartSessionThatRunsOutOfMemoryIsOutOfMemoryNotAnUnavailableModel`: 콜드 첫 호출 -2, 알림 0건, 메모리가 있는 다음 호출이 라벨 `CHEST` 로 성공(기억되지 않음)
  - `AiOom.ABoneSessionThatRunsOutOfMemoryIsOutOfMemoryOnTheInProcessPath`: -2, 출력 -777 그대로, 이후 정상
  - `WorkerOom.ABoneSession…InTheWorker…`: 실제 워커에 `XPE_AI_TEST_FAIL_SESSION_CREATE=oom`(시험 빌드의 워커만 읽음)을 주면 3번 호출이 모두 -2 이고 연속 실패 수 1·2·3, 4번째는 `PROCESSING_FAILED` + `DISABLED`; 같은 디렉터리, 변수 없음 → `XPE_OK`·×2(대조)
  - `WorkerOom.ABodyPartSession…InTheWorker…`: `UNKNOWN` + `PROCESSING_FAILED`, 실패 수 1, 알림 1건에 `code -2` 가 있고 `unavailable` 는 없다
  - `WorkerExeMissing.*` 2건(1.3)
- 반증(`arms_out.txt`), 복원 바이트 동일, 대조군 깨끗: G1 로더가 OOM 을 안 나눔 / G2 프로세스 안 부위 인식이 -2 를 안 돌려줌 → `AiOom` 부위 인식 시험만 빨강; G3 워커의 뼈 억제 OOM 이 다시 -3 → `WorkerOom` 뼈 억제 시험만 빨강; G4 워커의 부위 인식 OOM 이 다시 "사용 불가" → `WorkerOom` 부위 인식 시험만 빨강. **H1(카드가 요구한 반증): CMake 검사가 없는 타깃을 가리키게 하면 `cmake --preset ci-ai` 가 종료 코드 1 과 "xpe_ai: nlohmann_json::nlohmann_json is required" 로 멈추고, 복원하면 0.**
- 최소 파서를 지우자 `LoadMetadataFromText` 의 JSON 경로가 spdlog 없이 컴파일되는 타깃에서 처음 컴파일되어 로그 매크로가 비고 `'e'` 미사용 경고(C4101)가 났다(어느 타깃인지는 확인하지 않았다, 워커로 추정) → `(void)e;` 로 고쳤고 이후 경고 0.

## 3. 기준 (Baseline)

같은 트리 `dev/postprocess`(77c7f868 위), `cmake --build --preset ci-ai` 와 `ci-post`(스텁), 이 실행의 출력.

## 4. 미검증 (Gaps)

- **스윕 확대는 할당 단위로 못 했다.** 카드는 "스윕을 부위 인식 첫 호출(세션 생성)·워커 뼈 억제까지 넓힌다"고 했으나, 콜드 부위 인식은 비어 있지 않은 nlohmann DOM 을 읽고, 그 소멸자의 할당이 실패하면 프로세스가 종료된다(헤더의 (1), 리더가 문서화로 정한 한계). 그래서 세션 생성 지점에서 `std::bad_alloc` 을 던지는 시험 전용 훅(`OnnxSession::TestSetBeforeSessionHook`, `XPE_AI_TEST_HOOKS` 빌드만, 워커는 환경변수 `XPE_AI_TEST_FAIL_SESSION_CREATE=oom`)으로 그 지점의 부족을 결정적으로 만들었다. **세션 생성 이외의 지점(라벨 JSON 읽기 등)에서 생기는 부족은 이 시험들로 증명되지 않는다.**
- ONNX Runtime 자신의 할당 실패는 어느 시험도 주입하지 않는다(헤더 (2)). 훅은 그 지점의 `bad_alloc` 이 오는 방식을 흉내 낸 것이고, ORT 가 `bad_alloc` 이 아니라 `Ort::Exception` 으로 부족을 알리는 경우는 `kModelLoadFailed` 로 간다(코드 읽기, 미실행).
- nlohmann 해결은 `ci-ai`·`ci-post` 두 프리셋에서 빌드로 확인했다. 루트 `CMakeLists.txt`(54~79행)가 못 찾으면 FetchContent 로 가져오므로 다른 프리셋에서도 해결되는 구조이지만, 나머지 프리셋을 configure 해 보지는 않았다.
- 문서: `docs/project/sdd_ai.md`(321·374행)와 `srs_ai.md`(537행)가 `XPE_AI_USE_NLOHMANN_JSON` 을 "AUTO"/설정 매크로로 적고 있다 — 이제 틀리다. 문서는 리더 소유라 고치지 않았다.
- 워커가 없을 때의 실제 gui C09 이식은 하지 않았다(사실만 전달).

## 5. 잔여 위험

- 뼈 억제 워커 경로의 OOM 은 센다: 부족이 일시적이어도 연속 3번이면 워커가 꺼진다(기존 정책과 같다).
- 워커의 새 환경변수는 시험 빌드에서만 읽힌다. 출하 빌드(`XPE_AI_TEST_HOOKS=OFF`)에는 없다는 것은 코드(`#ifdef`)로만 확인했고 `release` 프리셋 전체 빌드는 하지 않았다.
