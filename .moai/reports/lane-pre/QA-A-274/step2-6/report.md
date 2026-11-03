[2026-10-21 16:12 KST] [xpe-pre]

# QA-A-274 2-6 — Reviewer #260 보류 대응 (F260-1 수정 patch)

카드 QA-A-274d. 커밋·stage·push 는 하지 않았다. 작업 트리 dirty 는 건드리지 않았고(`git diff --cached` 0건, 추적 수정 29건 그대로), 적용·빌드·시험은 모두 임시 사본 `D:\xpe-h274j` 에서만 수행했다. 기준 HEAD `33b235ece0dadc5307a65947322cc3a17a45d4fd`.

## 1. patch 해시 (sha256)

| 파일 | sha256 | 비고 |
|---|---|---|
| `patches/2-3-c1a.patch.txt` | `e2fab16382d5cd36c3fdd79222711fef61938b8bf9ccf0b5384d8d01608b7452` | 변경 없음 |
| `patches/2-4-b1.v2.patch.txt` | `13cb71da7b619ec35bfad77a9e1fed8cff1a39955252c074bb2c9fadda712d9f` | 새 파일 |
| `patches/2-5-dualwrite.v2.patch.txt` | `066930ba5cd13c28220f8ba529f3d88151c2d4e7a1402294083bd94a8720845f` | 새 파일 |
| 옛 `2-4-b1.patch.txt` / `2-5-dualwrite.patch.txt` | `1c4a75b2…b9ed` / `43880776…cae9` | 보존, 해시 불변 |

생성기: `make_v2.py.txt` (입력은 `expected-states.json`, 출력 `expected-states-v2.json`).

## 2. F260-1 수정 내용

원인: `LastEvidenceSnapshotPath` 는 직전 성공 경로를 보존하도록 만들어졌으므로, 스냅샷 쓰기가 실패한 뒤에도 이전 파일을 가리킨다. 자동화(MainWindow)와 SelfCheck 는 그 경로의 존재만 보고 성공으로 판정했다.

수정(주석이 아니라 동작 변경):
- 새 파일 `Services/EvidenceExportResult.cs`: 내보내기 시도 한 번의 결과(내보내기 고유 식별자 `ExportId`, 이번 시도의 스냅샷 경로 또는 오류). 2-5 에서 고정 별칭 결과(`FixedCopyPath`, `FixedCopyError`, `FixedCopyUpdated`)가 더해진다.
- 뷰모델 `LastEvidenceExport`: 성공이든 실패든 시도마다 새 결과를 남긴다. `LastEvidenceSnapshotPath` 는 의미를 유지한다(직전 성공 증거 보존).
- 판정 함수 `EvidenceExportResult.NewSuccessPath` / `RequireNewSuccess`: 명령 전에 본 결과와 비교해 (a) 새 내보내기가 돌지 않았거나 (b) 실패했거나 (c) 파일이 없으면 실패로 판정한다.
- 소비자 교체: MainWindow 자동화(`MenuCommandReportCreated` 등 판정), SelfCheck `ReportRoot` / `ReportDisplayPipeline` 이 모두 이 판정 함수를 쓴다.
- 식별자는 내보내기 단위(`Guid`)다. `RunSet.RunId` 는 쓰지 않았다.
- 별칭만 실패한 경우: 스냅샷 성공으로 판정하되 `FixedCopyUpdated=false`, `FixedCopyError` 와 StatusText(`NOT updated`)로 구분된다.

## 3. 시험 (임시 사본 `D:\xpe-h274j`, 동시 1개)

| 항목 | 결과 |
|---|---|
| 적용 | 2-3 → 2-4 v2 → 2-5 v2 모두 `git apply` 성공(`core.autocrlf=false`) |
| 임시 사본 대조 | 24개 파일 기대 상태와 바이트 불일치 0 |
| 빌드 CalibrationChecks / E2E(컴파일만) / SelfCheck | 종료 코드 0 / 0 / 0, 경고 22 / 0 / 1, 오류 0 |
| CalibrationChecks | 종료 코드 0, PASS 6개, `failures=0` |
| SelfCheck | 종료 코드 0, "Lifetime scenarios passed (29 scenarios)", "GUI-S0 self-check passed" |
| 별칭 = 최신 스냅샷 | 바이트 동일, 별칭이 최신 스냅샷을 가리킴, 스냅샷 30개, 보존 복사 0개 |

CalibrationChecks 6개: 기존 2개(Reload, Stage selection) + 2-5 의 2개(이전 별칭 바이트 보존, 한쪽 쓰기 실패 순서) + **새 2개**.
- "A failed export is judged as a failure, not as the previous export's success": **뷰모델 수준**. 같은 VM 에서 첫 내보내기 성공 → 스냅샷 폴더에 "파일 만들기 거부" ACL 을 걸어 두 번째 내보내기 실패(이전 스냅샷은 제자리에서 읽힘) → 소비자 판정 함수가 실패로 판정, 이전 성공 경로·바이트 보존, 새 내보내기 없음도 거부.
- "When only the fixed copy fails…": 별칭 이름에 폴더를 두어 별칭만 실패 → 스냅샷 성공 판정 + 고정 복사 미갱신 보고.
- 점검을 완화한 것은 없다. 기존 점검 단언은 그대로다(2-5 의 변경된 단언은 이전 보고서 §대로).

## 4. 반증 (8개 변형, 각각 빌드 후 점검 실행, 모두 종료 코드 1)

| 변형 | 내용 | 실패한 점검 |
|---|---|---|
| A | 이전 별칭을 보존하지 않음 | 별칭 바이트 보존 / 한쪽 실패 순서 |
| B | 스냅샷 실패인데 별칭을 씀 | 한쪽 실패 순서 |
| C | 별칭이 엉뚱한 스냅샷을 가리킴 | 별칭 바이트 보존 / 한쪽 실패 순서 |
| D | 이름만 믿고 바이트 비교 생략 | 한쪽 실패 순서 |
| E | 중복 방지 제거 | 별칭 바이트 보존 / 한쪽 실패 순서 |
| **F** | 뷰모델: 실패한 내보내기가 직전 성공 경로를 자기 결과로 가짐(F260-1 되살림) | **새 뷰모델 점검**: "failed export did not leave its own failed result" |
| **G** | 판정: 새 내보내기가 없는 결과도 성공으로 인정 | 새 뷰모델 점검: "A result that no new export produced was judged a success" |
| **H** | 판정: 이번이 실패하면 직전 결과 경로로 대체(옛 동작) | 새 뷰모델 점검: "A failed export was judged a success (the previous file still exists)" |

원본 로그: `falsify.txt`, `falsify-*-run.log.txt`. 변형마다 새 출력 폴더(고정 보고서 0개)에서 실행했다.

## 5. 기준 상태 두 가지 (`baselines.json`)

(1) 원래 480 보존: 480항목 중 디스크에서 사라진 것 0, 453은 여전히 status 에 있음, 27은 2-1·2-2 커밋으로 status 에서 빠짐.
(2) 현재 501 스냅샷: 빌드 입력 438개 sha256 모두 동일(변경 0, 누락 0). 501행 중 474 유지, 27 커밋으로 이동, 그 외 사유로 사라진 행 0. 스냅샷 이후 새 status 행 132개(`.moai/reports/lane-pre/QA-A-274/` 하위 산출물이 대부분).

참고: step2-2 의 dirty 해시 기록과 비교하면 `.moai/harness/usage-log.jsonl`, `.moai/lessons-inbox.jsonl` 2개가 바뀌었다. 하네스가 자동 기록하는 로그이며 제품 입력이 아니다.

## 6. 남은 프로세스

`dotnet build-server shutdown` 후 dotnet/msbuild/컴파일러/시험 프로세스 0개(`processes-after.txt`). 보호 GUI PID 는 조작하지 않았다.

## 7. 한계와 미검증

- F260-2(P2, 비차단): 출력 폴더 문자열 방어(`Contains(token)`)는 격리를 입증하지 못한다. 격리 근거는 별도 임시 출력 폴더, 시작 전 고정 보고서 0개, 임시 사본 일치다. 설계 변경 없음.
- Lane C FlaUI E2E 는 실행하지 않았다(GUI 조작 시험). 소스만 읽었고 `clients/` 는 바꾸지 않았다.
- 실패 주입은 ACL 거부(스냅샷 폴더)와 이름 점유(별칭)다. 설치 디렉터리 읽기 전용, 디스크 가득 참은 시험하지 않았다. 보존 복사 자체가 실패하는 경로는 여전히 코드만 있고 실패 주입 시험이 없다.
- ACL 거부 시험은 이 PC(Windows, 현재 사용자 SID)에서만 확인했다. CI 러너(관리자 권한)에서 명시적 거부 항목이 같은 결과를 내는지는 관측하지 않았다.
- 모두 Mock 백엔드에서 실행했다. Native 백엔드 내보내기, 전체 suite, CI 는 실행하지 않았다.
- C++ `xpe_calib_fixture_gen` 은 이번 수정과 무관하여 다시 빌드하지 않았다(2-5 보고서의 결과가 유효, 해당 파일 변경 없음).
- 임시 폴더 `D:\xpe-h274e` … `D:\xpe-h274j` 는 남아 있다(리더 지시가 있을 때 정리).
