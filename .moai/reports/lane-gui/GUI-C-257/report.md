2026-11-04 07:36 KST — GUI-C-257 (Refs #281) 코드 인계. 실제 Native GUI 캡처 대기.

## 확인한 동작

레인 B 선택칸을 정본으로 하는 `CandidateGsvgMode`를 추가하고 실제 Parameters 라디오를 연결했다. 별도 저장값은 만들지 않았다. Virtual 선택→Virtual 라디오 체크, Suppress 라디오→Grid suppression 선택칸 변경을 실제 AnalysisPanel의 WPF 바인딩으로 확인했다. A의 `Settings.GsvgMode`는 이 조작으로 바뀌지 않는다.

기존 코드에서도 **이름이 다른 B=Virtual grid 요청은 Candidate 백엔드에 도달했다**. 재현에서 이 단언은 이미 통과했다. 기존 결함은 라디오의 A 설정 연결, B 결과가 빠진 체인 줄, 요청명을 그대로 쓰는 태그였다. 현재 체인 줄은 B 단계 상태와 미적용 이유도 표시한다.

추가 경계로 A/B 선택 이름이 같아도 기존 저장된 main mode가 다른 경우 B 요청이 생략됨을 실제 실패 시험으로 확인했다. 이제 이름 외에 preset 적용 뒤의 grid mode/PRE 요청도 비교한다. 실제 요청 설정이 같으면 기존처럼 한 번만 처리한다.

태그는 해당 렌더의 stage 결과를 따른다. Grid Applied라도 `Preprocess + virtual grid`의 PRE가 Skipped면 `(not applied)`다. AppliedNoChange는 적용으로 인정한다. 실행 전 수정은 기존 stale 플래그를 켜고 이전 렌더 태그를 유지한다. 실패한 Candidate는 그림·태그·단계 사실을 함께 제거한다.

## 바뀐 파일

| 파일 | 변경 |
|---|---|
| `gui/ImageProcTest/ViewModels/MainWindowViewModel.cs` | CandidateGsvgMode facade/알림; B 결과를 포함한 ChainStatus; 실제 결과 기반 태그; Candidate 결과·window snapshot 회수; 동등 설정일 때만 A 결과 공유 |
| `gui/ImageProcTest/Models/RenderRecord.cs` | LaneAFacts/LaneBFacts 및 LaneRenderFacts/RenderedStage. 보유 값은 stageId/status/reason/ms와 실제 VOI뿐. 새 픽셀/float 배열 보유 없음 |
| `gui/ImageProcTest/Views/AnalysisPanel.xaml` | 기존 Grid correction 라디오 3개를 facade에 바인딩 |
| `gui/ImageProcTest.SelfCheck/LifetimeScenarios.cs` | Mock 기반 실제 VM·AnalysisPanel 계약 시험, 선택 실행, 기존 태그 기대값을 카드의 미적용 표시에 맞춤. 동일 레인 시험은 이름뿐 아니라 실제 설정을 같게 준비 |
| `gui/ImageProcTest.SelfCheck/Program.cs` | freshness guard 뒤 `--lane-contract` 선택 실행 |

## 실행·관측

기준: `D:/workspace-github/xpe-gui`, `dev/gui`, HEAD `1f6746096d602f5d56185a8bae6e11f3742bb9ec` + 이번 미커밋 변경. 256 완료 코드 및 기존 외부 변경을 보존했다. stage/commit/push 등 git 변경은 하지 않았다.

```text
.moai\reports\lane-gui\GUI-C-254c\env.cmd C:\Progra~1\dotnet\dotnet.exe build gui\ImageProcTest.SelfCheck\ImageProcTest.SelfCheck.csproj -c Debug --nologo -v:q
.moai\reports\lane-gui\GUI-C-254c\env.cmd C:\Progra~1\dotnet\dotnet.exe gui\ImageProcTest.SelfCheck\bin\Debug\net8.0-windows\ImageProcTest.SelfCheck.dll --lane-contract
```

초기 RED: 새 통합 시나리오 **1개**, 실패 단언 7개, Exit `-532462766`. 원문 `red.txt`. 선택 모드가 다른 Virtual 요청의 전달은 처음부터 통과했으며, 이를 요청 실패로 보고하지 않았다. 후속 RED: 같은 이름/다른 실제 main mode 경계 단언 **1개** 실패, Exit `-532462766`, `red-equal-mode.txt`. 수정 전에 각각 실행했다.

최종 분모는 통합 계약 1개 + 기존 인접 회귀 5개 = **6개 시나리오**다. 실패 단언 수를 독립 시험 개수로 세지 않는다. 기본 전체 suite에도 새 계약 시험이 포함된다. 전체 suite를 실행한 것은 아니다.

최종 GREEN 원문(`green.txt`, Exit 0):

```text
  Candidate Virtual request observed; actual radios, stale tag, refused/PRE-skipped/AppliedNoChange/NotRequested outcomes checked
  scenario 'GUI-C-257 lane selection and result contract': 470 ms
  scenario '18 Lane A preset keeps main preprocessing': 62 ms
  scenario '5b equal lanes = one AI call per Apply': 32 ms
| lane tags after render Grid suppression (not applied)/Grid suppression (not applied) -> after unapplied change / (requested No grid correction/No grid correction, record No grid correction/No grid correction); stages not run (native backend required); Candidate failed -> lanes none/none |
  scenario '14 lane tags, stage count, failed candidate': 362 ms
  scenario '3 stale failing Lane B': 127 ms
  scenario '3b stale succeeding Lane B': 126 ms
Lifetime scenarios passed (6 scenarios).
```

인접 회귀의 oracle: 기존 PRE on/off 보존 및 PRE 포함 preset의 on 전환; 실제 동일 설정의 chain/AI/display 호출 각 1회; 미실행 수정 시 이전 태그 보존, Candidate 실패·백엔드 교체 시 그림과 태그 제거; 현재 Candidate 실패 시 두 lane facts 제거; 예전 backend의 늦은 실패는 새 lane facts 객체를 그대로 보존; shutdown 뒤 늦은 성공은 lane facts를 복구하거나 교체하지 않음. 모두 위 6개 시나리오에서 실제 실행했다. 새 시험의 Applied/AppliedNoChange/PRE-Skipped 결과는 기존 Mock 위에서 명시적으로 주입한 stage 결과이며 실제 native 처리 성공이나 픽셀 정확도의 증거가 아니다.

최종 runner 증분 빌드 Exit 0, 기존 CS8604 경고 1개·오류 0개(`build.txt`, 1.48초). 앱 코드 수정 뒤 빌드는 경고 23개·오류 0개였고 초기 시험 전용 빌드는 경고 12개·오류 0개였다. WPF 임시 프로젝트 재컴파일 범위가 달라 전체 경고 증감 비교 근거로 쓰지 않는다. `git diff --check -- gui/ImageProcTest gui/ImageProcTest.SelfCheck` Exit 0, 출력 없음.

## 미확인·남은 검토

- 구현 담당의 위 시험은 Mock 기반이다. 부모의 실제 GUI 결과는 아래 별도 기록을 따른다. 기존 DLL 관측과 최신 엔진 검증은 분리한다.
- 보정/그리드 파라미터 제공, gsvg.dll 배치, 실제 픽셀 품질·차이의 정답 판정은 범위 밖이거나 미검증이다. A 가상 그리드 확장은 하지 않았다.
- 커버리지와 LSP 서비스 진단은 측정하지 않았다. 85% 달성이나 전체 suite 통과를 주장하지 않는다. 통합 브랜치 CI `gui-shell-runners`의 저장소 전체 판정은 PENDING이다.
- W-23/W-26 제품 E2E 자체는 실행하지 않았다(구현 에이전트의 제품 GUI 실행 금지 범위). 관련 최소 oracle는 기존 Mock equal-lanes 시나리오의 chain/AI/display 각 1회로 측정했다. 기존 설정에서 이름만 같고 실제 main mode가 다르면 정본 B 요청을 지키기 위해 추가 호출이 생길 수 있으므로 제품 E2E의 저장 설정과 호출횟수는 부모/CI 검토 대상이다. A의 저장 계약은 확장하지 않았다.
- Slim facts의 Reason/actual VOI는 후속 258에서 재사용할 수 있지만 258 기능·표시·시험은 여기서 구현하지 않았다.

부모가 실제 GUI 증거·독립 검토·정상 훅 커밋을 맡는다. 구현 에이전트는 이 인계 뒤 파일 쓰기를 중단한다.

## 부모 실제 GUI — 2026-11-04 07:37~07:39 KST

- 기준: dev/gui HEAD `1f6746096d602f5d56185a8bae6e11f3742bb9ec` + 이 카드 변경. 이번 Debug EXE를 PID7440으로 시작하고, 기존 DLL `D:/workspace-github/image-processing/build/e2e-native-dlls`, 기존 보정 `D:/workspace-github/xpe-data/calib-real-v2`, width/height3072를 지정했다. `--automation-raw`는 저장소 손목 RAW 전체 경로이며 report 인수 없이 interactive 창을 시작하는 기존 진입점이다. 사용자 기존 설정과 분리한 bin 아래 `gui-card-check`를 사용했다.
- DLL은 256에서 관측한 STALE provenance6b8477ef와 같은 폴더다. 최신 엔진·영상 품질 수용은 주장하지 않는다. 화면에는 mode=Native/common=0.1.0/display=1.0.0이 표시됐다.
- B selector에서 Virtual grid를 선택하니 기존 stale 배너와 이전 Grid suppression 태그가 유지됐다. Pipeline의 Run Preprocessing을 누른 후 실제 B 체인은 `preprocess=Applied, enhance_basic=Applied, gsvg=Applied, ai_bone_suppress=NotRequested`였고 태그는 `B Virtual grid`였다. A gsvg는 NotRequested로 별도 표시됐다. 현재 DLL은 Applied를 반환했으므로 이 실제 결과를 RequestedNotApplied라고 바꾸어 보고하지 않는다.
- Parameters Grid correction의 Virtual 선택을 화면에서 확인했다. `virtual-applied.jpg`에 상단 B 선택·실제 태그·B 체인·Virtual 라디오를 함께 보존했다.
- Parameters Suppress 라디오를 클릭하니 상단 B가 Grid suppression으로 바뀌었다. 아직 실행 전이므로 이전 `B Virtual grid` 태그와 `Candidate: settings changed - apply again`이 유지됐다. `suppress-roundtrip-stale.jpg`에 이 역방향 동기화를 보존했다.
- 미적용 태그는 위 Mock 주입 회귀에서 검증한 것이며, 이 실제 Native 실행에서 고장/누락 테이블을 새로 조성하지 않았다. RAW·보정·DLL·기존 사용자 설정을 수정하지 않았다.
- 검증용 GUI 종료 뒤 `gui-card-check`의 해석된 절대경로가 자체 bin 하위임을 확인하고 이 폴더만 삭제했다. 삭제 확인 출력: `gui-card-check exists=False`. 256의 같은 이름 자체 폴더도 앞 단계에서 삭제했다. 요청한 작은 로그·캡처만 보고서 폴더에 보존한다.
