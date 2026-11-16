1차 조사 보고 — GUI-C-268, 수정 없는 D258 단독20P/0F/0S(로컬 실패0/20); CI 실패2건 원문 확인, 원인 미확정·제품 수정 없음·임시 계측 접근 승인 대기

## Claim

이 카드의 원인이나 해결을 확정하지 않았다. 현재 코드의 D258 단독 반복은20회 모두 통과했다. CI의 혼합 footer 실패2건은 직접 회수한 원문으로 확인했으나, 로컬 실패 회차 및 개별 UIA 읽기 대 SetRender 시각 기록은 얻지 못했다. 카드의 "확정 못 하면 수정하지 않고 사실만 보고"에 따라 원인 수정은 하지 않았다. main CI 초록 완료 조건은 충족했다고 주장하지 않는다.

## Evidence / Baseline-attribution

2026-11-17 02:44:40.0937289~03:03:28.2317985 KST, dev/gui HEAD `60a88521de73f401f49e9158dc541197bda24799`. 별도 testhost로 D258만20회 실행했다. 각 TRX의 total1/passed1/failed0/skip0, 모든 명령 exit0, 모든 종료 appAfter 빈 배열. 집계20/20P/0F/0S다. 정확한 명령, 회차별 시작·종료·분모·결과·원문 SHA256은 `baseline-summary.md`에 있다. raw stdout/stderr/TRX/result는 `baseline/`에 원형 보존했다. 실패를 통과로 바꾸는 내부 재시도는 넣지 않았고20개 독립 표본을 사전에 선언했다.

실행은 `--no-build --no-restore --filter FullyQualifiedName~D258_MetricsFooterAndPipelineOrder_AreVisibleAndDistinct`다. 기존 GUI-C-267에서 빌드한 실행물과 카드 native 폴더(PRE d3216b2a, 다른 모듈 기존 bundle 혼합, CSV·출처 기록 포함)를 사용했다. Native3 경로를 고정하고 전역 키보드·마우스 입력은0을 유지했다. 그 밖의 상속/실효 XPE 환경과 소스 해시는 `baseline/preflight.json`에 보존했다. main4dfd30ef와 관련 GUI/시험 소스 diff는 출력 없음이었다. 측정 종료 후 고유 소스434파일 해시 불일치0건이며 이번 조사에서 제품·시험 소스는 수정하지 않았다. CI 실행물·desktop 조건과 동일하다는 주장은 하지 않는다.

## 현재 판정과 다음 행동

관측 실패 빈도는 이 로컬 조건에서0/20이다. 실제 실패 확률0이나 CI 실패가 해결됐다는 뜻이 아니다. 기존 코드에서 footer 세 값은 다른 UIA 호출로 순차 읽히고, 최초 RenderRecord 대입과 파생 속성 알림은 그 사이에 일어날 수 있다. 그러나 VM 상태 전환·바인딩 반영·UIA 표현 중 어느 경계인지 직접 기록이 없어 가설로 남긴다. 같은 fixture의 나머지 시험은 RAW 재로드/렌더 무효화 동작이 없었고, 회수한 두 CI 로그에서 footer 시험 실패 뒤 다른 두 시험의 결과가 기록돼 있다. 다른 시험의 상태 재설정을 원인으로 확정할 근거는 찾지 못했다.

계측안은 아래 두 지점으로 제한해 질문 채널에 요청했다. 승인 전에는 적용하지 않는다. 답변이 오면 원본 빈도와 분리된 진단 실행에서 실패 읽기와 commit 시각을 대조하고, 그때도 원인이 확정돼야 근본 수정 한 건을 진행한다. 현재 보고서는 사실 회수이며 카드 종료/버그 해결 판정이 아니다.

## CI 원문 재관측

GitHub 연결 도구로 run43852576742/job133271719096 및 run43905322843/job133458155099의 gui-e2e-native 로그를 직접 회수했다(`ci-43852576742.raw.log`, `ci-43905322843.raw.log`). 첫 로그3450행은 `Chain not run · window not measured | preprocess 3.63 ms · enhance_basic 109.036 ms … | WARN…`, 둘째4064행은 같은 초기 문구와 `preprocess 3.194 ms · enhance_basic 72.368 ms … | WARN…`다. 모두 D258의56행 Contains 판정 실패다. GitHub 로그의 타임스탬프는 출력 기록 시각이며 개별 UIA 읽기 시각이나 SetRender 시각으로 취급하지 않는다.

현재 소스의 SetRender는 MainWindowViewModel.cs:1211–1217, null 상태 문구는1165–1172, async 처리 완료 후 SetRender 호출은3063행이다. ReportChain은3036행으로 그보다 앞서므로 체인 로그가 먼저 보이는 것만으로 footer 준비를 보증하지 않는다. 기존 timing 옵션은 프로세스 시작·창 획득·질의 시각만 기록하고 SetRender/개별 footer 읽기 시각은 기록하지 않는다. --automation-report 옵션은 Loaded의 실행 분기를 바꾸므로 원본 재현 조건의 관찰 도구로 사용하지 않았다.

## 수신·범위

20261117-0244-gui.md 및 inbox/GUI-C-268.md를 읽었다. D258 단독 반복으로 빈도를 측정하고 실패 회차의 UI 읽기와 체인 완료 순서를 확인한다. 대기 증가·재시도로 통과시키지 않는다. 원인 확정 전 제품 수정은 하지 않는다.

## 조사 기준과 순서

현재 dev/gui HEAD는 60a88521de73f401f49e9158dc541197bda24799다. card가 지정한 main4dfd30ef와 clients/ImageProcTest.E2ETests 및 gui/ImageProcTest의 코드 diff는 출력 없음이었다. 기존 작업·원문·설정을 보존한다. 먼저 현재 시험 그대로20회 독립 실행하고 결과 원문을 각각 보존한다. 실패가 나오면 기존 상태 생성·UI 바인딩·읽기 순서를 시간 기록으로 대조한다. 필요 시 진단 출력만 추가하며 단언·대기·재시도는 바꾸지 않는다.

## 읽기 추적 / 가설

D258은 StateDisplayScenarios.cs:52에서 ChainStatusText → FrameTimingText → StatusBarText를 순서대로 읽는다. MainWindowViewModel의 FooterSummary는 _render가 null이면 초기 문구를 반환하고, SetRender는 _render 대입 후 파생 속성 변경을 순서대로 알린다. ApplicationFixture의 초기 UI 준비는 render 완료를 보장하는 계약인지 확인 중이다. 최초 렌더가 UIA의 연속 읽기 사이에 완료되는 경합은 가설이며 런타임 증거 전에는 결함 확정으로 보고하지 않는다.

## Gaps / Residual-risk

CI 실패2회는 위 GitHub 원문으로 재관측했다. 과거11-09 동일 현상은 리더 제공 기록이며 원문 재확인은 하지 않았다. 로컬 반복에서는 아직 실패가 없고, 실패 회차의 개별 UIA 읽기 대 SetRender 순서 기록은 없다. 원인 확정·수정 여부 판정·커밋은 아직 남아 있다. main CI 초록 판정과 push는 리더 범위다.

## 직접 시각 계측안 / 아직 미적용

현재 기록만으로는 VM-null 상태 읽기와 바인딩 반영 지연을 구분할 수 없다. 직접 순서를 얻기 위한 임시 계측은 두 파일로 한정한다. StateDisplayScenarios.cs의 기존 세 Name 읽기 각각의 시작/종료 QPC·UTC와 반환값을 저장하고, 세 판정값을 모두 저장한 뒤 출력한다. MainWindowViewModel.cs의 SetRender는 환경변수로 지정된 진단 실행에서만 진입/상태 대입/알림 완료 시각과 render ID/null 여부를 기록한다. 파일 출력은 알림 뒤에 수행하고 기존 분기·await·단언·대기·재시도는 바꾸지 않는다. 계측 자체의 비용이 경합 빈도에 영향을 줄 수 있으므로 원본20회와 분리하며 원본 빈도의 대체로 쓰지 않는다. 이 계측은 근본 수정이 아니고 최종 수정에 섞지 않는다. AGENTS.md §13의 비단순 코드 접근 승인에 따라 적용 전 질문 채널로 확인을 요청한다.
