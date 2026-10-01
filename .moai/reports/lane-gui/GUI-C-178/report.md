# GUI-C-178 — 행 10 을 AI(`xpe_bone_suppress`)에 연결하려면 무엇이 필요한가 (조사만, 구현 없음)

레인: gui · 이슈: `#225`, `#130` · 읽은 것: post 워크트리의 `modules/ai` 를 커밋 **`5c3588e4`**(dev/postprocess) 기준으로 `git show` 로 읽음. 아무것도 실행·빌드하지 않았다. 이 트리는 main `edf93005` 를 병합한 `dev/gui`.

## 결론

1. **GUI 가 부를 API 는 셋이면 된다**(`xpe_ai_init` · `xpe_bone_suppress` · `xpe_ai_shutdown`). 워커 경로는 `xpe_bone_suppress` 에만 있고 설정 키는 `use_worker`(기본 **false**)·`timeout_ms`.
2. **실패하면 출력은 입력과 바이트 동일**이고 반환 코드는 0 이 아니다. 그런데 **오늘 GUI 의 체인은 단계의 성패를 "픽셀이 같은가"로 가른다**(`ProcessingChainRunner.cs:81`, `SequenceEqual`) — AI 단계를 그 경로에 그대로 태우면 실패가 `AppliedNoChange`("돌았고 변화 없음")로 읽혀 **조용한 성공처럼 보인다.** 이것이 이 연결의 가장 날카로운 위험이다(§3).
3. **행 10 을 켜기 전에 막힌 것이 11개다**(§4). 코드 쪽 gui 만으로 풀리지 않는 것이 대부분이고, 그중 셋은 post 레인이 답해야 한다(상태 조회 API · float32 스케일 계약 · main 반영).

## ① post 의 API — GUI 가 부를 것 (`ai_api.h` @ `5c3588e`)

근거 발췌: `post_ai_excerpts.txt`(줄 번호 포함). 헤더는 main 의 것과 다르다(`diff` 출력 43줄, 사실상 추가뿐 — `use_worker` 단락, 3회 상한, 64 MB 거부, 그리고 한 줄 삭제). **즉 이 API 는 아직 main 에 없다.**

| 함수 | GUI 가 언제 | 핵심 |
|---|---|---|
| `xpe_ai_init(modelDirPath, configJson)` | AI 를 켤 때 한 번 | `modelDirPath` 는 NULL 불가. 설정 `{"use_worker": true, "timeout_ms": N}` — 기본 시간 예산 5 s. 이미 초기화돼 있으면 무시하고 `XPE_OK`. 잘못된 JSON 은 경고만 하고 기본값으로 **성공**(오류가 아님) |
| `xpe_bone_suppress(img, softTissueOut, configJson)` | 단계 실행 때 | 입력·출력 모두 **`XPE_PIXEL_FLOAT32`, 같은 폭·높이**. 워커 경로(`use_worker`)에서는 약 64 MB 넘는 이미지를 시도 없이·알림 없이·실패 횟수에 넣지 않고 `-7` 로 거부(4096×4096 float32 가 바로 넘는다) |
| `xpe_ai_shutdown()` | AI 끌 때, 그리고 **"이번 세션 비활성"에서 복구할 때**(`shutdown` → `init`) | 호출 후 다시 `init` 가능 |

`xpe_bone_suppress` 의 반환 코드(수치는 `xpe_error.h`): `0` OK · `-1` 잘못된 입력 · `-3` 처리 실패(스텁 빌드에서는 검증 통과 후 **무조건**) · `-4` 모델 파일이 있으나 못 읽음 · `-6` 초기화 안 됨 · `-7` float32 아님 또는 너무 큼 · `-9` 모델 파일 없음. **워커 경로가 실패하면 "워커 또는 전송이 준 코드"를 돌려주므로 위 목록 밖의 값도 올 수 있고, 절대 0 은 아니다.** → GUI 는 `== 0` 만 성공으로 읽어야 한다.

**출력이 어떻게 되나**: 워커 경로 실패 시 `softTissueOut` 는 `img` 와 **바이트 동일**. 검증 거부(`-1`·`-7` 등)에서는 출력을 **건드리지 않는다** — 그 출력을 읽으면 안 된다.

**알림은 모듈이 올린다**(GUI 는 큐를 읽기만 — `ai.cpp` 의 주석, 기존 `NativeAlertDrain` 이 그 일을 한다). 문구는 `ai.cpp`(같은 커밋) 에서 읽음:
- 성공: Info `AI-processed: bone suppression applied (SRS-ALERT-004)` — **성공 단 한 곳**(`memcpy` 뒤).
- 실패마다 Warning(세션당 최대 3개): `AI worker failed (code %d, failure %u of 3): the input image is returned unchanged (REQ-AI-002, REQ-AI-092)`.
- 3번째: `AI worker failed (code %d, failure 3 of 3) and is disabled for this session: input images are returned unchanged (REQ-AI-002, REQ-AI-092)`.
- 그 뒤: 호출마다 `-3` + 출력 = 입력, **알림 없음**, 워커도 안 띄움.

## ② GUI 현황 재확인과 들어갈 자리

**재확인: AI 연동 0건.** 저장소 전체의 `.cs` 중 AI 내보내기 이름(`xpe_ai_init`·`xpe_ai_shutdown`·`xpe_bone_suppress`·`xpe_bodypart_recognize`·`xpe_dl_denoise`·`xpe_stitch_images`·`xpe_ai_set_fallback`)을 담은 파일은 **0개**다. 같은 검색이 살아 있는 `xpe_gsdf_calibrate` 에서는 파일을 찾는다(대조군, `gui_search.txt`).

주의 둘:
- **앱이 둘이다.** 행 10 은 `gui/ImageProcTest/MainWindow.xaml` 에 있다. `clients/ImageProcTest` 는 별개의 옛 P/Invoke 시험 앱인데 `xpe_ai` **카탈로그 항목·준비 상태 문구**(`AI C5/C6 execution is disabled until xpe_ai_worker heartbeat…`)가 있다. 호출은 없고 문구뿐이다 — 행 10 의 자리가 아니다.
- **`PipelineOrchestrator.cs` 는 자리가 아니다.** 그 안의 TODO 는 dicom·preprocess·enhance_basic·display 이고 AI 는 없다(C-152 §3: 어디서도 참조되지 않는 파일).

**들어갈 자리**(코드 사실만): 체인은 `StageIds`(`preprocess`·`gsvg` 둘뿐, `ProcessingChain.cs`)와 `ProcessingChainRunner` 로 단계를 돌리고, 픽셀은 **`ushort[]`** 로 흐른다. AI 단계는 ① 새 `StageIds` 값, ② `RealXpeBackend` 의 네이티브 단계, ③ 새 `NativeInterop`(P/Invoke 선언), ④ **uint16 ↔ float32 변환 둘**이 필요하다. 설계 결정은 하지 않았다.

## ③ 화면이 보여야 하는 것 — 제안

1. **성패는 반환 코드로만.** `rc == 0` → `Applied`, `rc != 0` → `RequestedNotApplied`(픽셀 `null` — 지금 "실행 못 함" 경로가 하는 그대로, `ProcessingChainRunner.cs:68,74`). **AI 단계는 `SequenceEqual` 분류를 우회해야 한다.** 실패 출력은 입력과 같으므로 그 분류를 타면 `AppliedNoChange` 가 된다.
2. **"AI-processed" 라벨은 `Applied`(rc == 0)일 때만.** `RequestedNotApplied`·`AppliedNoChange` 에는 절대 없다. 라벨의 근거는 알림 문구가 아니라 반환 코드다(알림 문구는 레인 간 계약이라 파싱하면 어긋난다 — 문구가 바뀌면 조용히 라벨이 사라진다). 대신 **시험에서** "라벨이 있다 ⇔ 그 호출에서 Info `AI-processed` 알림이 드레인됐다" 를 단언해 둘의 일치를 지킨다.
3. **실패 표시**: 화면에는 **입력 이미지**(체인이 입력으로 되돌아가는 기존 의미)와 함께, 비-OK 임을 숫자로 말하는 줄 — 예: `AI bone suppression NOT applied (code -3): original image shown`. 모듈이 올린 Warning 알림은 기존 알림 경로로 그대로 보인다(GUI 가 따로 만들지 않는다).
4. **"이번 세션 비활성" 은 영속 표시가 필요하다.** 3번째 이후엔 알림이 오지 않으므로 알림만 보면 상태가 사라진다. 그런데 **GUI 가 그 상태를 물을 함수가 없다**(헤더의 10개 내보내기 중 조회형은 없음). 선택지는 셋 — (a) post 에 읽기 전용 상태 조회를 요청(권장), (b) 알림 문구를 파싱(깨지기 쉬움), (c) GUI 가 연속 실패를 세어 3 을 복제(두 곳이 갈라진다). 지금은 (a) 를 요청해야 한다(§4-3).
5. **`-7`(너무 큼)은 "AI 실패"가 아니다.** 시도하지 않았고 알림도 없고 횟수에도 안 들어간다 → `not attempted: image too large for the AI path (code -7)` 로 따로 말한다.
6. **복구 경로**: "AI 다시 시작"(= `shutdown` → `init`)을 제공하지 않으면 3회 뒤엔 앱을 닫는 수밖에 없다.

## ④ 행 10 을 켜기 전에 막히는 것

| # | 막는 것 | 근거 | 누가 |
|---|---|---|---|
| 1 | **QA-B-171C 가 main 에 없다.** 워커 경로·3회 상한은 `5c3588e` 에만 있고 Codex 재감사 중 | 헤더 `diff`(main ↔ `5c3588e`) | post·리더 |
| 2 | **행 10 의 범위가 다르다.** 메뉴는 "Run Full Pipeline (Phase 2/3)", 툴팁은 *"optional premium and assistive modules"* — bone suppression 은 assistive 하나이고 premium(`enhance_advanced`)은 GUI 에 호출이 **0건**(C-175 에서 `xpe_enhance_adv*` 를 `gui/**/*.cs` 에서 검색, 이번에 다시 검색하지는 않았다). "AI 한 단계"로 좁힐지 풀 파이프라인인지 | `MainWindow.xaml`, §2 검색 | 리더 |
| 3 | **"세션 비활성"을 물을 API 가 없다** | 헤더 전체 | post |
| 4 | **float32 입력 스케일 계약이 없다.** 헤더는 float32 만 요구하고, SDD 는 "normalize input to [0, 1]" 이라 적는데 **누가** 하는지(GUI 가 넘기기 전에? 모듈 안에서?) 어느 문서·소스에도 없다(`ai.cpp`·`ai_worker_main.cpp`·`ai_onnx_session.cpp` 에서 `normali[sz]`·`65535`·`scale` 검색: 장난감 모델 주석 1건뿐 — 다른 파일은 검색하지 않았다). 출력의 스케일도 | SDD-002 §bone suppression, 검색 | post |
| 5 | **네이티브 E2E 가 `xpe_ai.dll`·워커 exe 를 스테이징하지 않는다.** 스테이징 목록은 `xpe_common`·`xpe_preprocess`·`xpe_display`·`gsvg` 뿐(`ci.yml:788`). 워커는 `xpe_ai.dll` 과 **같은 디렉터리에서만** 찾는다 | 헤더·`ci.yml` | 리더(.github) |
| 6 | **기본 빌드는 스텁이라 모든 AI 호출이 `-3` 이다.** ONNX 는 별도 프리셋 `ci-ai`(`XPE_AI_USE_ONNXRUNTIME=ON`)와 별도 잡에서만 켜진다(큰 다운로드라 의도적으로 분리 — `ci.yml:404-` 주석). 스텁에서는 **실패 경로만** 관측되고 성공 경로(라벨)는 관측할 수 없다 | `ci.yml`, `CMakePresets.json` | 리더·post |
| 7 | **실제 모델이 저장소에 없다.** `.onnx` 는 시험용 장난감(`modules/ai/tests/data/models_x2/bone_suppress.onnx` 등)뿐. 헤더는 "signed .onnx" 를 말한다 | `git ls-files` | post·사람 |
| 8 | **설정 키가 필요하다**: 모델 디렉터리, `use_worker`(기본 false 라 GUI 가 `true` 를 넘겨야 REQ-AI-003 을 지킨다), `timeout_ms`. `AppSettings` 에 없다 | 헤더, §2 | gui |
| 9 | **적용 대상 제한.** SDD 는 "Only applicable to PA/AP chest" + 비흉부는 건너뛰고 입력 반환. GUI 의 `BodyPartSelector` 와 어떻게 묶을지 | SDD-002 edge case 표 | 리더 |
| 10 | **체인 모델 변경**: `StageIds`, 체인 요청, uint16↔float32 변환 둘, 성패 분류 우회(§3-1) | §2 | gui |
| 11 | **켤 때 지킬 것**: `IsEnabled` 를 풀면 `DisabledFutureCommandCount` 이름 목록·툴팁·A11 세 축 개수를 같이 맞춰야 한다(`#165` 와 같은 사고 방지) | C-176/177 선례 | gui |

## 미검증 · 한계

1. **아무것도 실행하지 않았다.** 헤더·`ai.cpp` 의 주장(알림 문구, 출력=입력, 3회 상한)은 **읽은 것**이지 관측이 아니다. 테스트(`test_alert_ai_processed.cpp` 등)를 돌려 보지 않았다.
2. **경로 한 개만 읽었다.** 알림 문구는 `ai.cpp` 의 워커 경로 블록(`ai.cpp:783-817`)에서 읽었고, 같은 함수의 **프로세스 내 경로**(`use_worker=false`)의 알림·출력 동작은 읽지 않았다. 헤더가 "기본 false" 라고 하므로 GUI 가 `true` 를 넘기지 않으면 모델이 호스트 프로세스 안에서 돈다는 것까지만 안다.
3. **`5c3588e` 가 감사를 통과할 최종 형태인지 모른다.** 리더 말대로 Codex 재감사 중이고 문구·코드가 바뀔 수 있다. 연결 구현은 main 반영 뒤의 헤더로 다시 확인해야 한다.
4. §2 의 "0건" 은 `.cs` 파일의 **이름 검색**이다. `.xaml`·문자열 리소스·설정에서 AI 를 다른 철자로 부르는 곳은 보지 못했다(`clients/ImageProcTest` 의 문구는 봤다).
5. `clients/ImageProcTest`(옛 앱)가 실제로 빌드·사용되는지, 행 10 과 관계가 있는지는 조사하지 않았다.
6. §4-9 의 "흉부" 가 GUI 의 `BodyPartSelector` 항목(`Abdomen`/`Lung`/`Bone`…)과 어떻게 대응하는지는 보지 않았다.

## 증거 파일

`post_ai_excerpts.txt`(출처 SHA·줄 번호가 있는 발췌) · `gui_search.txt`(검색 결과와 대조군) · `text_lint.txt`

🗿 MoAI
