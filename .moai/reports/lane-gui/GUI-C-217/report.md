# GUI-C-217 — 뼈 억제 -4 설명이 원인을 단정한다 (Codex #102, #130)

## 결론

`-4` 문구가 "모델이 쓸 수 없는 상태"를 원인(파일 손상·서명 거부)으로 단정하던 것을, 가능한 원인을 모두 **가능성으로** 적고 "잠시 뒤 다시 시도"를 안내하는 문구로 바꿨다. 접두사 `AI bone suppression NOT applied (code -4)` 는 그대로다(C08·C09 단언 불변). 모듈 계약은 건드리지 않았다.

| 항목 | 값 |
|---|---|
| IntegrationTests | 817/817 통과 |
| E2E Native (post 모듈 `99e5dd1e`, 실제 -4 경로) | C08 · C09b · C10 · C09 통과, C11 은 설계대로 건너뜀(아래) |
| 반증 | 6건 전부 빨강 |

## 문구 (`gui/ImageProcTest/Services/AiBoneSuppressionStage.cs`, `ModelUnavailable` 분기)

이전: `… the model in the model directory cannot be used by the module (the file is damaged or is not a model, or its signature did not verify; …)` — "damaged", "did not verify" 를 단언.

이후(E2E 가 실제 모듈에서 받은 문장 그대로, `e2e_native_post_module.txt`):

> AI bone suppression NOT applied (code -4): the module cannot use the model in the model directory right now. The file may be damaged or not a model, its signature may not have verified, or another call may still be loading and verifying the model (try again in a moment); the alert list names the cause when the module raised one. The original image is shown. Whether this call counts toward the AI worker's failure total is the module's decision; the AI worker mark shows when it has switched off.

- 세 원인(손상·비모델 파일 / 서명 미검증 / 다른 호출이 적재·검증 중)을 모두 "may"로 적고 어느 것도 단정하지 않는다. 구분은 안 한다 — 모듈이 코드로 구분해 주지 않기 때문이다.
- 이전 판의 부정 단언(워커 실패 총계에 대한 주장 없음, "consecutive failures switch" 없음)은 그대로 유지했다.
- 타입 주석(`ModelUnavailable`)도 "모델 파일이 있고 나쁘다"에서 "모듈이 지금은 모델을 쓰지 않는다 — 198c 에서 같은 역할의 적재·검증 중에 겹친 호출도 즉시 -4 를 받는다"로 고쳤다.

## 시험

- `AiBoneSuppressionStageTests.AModelThatTheModuleWillNotUse_IsSaidSo_WithoutAClaimAboutTheWorkersFailureTotal`: 접두사, 세 원인 각각의 "may" 문구, "try again in a moment", 단정형 문구 부재(`is damaged`, `did not verify`), 기존 부정 단언, 다른 오류 코드는 일반 문구 유지.
- E2E `C09b`(실제 `-4` 가 나올 때만 문구를 단언하는 기존 구조): `the module cannot use the model` 과 `another call may still be loading and verifying the model` 로 갱신. 동시 호출 자체를 일으키는 방법은 gui 쪽에 없어 **동시 호출 시나리오는 만들지 않았고**, 문구는 단위 시험과 단일 호출의 실제 `-4` 로 확인한다(요청대로).
- 로컬 E2E 를 post 모듈(`$TEMP/c215_native`, 215 에서 조립)로 돌려 `-4` 경로를 실제로 탔다. 그 빌드의 모델 검사는 가짜 파일에 `-4` 를 돌려주므로 C09b 가 이 문구를 직접 읽었다. 로컬의 main 스텁 모듈에서는 `-3` 이라 이 문구가 나오지 않는다.

### 반증 (`falsification_arms.txt`, 6건 모두 빨강)

옛 문구 복원 / "다른 호출이 적재 중" 원인 제거 / "다시 시도" 안내 제거 / 접두사 `NOT applied` 변경(2건: 단위 + 접두사 단언) / 서명 원인을 다시 단정형으로 / `-4` 문구에 일반 "consecutive failures switch…" 문장 추가.

## 제안 (적용하지 않음, 모듈 계약 변경)

"일시 사용 불가"(다른 호출이 적재 중)를 별도 반환 코드로 구분하면, gui 가 "잠시 뒤 다시 시도"를 단정할 수 있고 사용자가 원인을 문구 한 줄로 알게 된다. 지금은 `-4` 하나가 영구 결함(없는 파일·서명 거부)과 일시 상태(적재 중)를 함께 담아, 문구가 가능성을 나열할 수밖에 없다. 필요하다고 보면 post 와 정할 사항이다.

## 한계

- 겹친 호출이 실제로 `-4` 를 받는 순간(198c 의 동시성)은 이 레인에서 재현하지 않았다. post 의 설명에 기댄다.
- 이전 문구를 읽는 곳을 검색했으나(`cannot be used by the module`) 시험 2곳과 소스 1곳뿐이었고 모두 갱신했다. 문서·XAML 에는 없다.

# 부록 — 리더가 요청한 enhance_basic 호출처 목록 확인 (xpe_edge_enhance 출력 변경, post QA-B-200 M2b)

검색은 소스 읽기(`grep`)이고 어떤 것도 실행하지 않았다.

**리더의 호출처 목록 확인**: 맞다. 단 아래가 더 있다.

| 추가로 찾은 것 | 의미 |
|---|---|
| `clients/ImageProcTest/Diagnostics/XpeEnhanceBasicReadinessProbe.cs` | 레거시 준비도 스모크가 `xpe_edge_enhance` 를 `XpeUsmParams.Default` 로 부른다. 판정은 **반환 코드 전부 OK + 화소·시그마가 모두 유한**일 뿐이라 음수 화소는 통과한다(유한). 값이 무한·NaN 이 되면 실패한다 |
| `clients/ImageProcTest/Services/AlgorithmValidationCatalogService.cs` | 시험 항목 설명 문자열("edge detail, overshoot…")뿐, 값 비교 없음 |
| `clients/ImageProcTest.IntegrationTests/Functional/EnhanceBasicStageTests.cs`·`BaselineReviewFixTests.cs` | **가짜 백엔드**로 호출 순서·파라미터(`usm 0.5/2/10`)만 확인, 실제 출력값 아님 |
| `.github/workflows/ci.yml` | 주석에서 네이티브 시험 `EdgeEnhance.Performance_3072x3072_Within20ms` 를 언급할 뿐(`*Performance*` 필터로 제외). 선명화가 강해져 느려지면 이 시험이 잡는 것은 아님 |

**해시·기준값을 저장해 비교하는 gui/clients 시험은 없다**(리더의 결론과 같다). 확인한 것: `BaselineDeterminismTests`(해시 함수 자체와 같은 빌드의 두 실행 비교), `BaselineExecutionTests`(해시 길이 64·단계 해시 개수), `AutomationReportBackendTests`(`BaselineOutputSha256` 길이 64) — 값이 아니라 형태만 본다. 저장된 enhance 픽스처도 없다(`ci-provenance.json` 은 DLL 출처). `P10PreviewTileSignature.txt` 와 `PreviewBaselineHash` 는 wrist-slice 시나리오(gsvg·display)용이고 그 시나리오는 enhance_basic 단계를 켜지 않는다(검색 결과 없음).

**음수 화소에 대한 gui 쪽 사실**(하한 처리 결정에 쓸 수 있음): 사용자 앱 `EnhanceBasicStage` 는 모듈 출력의 float 를 16비트로 바꿀 때 **자체적으로 0 으로 자르고**(`< 0` → 0, `> 65535` → 65535) 잘린 개수를 요약에 쓴다("clamped below N above M"). 비유한(NaN/Inf)은 거부한다. 즉 사용자 앱에서는 음수 화소가 이미 0 으로 잘려 다음 단계로 간다. 레거시 미리보기(`NativeEnhanceBasicPreviewService`)가 음수를 어떻게 렌더링하는지는 이번에 보지 않았다.
