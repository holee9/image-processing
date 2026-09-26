# GUI-C-38 — CreateSettings 두 갈래 통합 + 소비 가드 (#136 #141)

- 카드: GUI-C-38 · Refs #136 #141 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 2건: `c0cb7b2`(구조 통합) · `8996415`(가드) — 미푸시
- **결과: 통합 0/180/1/181 · E2E Mock 9/9 · E2E Native 9/9 · 자동화 Native `PreprocessRan=true` · 자동화 Mock `arg` · slnx 0/0**

---

## 1. 왜 구조를 바꿨나 — 같은 결함이 두 번 났다

| 카드 | 스위치 | 증상 |
|---|---|---|
| GUI-C-31 | `--automation-backend` | Native 요청인데 리포트가 `mode=Mock` |
| GUI-C-37 | `--automation-calib` | 파일이 있는데 `calibration file(s) not found` |

원인은 같다. 둘 다 파싱·검증·`App` 저장까지 정상이고, **적용이 `CreateSettings` 의 격리
분기 안에만** 있었다. 그 분기는 `--automation-report` 가 함께 있을 때만 도는데 **E2E 픽스처는
일부러 report 를 주지 않는다.** 그래서 스위치는 받아들여지고, 앱은 뜨고, 리포트는 요청한
모양이며, 옵션만 아무 일도 하지 않았다.

C-37 보고서 §6 이 이것을 잔여 위험으로 적었다 — "새 `--automation-*` 스위치를 더할 때마다
같은 함정". 이 카드가 그 지적을 실행한 것이다.

## 2. 커밋 1 — 두 갈래 통합 (`c0cb7b2`)

분리 기준을 바꿨다. 전에는 **자동화냐 아니냐**로 갈렸고, 이제는 **무엇을 고르느냐 / 어디에
저장하느냐**로 갈린다.

| 결정 | 어디서 | 조건 |
|---|---|---|
| 실행이 **무엇을** 고르는가 (backend / calib / 크기) | `ApplyRunSelection` | **무조건** — 모든 기동 모드 |
| 설정이 **어디에** 사는가 (격리 파일, 기본값 시작) | `CreateSettings` | 자동화 모드에서만 |

기동 선택은 저장된 값이 아니다. 이 한 문장이 결함의 원인을 없앤다 — 격리는 여전히 자동화
전용이지만(C-23 의 이유: 이전 실행의 잔여 상태가 판정을 뒤집는다), 선택 적용은 갈래 밖으로
나왔다.

함께 정리한 것:

- `OnLoaded` / 자동화 시나리오에 있던 **크기 재적용을 제거**했다. 같은 규칙이 두 곳에 있는 것이
  앞선 두 결함이 살아남은 경로다.
- VOI 센티널 주차(-1/1)를 프리셋 클릭 앞에 두고, `RunPreprocessingMenuItem` 클릭을
  `DisplayPipelineApplied` 뒤로 옮긴 C-37 배선은 그대로 유지된다.

## 3. 커밋 2 — 소비 가드 (`8996415`)

구조를 고쳐도 **다음 스위치**는 같은 실수를 할 수 있다. 그래서 기계로 막는다.

`AutomationRunSelectionConsumptionTests` — 세 단언:

| 단언 | 무엇을 막나 |
|---|---|
| `EveryRunSelectionField_IsAppliedByApplyRunSelection` | 파싱만 되고 아무도 쓰지 않는 스위치 |
| `ExclusionList_NamesOnlyPropertiesThatExist` | 이름을 바꿔 제외 목록으로 조용히 빠져나가는 경로 |
| `RunSelection_CoversTheTwoSwitchesThatRegressed` | 검사 대상이 0개로 줄어드는 공허한 통과 |

`AutomationArgs` 의 속성을 리플렉션으로 열거하고, **실행 선택이 아닌 것만 이름과 사유를 명시해
제외**한다(`RawPath`·`ReportPath`·`Error`). 추론 규칙이 아니라 명시 목록인 이유는, 추론이면
새 속성을 조용히 흡수해 버리기 때문이다 — 이 클래스가 잡으려는 바로 그 실패다.

소스 텍스트 가드다. 실패가 **부재**이기 때문에 — 어떤 실행 관측도 "적용했는데 차이가 없었다"
와 "한 번도 적용되지 않았다" 를 구분하지 못한다.

### 반증

`ApplyRunSelection` 에서 calib 적용 블록만 지웠다:

```
These AutomationArgs fields are parsed but never applied in gui/ImageProcTest/MainWindow.xaml.cs:
CalibrationDirectory. Expected a read of App.Automation<Name> inside ApplyRunSelection …

실패!  - 실패: 1, 통과: 179, 건너뜀: 1, 전체: 181
```

181건 중 **이 가드 1건만** 실패한다. 원복 후 재확인.

## 4. 세 기동 모드 실측 (verbatim)

```
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet test clients/ImageProcTest.E2ETests/… -c Debug                       (Mock)
통과!  - 실패: 0, 통과: 9, 건너뜀: 0, 전체: 9 (9 s)

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests…       (앱 폴더 비움)
통과!  - 실패: 0, 통과: 9, 건너뜀: 0, 전체: 9 (1 m 3 s)

ImageProcTest.exe --automation-backend Native --automation-calib <set> --automation-width 1024 …
exit=0 · Passed=True · BackendMode=Native · BackendModeSource=arg
        PreprocessRan=True · "Preprocess: offset -> gain -> defect on 1024x1024 (Abdomen)."

ImageProcTest.exe --automation-backend Mock --automation-width 1024 …
exit=0 · Passed=True · BackendMode=Mock · BackendModeSource=arg
        PreprocessRan=False · "not attempted (menu enabled=False, frame loaded=True)"

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: 통합 **172**(`c0cb7b2` 시점) → **178**(C-39 가드 +6) → **181**(이 가드 +3).
증가분은 전부 새 가드이고 기존 171 통과·1 스킵은 그대로다. E2E 9건은 C-37 과 동일.

`BackendModeSource=arg` 와 `PreprocessRan=true` 가 함께 나온 것이 이 카드의 핵심 관측이다 —
두 스위치가 **report 를 준 기동에서도, 주지 않은 E2E 기동에서도** 실제로 소비된다.

### E2E Native 첫 시도는 실패했다 — 원인은 탐색 순서

첫 실행에서 `S05_RuntimePanel_ShowsBackendVersion` 이 `Not found: "src=bin"` 로 실패했다.
앱 출력 폴더(`gui/ImageProcTest/bin/Debug/net8.0-windows/`)에 `xpe_common.dll` 등 3개가
있었고, #129 탐색 순서상 `AppContext.BaseDirectory` 가 **주입 디렉터리보다 먼저**다.
`XPE_NATIVE_DIR_EXCLUSIVE` 는 "여기서 멈춰라" 이지 "여기만 봐라" 가 아니므로 앱 폴더가 이긴다.

**이 커밋의 회귀가 아니다** — 정책대로 동작한 것이고, C-37 이 같은 측정을 "앱 폴더 비움"
조건에서 한 이유다. 3개를 옮겨 두고 재측정해 9/9 를 얻은 뒤 원복했다.

## 5. 미검증 (Gaps)

- **가드는 이름의 존재만 본다.** `ApplyRunSelection` 안에 `App.AutomationX` 가 등장하면
  통과한다 — 그 값이 **올바른 설정 필드에** 들어가는지는 보지 않는다. calib 을 offset 한 곳에만
  넣고 gain/defect 를 빠뜨려도 이 가드는 통과한다.
- **`ApplyRunSelection` 이 실제로 호출되는지는 이 가드가 보지 않는다.** `CreateSettings` 의 두
  경로 모두에서 호출되는 것은 §4 의 세 기동 모드 실측으로만 뒷받침된다.
- **`--automation-calib` 를 세 디렉터리에 같은 값으로 넣는다**(C-37 과 동일). 서로 다른 위치
  구성은 확인하지 않았다.
- **결과 픽셀의 정확성은 여전히 확인하지 않았다** — 스테이지가 `XPE_OK` 를 돌려주고 뷰포트
  경로를 지난다는 것까지다(모듈 테스트 소관).
- 1024×1024 한 크기, seed 0 한 조합.
- CI 반영 결과 미확인.

## 6. 잔여 위험 (Residual risk)

- **가드가 소스 텍스트에 의존한다.** `ApplyRunSelection` 이 이름을 바꾸거나 로직이 다른
  메서드로 옮겨 가면 `Assert.Fail`(찾지 못함)로 떨어진다 — 조용히 건너뛰지 않게 했으므로
  눈에 띄지만, 그때 가드를 새 위치로 옮기는 것은 사람 몫이다.
- **제외 목록이 사람의 판단이다.** 실행 선택 필드를 제외 목록에 넣으면 가드가 통과한다.
  사유를 이름 옆에 적어 두어 리뷰에서 보이게 했지만, 기계가 막지는 못한다.
- **앱 폴더의 네이티브 DLL 이 로컬 E2E Native 측정을 좌우한다**(§4). 측정 전에 비우지 않으면
  `src=` 단언이 실패한다 — 실패가 곧 신호이므로 조용히 틀리지는 않는다.
- E2E Native 가 1 m 3 s 로 스모크 게이트(30 s)와는 다른 규모다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
dotnet test clients/ImageProcTest.E2ETests/… -c Debug                        # Mock
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …E2ETests…
./build/ci-common/bin/xpe_calib_fixture_gen.exe --out <dir> --width 1024 --height 1024 --seed 0
ImageProcTest.exe --automation-backend Native --automation-calib <dir> …
ImageProcTest.exe --automation-backend Mock …
```
