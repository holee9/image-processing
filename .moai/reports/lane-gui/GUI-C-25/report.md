# GUI-C-25 — 자동화 설정 격리 + 래퍼 누락 가드 + CI 실행 가능성 (#136, #134)

- 카드: GUI-C-25 · Refs #136 #134 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 2건: `ba3fa47`(설정 격리), `a288fbd`(래퍼 누락 가드) — 미푸시
- 선행 예외: 카드는 `904efbc` push 뒤 착수였으나 **leader 가 "origin/main 불필요, dev/gui 에 C-24(bbdff9e) 가 이미 있으니 착수" 로 지시**(커버리지 dispatch 동안 push 동결). 그에 따라 병합 없이 진행했다. `origin/main` 은 착수 시점 `dd9cc90` 이고 로컬은 그것을 이미 포함한다.
- **결과: gui 재빌드 0/0 · clients 재빌드 0/0 · dotnet 실패 0 / 통과 152 / 건너뜀 1 / 전체 153** (151 → +2)

---

## 1. 관측 — 판정이 실행이 아니라 파일을 측정하고 있었다 (카드 1항 전반)

### 1.1 첫 후보는 틀렸다

`ComparisonZoomScale` 로 잡고 줌 클릭을 제거해 보니 값이 **0** 이었다(`observe-before-isolation.json`). 자동화가 중간에 `ZoomFit` 으로 초기화해서 영속값에 의존하지 않는다. **가설을 관측이 부정했고, 관측 대상을 바꿨다.**

### 1.2 실제로 의존하는 항목

자동화가 쓰는 설정은 5개뿐이다:

```
grep -n "viewModel.Settings\.[A-Za-z]* =" gui/ImageProcTest/MainWindow.xaml.cs
  RawWidth / RawHeight / OffsetCorrectionMode / DefectCorrectionMode / VOI 센티널
```

판정이 읽는 나머지(`SelectedBodyPart`, `ComparisonMode`, `ComparisonSwipePosition`, `ShowDisplayPanel`)는 **전부 영속 파일에서 온다.** 그중 `SelectedBodyPart` 로 보였다 — 앱을 전혀 건드리지 않고, 배포 `appsettings.json` 의 값만 바꿔 같은 시나리오를 두 번 돌렸다:

| 영속 `selectedBodyPart` | 결과 |
|---|---|
| `Lung` | `Passed=false`, `VoiPresetApplied=false` |
| `Abdomen` | `Passed=true`, `VoiPresetApplied=true` |

(`observe-before-Lung.json`, `observe-before-Abdomen.json`)

**같은 바이너리·같은 시나리오인데 파일에 남은 값 하나가 판정을 뒤집는다.** 시나리오는 그 값을 한 번도 쓰지 않으므로, 그 판정은 실행이 아니라 파일을 측정하고 있었다.

## 2. 커밋 1 — 설정 격리 (`ba3fa47`)

`MainWindow.CreateSettings()`:

- 자동화 모드는 **기본값에서 출발**하고, 설정 저장은 `%TEMP%/xpe_gui_automation_<guid>/appsettings.json` 로 보낸다. 배포 파일은 읽지도 쓰지도 않는다.
- **`BackendMode` 만 예외로 이어받는다** — 어느 백엔드를 실행할지 고르는 **입력값**이지 실행이 만든 상태가 아니다.
- `LastRawDirPersisted` 검사가 이번 실행이 실제로 쓴 파일을 읽도록 교정(`AppContext.BaseDirectory` 하드코딩 → `_settingsFilePath`).

### 전/후 (영속 `selectedBodyPart=Lung` 을 남긴 채)

| 모드 | 격리 전 | 격리 후 |
|---|---|---|
| Mock | — | `Passed=true`, preset C/W=32768/65535 |
| Native | `Passed=false` | **`Passed=true`**, preset C/W=40/400 |

(`after-isolation-Mock.json`, `after-isolation-Native.json`) 실행 후 배포 `appsettings.json` 은 `selectedBodyPart: "Lung"` 그대로 — 자동화가 더 이상 덮어쓰지 않는다.

### C-23 의 VOI 센티널은 **유지한다** (카드가 요구한 사유)

격리로 대체되지 않는다. `AppSettings` 기본값이 `_voiWindowCenter=32768 / _voiWindowWidth=65535`(`AppSettings.cs:26-27`)인데 이는 `MockXpeBackend` 의 Abdomen 프리셋과 **같은 값**이다(`MockXpeBackend.cs:301`). 센티널이 없으면 Mock 에서 프리셋 명령이 돌지 않아도 판정이 참이 된다 — C-23 이 잡은 바로 그 구멍이 기본값 경유로 되살아난다.

## 3. 커밋 2 — 래퍼 누락 가드 (`a288fbd`)

`NativeCallWrapperGuardTests` — 소스 텍스트 대조(`ErrorCodeHeaderParityTests` 와 같은 방식). `RealXpeBackend.cs` 의 `XpeCommonNative.` / `XpeDisplayNative.` / `NativeLibrary.` 호출은 `InvokeNative` 를 타는 멤버 안이거나 면제 목록에 있어야 한다.

면제 3건은 **사유와 함께 데이터로** 뒀다(`ExemptMembers()`) — 추가가 정규식 손질이 아니라 의도적 편집이 되도록:

| 멤버 | 사유 |
|---|---|
| `DrainNativeAlerts` | 배출 자신 — 감싸면 재귀 |
| `ReadNativeAlert` | 배출을 대신해 큐 항목 1개를 읽음 |
| `HasExports` | 인스턴스 생성 전에 도는 정적 export 프로브 |

### 가드 자신의 결함을 먼저 잡았다

첫 실행에서 `ApplyDisplayPipelineCore` 의 4줄이 누락으로 잡혔다. 실제 누락이 아니라 **가드가 위임 호출을 따라가지 못한 것**이다 — 그 본체는 `InvokeNative(() => ApplyDisplayPipelineCore(...))` 로만 도달한다. 래퍼 호출 줄에 등장하는 식별자도 래핑된 것으로 세도록 고쳤다. 로그: `step2-guard.log`.

**가드가 죽는 경우도 막는다** — `InvokeWithDrain` 이 사라지거나 어떤 멤버도 래퍼를 타지 않으면 두 번째 케이스가 실패한다. 면제 목록이 자라 전부를 덮으면 첫 케이스가 조용히 통과할 수 있기 때문이다(gate #11 과 같은 구조적 사각 대비).

### 반증 (`step3-falsify.log`)

`GetDisplayVersion` 을 래퍼 밖으로 꺼냈다:

```
실패!  - 실패: 1, 통과: 1, 전체: 2
gui/ImageProcTest/Services/RealXpeBackend.cs:86 in 'GetDisplayVersion':
  public string GetDisplayVersion() => XpeDisplayNative.GetVersion();
```

파일·줄·멤버명을 짚어 그 케이스만 실패한다. 원복 후 재확인.

## 4. CI 실행 가능성 (카드 3항, 커밋 없음)

| 관측 | 결과 |
|---|---|
| `ci.yml` 의 GUI/WPF 잡 선례 | **없음**. `dotnet-tests`(windows-latest)가 통합 테스트만 돌린다 |
| 창을 숨긴 비대화형 실행 (`-NoProfile -NonInteractive`, `-WindowStyle Hidden`) | **exit=0, 6초, `Passed=true`, Native 백엔드** (`headless-probe.json`) |

**관측한 것**: 보이는 창도, 사람의 조작도 필요 없다 — 자동화 모드가 스스로 돌고 종료한다. 실행 시간 6초는 CI 잡으로 감당할 크기다.

**관측하지 못한 것**: 데스크톱 세션이 없는 환경에서 뜨는지. GitHub 호스티드 러너는 에이전트가 서비스로 도는 환경이라 윈도우 스테이션 사정이 다르고, **로컬에서는 같은 조건을 만들 수 없다.** SYSTEM 계정 예약 작업으로 세션 0 을 흉내 내려면 관리자 권한이 필요해 시도하지 않았다.

**결론: 로컬 관측만으로는 판정할 수 없다.** 유일하게 결정적인 실험은 CI 에서 한 번 돌려 보는 것이고, 실패해도 잡 하나가 붉어질 뿐이므로 시도 비용이 낮다 — 판단은 leader 몫이다.

## 5. 실측 (verbatim)

```
dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   152, 건너뜀:     1, 전체:   153
```

Baseline 귀속: C-24 최종 `0/150/1/151`(`GUI-C-24/step5-final.log`) → 증가 2 = 가드 2건. 설정 격리는 gui 자동화 한정이라 dotnet 스위트에 항목을 더하지 않는다.

## 6. 미검증 (Gaps)

- **격리가 다른 판정항까지 실제로 지켜 주는지는 `SelectedBodyPart` 하나로만 확인했다.** `ComparisonMode`·`ComparisonSwipePosition`·`ShowDisplayPanel` 도 같은 구조지만 각각 오염시켜 보지는 않았다.
- **가드는 텍스트 대조다.** 별칭(`using X = XpeDisplayNative;`), 리플렉션, 다른 파일로 옮긴 호출은 잡지 못한다. `RealXpeBackend.cs` 한 파일만 본다.
- **CI 헤드리스 여부는 미검증**(§4). 이 카드가 낸 것은 "로컬에서 창 없이 6초에 돈다" 까지다.
- 임시 설정 디렉터리는 실행마다 새로 만들고 지우지 않는다 — `%TEMP%` 에 누적된다. 크기는 수 KB.
- CI 실행 결과는 아직 없다.

## 7. 잔여 위험 (Residual risk)

- **`BackendMode` 를 이어받는 것은 의도된 구멍이다.** 그 값만은 여전히 배포 파일에서 온다. 자동화에 `--automation-backend` 같은 인자를 두면 완전히 닫히지만, 카드 범위 밖이라 하지 않았다.
- 격리 이후 자동화는 **배포 `appsettings.json` 을 검증하지 않는다.** 그 파일의 기본값이 깨져도 자동화는 통과한다 — 이전에는 우연히 함께 검사되고 있었다.
- 가드의 면제 목록은 사람이 관리한다. 새 면제를 근거 없이 추가하면 가드가 그만큼 무력해진다.
- WPF 자동화는 여전히 CI 밖에 있다. 이 카드가 만든 두 장치 중 가드만 CI 에서 돌고, 격리는 사람이 자동화를 돌릴 때만 효력이 있다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
# 관측: 배포 appsettings.json 의 selectedBodyPart 만 바꿔 같은 시나리오 2회
ImageProcTest.exe --automation-raw … --automation-report … --automation-width 1024 --automation-height 1024
dotnet test … --filter "FullyQualifiedName~NativeCallWrapperGuardTests"   # 가드·반증
powershell -NoProfile -NonInteractive -Command "Start-Process … -WindowStyle Hidden -Wait"   # 헤드리스 프로브
dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
