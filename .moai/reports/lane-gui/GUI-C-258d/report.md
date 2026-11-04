[2026-11-04 11:53:14 KST] GUI-C-258d · Refs #282 · 시험 인용·바인딩 기대 정정, 전체 통합 1026 PASS / 0 FAIL / 3 SKIP (총 1029), staged 공백 검사 통과 · Reviewer 기술수용, 미푸시

## 수신·착수

- 카드: `D:/workspace-github/image-processing/.moai/state/codex-msgs/20261104-1146-gui.md` (리더 표기 11:45 KST).
- baseline: `D:/workspace-github/xpe-gui`, `dev/gui`, HEAD `339787ec95e43074857cf81234e3f566bec019a2` (부모의 origin/main fast-forward 후). 대상 통합 시험 소스의 기존 diff는 없었다.
- 구현 담당 범위는 `clients/ImageProcTest.IntegrationTests/Functional/BackendLifecycleTests.cs`, `SettingsProcessingConnectionTests.cs` 및 이 보고서 폴더다. 다른 세션의 변경을 보존했다. 제품 소스·설정·빌드 환경은 변경하지 않았다.
- 전체 RED 후 실제 누락 바인딩 이름과 두 시험의 최소 수정안을 부모에게 먼저 전달했고 승인받은 뒤 수정했다. 커밋·푸시는 하지 않았다.

## Claim

기존 제품 동작을 바꾸지 않고 오래된 소스 인용과 의도적으로 사라진 설정 바인딩 기대를 정정했다. 필터 없는 전체 `ImageProcTest.IntegrationTests`가 exit `0`으로 통과했다. 총 1029개 중 실행한 1026개는 PASS이며 원래 조건부 시험 3개는 SKIP이다. SKIP을 통과 수에 포함하지 않는다.

## Evidence

완전 RED와 GREEN에 사용한 명령(필터 없음):

```text
D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-254c\env.cmd C:\Progra~1\dotnet\dotnet.exe test clients/ImageProcTest.IntegrationTests -c Debug
```

| 실행 | 결과 원문 | 종료 코드 | 원문 파일 |
|---|---|---|---|
| RED | `실패!  - 실패:     2, 통과:  1024, 건너뜀:     3, 전체:  1029, 기간: 23 s - ImageProcTest.IntegrationTests.dll (net8.0)` | 1 | [red.txt](red.txt) |
| GREEN | `통과!  - 실패:     0, 통과:  1026, 건너뜀:     3, 전체:  1029, 기간: 23 s - ImageProcTest.IntegrationTests.dll (net8.0)` | 0 | [green.txt](green.txt) |

각 파일은 도구가 반환한 전체 stdout/stderr를 UTF-8로 저장했고, 줄 끝 공백과 EOF의 빈 줄만 제거했다(마지막 개행 하나 유지). 숫자·오류·경고·시험 이름은 수정하지 않았다. GREEN의 기존 경고(CS8602·CS8631·CA2014·xUnit2000/1031/2013/2029/2030/2031)는 남아 있으며 이를 해소했다고 주장하지 않는다.

### 원인·수정·유지한 oracle

| 파일/함수 | 원인과 실제 근거 | 변경 및 유지한 검증 |
|---|---|---|
| `BackendLifecycleTests.cs:222` `TheCandidateLane_RunsItsChainInTheBackground_ForTheBackendThatWasCurrent` | `:230`은 이전 `ImageSource? RenderLane` 반환 서명을 인용했다. GUI-C-257 `84c2551f9e02c85ffcf447621dfd9a31516852c8`에서 반환값이 이미지+facts tuple로 바뀌었다. `:232`도 이전 `candidateImage` commit 문장을 인용하고 있었다. | `:230`은 `(System.Windows.Media.ImageSource? Image, LaneRenderFacts Facts)` 서명으로, `:232`는 `LaneB = candidateResult.Image,` commit prefix로 정정했다. `await Task.Run`·backend 인수·동기 중복 호출 없음·`IsCurrent(ticket)` 직전 guard·300자 이내 proximity 단언은 그대로다. |
| `SettingsProcessingConnectionTests.cs:283` `Collection_FindsTheKnownBindings` | RED는 distinct 설정 기대 42 / 실제 41이었다. 이전 `1f6746096`과 현 HEAD에 동일한 XML binding 수집 규칙(직접 `Settings.P` 및 VM의 `get => Settings.P` wrapper)을 적용한 읽기 대조 결과 `before distinct=42 current distinct=41`, 제거된 유일한 이름은 **`GsvgMode`**였다. | count를 41로 정정하면서 `GsvgMode`가 수집되지 않음도 명시 단언했다. XAML의 `CandidateGsvgMode` 바인딩 3개, 옛 `Settings.GsvgMode` 바인딩 없음, facade getter의 `AlgorithmPreset.For(LaneBAlgorithm).GsvgMode` 연결 및 setter의 `LaneBAlgorithm = mode switch`를 직접 단언했다. 기존 수집/processing 분석 알고리즘과 미연결·disabled 검증은 변경하지 않았다. |

누락 바인딩은 카드의 잠정 추정인 258에서 사라진 것이 아니라 **257의 의도된 변경**이다. 다음 읽기 명령으로 실제 diff를 확인했다.

```text
git -C D:/workspace-github/xpe-gui diff 1f6746096 84c2551f -- gui/ImageProcTest/Views/AnalysisPanel.xaml
```

`gui/ImageProcTest/Views/AnalysisPanel.xaml:269/272/275`의 라디오 3개가 `Settings.GsvgMode`에서 `CandidateGsvgMode`로 바뀌었다. 현재 VM `CandidateGsvgMode`는 `LaneBAlgorithm`을 읽고 쓴다. 기존 수집기는 `get => Settings.P` 형태만 해석한다고 같은 시험 파일 `:15-24`, `CollectBindings :704-734`에 명시되어 있으므로, 계산형 facade를 임의로 새 persisted 설정으로 수집하지 않는다. B selector의 기존 `LaneBAlgorithm` 바인딩은 남아 있다. 총수만 낮춰 미확인 제거를 숨기지 않도록 실제 빠진 이름과 대체 연결을 함께 고정했다.

이번 backend 시험은 source text guard다. 실제 WPF/백엔드 동작 검증으로 확대하지 않는다. 기존 GUI-C-258c의 `green-full.txt`에는 SelfCheck 3/3b의 오래된 Candidate 성공/실패와 backend 교체·shutdown 행동 검증이 있으나, 이는 별도 이전 baseline의 관찰이며 이번에 재실행하지 않았다. 제품 코드를 수정하지 않은 이번 카드에 새 WPF 시험 기반을 추가하지 않았다.

Reviewer가 두 시험 diff·현 제품 코드·collector·보고서·RED/GREEN을 읽기 대조한 뒤 기술수용 및 신규 차단 없음으로 회신했다. 직접 문자열 guard의 한계와 기존 3 SKIP을 유지해서 판단했으며 독립 재실행은 하지 않았다.

### 시험 실행과 범위

- 처음에는 같은 전체 프로젝트에 `--logger console;verbosity=detailed`를 추가하여 RED를 실행했다. 출력이 도구 한도에서 잘려 완전 증거로 사용할 수 없었다. 완전 원문을 확보하려고 기본 logger의 전체 명령을 한 번 더 실행했다.
- 첫 프로세스가 아직 종료되지 않은 상태에서 재실행을 시작한 실수로 두 RED 실행이 잠시 겹쳤다. 이후 두 프로세스 모두 종료한 것을 확인했다. 둘 모두 같은 2 FAIL / 1024 PASS / 3 SKIP으로 종료했다. 표의 RED 분모에는 완전 원문을 확보한 두 번째 실행 한 번만 집계했다. 이 중첩을 정상적인 순차 검증으로 주장하지 않는다.
- GREEN은 RED 두 프로세스의 종료를 확인한 뒤 한 번만 순차 실행했고 exit 0을 관찰했다. 추가 재실행은 하지 않았다.
- 전체 통합 시험에는 기존 Native/child-process 시험이 포함되어 있다. 상세 첫 RED에서 기존 `build/ci-common/bin`의 `xpe_enhance_basic version: 1.0.0` 및 실제 모듈 호출/기존 preprocess synthetic oracle/GSDF 호출 출력이 관찰되었다. RunnerProcess 및 oracle child-process 시험도 기존 전체 범위로 실행됐다. 이번에 DLL을 새로 빌드하거나 제품 GUI를 별도 실행하지 않았다. 따라서 이 전체 실행을 순수 Mock 시험이라고 부르지 않는다.
- 세 SKIP 이름은 RED/GREEN에서 같았다: `OffsetQualityTests.TheRealModule_OnDark16AndTheCleanSetMap_MeasuresTheSignedBiasThePaperReports`, `BaselineFingerprintProbe.PrintTheStageHashesOfTheBaselineChainOnRealData`, `DllSearchPathSafetyTests.AFileLinkInsideAnApprovedFolder_ThatPointsOutside_IsRejected`. SKIP 조건을 바꾸지 않았다.

## Baseline-attribution

- RED: HEAD `339787ec95e43074857cf81234e3f566bec019a2`, 대상 두 파일 변경 전.
- GREEN: 같은 HEAD에 이 카드의 두 시험 파일만 수정한 working tree. 이후 제품 source 변경 없음.
- source diff: 두 파일 11 insertions / 3 deletions. `git diff --check -- clients/ImageProcTest.IntegrationTests/Functional/BackendLifecycleTests.cs clients/ImageProcTest.IntegrationTests/Functional/SettingsProcessingConnectionTests.cs` 출력 없음, exit `0`.
- 커밋 전 `git diff --cached --check`: 대상 시험 2개와 보고서·RED·GREEN 3개를 명시적으로 stage한 뒤 실행, **exit 0 / 출력 없음**. 증거 폴더의 `git grep --cached -n -I -E [[:blank:]]$ -- .moai/reports/lane-gui/GUI-C-258d`도 출력 없음(exit 1: 매치 없음). 외부 변경은 stage하지 않았다.

## Gaps

- 이 보고서 작성 시점에는 커밋 전이다. 부모 레인이 정상 별도 커밋의 SHA를 리더/Reviewer에게 반환하며 푸시하지 않는다. 전체 repository CI는 리더의 통합 branch 실행 소관이며 PENDING이다.
- Native 시험이 사용한 기존 DLL의 최신 provenance·임상 화질·배포 EXE 버전은 이 카드에서 검증하지 않았다.
- 소스 텍스트 guard가 실제 runtime 전체 행동을 보장하지는 않는다. 기존 별도 행동 증거와 구분했다.
- LSP·coverage는 측정하지 않았다. 기존 3 SKIP의 대상 행동은 이번 전체 실행에서도 미관찰이다.

## Residual-risk

시험의 source 인용은 앞으로 메서드 표현이 바뀔 때 다시 갱신이 필요할 수 있다. 현재는 background 실행, captured backend 인수, stale ticket guard 및 중복 동기 호출 금지 의도를 유지한 최소 정정이다. Candidate facade는 기존 수집기의 해석 범위 밖이므로 별도 직접 연결 단언으로 보강했으며, 새 계산형 wrapper 전반을 자동 분석하도록 범위를 넓히지 않았다. 다른 세션의 작업과 Tuner 보류를 보존한다.
