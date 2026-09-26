# GUI-C-86 보고 — RunId 를 실행 묶음 시작 때 정한다 (#178)

커밋(dev/gui, 미푸시): 73ca9a0

## 1. 실행 묶음의 시작점 — 코드에서 찾은 것

- **`StartedAt`**: `RunSetState` 에 선언만 있고 대입이 없었다. `gui/ImageProcTest` 의 `*.cs`·`*.xaml` 에서 `StartedAt` 을 검색하면 선언 1건뿐이다.
- **`RunSet` 인스턴스**: 뷰모델 필드 초기화(`private RunSetState _runSet = new();`) 한 곳에서만 만들어진다. setter 를 호출하는 곳은 없다.
- **"Run on all queued"**: `Log("RunOnAllQueuedCommand: not implemented (Slice 7).")` 만 한다.
- **판단**: 코드에 실재하는 실행 묶음의 시작은 `RunSetState` 가 만들어지는 때, 곧 **앱 실행 1회당 1번**뿐이다. 그래서 생성자에서 `StartedAt` 과 `RunId` 를 한 번 정했다.
  - 리더 결정("시작될 때 한 번")을 이 코드에 적용한 해석이다.
  - "Run on all queued" 가 구현되면 그 명령이 새 `RunSetState` 를 만들도록 하면 같은 규칙이 그대로 이어진다.
- **RunId 형식**: `yyyyMMdd-HHmmss-xxxxxx`. 앱 시작 지역 시각에 16진수 난수 6자리를 붙인 것이다. 실측 예: `20260917-123717-7aea47`.
- **가드**: `RunId` 가 비어 있을 때 동작을 막았다.
  - `RecordVerdict` 는 증거를 쓰지 않고 로그만 남긴다.
  - `ExportEvidenceBundle` 은 `Export failed: no run set has started.` 로 실패한다.

## 2. 시험과 결과

| 시험 | 수정 전 트리 (`red.txt`) | 수정 후 (`green.txt`) | 반증: `_runId = CreateRunId(...)` 끔 (`falsify.txt`) |
|---|---|---|---|
| EB-01 내보내기 → zip 안 `backend.json` 의 실제 모드 | 실패 — `Export failed: … bundles\.zip … being used by another process` | 통과 — `entries=[backend.json, verdicts.json]`, `actual=Mock requested=Mock` | 실패 — `Export failed: no run set has started.` |
| EB-02 실행 묶음 둘 → 서로 다른 RunId, 폴더 둘, 각자의 모드 | 실패 — `The first run set has no run id` | 통과 — `first=20260917-123722-fc44ae (Mock/Mock)`, `second=20260917-123806-6c3336 (Mock/Native)` | 실패 |
| EB-03 `evidence/` 바로 아래에 파일이 생기지 않음 | 실패 — `after=[backend.json, verdicts.json]` | 통과 — `after=[]` | 통과 (가드가 막음) |

- **Native 로 돈 경우** (`full-native/run-1.trx`):
  - EB-01: `actual=Native requested=Native`
  - EB-02: `first … actual=Native requested=Native; second … actual=Mock requested=Native` — 두 폴더의 **실제 모드가 서로 다르다**.
- **EB-02 의 두 번째 실행**: Native 요청 + 빈 DLL 폴더로 띄운 앱이다. 이 앱의 (요청, 실제) = (Native, Mock) 은 스위트가 어떤 백엔드로 돌든 고정이다. 그래서 두 폴더의 기록이 우연히 같아질 수 없다.

## 3. 회귀

- **Mock E2E** (`full-mock.txt`): 실패 0 / 통과 87 / 건너뜀 1.
- **Native E2E** (`full-native`, 로컬 DLL 83ffa7a): 통과 87, 실패 **W-07 1건**. C-87 에서 이미 알려진 결과로, 로컬 DLL 이 a4253be 이전 빌드라 옛 프리셋(−600/1600)을 돌려주기 때문이다. trx 의 Failed 2건 중 1건은 실행 전체 요약이다.
- **통합** (`full-int.txt`): 실패 0 / 통과 209 / 건너뜀 1.
- **빌드**: `BUILD_EXIT=0`. 수정 후(`build-green.txt`), 반증(`build-falsify.txt`), 복원 후(`build-restored.txt`) 모두 경고 0 / 오류 0.

## 4. 증거 폴더를 읽는 곳 — 검색 결과

- **검색 1**: `git grep -n -I "evidence" -- clients tools .github`. 결과에서 `EvidenceExported`·`Evidence Snapshot`·`comparison-evidence` 문구는 걸러냈다.
  - GUI 의 `evidence/<RunId>` 폴더나 `bundles/` 를 읽는 스크립트·시험은 **없었다**.
  - 걸린 것은 주석·문구뿐이다. `clients/ImageProcTest`(다른 앱)의 "Open evidence folder" 는 그 앱 자체의 기능이다.
- **검색 2**: `gui/ImageProcTest` 안에서 `"evidence"` 를 쓰는 곳은 `RecordVerdict` 와 `ExportEvidenceBundle` 두 곳뿐이다. "Open Evidence Folder" 메뉴는 비활성 자리표시자다.
- **`tools/`(lead 소유)**: 해당하는 곳이 없다.

## 5. 미검증 / 잔여 위험

- **증거 폴더 누적**: 판정을 기록한 앱 실행마다 `bin/.../evidence/<RunId>/` 가 하나씩 생긴다. 이번 시험 실행으로 10개 넘게 쌓였다. 정리 정책은 없다(이전에는 매번 같은 곳을 덮어썼다).
- **`verdicts.json` 은 `{}`**: 활성 스터디가 없기 때문이다(#178 본문 관측, 이 카드 범위 밖). 스터디를 불러온 상태의 판정 흐름은 여전히 측정하지 않았다.
- **RunId 충돌 가능성**: 같은 초에 두 앱이 시작되면 16진수 6자리 난수만으로 구별한다. 충돌 확률은 1/16,777,216 이다. 측정한 것이 아니라 설계상의 값이다.
- **실행 묶음 시작 = 앱 시작이라는 해석**: 앱 하나를 연 채로 백엔드를 바꿔도 같은 `RunId` 를 쓴다. 그래서 백엔드를 바꾸기 전과 후의 판정이 같은 폴더에 들어간다. 이때 `backend.json` 은 마지막 판정 시점의 백엔드로 덮어써진다.
- **CI 결과**: 없음.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| RunId 결정·증거 묶음 내보내기 | GUI-C-86 (이 카드) |
| 증거 폴더 누적 정리 정책 | 신규 카드 후보 |
| 앱 실행 중 백엔드 전환 시 `backend.json` 덮어쓰기 | 신규 카드 후보 (lead 판단) |
| W-21·22·23·25 새 DLL 확인 | GUI-C-88 (다음) |
