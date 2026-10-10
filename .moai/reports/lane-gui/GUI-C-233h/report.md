# GUI-C-233h 보고 — Codex #177 세 건 (Refs #251)

dev/gui 에서 작업. 푸시 안 함.

## ① 비교 레인 영상을 렌더 기록 안으로 (높음)
- `RenderRecord` 에 `LaneA`/`LaneB`(비교 뷰포트의 Reference/Candidate 영상)와 `Id` 를 추가했다. `LaneAImage`/`LaneBImage` 는 이제 `_render` 에서 계산되는 프로퍼티이고, `LaneBIsStale` 도 기록의 `Inputs` 와 현재 설정을 비교한다(옛 `_renderedLaneB*` 필드 3개 삭제). 따라서 새 프레임·백엔드 교체의 `InvalidateRender` 한 곳이 레인도 같이 비운다.
- 레인 영상은 커밋 **뒤에** 같은 백엔드가 만들므로 `UpdateRender(id, …)` 로 그 렌더의 기록에 더한다. 기록 비교는 객체가 아니라 `Id` 로 한다 — 갱신마다 새 기록 객체가 만들어지므로 객체 비교로는 두 번째 갱신(시간)이 버려진다. 시간(`Timings`)도 같은 방식으로 바꿨다.
- 정책: **같은 프레임의 재렌더 실패**는 주 영상과 기록이 그대로이므로 레인도 함께 유지한다(표 4행). 새 렌더가 성공하면 그 렌더의 레인이 만들어지기 전까지 레인 영역은 비어 있다(이전 렌더의 영상을 새 렌더의 것으로 오해하지 않도록).
- 이벤트 × 필드 표(SelfCheck 13)에 `lanes A/B` 열을 추가했다(실행 출력):

| event                                        | record | appl. | by     | display version    | windowSource  | cal. | chain  | input   | stale     | lanes A/B     | actual |
| start-up (nothing loaded)                    | none   | False | null   | null               | not applied   | null | empty  | not run | -         | -/-          | Mock |
| A rendered (Mock)                            | Mock   | True  | Mock   | v0.0.0-mock-display | automatic     | set  | stages | raw     | -         | A/B by Mock  | Mock |
| setting changed, not re-applied              | Mock   | True  | Mock   | v0.0.0-mock-display | automatic     | set  | stages | raw     | settings  | A/B by Mock  | Mock |
| same frame re-render, display fails          | Mock   | True  | Mock   | v0.0.0-mock-display | failed        | set  | stages | raw     | failed    | A/B by Mock  | Mock |
| new frame, display fails                     | none   | False | null   | null               | failed        | null | empty  | not run | failed    | -/-          | Mock |
| B rendered (Mock)                            | Mock   | True  | Mock   | v0.0.0-mock-display | automatic     | set  | stages | raw     | -         | A/B by Mock  | Mock |
| Mock -> Native                               | none   | False | null   | null               | not applied   | null | empty  | not run | backend   | -/-          | Native |
| re-render succeeded (Native)                 | Native | True  | Native | native-display-9.9 | automatic     | set  | stages | raw     | -         | A/B by Native | Native |
| Native -> Mock                               | none   | False | null   | null               | not applied   | null | empty  | not run | backend   | -/-          | Mock |
| re-initialise (same mode)                    | none   | False | null   | null               | not applied   | null | empty  | not run | backend   | -/-          | Mock |

- 시험 단언: 기록이 없는 모든 행에서 `LaneAImage`·`LaneBImage` 가 비어 있고 `LaneBIsStale` 이 거짓; A 렌더 행에서 레인이 기록의 것이며 Reference 가 주 영상과 같은 객체; 같은 프레임 재렌더 실패 후에도 같은 레인 객체가 유지; Native 재렌더 후 레인이 Native 렌더의 것.
- 반증: 레인이 무효화에서 살아남게 하면(옛 동작) 시나리오 13 이 4칸 빨강 — 새 프레임 실패, Mock→Native, Native→Mock, 재초기화 모두 "the comparison lanes still show a render's pictures (A=set, B=set)" (`falsification_lanes_survive_invalidation.txt`).
- 한계: 레인 영상의 **픽셀 내용**이 옳은지는 이 카드가 보지 않는다(소유와 무효화만 다룬다). 시각적 비교 뷰포트(`ViewportShell.xaml`)를 실제 화면에서 보지는 않았다.

## ② E2E 스크립트 키 (중간)
- 리더 지정 한 건: `tools/e2e/Invoke-ImageProcTestGuiRealE2E.ps1` 의 두 줄만 `RequestedCalibrationEvaluationSummary` 로 바꿨다(diff 2줄/2줄). 다른 tools/ 파일은 건드리지 않았다.
- **실제 fixture 로 스크립트를 한 번 실행했으나 스크립트는 끝까지 통과하지 못했다.** 자체 점검(SelfCheck)은 통과했고 Native 앱 자동화는 끝나 보고서를 썼다(`Passed=True`, `ActualBackendMode=Native`, `DisplayPipelineApplied=True`, 새 키 `RequestedCalibrationEvaluationSummary = "Offset=Off, …, Defect=On, …"`). 그다음 스크립트가 **이번 변경과 무관한 기존 단언**에서 멈춘다: `BackendVersion` 이 manifest 의 `expectedTelemetry.backendVersion = "v0.0.0-mock"` 와 같기를 요구하는데, 스크립트는 2026-09-17 이후 Native 를 명시 요청해서 보고된 값은 `xpe_display 1.0.0` 이다(`ps1_run_native_c233.txt`: "Unexpected backend version in automation report."). 즉 이 스크립트는 Native 로는 이 단언에서 이미 통과할 수 없는 상태이며, 고치려면 manifest 나 스크립트 단언을 바꿔야 하는데 리더가 허용한 범위(키 두 줄) 밖이라 건드리지 않았다. 부모 커밋에서도 같은 단언에서 멈췄을 것이라는 점은 코드로 읽었을 뿐 부모 커밋에서 실행해 확인하지는 않았다.
- 바뀐 두 줄은 스크립트 밖에서 같은 보고서에 대해 따로 평가했다(`ps1_changed_assertions_on_real_report.txt`): 새 키 `-like '*Offset=Off*'` = True, `'*Defect=On*'` = True; 옛 키를 읽으면 StrictMode 에서 "개체에서 'CalibrationEvaluationSummary' 속성을 찾을 수 없습니다" 로 던진다(곧 키 변경을 안 했다면 이 단언이 실패했을 것). 이 평가는 스크립트가 통과한 증거가 아니다.
- 사용한 DLL: `c233_native`(CI 빌드 `ci-dlls-39f9727a` 7개 + post 의 `xpe_display.dll`, `gsvg.dll` — 출처 미증명).

## ③ 간헐 실패 원인 표기 + 시작 시 단언 (중간)
- **정정**: 233g 보고서의 "xUnit 이 병렬로 돌려 겹쳤을 것" 은 틀렸다. `clients/ImageProcTest.IntegrationTests/xunit.runner.json` 이 이미 `parallelizeTestCollections: false`, `maxParallelThreads: 1` 이다. 그 파일을 확인하지 않고 가정했다. 233g 보고서의 해당 절과, `PreprocessModuleCollection`·가드 시험·핸드셰이크 시험의 주석을 정정했다. 7개 클래스를 묶은 collection 은 **병렬화를 켤 때를 대비한 보호 장치**이고 현재의 간헐을 고치지 않는다.
- **원인: 미확인.** 실제 실패 TRX 를 남기지 못했고 재현도 못 했다.
- **찾은 누수**: 7개 클래스의 `init`/`shutdown` 짝을 읽다가 `DataSizeContractTests` 의 `OffsetCorrect_*` 도우미가 `shutdown(); init();` 으로 시작하고 **끝에서 shutdown 하지 않는 것**을 발견했다(그 시험은 DLL 핸들만 해제). 즉 이 3개 시험은 모듈을 초기화된 채로 남긴다. 핸드셰이크 시험은 (233g 이전에) 시작 때 shutdown 이 없어서 바로 뒤에 오면 `init` 이 `INVALID_INPUT` 으로 거부되어 `Assert.Equal(OK, init(...))` 이 실패한다. 이는 관측된 실패의 **후보 원인**이다(xUnit v2 는 collection 순서를 실행마다 다른 식별자로 정하므로 어느 시험이 누수 시험 바로 뒤에 오는지가 실행마다 달라 간헐적일 수 있다). 그러나 **실제 실패한 실행에서 이 순서였는지는 확인하지 못했다.**
- 결정론적 증거: 새 시험 `PreprocessModuleLeakTests.EveryTestMethodOfTheModuleClasses_LeavesTheModuleUninitialised` 가 6개 클래스의 시험 26개를 하나씩 돌리고 각각 뒤에 모듈을 탐침(init 이 받아들여지는지)해 누수한 시험의 이름을 낸다. 수정 전: "DataSizeContractTests.OffsetCorrect_ExactDataSize_PassesSizeGate, …_ShortDataSize_ReturnsInvalidInput, …_ZeroDataSize_PassesSizeGate" (`leak_sweep_BEFORE_fix.txt`). 수정 후(`DataSizeContractTests` 도우미가 finally 에서 shutdown) 통과(`leak_sweep_AFTER_fix.txt`). 목록은 손으로 쓴 호출 26개이며(저장소의 안전 가드 `MockBlockingStaticTests` 가 시험 소스의 리플렉션 생성 API 를 금지해서 `Activator` 방식은 쓰지 못했고 허용 목록도 넓히지 않았다), `TheList_CoversEveryTestMethodOfTheClasses` 가 시험이 추가·삭제되면 빨개지게 목록의 완전성을 지킨다.
- 요청대로: `PreprocessHandshakeTests` 시작 때의 `shutdown()` 을 지우고 모듈이 **미초기화 상태임을 단언**한다(`init` 이 거부되면 "an earlier test left it initialised" 라는 메시지로 빨강). 메커니즘 시험도 시작 shutdown 없이 첫 init 이 OK 여야 한다.
- 7개 클래스의 짝 확인: `PreprocessCorrectionChainSmoke`(첫 시험 finally shutdown ✓, `RunCalibratedChain`/helper finally shutdown ✓), `GainPolyClampAlert`(finally shutdown ✓), `NonlinearityStageWiring`(두 시험 모두 finally shutdown ✓), `PreprocessCorrectionBoundary`(`LoadCalibrated` 가 init, `Release` 가 shutdown — 시험은 finally 에서 `Release` ✓; `LoadCalibrated` 안에서 init 뒤 예외가 나면 `NativeLibrary.Free` 만 하고 shutdown 은 안 한다 → 이 경로의 누수는 스윕이 보지 못했다), `BaselineReviewFix`(실모듈 러너 시험: 러너는 자기 finally 에서 shutdown, 시험의 마지막 `shutdown()` 은 finally 밖 → 시험 도중 단언이 실패하면 남을 수 있음; 스윕에는 이 클래스를 넣지 않았다), `DataSizeContract`(누수, 수정).
- 한계: 위 두 경로(`LoadCalibrated` 의 예외 경로, `BaselineReviewFix` 의 단언 실패 경로)는 **고치지 않았고 시험으로도 덮지 않았다**.

## 통과·범위
- 통합 시험 연속 3회: 951 통과 / 0 실패 / 2 건너뜀 (`integration_suite_runs.txt`). SelfCheck 전 시나리오 통과 (`selfcheck_final.txt`).
- Native E2E·결합 CI 는 돌리지 않았다. 이번 카드에서 실제로 돌린 실행 파일은 위 스크립트의 Native 자동화 1회뿐이다.
