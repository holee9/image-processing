# GUI-C-23 — Native 자동화 `VoiPresetApplied=False` 원인 확정 + 수정 (#135)

- 카드: GUI-C-23 · Refs #135 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 선행: `git merge origin/main` → `53a5b71` (C-22 병합본 포함, `.claude` 변경 0건)
- 네이티브: CI run 34477554287 아티팩트(C-22 와 동일 스테이징)
- **결과: Native `Passed=true` 관측 · Mock 무회귀 · clients 0/0 · gui 0/0 · dotnet 실패 0 / 통과 147 / 건너뜀 1 / 전체 148**

---

## 1. 재현 (커맨드 1줄)

```bash
ImageProcTest.exe --automation-raw fixtures/gui-s0/raw/synthetic_1024x1024.raw \
  --automation-report <report>.json --automation-width 1024 --automation-height 1024
# appsettings.json 의 backendMode 를 Native / Mock 으로 바꿔 대조
```

## 2. 원인 확정 (관측 먼저 — 가설로 고치지 않았다)

### 2.1 판정 코드 경로

| 파일·줄 | 내용 |
|---|---|
| `gui/ImageProcTest/MainWindow.xaml.cs:95` | `ApplyBodyPartPresetCommand.Execute(null)` |
| `MainWindowViewModel.cs:719-722` | `_backend.CreateVoiPreset(bodyPart)` → `Settings.VoiWindowCenter/Width/VoiLutMode` 에 기록 |
| `MainWindow.xaml.cs:119-122` (수정 전) | `VoiPresetApplied = SelectedBodyPart=="Abdomen" && Center==32768.0f && Width==65535.0f` |
| `MainWindow.xaml.cs:249` | 그 값이 종합 `Passed` 술어에 AND 로 들어감 |

### 2.2 두 백엔드가 내놓는 값

| 출처 | Abdomen 프리셋 |
|---|---|
| `gui/ImageProcTest/Services/MockXpeBackend.cs:301` | `(32768.0f, 65535.0f)` — 판정 코드의 상수와 **바이트 단위로 동일** |
| `modules/display/src/voi_lut.cpp:93-96` | `center=40.0f, width=400.0f` |
| `modules/display/include/xpe/display/display_api.h:203` | 그 값을 "clinically validated preset values" 로 규정 (HU 윈도) |

### 2.3 대조 실행 — 같은 앱 코드, 백엔드만 교체

```
Mock    Passed=True   VoiPresetApplied=True   Backend=v0.0.0-mock         VOI(Linear, C=32768, W=65535)
Native  Passed=False  VoiPresetApplied=False  Backend=xpe_display 1.0.0   VOI(Linear, C=40, W=400)
```
(`before-Mock.json`, `before-Native.json`)

### 2.4 **원인**: 판정이 MockXpeBackend 의 값을 정답으로 박아 뒀다

Native 에서도 **프리셋은 정상 적용됐다** — 설정이 40/400 으로 바뀌었고 파이프라인이 그 값으로 돌았다. 판정만 32768/65535 를 기대해 어긋난 것이다.

**증상이 아니라 원인임을 어떻게 아는가**: 실패한 술어항이 `VoiPresetApplied` 하나뿐이고(나머지 30여 항 전부 true), 그 술어는 관측된 설정값과 **리터럴**의 비교다 — 두 피연산자를 모두 관측했고(40/400 대 32768/65535), 리터럴의 출처가 Mock 구현임을 파일·줄로 확인했다. 상류 실패 가능성도 배제했다: 프리셋 호출이 던졌다면 `VOI_PRESET_FAILED` 알림이 늘었을 텐데 알림 수는 로드 전후 모두 1로 변동이 없다(`InitialAlertCount == AlertCountAfterLoad`).

**원인은 앱(gui)이다.** 네이티브는 계약대로 동작한다 — 수정 대상이 아니다.

## 3. 수정 (커밋 1개)

| 파일 | 변경 |
|---|---|
| `MainWindowViewModel.cs` | `LastAppliedVoiPreset` 프로퍼티 — 활성 백엔드가 내놓은 프리셋을 기록 |
| `Models/GuiAutomationReport.cs` | `VoiPresetCenter` / `VoiPresetWidth` — 관측값을 리포트에 남겨 이후 드리프트가 보이게 |
| `MainWindow.xaml.cs:119` | 판정을 **상수 비교 → 적용 여부 비교**로 교체 |
| `MainWindow.xaml.cs:95` | 프리셋 실행 전 센티널(-1 / 1) 주차 — §4 참조 |

판정의 의미가 "백엔드가 Mock 인가" 에서 "명령이 활성 백엔드의 프리셋을 실제로 설정에 썼는가" 로 바뀐다. 백엔드에 중립적이라 앞으로 프리셋 값이 바뀌어도 이 판정은 깨지지 않는다.

## 4. 반증 — 2회, **1차는 반증에 실패했다**

정직하게 둘 다 적는다.

**1차 (무효).** `Settings.VoiWindowCenter = preset.Center;` 를 주석 처리하고 Native 재실행 → `VoiPresetApplied=True` 로 **그대로 통과**했다. 변형이 무력했던 것이 아니라, **비교 대상이 이미 같았다**:

```
gui/ImageProcTest/bin/Debug/net8.0-windows/appsettings.json
  "voiWindowCenter": 40,
  "voiWindowWidth": 400,
```

앱이 설정을 영속화하기 때문에 **직전 Native 실행이 저장해 둔 창**이 그대로 로드됐고, 프리셋을 쓰지 않아도 값이 일치했다. 즉 수정된 판정은 "이번 실행에서 썼다" 가 아니라 "값이 우연히 같다" 로도 참이 될 수 있었다 — 내 수정 자체의 결함이다.

**교정.** 프리셋 실행 직전에 어느 프리셋도 내놓지 않는 센티널(center=-1, width=1)로 창을 주차하도록 자동화를 고쳤다(`MainWindow.xaml.cs:95`).

**2차 (유효).** 같은 변형을 다시 걸고 재실행:

```
반증(2차, 프리셋 미반영): Passed=False  VoiPresetApplied=False  preset C/W= 40 / 400
```

`VoiPresetCenter=40` 은 백엔드가 프리셋을 만들어 주긴 했음을 보이고, 설정은 센티널에 머물러 판정이 거짓이 된다 — 정확히 의도한 실패다. 로그: `falsify-Native.json`. 원복 후 재확인(§5).

**"반증이 통과했다" 를 "가드가 튼튼하다" 로 읽지 않는다**(gate #14). 1차는 실험 실패였고, 그 덕에 수정의 실제 결함을 찾았다.

## 5. 수정 후 자동화 (전/후 대조)

| 모드 | 전 | 후 |
|---|---|---|
| Mock | `Passed=True`, `VoiPresetApplied=True` | `Passed=True`, `VoiPresetApplied=True`, `preset C/W=32768/65535` |
| Native | `Passed=False`, `VoiPresetApplied=False` | **`Passed=True`**, `VoiPresetApplied=True`, `preset C/W=40/400` |

Mock 무회귀 확인. 리포트 JSON 4건 동봉(`before-*.json`, `after-*.json`) + 반증 1건.

## 6. 실측 (verbatim)

```
dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   147, 건너뜀:     1, 전체:   148
```

Baseline 귀속: C-22 최종 `0/147/1/148`(`GUI-C-22/step5-final.log`) → **변동 없음**. 이번 변경은 gui 자동화 하네스에 한정되고 clients 테스트 스위트를 건드리지 않으므로 증감 0이 예상값이며, 실측도 그렇다.

## 7. 미검증 (Gaps)

- **회귀는 xUnit 이 아니라 자동화 리포트다.** gui 에는 테스트 프로젝트가 없고 이 코드는 WPF 타입에 묶여 링크 소스로도 뺄 수 없다. 즉 이 판정은 **CI 가 아니라 사람이 자동화를 돌릴 때만** 검사된다.
- **Abdomen 이외의 부위**는 확인하지 않았다. Bone/Lung/Head 프리셋의 네이티브 값도 Mock 과 다르겠지만 자동화가 Abdomen 만 누른다.
- 네이티브 40/400 이 **임상적으로 옳은지**는 판단하지 않았다 — 헤더가 그렇게 규정한다는 사실만 인용했다.
- 센티널 주차가 다른 술어항(`DisplayPipelineApplied` 등)에 영향을 주지 않는지는 두 모드 `Passed=true` 로만 확인했다. 항목별 영향은 따로 보지 않았다.
- CI 실행 결과는 아직 없다.

## 8. 잔여 위험 (Residual risk)

- **설정 영속화가 자동화 판정 전반의 함정이다**(§4). 이번엔 VOI 만 센티널로 막았고, `ComparisonZoomScale`·`SelectedBodyPart` 등 다른 항목도 이전 실행 값에 기대 통과할 수 있다. 자동화 시작 시 설정을 초기화하는 편이 근본적이지만 카드 범위 밖이라 하지 않았다.
- 판정이 "백엔드가 준 값을 그대로 썼는가" 이므로, 백엔드가 **잘못된 프리셋**을 주면 그대로 통과한다. 값의 타당성 검증은 이 판정의 몫이 아니다 — 네이티브 단위 테스트(Lane B) 영역이다.
- `LastAppliedVoiPreset` 은 프로덕션 코드에 붙은 관측용 프로퍼티다. 자동화만 읽지만 공개 API 표면이 1개 늘었다.

## 부록 — 사용한 명령

```bash
git fetch origin main && git merge origin/main            # → 53a5b71
export PATH="/c/Program Files/dotnet:$PATH"
# 대조 실행 (backendMode 를 Mock/Native 로 바꿔가며)
ImageProcTest.exe --automation-raw … --automation-report … --automation-width 1024 --automation-height 1024
dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
