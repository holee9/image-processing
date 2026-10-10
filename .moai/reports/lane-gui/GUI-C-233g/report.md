# GUI-C-233g 보고 — 렌더 기록을 한 객체로 (Codex #176, Refs #251)

dev/gui 에서 작업. 푸시 안 함. 233f(자동 점검 키 이름 `Requested*`)는 `c050f359` 로 따로 커밋해 두었고 이 카드와 겹치지 않는다.

## 구조 (리더 결정 1~4 대로)
- `RenderRecord` (`gui/ImageProcTest/Models/RenderRecord.cs`): 렌더가 **커밋되는 순간** 만들어지는 불변 객체. 담는 것 — 적용 입력 전체(`Inputs` = 렌더 때의 설정 스냅샷), **그 렌더를 만든 백엔드**(`RenderBackend`: 모드·이름·common/display 버전·출처, 커밋 시 티켓의 백엔드에서 읽음), 적용 VOI(`RenderedVoi`), 처리 체인 결과와 상태 줄, AI 라벨, 전처리 실행 여부·사유, 표시 요약, 시간.
- 화면(HUD·상태·진단 패널)과 보고서의 **적용값은 전부 `_render` 에서만** 읽는다: `LastChain`, `ChainStatus`, `PipelineTimings`, `DisplayPipelineSummary`, `RenderedVoi*`(4), `PreprocessRan/Stages`, `AiProcessedLabel`, 보고서의 `bodyPart/gsdf/교정 7+summary/체인/applied/version`. 이 프로퍼티들은 이제 계산식(setter 없음)이다.
- **쓰는 곳은 둘뿐**: `SetRender(record|null)`(파생 프로퍼티 13개를 한 곳에서 알림)과 `InvalidateRender(reason)`(새 프레임 수락, 백엔드 교체). 233b~233e 의 필드별 초기화(`SetRenderedVoi`, `LastChain = null`, `ChainStatus = …`, `PreprocessRan = false`, `AiProcessedLabel = …` 등)는 **삭제**했다 — 남기면 두 경로가 갈라진다.
- 시간(`Timings`)은 레인이 끝난 뒤에야 알려지므로 같은 렌더의 기록에 `with` 로 더하되, **아직 그 렌더가 화면의 기록일 때만**(`ReferenceEquals`) 더한다.
- 현재 설정은 `requested`, 현재 런타임은 최상위 `backend/actualBackendMode/mockBackend`(소비자가 있어 위치는 그대로), 영상을 **만든** 백엔드는 `displayPipeline.renderedBy` 로 구분된다.

## 결정 (리더가 "둘 중 하나를 골라 이유를 보고서에" 요구한 것)
백엔드 전환 직후: **처리 영상을 버리고 원본만 보여 주며 "STALE — the backend was replaced…" 로 표기**한다 (`StaleBackendReplaced`). "옛 기록으로 계속 표시" 를 택하지 않은 이유: 그 기록이 가리키는 백엔드는 이미 `Dispose` 된 뒤라, 그 출처를 현재 상태로 읽는 곳(HUD·`[MOCK]` 표시·보고서)이 늘 두 값 사이에서 갈라진다. 저장 후보(`_committedCorrected`)도 같이 버린다: 후보는 옛 백엔드의 결과이고, 원본 프레임의 같은 `RawPixels` 참조가 그대로라 `CanSaveCorrected` 가 참으로 남는 것을 막기 위해서다.
같은 프레임의 **재렌더 실패**는 기록을 버리지 않는다: 화면의 그림이 바뀌지 않았으므로 기록이 여전히 그 그림의 것이고, `windowSource="failed"` 와 stale 표시가 마지막 시도의 실패를 말한다 (표의 4행).

## 이벤트 × 필드 표 (SelfCheck 13, 실제 실행 출력)
열: record=`CurrentRender` 의 백엔드 모드 · appl.=`displayPipeline.applied` · by=`renderedBy.backendMode` · display version=`displayPipeline.version` · windowSource · cal.=`calibrationEvaluation.summary` · chain=`processingChain.stages` · input=`displayInput` · stale=`PreviewStaleReason` · actual=현재 런타임 모드.

| event                                        | record | appl. | by     | display version    | windowSource  | cal. | chain  | input   | stale     | actual |
| start-up (nothing loaded)                    | none   | False | null   | null               | not applied   | null | empty  | not run | -         | Mock |
| A rendered (Mock)                            | Mock   | True  | Mock   | v0.0.0-mock-display | automatic     | set  | stages | raw     | -         | Mock |
| setting changed, not re-applied              | Mock   | True  | Mock   | v0.0.0-mock-display | automatic     | set  | stages | raw     | settings  | Mock |
| same frame re-render, display fails          | Mock   | True  | Mock   | v0.0.0-mock-display | failed        | set  | stages | raw     | failed    | Mock |
| new frame, display fails                     | none   | False | null   | null               | failed        | null | empty  | not run | failed    | Mock |
| B rendered (Mock)                            | Mock   | True  | Mock   | v0.0.0-mock-display | automatic     | set  | stages | raw     | -         | Mock |
| Mock -> Native                               | none   | False | null   | null               | not applied   | null | empty  | not run | backend   | Native |
| re-render succeeded (Native)                 | Native | True  | Native | native-display-9.9 | automatic     | set  | stages | raw     | -         | Native |
| Native -> Mock                               | none   | False | null   | null               | not applied   | null | empty  | not run | backend   | Mock |
| re-initialise (same mode)                    | none   | False | null   | null               | not applied   | null | empty  | not run | backend   | Mock |

각 행 뒤에 시험이 단언하는 것: 기록이 없는 모든 행(`none`)은 `CheckNoRender` 가 프로퍼티 13개와 보고서 전체(displayPipeline, calibrationEvaluation, processingChain)가 "없음" 임을 확인한다 — HUD 창값, 상태/시간/요약, 전처리·AI 진단, `renderedBy`/`version`/`bodyPart`/교정 요약/단계/EI 관련 필드. 기록이 있는 행은 HUD·상태 프로퍼티가 기록과 같음(A 행)과 `renderedBy` 가 만든 백엔드(Mock/Native)임을 확인한다.

## 반증
- 백엔드 교체 때의 `InvalidateRender` 를 빼면 시나리오 13 이 25칸 빨강 (`falsification_no_invalidation_on_backend_replace.txt`: "after Mock -> Native: a render record still stands", "…the HUD window is still a render's", "…the report names a producer or says applied" 등).
- 새 프레임 때의 `InvalidateRender` 를 빼면 시나리오 11·13 이 17칸 빨강 (`falsification_no_invalidation_on_new_frame.txt`).
- 두 번 모두 복구 후 통과 (`selfcheck_final.txt`).

## PreprocessHandshakeTests 간헐 실패 (Codex #176 발견 2)
- **확정한 것(결정론적)**: 모듈은 프로세스 전역 상태 하나다. 초기화된 동안 두 번째 `xpe_preprocess_init` 은 `XPE_ERR_INVALID_INPUT` 을 돌려준다(`modules/preprocess/src/preprocess.cpp`). 새 시험 `PreprocessInit_WhileAnotherCallerHoldsTheModuleInitialised_IsRefused_AndSucceedsAfterAShutdown` 이 이를 고정한다(init→OK, 재init→OK 아님, shutdown 뒤 init→OK). 실패한 단언(`init == OK`)과 정확히 일치한다.
- **병렬성이 가능한 경로**: `xpe_preprocess_init` 을 부르는 시험 클래스가 7개(Handshake, DataSizeContract, GainPolyClampAlert, NonlinearityStageWiring, PreprocessCorrectionBoundary, PreprocessCorrectionChainSmoke, 그리고 233c 의 실모듈 러너 시험을 가진 BaselineReviewFix)인데 어느 것도 collection 이 없었다 → xUnit 이 병렬로 돌릴 수 있다. 이 실패를 처음 본 것은 233e 의 전체 실행이다(233c 가 같은 모듈을 부르는 시험을 추가한 뒤이지만, 그 전에 없었는지는 확인하지 못했다).
- **고친 것**: 그 7개 클래스를 한 collection(`PreprocessModuleState`)에 넣었고(직렬), 핸드셰이크 시험은 시작 전 shutdown·끝에서 finally shutdown 으로 맞췄다. 새 가드 시험(`PreprocessModuleCollectionGuardTests`)이 "모듈을 init/실행하는 호출을 가진 파일은 모두 이 collection 안" 을 파일 내용으로 검사한다(양성 대조 포함). 클래스 하나에서 속성을 빼면 빨강 (`falsification_collection_attribute_removed.txt`).
- **확정하지 못한 것**: 실제 실패가 정확히 어느 두 시험의 겹침이었는지는 못 잡았다. 실패 TRX 를 남기지 못했고(233e 첫 관측은 한 줄만 보존, 이후 한 번은 리다이렉트 실수), 이 카드에서의 재현 시도는 **재현 실패**다: 러너 시험+핸드셰이크 시험을 함께 20회 실행해 0건, 그 뒤 전체 스위트를 19회 더 돌려(−v q, 일부 −v n) 핸드셰이크 실패 0건 (`handshake_repro_runs.txt`). 즉 "병렬 겹침이 원인" 은 위 메커니즘과 병렬 가능 경로로 **추정**이며, 수정 뒤 간헐이 사라졌음을 입증한 것은 아니다. 부모 커밋(`17d77303`)과의 비교로 회귀를 분리하지는 못했다. 관찰이 쌓일 때까지 가설로 둔다.
- 별개로, 이 카드 중 전체 스위트 10회에서 매번 1건 실패한 것은 **이 카드의 변경이 원인**이었다: `CorrectedImageCommitTests` 의 "후보 쓰기는 한 곳" 정규식이 `InvalidateRender` 의 `_committedCorrected = null;` 도 쓰기로 셌다(`\s*` 역추적). 널 대입을 제외하도록 고쳤다 (원문 1건: `full_suite_failing_run_raw_1.txt`).

## 통과·범위
- 통합 시험 연속 3회: 949 통과 / 0 실패 / 2 건너뜀 (`integration_suite_runs.txt`). SelfCheck 전 시나리오 통과 (`selfcheck_final.txt`).
- 이 카드에서 Native E2E·결합 CI 는 돌리지 않았다. Native 백엔드 전환은 스크립트된 `NativeNamedBackend`(RealXpeBackend 라고 자칭하는 시험용 백엔드)로만 검증했다 — 실제 DLL 로 Mock↔Native 를 오가는 경로는 미검증.

## 범위 밖(열린 것)
- Lane B/후보 레인 영상(`LaneBImage` 등)은 기록에 포함하지 않았다. 새 프레임·백엔드 교체 때 그 영상이 어떻게 되는지는 이 카드가 바꾸지 않았다.
- 보고서 최상위 `backend/actualBackendMode/mockBackend` 는 "현재 런타임" 이다. 옮기지 않은 이유는 소비자(E2E)가 있어서이며, `runtime` 아래로 옮기는 것은 별도 결정이다.
