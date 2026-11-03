2026-11-04 07:21 KST — GUI-C-256 (Refs #280), 코드 인계 보고. 실제 GUI 화면 검증 대기.

## 확인한 동작

- 성공한 RAW 열기는 파일명을 이름으로, `manual`을 출처로 하는 큐 항목을 만든다. 같은 정규화 경로를 다시 열면 기존 객체·ID·출처·판정·메모를 유지한다.
- 판정 파일 쓰기가 성공하면 항목 상태와 Run-set 집계가 즉시 바뀐다. Defer→Pass→Fail 전환에서 이전 집계가 남지 않는다. 메모는 항목과 기존 판정 파일에 기록한다.
- 마지막 Save & Next는 항목·판정·메모를 유지하고, 판정 버튼 뒤 편집한 메모도 저장한다. 로드한 항목이 없거나 판정을 고르지 않았으면 기록하지 않은 이유를 표시한다.
- 기존 ExportEvidenceBundleCommand를 실제 호출해 ZIP을 열고 `verdicts.json`의 수동 항목 ID, `Fail`, 수정한 메모를 역직렬화해 확인했다. Export 구현은 변경하지 않았다.
- 실제 WPF TextBlock을 TopBar와 같은 `RunSet.Evaluated`에 바인딩해, 값과 화면 바인딩이 함께 갱신되는 것을 확인했다.

## 변경 파일·역할

| 파일 | 변경 |
|---|---|
| `gui/ImageProcTest/ViewModels/MainWindowViewModel.cs` | LoadImageFromPathAsync 성공 뒤 큐 연결; RecordVerdict의 로드 항목 확인·기록 성공 뒤 상태/집계; SaveAndNext 마지막 항목 유지; LoadedStudy/RefreshRunSetCounts 보조 함수 |
| `gui/ImageProcTest/Models/StudyEntry.cs` | 선택적 Source, 항목별 VerdictNotes. 기존 필수 필드·manifest 형식 유지 |
| `gui/ImageProcTest/Models/RunSetState.cs` | Passed/Failed/Deferred 변경 시 종속 속성 Evaluated 알림 |
| `gui/ImageProcTest/Views/StudyQueue.xaml` | 파일명(Name)과 출처 표시, 경로 툴팁 |
| `gui/ImageProcTest.SelfCheck/LifetimeScenarios.cs` | ManualStudyVerdictsStayWithTheLoadedImage 동작 회귀 1개; 기본 Lifetime 실행에도 포함. 전용 선택 실행은 고정 bin 하위 임시 폴더를 사용하고 삭제 |
| `gui/ImageProcTest.SelfCheck/Program.cs` | 기존 freshness guard 뒤 `--study-contract` 선택 실행 |

## 재현·검증 근거

측정 트리: `D:/workspace-github/xpe-gui`, `dev/gui`, HEAD `945851b8cb4fcbb7b0cda3828ab6a81b791682bb` + 이번 미커밋 변경. 기존 설정·프레임워크·과거 보고서 변경은 보존했다. git stage/commit/push는 하지 않았다.

실행 명령(각 명령 앞의 기존 env.cmd는 Windows 환경 초기화):

```text
.moai\reports\lane-gui\GUI-C-254c\env.cmd C:\Progra~1\dotnet\dotnet.exe build gui\ImageProcTest.SelfCheck\ImageProcTest.SelfCheck.csproj -c Debug --nologo -v:q
.moai\reports\lane-gui\GUI-C-254c\env.cmd C:\Progra~1\dotnet\dotnet.exe gui\ImageProcTest.SelfCheck\bin\Debug\net8.0-windows\ImageProcTest.SelfCheck.dll --study-contract
```

분모는 **통합 동작 시나리오 1개**다. RED 메시지 수를 독립 시험 개수로 세지 않았다. 실제 VM·WPF Dispatcher·기존 Mock 기반 CandidateBackend를 사용한다. 새 DLL/엔진·제품 GUI는 실행하지 않았다. 픽셀 정확도 시험이 아니다.

| 단계 | 실제 결과·oracle |
|---|---|
| 첫 RED | 구현 전 실행. 실패 단언 12개: 빈 영상 기록/파일 생성/Save & Next, 수동 큐·이름·활성 ID, Pass/Fail 집계, 마지막 항목, ZIP 판정, 기존 ID/정규화 경로. 원문 `red.txt` |
| 출처 단언 추가 RED | 구현 전 두 번째 실행. 같은 12개 + manual 출처 = 실패 단언 13개. Exit `-532462766`. 실제 큐=0, Total=0, Pass=0, Fail=0 |
| 핵심 GREEN | 큐/판정 연결 후 시나리오 1/1 통과. 이후 상단 평가 건수 바인딩 단언을 추가 |
| WPF RED | Passed=1인데 Evaluated TextBlock은 `0`. 실패 단언 1개, Exit `-532462766`. `red.txt`에 별도 원문 |
| 최종 GREEN | 평가 건수 알림 수정 후 현재 시나리오 1/1 통과, Exit 0. 실제 ZIP ID/판정/메모, Defer 전환, 미선택 판정 거절, 실패한 열기 보존, 수동 재열기와 기존 큐 출처 보존도 단언 |

최종 실행 원문(`green.txt`):

```text
  observed final queue=1; assertions executed for verdict replacement and ZIP ID/status/notes
  scenario 'GUI-C-256 manual study contract': 288 ms
Lifetime scenarios passed (1 scenarios).
```

최종 runner 빌드 Exit 0, 경고 1개·오류 0개. `build.txt` 원문에 기록했다. 경고는 변경 전 시험 전용 빌드에서도 관측한 `LifetimeScenarios.cs:269`의 CS8604다. 중간 앱 재빌드는 경고 23개·오류 0개였으며, 전체 원문이 출력 제한으로 잘려 경고 집합 전체 비교 근거로 삼지 않았다. 중간 `Settings.BodyPart` 오기 빌드 오류 1개는 실제 기존 속성 SelectedBodyPart로 바로 정정했고 최종 빌드는 성공했다.

`git diff --check -- gui/ImageProcTest gui/ImageProcTest.SelfCheck`: Exit 0. RunSetState.cs의 기존 CRLF→LF 안내가 나왔으며 공백 오류 출력은 없었다.

## 미확인·잔여 범위

- 부모 실제 GUI 확인은 아래 별도 기록을 따른다. 구현 담당의 위 시험은 Mock 기반이며 실제 GUI 결과와 분리한다.
- 기존 큐 항목 보존은 실제 StudyEntry를 미리 넣어 측정했다. manifest 파일 파서/실제 manifest 주입 경로의 종단 검증은 하지 않았다. 이름이 비슷한 다른 GUI에는 이 결과를 적용하지 않는다.
- 다음 항목이 있을 때의 기존 선택 전환은 이 카드에서 확장하지 않았다. 마지막 항목 유지와 로드한 영상에 맞는 판정만 이번 시험 범위다.
- 테스트 커버리지 도구 측정/LSP 서비스 진단·저장소 전체 suite는 수행하지 않았다. 85% 목표 달성을 주장하지 않는다. 저장소 전체 판정 소유자는 통합 브랜치 CI `gui-shell-runners`이며 보고 시점 PENDING이다.
- Tuner, 257/258, 비교 모드/뷰포트, manifest schema, 설치·배포·게시를 변경하지 않았다. Source 기본값은 빈 문자열이며 기존 항목의 출처를 임의로 manifest라고 만들지 않았다.

부모 인계 후 구현 에이전트는 파일 쓰기를 중단한다. 실제 GUI 확인·독립 검토·정상 훅 커밋은 부모가 수행한다.

## 부모 실제 GUI 확인 — 2026-11-04 07:23~07:26 KST

- 같은 트리의 이번 Debug EXE `gui/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe`를 기존 `tools/Start-ImageProcTestNative.ps1`로 시작했다(PID6512). 실행 인수: Native, calibration `D:/workspace-github/xpe-data/calib-real-v2`, width/height3072. 프로세스 전용 `XPE_GUI_DATA_DIR`는 해당 bin 아래 `gui-card-check`로 분리했다. 기존 사용자 앱/설정은 사용하지 않았다.
- 사용 DLL 경로는 `D:/workspace-github/image-processing/build/e2e-native-dlls`. 런처 출력 원문: `DLL check: STALE - provenance 6b8477ef (run 38509221829): 63 file(s) under modules/ changed since, e.g. modules/preprocess/CMakeLists.txt`. 따라서 최신 엔진 검증으로 주장하지 않는다. GUI 상태는 `mode=Native | common=0.1.0 | display=1.0.0 | src=e2e-native-dlls`였다.
- Load Raw Image 대화상자에서 저장소 원본 `gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw`를 수동 선택했다. 최초 대화상자 위치는 이전 OS 최근 위치였으나, 입력한 전체 경로와 로드 후 항목 툴팁에서 위 저장소 경로를 확인했다. UIA 모달 element208 접근은 두 번 거절되어 더 재시도하지 않고, 화면에서 입력란을 확인한 뒤 키보드 입력으로 열었다.
- 실제 관측: 큐1건, 이름 `wrist_lat_3072x3072.raw`, 출처 `manual`, ID `48d8897355484008a623fa6a45e22c46`. Pass 클릭 후 상단 `1 / 1`, `1 pass`, `0 fail`; Run-set 탭 Passed1/Failed0/Deferred0/Total1.
- 요청 캡처: `pass-top.jpg`, `pass-runset.jpg`(이 보고서 폴더). 원본 스크린샷이며 재생성/편집하지 않았다.
- 같은 항목 Fail 클릭 후 새 화면에서 상단 `0 pass / 1 fail`, 탭 Passed0/Failed1/Total1 확인. 마지막 Save & Next 후 `No more queued studies; current study retained.`와 같은 ID 유지 확인.
- 위 관측은 GUI 기록·집계 계약이다. 임상 합격/영상 품질 또는 최신 DLL 정확성의 판정이 아니다. 원본 RAW·보정 지도·DLL을 복사하거나 수정하지 않았다.
