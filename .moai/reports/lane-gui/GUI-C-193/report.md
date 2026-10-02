# GUI-C-193 — 시험용 고장 스위치를 출하 빌드에서 코드째 제거 (사용자 결정 2026-10-02, #225)

레인: gui · 푸시 없음(푸시는 리더). 카드: `.moai/lanes/gui/inbox/GUI-C-193.md`. 5절 형식. 498775a5(GUI-C-192) 위에 쌓았다(192b 가 이 카드보다 먼저라는 지시를 받은 시점에 이 작업은 끝나 있어 먼저 커밋한다).

## 1. 주장

1. **두 스위치(`--automation-fault ai-worker-disabled`, `display-pipeline-after:N`)는 시험 빌드(Debug)에서만 컴파일된다.** 출하(Release) 바이너리에는 스위치 문자열, 파서 분기, `FaultInjectingBackend` 타입, 창 제목 표지, 배선 코드가 없다(§2-2).
2. **출하 빌드에서 플래그를 주면 앱은 기존 규칙대로 거부한다**: 알 수 없는 `--automation-*` 인자는 원래 "시작하지 않고 종료 코드 2 + (보고서 경로가 있으면) 거부 보고서"였다. 새 문구를 만들지 않고 그 경로를 그대로 탄다 — 메시지가 아무 이름이나 지어낸 스위치(`--automation-faultxyz`)와 같은 형식이다(§2-3).
3. **CI·시험은 워크플로 변경이 필요 없다**: GUI 를 빌드하는 모든 줄이 `-c Debug` 이고 E2E 픽스처가 `bin/Debug` 를 띄운다. 상수는 Debug 에서 기본으로 켜진다. 그래서 `ci.yml` 의 diff 는 없다. 대신 **선택 사항으로** "Release 빌드 바이너리에 시험 장치가 없음" 검사 단계 초안을 §2-6 에 적었다(리더가 넣을지 결정).
4. 구조가 침식되지 않게 지키는 시험: 시험 장치를 가드 밖에서 쓰면 빨개진다(§2-5).
5. 다른 `--automation-*` 인자는 바꾸지 않았다. 출하 빌드에서도 CI 의 자동화 단계가 Debug 와 똑같이 통과한다. 위험할 수 있는 것의 목록은 §2-7.

## 2. 증거

### 2-1. 구현

| 조각 | 위치 |
|---|---|
| 빌드 규칙 | `gui/XpeTestFaults.props`(새): `XpeTestFaults` 가 Debug 일 때 기본 true → `XPE_TEST_FAULTS` 정의. `-p:XpeTestFaults=true` 로 다른 구성에서도 켤 수 있다. 앱·SelfCheck·Functional 시험 프로젝트가 import |
| 파서 | `AutomationArgs.cs`: 두 상수, 레코드 멤버 2개, 파싱 분기 2개가 모두 `#if XPE_TEST_FAULTS`. 출하에서는 `--automation-fault` 가 마지막 `else`(알 수 없는 스위치)로 간다 |
| 래퍼 | `FaultInjectingBackend.cs` 전체가 `#if`, 그리고 앱 csproj 가 시험 빌드가 아니면 이 파일을 **컴파일에서 뺀다**(`Compile Remove`) |
| 배선 | `App.xaml.cs`, `MainWindow.xaml.cs`(출하: `XpeBackendFactory.Create` 를 그대로 넘김), `MainWindowViewModel.cs`(`AnnounceFaultInjection`, `_faultInjectionAnnounced`, 제목 표지): 모두 `#if` |
| 자동화 상태 | `FaultInjectionStatus` 는 출하에서 상수 `faultInjection=off` — "평상 실행은 고장 없음"이라는 자동화 계약(E2E W-24)이 두 구성에서 같다 |
| SelfCheck | 시나리오 6 은 `#if`(출하 구성은 12개 시나리오). 통과 줄은 실행한 개수를 센다 |
| 시험 | `AutomationArgsTests`: 고장 시험은 `#if`, 출하 구성에서는 대응 시험(`…IsNotRecognised_InAShippedBuild`)이 컴파일된다. `AutomationRunSelectionConsumptionTests` 제외 목록의 두 항목도 `#if` |

### 2-2. 출하 바이너리에 없다는 증거 (대조군 포함)

`il_scan.txt`(메타데이터 읽기: 형식·메서드·필드·속성 이름과 문자열 리터럴을 열어 봄, 어셈블리를 로드·실행하지 않음) 와 `byte_search.txt`(UTF-8·UTF-16LE 바이트 검색, `.dll`·`.exe`·`.pdb` 전부):

| 검색 | Debug(대조군) | Release |
|---|---|---|
| 메타데이터 스캔(스위치·타입·멤버·리터럴 10개 바늘) | **26개 항목 발견**(타입 `FaultInjectingBackend`, 리터럴 `ai-worker-disabled`·`display-pipeline-after:`·`FAULT INJECTION ARMED`, 메서드 `AnnounceFaultInjection` …) | **발견 없음** |
| 바이트 검색 8개 문자열 | 모두 발견 | 모두 없음(`ImageProcTest.pdb` 포함) |

(첫 Release 빌드의 `.pdb` 에는 비어 있는 `FaultInjectingBackend.cs` 의 **파일 이름**이 남아 있었다 — 코드가 아니라 기호 문서 목록이다. 파일을 컴파일에서 빼서 흔적까지 지웠다. 이 점은 바이트 검색 대조로 잡혔다.)

### 2-3. 출하 빌드에 플래그를 주고 실행 (`launch_observations.txt`)

| 구성 | 인자 | 결과 |
|---|---|---|
| Release | `--automation-fault ai-worker-disabled` | 시작하지 않고 **종료 코드 2**, 창 없음, 보고서 오류 `--automation-fault is not a recognised automation switch.` |
| Release | `--automation-fault display-pipeline-after:2` | 같음 |
| Release | `--automation-faultxyz …`(지어낸 스위치) | 같은 형식(`--automation-faultxyz is not a recognised automation switch.`) |
| Release | (플래그 없음) | 정상 실행, 제목 `[MOCK] ImageProcTest GUI-S0` |
| Debug | `--automation-fault ai-worker-disabled` / `display-pipeline-after:2` | 실행, 제목 `… — FAULT INJECTION ARMED` (대조군) |
| Debug | (플래그 없음) | 정상 실행, 표지 없음 |

**카드 문구와의 차이 하나**: 카드는 "출하 빌드에 플래그를 줘서 실행하면 창 제목에 `FAULT INJECTION ARMED` 가 없고 동작이 정상"이라고 썼지만 같은 카드가 "알 수 없는 인자와 같은 처리를 따를 것"이라고 했다. 앱이 알 수 없는 `--automation-*` 인자를 다루는 방식이 **시작 거부**라서 후자를 따랐고, 그 결과 출하 빌드는 이 플래그로는 창을 띄우지 않는다(제목에 표지가 없는 것은 창이 없어서다). "플래그를 무시하고 정상 시작"으로 바꾸려면 알 수 없는 인자 규칙(#136, 오타가 조용히 무시되지 않게 한 것)과 갈리므로 리더의 결정이 필요하다.

### 2-4. Release 구성에서 시험 실행 (`release_functional.txt`, `release_selfcheck.txt`)

- 시험 프로젝트를 Release 로 빌드해 `Category=Functional` 실행: **359 통과 · 건너뜀 1(기존) · 실패 0**. 고장 시험은 컴파일되지 않고, 대응하는 출하 시험 3건(`TheFaultSwitch_IsNotRecognised_InAShippedBuild` ×2, `…IsRefusedLikeAnyMadeUpSwitch…`)이 통과한다. Debug 구성에는 이 시험이 없다(대조군).
- SelfCheck 를 Release 로 빌드해 실행: 12개 시나리오 통과(Debug 13개 — 시나리오 6 은 시험 장치용).
- 앱의 CI "Run automation" 단계(`ci.yml:668-684` 와 같은 인자·검사)를 Release 에 실행: `exit=0 BackendMode=Mock source=arg Passed=True`, Debug 도 같음(`release_automation_run.txt`).

### 2-5. 구조를 지키는 시험과 반증

`FaultSeamCompiledOutTests` 5건: 앱의 모든 `.cs` 에서 시험 장치의 이름(12개)이 `#if XPE_TEST_FAULTS` 밖의 코드 줄에 나오면 위반(주석은 제외, `#else` 쪽·다른 기호의 `#if` 는 가드로 치지 않음), 가드 검사기 자체의 시험, 파일 전체 가드+`Compile Remove`, 상수의 정의가 Debug 뿐이고 세 프로젝트가 import 함, 고장 시험이 가드되고 출하 대응 시험이 존재함.

반증 4팔(실제 소스, 복구 바이트 동일; `falsification_arms.txt`):

| 팔 | 결과 |
|---|---|
| props 가 Debug 조건을 빼고 모든 구성에서 상수를 정의 | 시험 빨강 + **Release 빌드에 `ai-worker-disabled`, `display-pipeline-after`, `FAULT INJECTION ARMED`, `FaultInjectingBackend` 가 들어감**(바이트 검색으로 확인) |
| 창 배선의 가드를 뺌 | 시험 빨강 + **Release 빌드가 컴파일 실패**(`FaultInjectingBackend` 이름 없음) |
| `Compile Remove` 제거 | 시험 빨강 |
| 출하 대응 시험의 이름을 바꿈 | 시험 빨강 |

### 2-6. CI 에 넣을 만한 것 (리더가 결정 — `ci.yml` 는 건드리지 않았다)

필수 변경은 없다(§1-3). 구조 시험은 이미 Debug CI 에서 돈다. 더 강한 보장은 **Release 로 빌드한 바이너리를 검사**하는 것이다. 초안(`ci_step_check.ps1` 를 `.github/scripts/` 로 옮겼다고 가정, 로컬에서 실행해 확인: Release `OK` exit 0, Debug 대조군 exit 1 — `ci_step_check_run.txt`):

```yaml
      - name: Shipped (Release) GUI build carries no test fault seam
        shell: pwsh
        run: |
          dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Release
          ./.github/scripts/check-no-fault-seam.ps1 gui/ImageProcTest/bin/Release/net8.0-windows/ImageProcTest.dll
```

위치는 `gui-shell-runners`(`ci.yml` 626 줄 부근, GUI Debug 빌드 다음)가 자연스럽다. 이 단계의 비용은 GUI 를 한 번 더 빌드하는 시간이다.

### 2-7. 다른 `--automation-*` 인자 (출하 빌드에도 그대로, 바꾸지 않음 — 위험한 것만 목록)

| 인자 | 하는 일 | 위험 메모 |
|---|---|---|
| `--automation-raw`, `-width`, `-height`, `-calib` | 읽을 영상·크기·보정 디렉터리 | 읽기 전용 입력. 낮음 |
| `--automation-report <경로>` | 보고서(JSON)를 그 경로에 씀(`--automation-raw` 와 함께면 자동 실행 후 종료) | 임의 경로에 파일 쓰기. 낮음~중간 |
| `--automation-backend Mock\|Native` | 백엔드 강제 | Mock 이면 지속 경고 배너와 `[MOCK]` 제목이 뜬다(HAZ-GUI-005 통제 유지). 낮음 |
| `--automation-settings <경로>` | 설정 파일의 읽기·쓰기 위치를 바꿈 | 설정을 다른 위치로 돌림. 낮음~중간 |
| `--automation-export-render <경로>` | **렌더할 때마다 처리된 영상 픽셀을 그 경로에 파일로 씀** | 영상 데이터가 임의 경로로 나간다 — 환자 영상이면 반출 경로가 된다. 중간 |
| `--automation-selfcheck-exe <경로>` | **"Run Self-Check" 메뉴가 그 실행 파일을 실행**한다(`RunConsoleRunnerAsync` 의 `overridePath` → 프로세스 시작) | 명령줄이 지정한 임의 실행 파일을 앱 메뉴 한 번으로 실행. **가장 위험**(명령줄을 조작할 수 있는 사람은 이미 실행 권한이 있으므로 권한 상승은 아니지만, 바로가기 인자 하나로 앱 이름을 단 프로세스가 도는 경로다) |

이번 결정 범위 밖이라 코드는 바꾸지 않았다. 마지막 두 개(`-export-render`, `-selfcheck-exe`)가 같은 방식(시험 빌드 전용)으로 뺄 후보다 — 리더·사용자 결정 사항.

### 2-8. 로컬 실행 (최종 트리)

빌드(gui·clients, Debug): 오류 0, xUnit1031 0. Debug `Category=Functional`: **377 통과 · 건너뜀 1 · 실패 0**. SelfCheck(Debug): 13 시나리오, 12회 연속 0 실패(1.9–2.3 초). Mock E2E(Debug 앱): 종료 0, **53 통과 · 건너뜀 7(Native 전용) · 실패 0** — 시험 장치를 쓰는 W-23(표시 실패)·W-24(평상 실행은 고장 없음)·M01(최소 너비의 배너) 포함.

## 3. 다른 레인·문서가 말하는 내용이 달라지는 것 (목록)

1. **GUI-C-191b·192 보고서의 문장**: "출하 앱에 남는 시험 장치(명령줄 전용, 켜면 제목 표시)"는 이 카드로 **대체**된다(출하에는 없다). 보고서는 기록이라 고치지 않았다.
2. **E2E 는 Debug 앱 전용**: 픽스처가 `bin/Debug` 를 띄우는 것은 원래도 그랬지만, 이제 고장 스위치를 쓰는 시험(W-23, M01)은 Release 앱을 띄우면 앱이 거부하고 종료해서 실패한다. Release 로 E2E 를 돌리는 계획이 있다면 그 시험들은 시험 빌드를 써야 한다(현재 그런 계획은 저장소에 없다).
3. **시험 개수가 구성마다 다르다**(Debug 377, Release 359).
4. 리더가 쓸 문서 두 곳의 문장 초안(§4).
5. 다른 레인(pre/post)이 말하는 내용: 이 카드는 `gui/`·`clients/` 안에서만 바뀌었다. `docs/`·`.github/`·`modules/` 는 건드리지 않았다.

## 4. 문서 문장 초안 (리더가 씀)

**`docs/post-processing/xpe/SHA-GUI-001_Software_Hazard_Analysis.md` — HAZ-GUI-005 의 "위험 통제" 칸에 추가할 항목**

> (6) 시험용 고장 주입 스위치(`--automation-fault`, 값 `ai-worker-disabled`·`display-pipeline-after:N`)는 시험 빌드(Debug 구성, 컴파일 상수 `XPE_TEST_FAULTS`)에만 컴파일된다. 출하(Release) 빌드의 바이너리에는 해당 코드·문자열이 없고, 출하 빌드는 이 인자를 "인식할 수 없는 자동화 스위치"로 거부하며 시작하지 않는다(종료 코드 2). 시험 빌드에서 켜지면 창 제목에 "FAULT INJECTION ARMED" 가 표시되고 로그에 기록된다. 증거: GUI-C-193(바이너리 메타데이터 스캔·바이트 검색·실행 관측), 구조 시험 `FaultSeamCompiledOutTests`.

**`gui/ImageProcTest/README.md` — 자동화 절에 추가할 문단**

> **시험 전용 스위치.** `--automation-fault` 는 시험 빌드(Debug)에서만 존재한다: `display-pipeline-after:N`(표시 파이프라인이 N번 뒤부터 일부러 실패), `ai-worker-disabled`(AI 워커 상태가 "꺼짐 3/3" 으로 읽힘). 출하(Release) 빌드는 이 스위치를 코드째 포함하지 않으며, 주어도 "인식할 수 없는 자동화 스위치"로 거부하고 시작하지 않는다. 시험 빌드에서 켜면 창 제목에 `FAULT INJECTION ARMED` 가 붙는다. 다른 `--automation-*` 스위치는 두 구성 모두에서 동작한다.

## 5. 기준 귀속 · 미검증 · 잔여 위험

기준 귀속: 이 트리(`dev/gui`, 498775a5 위 작업 트리)에서 이 실행으로 얻었다. Release 산출물은 `gui/ImageProcTest/bin/Release` 와 SelfCheck 의 Release 빌드다.

미검증:
1. **게시(publish)·설치 패키지**를 만들어 검사하지는 않았다: 저장소에는 GUI 를 출하하는 파이프라인이 없다(`release-bundle.yml` 에 GUI 가 없다). `dotnet build -c Release` 의 출력만 검사했다. `dotnet publish` 가 같은 어셈블리를 쓴다는 것은 읽기이며 시험하지 않았다.
2. 조건부 컴파일이라 **Debug 가 아닌 구성 이름**(예: 사용자 지정 구성)은 출하 규칙을 탄다(상수 없음). 이는 의도지만 시험 빌드를 그런 구성으로 만들려면 `-p:XpeTestFaults=true` 가 필요하다.
3. 실행 관측은 Mock 백엔드, 이 기계 한 대다. Native 를 붙인 Release 앱은 보지 않았다.
4. 구조 시험은 소스 텍스트를 읽는다(컴파일 결과가 아님). 컴파일 결과는 §2-2 의 일회성 검사와 반증 팔의 Release 빌드가 본다. 지속적인 보장은 §2-6 의 CI 단계 제안이다.

잔여 위험: 위 §2-7 의 `--automation-selfcheck-exe`·`--automation-export-render` 가 출하 빌드에 남아 있다(이번 결정의 대상이 아님). 문서 파일 `ImageProcTest.xml` 은 검사했고, 처음에는 `--automation-fault` 가 남아 있었다(뷰모델 `FaultInjectionStatus` 의 `///` 주석, 가드 밖). 주석을 스위치 이름 없이 고쳤고, 가드 시험이 `///` 줄도 검사하게 했다(`AutomationArgs.cs` 의 `cref` 한 줄도 같이 발견·수정). 재검사: `byte_search.txt` 에서 Release dll·pdb·xml 모두 none, Debug 는 전부 발견.

## 증거 파일

`il_scan.txt` · `byte_search.txt` · `launch_observations.txt` · `release_automation_run.txt` · `release_functional.txt` · `release_selfcheck.txt` · `ci_step_check.ps1` · `ci_step_check_run.txt` · `falsification_arms.txt` · `selfcheck_runs.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
