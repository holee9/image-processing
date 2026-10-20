[2026-10-21 08:35 KST] QA-A-273 보고서 — 2G·신규 PRE 개발용 비교 창 (0번 기준선 + 2번 가져오기 최소판), Refs #278

## 0. 한눈에

- 범위는 두 엔진뿐입니다. A = 공식 Samsung SDSDK 2.3.0 B03 X64W, B = 신규 PRE. TAS 는 어디에도 넣지 않았습니다.
- 이 창은 이미 존재하는 출력 파일 두 개를 가져와 같은 조건으로 보여 주고 A−B 수치를 계산합니다. 제조사 DLL 을 불러오거나 실행하지 않았고, 장비를 만지지 않았고, 화질을 판정하지 않습니다.
- 검증과 계산은 C# 으로 다시 구현하지 않았습니다. 리더 지시(2026-10-21, Reviewer #249·#250)에 따라 계약 스크립트(`comparison_manifest.py`, 스키마 `xpe-2g-pre-comparison/4`, main 로컬 커밋 `47d932e3`)를 자식 프로세스로 실행하고 그 JSON 만 읽습니다.
- Reviewer #251 이 계약 4판을 수용했다는 리더 연락(08:21)을 받고 호출부까지 이번 커밋에 포함합니다. 수용 범위는 계약 도구뿐이며 G-IMPORT 완료·GUI·실제 2G·화질 승인이 아닙니다.

## 1. 구현한 것

| 항목 | 내용 | 위치 |
|---|---|---|
| 0번 기준선 | 워크트리 480건, 빌드 입력 SHA-256, 수정 전 사본, 명령·바인딩·AutomationId 목록 | 커밋 `54f6859b`, `run-20261020T230045Z/` |
| 메뉴 항목 | 도구 메뉴에 "2G · 신규 PRE 출력 가져오기 비교" 한 줄(+4줄 xaml, +3줄 코드) | `gui/ImageProcTest/MainWindow.xaml`, `MainWindow.xaml.cs` |
| 자식 프로세스 클라이언트 | 인자 목록 호출, stdout·stderr 동시 읽기, UI 스레드 비차단, 10분 시간 제한, 취소 시 자식 트리 종료 | `Services/Comparison/ComparisonBackend.cs` |
| 결과 수락 규칙 | 새 실행 폴더(요청 전에 없던 폴더, 출력 폴더 바로 아래)·`run_id`=폴더 이름·`result.json` 의 매니페스트가 보낸 매니페스트와 동일·`schema` 가 `/4`·`status` 가 알려진 값이며 스크립트가 말한 상태와 같고 종료 코드와 일치할 때만 수락. 어느 하나라도 틀리면 "실패" | 같은 파일 |
| 검토된 계약 표시 | 검토 통과 SHA-256 두 값(커밋 내용 LF `86104cc6…`, 리더 작업 사본 CRLF `e9e3850c…`; 내 사본에서 두 값 모두 재계산해 일치 확인)과 **실제로 계산한 해시**가 같을 때만 "검토된 계약". 다르면 모든 답에 "검토 안 된 계약" 을 붙이고 통과처럼 보이지 않게 함. 계산한 해시는 `gui-run-record.json` 에 기록 | `ComparisonContractReview` |
| 실행 기록 | 실행마다 스크립트 절대경로·SHA-256·`python --version`·명령 인자·종료 코드를 `gui-run-record.json` 으로 새 파일에 기록 | `ComparisonBackend.cs` |
| 창 | 스크립트 경로(기본값 없음, 마지막 값 기억)·매니페스트·산출물 폴더, 스크립트의 답과 사유 코드(한국어 설명 병기), 공통 W/L·공통 확대·같은 좌표 ROI, A−B 통계 계산·저장, 비활성 "실제 2G 실행"(G-2G-READY 사유 표시) | `Views/ComparisonImportWindow.cs`, `ComparisonReasonText.cs` |
| 표시 전용 읽기 | 스크립트가 "비교 가능"이라 한 뒤에만, 매니페스트의 배치 필드로 줄여 그린 복사본 생성. 숫자는 여기서 나오지 않음 | `ComparisonDisplayReader.cs` |
| 보관(빌드 제외) | 2판용 C# 이식본과 43건 대조 시험(50/50 통과) | `archive/csharp-contract-port-v2/*.txt` |

## 2. 시험한 것 (관측한 출력)

### 2.1 기존 동작 고정 (명령·바인딩·AutomationId)

`python check_inventory_preserved.py run-20261020T230045Z` → 기준선 580건 중 **사라진 것 0건**, 추가 3건(메뉴 항목의 AutomationId·Click·Header). 대조군: 기준선 항목 하나를 일부러 빼면 "사라짐 1건"으로 읽힘.

### 2.2 새로 빌드한 개발 GUI 를 실제로 띄워 UI Automation 패턴으로만 구동

- 빌드: `dotnet build gui/ImageProcTest/ImageProcTest.csproj --artifacts-path builds/gui1 …` → 오류 0개. 배포본·build7·실행 중인 GUI 는 건드리지 않음.
- 실행은 두 번(최종 빌드): 검토된 스크립트(PID 10520, `run-gui-contract-v4-20261020T232226Z/`)와 대조군인 한 줄 덧붙인 스크립트(PID 30156, `run-gui-contract-v4-modified-20261020T232243Z/`). 보호 PID 1868·17364·19184 는 구동하지 않았고 실행 뒤에도 그대로 남아 있음, 이번 두 PID 는 종료 확인(`exited: True`). 전역 키보드·마우스 입력 없음(Invoke·Value·ExpandCollapse·Window 패턴만).
- 실행 증거 폴더마다 — 화면 6장(`01`~`06`.png), `observations.json`(PID, exe 경로와 SHA-256 `BF940CD5…`, 관리 DLL SHA-256, 네이티브 파일 8개 SHA-256, 스크립트 SHA-256), `drive.log.txt`.
- 관측한 동작:

| 단계 | 관측 |
|---|---|
| 처음 | 상태 문구 "가져온 파일 비교이며 2G 실행이 아닙니다", 실제 2G 실행 버튼 비활성, 사유(G-2G-READY) 표시, 스크립트 경로 기본값 비어 있음 |
| 스크립트 경로 비움 | "실패 — 통과가 아닙니다", 계산 버튼 비활성 |
| 합성 비교 가능 쌍 | 검토된 스크립트: "스크립트의 답: 비교 가능 · common-condition"(꼬리표 없음). 한 줄 덧붙인 스크립트: 같은 답이지만 모든 답에 "[검토 안 된 계약]", 스크립트 경로·SHA-256·Python 3.12.10 표시, 같은 좌표 ROI(x=16,y=8,48×32)가 A·B 양쪽에 |
| W/L 400/1500, 100% | 두 영상에 같은 W/L 적용(화면 03) |
| 계산·저장 | computed, 유효 화소 1536, 평균 −9, 표준편차 1.3844, RMSE 9.1059, 최솟값 −11, 최댓값 −7, 원본 변경 없음, 2G 실행됨 "아니오", 화질 판정 "하지 않음" |
| 경로를 바꾸면 | 결과·판정이 즉시 비워지고 계산 버튼 비활성(낡은 결과 차단) |
| SHA 불일치 쌍 | 비교 불가, `SHA_MISMATCH`, 계산 버튼 비활성 |
| 설정 미확인 쌍 | 비교 불가, `SETTINGS_UNVERIFIED`, 계산 버튼 비활성 |

### 2.3 파이썬 직접 실행과의 대조 (Reviewer #250 의 7번 항목 일부)

같은 합성 쌍을 스크립트로 직접 `compare` 한 결과(`py-direct-results/`)와 GUI 가 만든 `result.json` 을 비교: status·reasons·valid_pixels·평균·표준편차·RMSE·평균 절댓값·최솟값·최댓값·sources_unchanged·validation 전부 일치(`actual_2g_executed` 는 둘 다 False).

## 3. Reviewer #250 이 GUI 쪽에 넘긴 7개 검수 항목 대응

| # | 항목 | 상태 |
|---|---|---|
| 1 | interpreter·script·schema 고정과 기록, 인자 목록 호출 | 구현·관측(기록 파일). NumPy 버전은 기록하지 않음 → 미검증 |
| 2 | imported-file 만 호출, 원본 읽기 전용, 고유 run 경로, manifest·run ID 결합 | 구현. 산출물 폴더가 매니페스트 폴더 안이면 거부(코드). 거부 동작 자체는 화면 시험 안 함 → 미검증 |
| 3 | exit code·파일 존재만으로 성공 처리 금지, 새 JSON·schema·status·reasons 검증 | 구현(§1). 결과 거부 분기(schema 다름·run_id 다름·매니페스트 다름)는 **의도적으로 깨뜨려 본 시험 없음** → 미검증 |
| 4 | W/L·확대가 통계를 바꾸지 않음, 같은 좌표 ROI | 화면에서 W/L 변경 뒤 통계가 스크립트 직접 실행과 같음을 확인(§2.3). 물리 좌표 확인은 계약 범위 밖 |
| 5 | UI 비차단, 교착 회피, 취소·시간 제한 뒤 낡은 결과 차단 | 구현(동시 읽기·세대 번호·10분 제한). **취소와 시간 제한을 실제로 일으켜 본 시험 없음** → 미검증 |
| 6 | 파일 수명 유지 | 스크립트가 실행 전후 SHA 를 비교(계약 기능). GUI 가 입력을 잠그지는 않음 → 미검증 |
| 7 | 같은 긍정·부정 픽스처 대조 | 긍정 1쌍·부정 2쌍(§2.2)과 직접 실행 대조 1쌍(§2.3). 다른 사유 코드 전부는 대조하지 않음 → 부분 |

## 4. 검증하지 않은 것 (미검증)

- 실제 2G 출력 쌍은 없습니다. 이번 입력은 전부 **합성 영상**입니다. 합성에서 성공한 것은 2G 실행의 증거도, 화질의 증거도 아닙니다.
- 수용된 스크립트와 해시가 같은 경우만 "검토된 계약"이며, 그것은 계약 도구의 수용일 뿐 GUI·2G·화질의 승인이 아닙니다.
- 위 §3 의 "미검증" 항목들(NumPy 버전 기록, 거부 분기 깨뜨리기, 취소·시간 제한 실제 시험, 산출물 폴더 거부 화면 시험).
- 스크립트의 사유 코드 전부에 대해 GUI 표시를 확인하지는 않았습니다. 모르는 코드는 원문 코드와 "이 창이 모르는 사유 코드" 문구로 표시됩니다.
- 큰 영상(수천×수천)의 표시 줄임과 성능은 이번 합성(96×64)으로 보지 못했습니다.
- 기존 시험 모음(CalibrationChecks, SelfCheck, E2E)은 돌리지 않았습니다. 이번 변경이 건드리는 것은 메뉴 항목 한 줄과 새 파일뿐이며, 기존 항목 보존은 §2.1 로 확인했습니다. 전체 시험은 CI 몫입니다.

## 5. 알림

- 2판용 C# 이식본은 파이썬과 같은 답을 내는지 43건으로 대조해 50/50 통과했고, 그 대조가 uint32 정밀도 버그를 1건 잡았습니다(분기마다 자료형이 달라 float 으로 합쳐짐). 재구현 축소 방향이 정해져 GUI 에는 넣지 않고 보관만 했습니다. 같은 버그를 막는 `(double)` 변환은 표시용 읽기에 남아 있습니다.
- 커밋에는 새 GUI 소스(`gui/ImageProcTest/Services/Comparison/*`, `Views/ComparisonImportWindow.cs`), HEAD 기준으로 다시 만든 메뉴 6줄, 이 폴더의 텍스트 증거가 들어갑니다. 첫 커밋 `b4c690b3` 은 커밋되지 않은 `GuiDataPaths.cs` 를 참조해 HEAD 단독으로는 빌드되지 않았고(리더 지적), 이번에 `ComparisonPaths.cs` 로 필요한 만큼만 새 파일에 두어 해소했습니다. 메뉴 6줄은 작업 트리를 바꾸지 않고 `git apply --cached` 로 index 에만 올렸습니다(`my-hunks-on-head.patch.txt`, 커밋 전후 작업 트리 두 파일 sha256 비교는 §6). 빌드 산출물·PNG·생성 데이터는 `.moai/reports` 무시 규칙에 따라 저장소 밖(로컬 보관)입니다.
- 합성 픽스처 생성기 `make_v4_fixtures.py` 와 구동 스크립트 `Drive-ComparisonWindow.ps1` 은 같은 폴더에 있습니다(구동 스크립트는 한글 때문에 BOM 이 필요합니다).
- 리더의 `collaboration-ledger.md`(untracked)에 "GUI writer = xpe-pre" 한 줄을 추가했습니다.

## 6. 2차 커밋(리더 요청 08:27) 추가 검증

### 6.1 HEAD 단독 빌드 (`602eff24`)

첫 커밋 `b4c690b3` 은 커밋되지 않은 `GuiDataPaths.cs` 를 참조해 HEAD 만으로는 빌드되지 않았습니다(리더 지적, 작업 트리 빌드로만 시험해서 드러나지 않았음). `ComparisonPaths.cs` 로 필요한 규칙만 새 파일에 두고, 메뉴 6줄은 HEAD 의 `CalibrationSettingsMenuItem`·`ExitMenuItem_OnClick` 앞에 넣은 패치(`my-hunks-on-head.patch.txt`)를 `git apply --cached` 로 index 에만 올렸습니다.

- `git archive HEAD gui clients` 를 `builds/headbuild-602eff24/` 에 풀어(처음엔 `clients/` 를 빠뜨려 CS2001 오류 7개 — 아카이브 범위의 실수였고 소스 문제가 아님) CI 와 같은 세 프로젝트를 Debug 로 한 번에 하나씩 빌드: ImageProcTest 종료 코드 0·오류 0개, SelfCheck 0·0개, E2E 0·0개(`headbuild-*.log.txt`).
- SelfCheck 실행: 종료 코드 0, "Lifetime scenarios passed (30 scenarios)", "GUI-S0 self-check passed"(`headbuild-selfcheck.run.txt`). 참고: SelfCheck 의 픽스처 경로 출력이 작업 트리의 `gui/ImageProcTest/fixtures/…` 를 가리켰습니다. 픽스처 파일 자체는 HEAD 에도 있으므로 결과 판단에는 영향이 없다고 보지만, 어느 쪽을 읽었는지는 확인하지 않았습니다 → 미검증.
- E2E 는 CI 가 실행하지만 요청 범위(빌드 3개 + SelfCheck 실행)가 아니라 실행하지 않았습니다.
- 커밋 전후 작업 트리의 `MainWindow.xaml`·`.xaml.cs` sha256 이 같음(`sha256sum -c` 두 줄 OK, `worktree-sha-before-commit2.txt`), 커밋 전후 `git status` 목록은 내 새 파일의 상태 변화(`M`/`??` → 커밋됨)만 다름. 실행 뒤 ImageProcTest 프로세스는 보호 PID 1868·17364·19184 만 남음.

### 6.2 미검증이던 두 가지를 실제로 시험

스크립트 한 줄만 일부러 바꾼 복사본 4개(`stub-scripts/`, 원본은 `contract-v4/` 의 `comparison_manifest.py`)로 같은 창을 UIA 로 구동했습니다. 예측(먼저 적음): 세 거부 분기는 "실패 — 통과가 아닙니다"와 해당 사유, 취소는 실패 표시와 자식 프로세스 0개.

| 바꾼 줄 | 창에 보인 결과 |
|---|---|
| `schema=SCHEMA` → `/3` | "result.json 의 schema 가 …/4 가 아닙니다(…/3). 실패로 처리합니다." |
| `run_id=run_dir.name` → `"other-run"` | "run_id(other-run)가 실행 폴더 이름과 다릅니다. … 거부합니다." |
| `manifest=manifest` → 한 필드 변경 | "result.json 에 기록된 매니페스트가 이번 요청에서 보낸 매니페스트와 다릅니다. 거부합니다." |
| 시작에 `time.sleep(120)` + 창에서 '취소' | 계산 중에 취소 버튼 활성·계산 버튼 비활성 → 취소 뒤 "취소했거나 시간 제한에 걸렸습니다 … 실패로 기록하며 통과가 아닙니다", 상태 문구 "계산 실패", 남은 파이썬 프로세스 0 |

세 거부 분기는 이 시험 전에는 한 번도 일어나 본 적이 없었으므로 위 결과가 첫 관측입니다. 10분 시간 제한 자체는 기다려 보지 않았습니다(취소와 같은 경로라고 코드로 읽었을 뿐) → 미검증으로 유지.

### 6.3 최종 빌드로 다시 구동

최종 작업 트리 빌드(`builds/gui1`)로 검토된 스크립트 구동 1회 더(`run-gui-final-20261020T233339Z/`): 비교 가능·computed 동일.

### 6.4 증거 텍스트의 줄 끝 공백

저장소의 텍스트 검사(`tools/ci/Test-TrackedTextFiles.ps1`)가 이미 커밋된 두 패치 기록(`my-hunks*.patch.txt`)의 빈 문맥 줄(공백 한 칸)을 줄 끝 공백 오류로 읽었습니다(오류 2건). 로컬에서 커밋 전에 돌린 검사는 추적되는 파일만 보아 새 파일을 놓쳤습니다. 해당 줄을 `(blank context line)` 표기로 바꿔 검사를 통과시켰고, 이 두 파일은 기록용이라 그대로는 `git apply` 되지 않습니다(실제로 index 에 적용한 패치는 커밋 `602eff24` 의 diff 입니다).
