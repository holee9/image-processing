# GUI-C-34 — FlaUI 워크플로 W-01·W-02 (+ W-07 차단 보고) (XPE-GUI-E2E-001 §4.2, Mock)

- 카드: GUI-C-34 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋: `aa42d2a` (미푸시) — **커밋 1건**(카드는 2건 예정, §3 사유)
- **결과: E2E 8/8(스모크 5 + 워크플로 3, 합 3 s) · slnx 0/0 · 통합 0/170/1/171 무회귀**

---

## 1. 착수 전 실측 — 계획서 3행 중 2행이 그대로는 성립하지 않는다

| 계획서 행 | 실측 | 이 카드의 처리 |
|---|---|---|
| W-01 Raw 로드 | 앱이 `--automation-raw` 로 시작 시 자동 로드하는 경로 보유 | **구동 가능** — 그 경로 재사용 |
| W-02 전처리 실행 | `RunPreprocessingMenuItem` 이 `IsEnabled="False"` 이고 **Command 도 Click 도 없다**(`MainWindow.xaml:207-211`, 툴팁 "Requires xpe_preprocess.dll.") | **좁힘** — 플레이스홀더 계약을 단언 |
| W-07 VOI 프리셋 | `ApplyBodyPartPresetCommand` 를 **바인딩한 XAML 요소 0건**, Body Part 선택 컨트롤도 0건 | **차단** — §3 |

W-02 는 "Mock 이라 안 된다" 가 아니라 **어느 모드에서도 아직 아무 일도 하지 않는다.** 메뉴 항목 뒤에 명령이 없다.

### 추가한 AutomationId (UI 변경 없음)

`LogListBox`, `VoiWindowCenterInput`, `VoiWindowWidthInput` (`Views/AnalysisPanel.xaml`).

다만 **셋 다 이번 단언에는 쓰지 못했다.** Analysis 패널의 탭 안에 있어 그 탭이 선택되기 전에는 UIA 트리에 존재하지 않는다 — C-29 에서 메뉴로 겪은 **지연 생성** 함정과 같다. 추가는 남겨 둔다(탭 전환을 다루는 후속 시나리오가 바로 쓸 수 있다).

## 2. 구현한 시나리오 3건

| # | 확인하는 상태 | 시간 |
|---|---|---|
| W-01 | 뷰포트 사각형 > 0, 상태바가 `CalibrationEval` 요약을 담음 | 88 ms |
| W-01b | 백엔드 재초기화로 상태바를 요약에서 뗀 뒤 Apply Display Pipeline → 상태가 **바뀌고** `Display:` 를 담음 | 2 746 ms |
| W-02 | `RunPreprocessingMenuItem` 존재 + **비활성** | 904 ms |
| 합 | | **3.7 s** (게이트 3 min 대비 2 %) |

**W-01b 의 재초기화가 이 카드의 요점이다.** 그냥 파이프라인을 실행하고 상태바를 보면, 시작 시 자동 로드가 남긴 값으로 통과한다 — **인과가 아니라 이력을 재게 된다.** 먼저 상태를 요약에서 떼어내야 그 뒤의 변화가 이 실행 때문임을 말할 수 있다.

W-01 이 `CalibrationEval` 을 보는 이유: 그 문자열은 프레임이 로드되어 파이프라인을 지난 뒤에만 쓰인다. 로그가 더 직접적이지만 §1 의 탭 문제로 읽을 수 없다.

**W-02 의 단언은 만료되도록 썼다** — 전처리가 구현되면 `IsEnabled` 가 true 가 되어 이 시나리오가 실패하고, 실패 메시지가 "진짜 W-02 로 교체하라" 고 말한다. 플레이스홀더 단언은 그렇게 끝나는 것이 맞다.

## 3. W-07 은 구동할 수 없다 — 커밋 2를 만들지 않았다

```
grep -rn "ApplyBodyPartPresetCommand" gui/ImageProcTest/*.xaml gui/ImageProcTest/Views/*.xaml
  → 0건
grep -rn "BodyPart" gui/ImageProcTest/MainWindow.xaml
  → 0건
```

호출부는 `MainWindow.xaml.cs:151` **하나뿐이고 그것은 자동화 하네스 코드**다. 즉 사용자가 Body Part 를 고를 UI 가 존재하지 않으며, **UIA 로 누를 대상이 없다.**

카드가 허용한 변경은 "AutomationId 만 추가" 다. 컨트롤 신설은 UI 변경이라 범위 밖이므로 **하지 않았다.** C-29 §2(버전 라벨 부재) → C-30(leader 가 UI 추가 허용) 과 같은 형태이며, 같은 결정이 필요하다.

**차단을 우회해 통과시키는 방법은 있었고, 쓰지 않았다**: 하네스처럼 커맨드를 직접 호출하거나 설정값을 코드로 바꿔 놓고 라벨만 확인하는 것. 그러면 "UI 를 통해 프리셋이 적용된다" 가 아니라 "커맨드가 동작한다" 를 재게 되고, 그것은 E2E 가 답해야 할 질문이 아니다.

## 4. 실측 (verbatim)

```
dotnet test clients/ImageProcTest.E2ETests/… -c Debug
통과!  - 실패: 0, 통과: 8, 건너뜀: 0, 전체: 8 (3 s)      ← 스모크 5 + 워크플로 3

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패:     0, 통과:   170, 건너뜀:     1, 전체:   171
```

Baseline 귀속: 통합은 C-33 최종 `0/170/1/171` 과 동일(E2E·gui XAML 변경이라 통합 항목 불변). E2E 는 5 → **8**(워크플로 3건 신규). 스모크 5건은 그대로 통과 — 무회귀.

## 5. 미검증 (Gaps)

- **W-07 은 아무것도 검증하지 않았다**(§3). 프리셋 값이 백엔드별로 맞는지는 C-23 이 자동화 리포트로 확인했을 뿐, E2E 경로로는 미검증이다.
- **W-02 는 "동작" 을 재지 않는다.** 비활성 상태만 본다 — 전처리 자체는 존재하지 않으므로 잴 것이 없다.
- **W-01 은 이미지 내용을 보지 않는다.** 뷰포트 사각형과 상태 문자열까지다. 픽셀이 맞는지는 이 층의 질문이 아니다.
- **탭 안의 요소는 여전히 미접근**(§1). `LogListBox` 등은 Id 를 붙였지만 읽지 못했다.
- **Native 모드 워크플로는 돌리지 않았다** — 카드가 제외했다.
- 반증을 수행하지 않았다. 카드의 반증 대상이 W-07(차단)이었기 때문이며, W-01/W-01b/W-02 에 대한 반증은 별도로 하지 않았다 — **이번 보고의 가장 큰 약점이다.**
- CI 실행 결과는 아직 없다.

## 6. 잔여 위험 (Residual risk)

- **W-01b 의 인과는 상태바 문자열에 의존한다.** 요약 형식(`… -> Display: …`)이 바뀌면 시나리오가 깨진다. C-30 이후 이 스위트는 표시 문구에 점점 더 묶이고 있다.
- **재초기화가 부작용을 남기지 않는다고 가정했다.** 알림·로그를 비우지만 로드된 프레임은 유지된다는 전제로 W-01b 가 성립한다 — 코드 독해 근거이고, 초기화 후 프레임이 사라지는 변경이 오면 조용히 깨진다.
- 워크플로 픽스처가 앱 인스턴스를 클래스 단위로 공유하므로 W-01b 의 재초기화가 W-01·W-02 의 상태에 영향을 줄 수 있다. 현재 xUnit 실행 순서에서는 관측되지 않았지만 **순서 의존을 배제하지 못한다.**

## 부록 — 사용한 명령

```bash
grep -rn "ApplyBodyPartPresetCommand" gui/ImageProcTest/*.xaml gui/ImageProcTest/Views/*.xaml   # → 0건
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… --filter "FullyQualifiedName~WorkflowScenarios" --logger "console;verbosity=detailed"
dotnet test clients/ImageProcTest.E2ETests/… -c Debug
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
```
