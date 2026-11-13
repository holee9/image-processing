[2026-11-13 17:07 KST] 검증 완료 — 지정6P/0F, 전체371=345P/1F/25SKIP(A06만 실패), 정상 훅 커밋 준비·push 없음

## 최종 Claim / Evidence

수정은 `clients/ImageProcTest.E2ETests/Scenarios/Workflows/EvidenceBundleScenarios.cs`의 EvidenceRoot helper뿐이다. XPE_GUI_DATA_DIR 없음/공백이면 GUI EXE 폴더, 절대 경로이면 Path.GetFullPath(root), 상대 경로이면 앱과 같은 InvalidOperationException을 던진다. 앱의 GuiDataPaths.Root는 호출 host의 AppContext.BaseDirectory를 사용하므로 FlaUI host에서 직접 쓰면 기본 경로가 GUI 폴더와 달라진다. 이 이유를 주석으로 남기고 검사식만 옮겼다. 단언·대기·기대값과 제품 코드는 변경하지 않았다.

| 검증 | 시작→종료 KST | 결과 | 원문 |
|---|---|---|---|
| 지정3개 / 변수 있음 | 16:19:24→16:20:30 | 3P/0F/0SKIP, exit0 | with-root.stdout.txt / stderr.txt / trx / result.json |
| 지정3개 / 변수 없음 | 16:20:30→16:21:33 | 3P/0F/0SKIP, exit0 | without-root.stdout.txt / stderr.txt / trx / result.json |
| 전체 / 변수 있음 | 16:21:33→17:05:45 | 345P/1F/25SKIP, total371, exit1 | full.stdout.txt / stderr.txt / trx / result.json |

`results.md`에 지정6개의 정확한 시험명·PASS와 전체 실패1개·SKIP25개의 이름·사유·stack을 TRX 원문으로 발췌했다. 지정 환경은 같은 Native3 경로/exclusive1·전역 입력 금지0이고, without-root 자식에서만 XPE_GUI_DATA_DIR를 제거했다. full은 재개4와 같은 데이터 루트 설정이며 필터·StopOnFail 없음. 명령 전체와 자식 환경은 각 result.json, 실행 절차는 run-once.ps1, 소스해시·환경·정책은 preflight.json에 보존했다.

전체 stdout 원문: `Total tests: 371 / Passed: 345 / Failed: 1 / Skipped: 25 / Total time: 44.1894 Minutes`. TRX Counters total371/executed346/passed345/failed1/aborted0/timeout0, 결과 NotExecuted25개. EB02·IB01·IB02는 전체 실행에서도 PASS, D258 MetricsFooter·W5도 PASS. 유일한 FAIL은 `AutomationReportBackendTests.A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported`: `The GUI E2E runner RAN and FAILED when launched from the app: 'GUI E2E FAILED (exit -532462766): Unhandled exception. System.InvalidOperationException: WPF E2E failed.'.` 이 카드에서는 고치지 않았고 재실행하지 않았다.

GUI-C-262의 A06 문구는 F262-1에 맞춰 “변수 제거 회차 PASS는 실측, 내부 Real 선택은 미관측 추정”으로 한정했다. GUI-C-262의 4P와 이번 전체 A06 실패를 함께 보존한다. main CI 성공은 리더 전달 사실이며 이번 로컬 검증의 baseline으로 섞지 않는다.

## Baseline-attribution / 정리

트리 `D:/workspace-github/xpe-gui`, 시작 HEAD `09d3cf6496875aa520d3c224f775449574a5eab0` + 위 helper 미커밋 diff를 측정했다. 실제 파일 해시는 preflight.json에 있다. 외부 Native DLL과 보정/raw/baseline은 기존 경로를 유지했다. 새 결과 폴더를 회차별로 만들지 않고 이 카드 폴더에 고정 이름으로 기록했다.

full 결과 appAfter=[]. 해당 full testhost PID23604의 A05 임시 폴더는 실행 구간 생성·일반 파일17개·하위 폴더/링크 없음 확인 → ZIP17항목별 SHA 일치 검증 → 파일별 삭제·빈 폴더 삭제했다. `temporary-selfcheck-A05-23604.zip` SHA256 `53BD3EDFC78FF4AA1433F03BE08AAD5CA50B876352D096E86E6FABF83E885990`; 임시 폴더 없음 재확인. 다른 Temp 전체를 정리하지 않았다. 설정·evidence를 수동 편집/복원/삭제하지 않았으며 시험·제품이 정상 동작 중 생성/저장한 파일은 유지했다.

## Gaps / Residual-risk

상대 경로 거부 분기는 앱의 같은 식·예외 메시지로 맞췄으나 이번 승인 검증은 절대 경로 있음/없음 두 조건이며 상대 경로 실행은 하지 않았다. 전체 suite는 A06 때문에 실패 상태이고 성공으로 보고하지 않는다. A06 내부 실제 백엔드 선택은 여전히 미관측이다. 다른 경로 가정은 아래 기재한 텍스트 검색 범위까지만 조사했다. 추가 수정·검증 확대·병합·push는 리더 판단 사항이다.

16:19 지시 파일을 읽고 질문을 해소했다. 실제 최초 재개 시계는 16:18:42 KST였다. GUI-C-263의 helper는 변수 없음/공백이면 GUI EXE 폴더, 절대 경로이면 정규화한 데이터 루트, 상대 경로이면 앱과 같은 InvalidOperationException 메시지를 쓴다. 단언·대기·기대값은 변경하지 않았다. 직접 GuiDataPaths.Root를 호출하면 시험 host의 AppContext.BaseDirectory를 기본 루트로 얻게 되므로 작은 검사식을 옮기고 그 이유를 주석에 남겼다. 변경은 시험 파일의 EvidenceRoot helper만이다.

검증 도우미는 기존 Native3·전역 입력 금지·보정/raw/baseline 조건을 유지한다. with-root(지정3개) → without-root(지정3개) → full(변수 있음·필터/StopOnFail 없음) 각1회이며 실패를 덮는 재시도는 없다. 기존 261 RED는 그대로 보존한다. 설정·evidence를 직접 수정하지 않으며 제품이 시험 중 자동 저장한 변화가 있으면 구분한다.

범위: EvidenceBundleScenarios의 루트 helper만 수정, 단언·대기·기대값 유지. GUI-C-262 A06은 변수 제거 회차 PASS 실측과 내부 Real 선택 미관측을 분리한다. 지정3개를 변수 있음/없음 각각1회, 전체 E2E를 변수 있음으로1회 실행한다. 제품·설정·기존 evidence 수동 수정 없음. 상대 경로 처리에 카드 문구와 앱의 실제 거부 규칙 차이가 있어 질문 채널로 확인 중이다. 답변 전 의존 코드 변경·실행은 하지 않는다.

## 현재까지 확인

- GUI-C-262 report의 첫 줄 및 Claim을 F262-1대로 고쳤다: 변수 제거 회차 4P는 실측이며 A06 내부 Real 선택은 미관측 추정이다.
- 수정 전 실패 증거는 GUI-C-261의 e2e.trx(EB02/IB01/IB02 FAIL)와 ab-A1/B1/A2/B2.trx(각 3개 모두 FAIL)에 이미 보존돼 있다. GUI-C-262는 변수를 제거한 환경에서 이 3개를 포함한 4P를 기록했다. 이는 기존 관측이며 이번 카드에서 새로 재실행한 수치가 아니다.
- 검색: E2E·Integration의 bin/obj/.moai를 제외한 .cs 파일에서 `"evidence"` 및 `EvidenceRoot(`를 조회했다. EXE 옆 경로 고정은 `EvidenceBundleScenarios.cs:209–210`의 helper 하나이며 EB03:84와 RecordAndExport:198이 공유한다. `UnappliedSettingsScenarios.cs:212`의 `evidence`는 JSON 속성 접근이고 디렉터리 계산이 아니다. Integration의 6개 일치는 표시용 문자열이다. 이 검색 패턴·범위 밖에 같은 가정이 전혀 없다고 주장하지 않는다. 다른 시험은 고치지 않았다.

## 확인이 필요한 요구 차이

카드 §2는 “XPE_GUI_DATA_DIR가 절대 경로면 그 아래 evidence, 아니면 EXE 옆”이라고 적었다. 앱 `GuiDataPaths.cs:12–15`는 변수 없음/공백일 때만 AppContext.BaseDirectory를 사용하고, 값이 상대 경로이면 `InvalidOperationException("XPE_GUI_DATA_DIR must be an absolute user-data path.")`로 거부한다. 두 동작은 상대 경로에서 다르다. 질문 채널에 “앱과 동일하게 상대 경로 거부 / 카드대로 상대 경로는 EXE 옆” 선택을 요청했다. 답변 전 루트 helper 수정·시험 실행·커밋을 보류한다. 단언·대기·제품·설정·evidence 변경 없음.
