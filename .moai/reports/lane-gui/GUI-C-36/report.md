# GUI-C-36 — 전처리 배선 + CI 빨간불 해소 (#141 #134 #136)

- 카드: GUI-C-36 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui` · 병합 `6d7063e`(XCal CLI 포함)
- 커밋 3건: `d5cc1e5`(0항 잔존 정리) · `927ad53`(CI 3원인) · `5e42079`(1·2항) — 미푸시
- **결과: E2E Mock 9/9 · Native 9/9 · 통합 0/170/1/171 · slnx 0/0**

---

## 0. 픽스처 잔존 인스턴스 정리 (`d5cc1e5`)

C-35 의 유출이 다음 실행으로 번지지 않게 한다. 이름이 아니라 **자기 exe 경로가 일치하는** 프로세스만 죽인다. 재현: 앱을 띄워 둔 채(잔존 1) E2E 9/9, 종료 후 0.

## 1. CI 빨간불 — 원인 3개, 둘은 내 결함 (`927ad53`)

run 34488408994 의 `gui-automation`·`gui-e2e-native` 가 빨갛고 로컬은 9/9였다. 환경 차이를 실측했다.

| # | 증상 | 원인 | 조치 |
|---|---|---|---|
| 1 | Native 잡에서 `Not found: "C=25000"` | **W-07 이 Mock 값을 무조건 단언**했다. 주석에는 "활성 백엔드의 값" 이라 쓰고 코드는 상수를 박아 뒀다 | `BackendMode` 로 분기 (Native = `C=-600`/`W=1600`, `voi_lut.cpp` `XPE_BODY_LUNG`) |
| 2 | 두 잡 모두 `RunPreprocessingMenuItem was not found`(10 s 타임아웃) | 메뉴 자식을 **창 하위**에서 찾았다. WPF 하위 메뉴는 자기 팝업 창에 그려지므로 메인 창의 자손으로 보이는지는 팝업 부모 관계에 달렸다 — 로컬은 보이고 CI 러너는 안 보였다 | **메뉴 항목 안에서 먼저** 찾고 창 하위는 폴백. S-04 도 동일 적용 |
| 3 | Native 로컬 재현 시 워크플로 4건이 1 ms 만에 `E_FAIL from a COM component` | **0항이 만든 경합.** xUnit 은 서로 다른 컬렉션을 병렬로 돌리는데, 이 스위트는 같은 exe 를 두 컬렉션에서 몰고 0항의 정리가 그 exe 를 도는 프로세스를 **전부** 죽인다 — 상대 컬렉션의 앱까지 | 어셈블리 직렬화(`DisableTestParallelization`) |

3번은 정리 범위를 좁히는 대신 직렬화를 택했다: **같은 GUI 두 인스턴스가 포커스와 설정 파일을 두고 경쟁하는 상태는 이 스위트가 관측할 상태가 아니다.**

1번은 C-35 에서 내가 "활성 백엔드 값으로 다뤘다" 고 보고한 것과 코드가 달랐다는 뜻이다 — **CI 의 Native 잡이 그 불일치를 잡았다.**

## 2. 서비스 재사용 실측 (카드 지시) — 재사용 불가

| 실측 | 값 |
|---|---|
| 실제 실행자 | `NativePreprocessPreviewService.Run(...)` (`PreprocessFixtureE2eService.cs:232`), **1 116 줄** |
| 호출하는 네이티브 | `xpe_preprocess_init` · `xpe_offset_correct` · `xpe_gain_correct` · `xpe_defect_correct` · `xpe_preprocess_shutdown` — **스테이지 단위**, `xpe_preprocess_pipeline` 은 쓰지 않는다 |
| `XpeCommonApi` 참조 | **49회** — 그 타입의 정적 생성자가 `SetDllImportResolver` 를 호출(`PInvokeWrapper.cs:17-20`) |

gui 는 C-32 에서 자기 리졸버를 등록했고 어셈블리당 하나만 허용되므로 Link 하면 첫 접촉에서 던진다(C-13 의 벽). → **gui 최소 구현**.

## 3. 1항 배선 (`5e42079`)

| 파일 | 역할 |
|---|---|
| `Services/Native/XpePreprocessInterop.cs` | 메타데이터 구조체 + P/Invoke 7개 |
| `Services/Native/GuiPreprocessRunner.cs` | init → 캘리브레이션 3종 → offset → gain → defect → shutdown |
| `Models/PreprocessRunResult.cs` | 결과 또는 **거부 사유**를 담는 값 |
| `IXpeBackend` / `MockXpeBackend` / `RealXpeBackend` | `RunPreprocessing` + `SupportsPreprocessing` |
| `MainWindowViewModel` / `MainWindow.xaml` | 커맨드 + `IsEnabled={Binding CanRunPreprocessing}` |
| `fixtures/gui-s0/fixture-manifest.json` | 캘리브레이션 출처를 **"실행 시 CLI 생성"** 으로 기록 |

설계에서 지킨 것:

- **버퍼 포맷은 헤더가 정한 대로**(offset UInt16→UInt16, gain UInt16→Float32, defect Float32→Float32). 잘못 할당하면 오류가 아니라 **조용히 틀린 답**이 나온다.
- **모든 실패가 예외가 아니라 값**이다. 캘리브레이션 부재는 예상된 상태다 — XCal 은 커밋되지 않고 `xpe_calib_fixture_gen` 이 실행 시 만든다.
- **네이티브 호출은 `InvokeNative` 안**(C-24 래퍼). 실패 경로에서도 알림이 배출되어야 하고, 스테이지가 거부하는 순간이야말로 큐에 볼 것이 있는 때다. C-25 가드가 이 규칙을 강제한다.
- 할당한 버퍼는 `finally` 에서 역순 해제한다.

## 4. 2항 W-02 교체 + W-01 자족화

**W-02**: Mock 은 비활성 계약 유지, Native 는 활성 + 실행 후 상태바에 전처리 라인.

캘리브레이션이 없는 지금 관측되는 라인은 **거부**다:

```
"Preprocessing skipped: calibration file(s) not found — …"
```

성공 라인(`Preprocess: offset -> gain -> defect …`)과 같은 부분 문자열로 단언하되, **성공 경로는 미검증으로 보고한다**(§6).

**W-01 은 자족적으로 고쳤다.** W-02 가 배선되자 xUnit 순서상 먼저 돌아 상태바에 전처리 라인을 남겼고, W-01 이 잔여 상태로 실패했다 — **C-34·C-35 보고서에서 위험으로 적어 둔 순서 의존이 실제로 터진 것**이다. 이제 W-01 이 파이프라인을 직접 실행해 자기가 단언할 상태를 만든다.

### 반증

메뉴를 `IsEnabled="False"` 로 되돌렸다:

```
실패!  - 실패: 1, 통과: 8, 전체: 9
W02_RunPreprocessing_MatchesTheBackendItRunsOn: Run Preprocessing is disabled on the native backend.
```

Native W-02 만 실패한다. 원복 후 재확인.

## 5. 실측 (verbatim)

```
dotnet test clients/ImageProcTest.E2ETests/… -c Debug                      (Mock)
통과!  - 실패: 0, 통과: 9, 건너뜀: 0, 전체: 9

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …              (Native, 앱 폴더 비움)
통과!  - 실패: 0, 통과: 9, 건너뜀: 0, 전체: 9

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 170, 건너뜀: 1, 전체: 171
```

Baseline 귀속: E2E 9건·통합 171건 모두 C-35 최종과 동일 — 이번 변경은 시나리오 **내용**을 바꿨을 뿐 개수를 바꾸지 않는다.

## 6. 미검증 (Gaps)

- **전처리 성공 경로를 한 번도 실행하지 못했다.** `xpe_calib_fixture_gen` 바이너리를 실은 CI run(6d7063e)이 아직 진행 중이고, **네이티브 빌드는 이 레인의 금지 조항**이라 로컬에서 만들 수 없다. 따라서 `xpe_offset_correct` 이하 세 스테이지가 실제로 `XPE_OK` 를 돌려주는지, 결과 픽셀이 옳은지는 **미검증**이다.
- **P/Invoke 시그니처를 실행으로 검증하지 않았다.** `XpeImageMetadataNative`(64바이트 배열 + `Pack=8`)는 헤더를 읽어 맞췄을 뿐이다 — 정렬이 틀리면 조용히 잘못된 값이 넘어간다. 성공 경로를 돌리는 순간 드러날 종류의 결함이다.
- **결과를 processed 뷰포트에 넣지 않았다.** `PreprocessRunResult.Pixels` 를 만들지만 뷰모델이 아직 소비하지 않는다 — 성공 경로가 검증되기 전에 화면 배선을 더하면 두 미검증이 겹친다.
- **자동화 리포트의 `PreprocessRan` 필드를 추가하지 않았다**(카드 2항). 성공 경로 없이 그 값은 항상 false 라 정보가 없다.
- CI 수정이 실제로 CI 를 녹색으로 돌리는지는 **미확인** — 로컬에서 CI 조건을 흉내 냈을 뿐이다.

## 7. 잔여 위험 (Residual risk)

- **거부 라인과 성공 라인을 같은 부분 문자열로 단언한다.** 캘리브레이션이 생긴 뒤에도 W-02 는 통과하지만, 그것이 "성공했다" 는 뜻은 아니다 — 성공 경로가 생기면 **단언을 좁혀야 한다**(예: `offset -> gain -> defect` 정확 일치).
- 순서 의존은 W-01 만 고쳤다. W-01b·W-07 도 상태바를 읽으므로 새 시나리오가 앞에 끼면 같은 형태로 깨질 수 있다.
- 잔존 정리와 직렬화는 **로컬 실행 시간**을 늘린다(E2E 6 s → 8~10 s). 지금은 게이트에 여유가 크다.
- 메타데이터 값(kVp 70 / mAs 2 / SID 1000 / pitch 0.14)을 코드에 고정했다. 실제 촬영 조건을 반영하지 않으며, 스테이지가 그 값에 의존하면 결과가 달라진다.

## 부록 — 사용한 명령

```bash
git merge origin/main                                   # → 6d7063e (XCal CLI)
gh run view 34488408994 --log-failed | grep -iE "W02|not found"
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… -c Debug                                   # Mock
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …         # Native
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
```
