# GUI-C-85 보고 — 보고서 3종이 실제 모드를 기록하는지 (#175, HAZ-GUI-005 통제 (3))

커밋 없음(코드 변경 없음). 측정에 쓴 임시 스크립트 두 개는 이 디렉터리에 `.txt` 로 보관했고, 저장소에서는 지웠다.

## 1. 결과표

| 보고서 | 실제 모드 필드 | 요청 모드 필드 | (a) Native·DLL 있음 | (b) Native·빈 폴더 | (c) Mock | 판정 |
|---|---|---|---|---|---|---|
| automation report (`--automation-report`) | `ActualBackendMode` (+`MockBackend`) | `BackendMode` | Native / False / Native | **Mock / True / Native** | Mock / True / Mock | 기록함 |
| menu-command-report (`<bin>/menu-command-report.json`) | `actualBackendMode` (+`mockBackend`, `backend.backendName`) | `requestedBackendMode`, `settings.backendMode` | Native / false / RealXpeBackend / Native | **Mock / true / MockXpeBackend / Native** | Mock / true / MockXpeBackend / Mock | 기록함 |
| 증거 묶음 — 내용물 `evidence/<RunId>/backend.json` | `actualBackendMode` (+`mockBackend`, `backendName`) | `requestedBackendMode` | Native / false / RealXpeBackend / Native | **Mock / true / MockXpeBackend / Native** | Mock / true / MockXpeBackend / Mock | 파일은 기록함 — **묶음은 만들어지지 않음 (2절)** |

- **대조군**: 세 보고서 모두 (a) 와 (b) 의 실제 모드 값이 다르다(Native / Mock). 두 경우의 요청 모드는 같다(Native). 따라서 이 필드는 상수가 아니고, 요청 모드를 옮겨 적은 것도 아니다.
- **(b) 와 (c) 의 구별**: 실제 모드는 둘 다 Mock 이고, 요청 모드가 Native / Mock 으로 다르다. 세 보고서 모두 두 필드를 함께 기록한다.
- **종료 코드** (자동화 실행): (a) 0, (b) 1, (c) 0.

## 2. 멈추고 보고 — 증거 묶음은 이 앱에서 만들어질 수 없다

- **증상**: "Export bundle" 버튼을 누르면 세 경우 모두 다음 메시지로 실패한다.
  `Export failed: The process cannot access the file '…\evidence\bundles\.zip' because it is being used by another process.`
- **원인**: `RunSetState.RunId` 에 값을 넣는 코드가 없어서 늘 빈 문자열이다. `gui/ImageProcTest` 의 `*.cs` 를 `RunId\s*=` 로 검색하면 결과는 필드 초기값 `_runId = string.Empty` 하나뿐이다.
- **결과**:
  - `ExportEvidenceBundle` 은 `evidence/` 전체를 `evidence/bundles/.zip` 으로 압축하려 한다.
  - 그런데 압축 대상 폴더 안에 자기 자신이 쓰고 있는 zip 파일이 들어 있어서 실패한다.
  - 경로 `evidence/<RunId>` 가 `evidence/` 로 무너지고, 파일 이름이 `.zip` 이 되는 것도 같은 원인이다.
  - 실측: 실행 뒤 `bin/.../evidence/bundles/.zip` 이 남는다(365 바이트, 첫 실행).
- **함께 관측한 것**: `verdicts.json` 은 세 경우 모두 `{}` 다. 활성 스터디가 없으면(`ActiveStudyId` 가 빈 값) 판정이 기록되지 않기 때문이다. 이 조건에서 묶음에 들어갈 판정 내용은 없고, `backend.json` 만 쓰인다.
- **판단 요청**:
  - 증거 묶음은 모드 기록 이전에 **내보내기 자체가 동작하지 않는다.** 통제 (3) 의 "증거 묶음" 칸은 "내용물 파일은 기록, 묶음은 생성 불가" 로 적어야 한다.
  - `RunId` 를 언제 어떻게 정할지는 설계 결정이라 고치지 않았다. 카드가 정한 범위(모드 기록)를 넘는 결함이다.
  - 제품 UI 에서 버튼이 눌리므로(TopBar "Export bundle", AnalysisPanel) 사용자가 실패 메시지를 보게 된다.

## 3. 증거 (이 디렉터리)

- **자동화 보고서·메뉴 보고서**: `c85-automation.txt`, `automation-report-{a,b,c}.json`, `menu-command-report-{a,b,c}.json`.
  - 측정 스크립트는 `probe-automation.ps1.txt` 다. CI 와 같은 인자로 앱을 실행하고, `XPE_NATIVE_DIR` 과 `XPE_NATIVE_DIR_EXCLUSIVE=1` 로 DLL 폴더를 고정했다.
  - 매 실행 전에 `menu-command-report.json` 을 지워서 이전 실행이 남긴 파일을 읽지 않게 했다.
- **증거 묶음**: `c85-bundle.txt`, `evidence-{a,b,c}-backend.json`, `evidence-{a,b,c}-verdicts.json`.
  - 측정 코드는 `probe-ZzEvidenceBundleProbe.cs.txt` 다. 임시 FlaUI 시험으로 대화형 앱을 띄운 뒤 다음 순서로 진행했다.
  - "✓ Pass" 판정 버튼 클릭 → `evidence/` 의 파일 읽기 → "Export bundle" 클릭 → 상태줄 읽기.
  - 앱을 띄울 때는 (a)(b) 는 강제 Native 픽스처(`build/ci-common/bin` / 빈 폴더), (c) 는 기본(Mock) 픽스처를 썼다.
  - 앱 상태줄 기록: (a) `mode=Native … src=bin`, (b) `mode=Mock (requested Native)`, (c) `mode=Mock`.
- **빌드**: 임시 시험을 지운 뒤 경고 0 / 오류 0.

## 4. 기준선 귀속

- 측정 트리는 이 세션의 dev/gui HEAD `9a51245` 다(C-84 커밋 위, 추가 변경 없음).
- 네이티브 DLL 은 기존 `build/ci-common/bin` 을 사용했다.

## 5. 미검증 / 잔여 위험

- **증거 묶음 zip 내부**: 묶음이 만들어지지 않아서 zip 안의 파일은 볼 수 없었다. "묶음에 `backend.json` 이 들어간다" 는 코드상 사실(`CreateFromDirectory(evidence/<RunId>)`)일 뿐 실측하지 않았다.
- **활성 스터디가 있는 경우**: 스터디를 불러온 상태의 판정 흐름은 측정하지 않았다. 이 경우 `verdicts.json` 에 판정이 기록된다.
- **menu-command-report 저장 위치**: 앱 실행 폴더(`AppContext.BaseDirectory`)에 쓰여서 실행마다 덮어쓴다. 여러 실행의 보고서를 따로 남기려면 호출자가 복사해야 한다.
- **실제 모드 판정 기준**: C-82 와 같은 `BackendName` 문자열 판정이다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 통제 (3) 보고서 3종 실측 | GUI-C-85 (이 카드) |
| `RunId` 가 설정되지 않아 증거 묶음 내보내기 실패 | 신규 — 설계 결정 (lead) |
| `WorkflowScenarios.cs:225` 프리셋 기대값 | 다음 카드 (B-82 병합 후) — 손대지 않음 |
