[2026-10-21 16:20 KST] [xpe-pre]

# QA-A-274 2-7 — Reviewer #261 승인 patch 로컬 커밋 (카드 QA-A-274e)

push·merge 는 하지 않았다. patch 는 인덱스에만 적용했고(`git -c core.autocrlf=false apply --cached`) 작업 트리 파일은 쓰지 않았다. `git add` 로 작업 트리 내용을 stage 하지 않았다.

## 1. 시작 전 확인

- HEAD `33b235ece0dadc5307a65947322cc3a17a45d4fd`, 브랜치 `dev/preprocess`, `git diff --cached` 0건.
- patch sha256 재계산이 카드와 일치: 2-3 `e2fab163…b7452`, 2-4 v2 `13cb71da…d9f`, 2-5 v2 `066930ba…8845f`(전체 값은 카드·`step2-6/report.md` 참조).

## 2. 커밋 (patch 하나에 커밋 하나)

| 순서 | patch | 파일 수 | 커밋 SHA |
|---|---|---|---|
| 1 | `2-3-c1a.patch.txt` | 4 | `0f191eedee3f6c7ec242c21611c957c5c1c21b7e` |
| 2 | `2-4-b1.v2.patch.txt` | 22 | `a3a573a86582b232f724a301624e35fec7c4cef6` |
| 3 | `2-5-dualwrite.v2.patch.txt` | 4 | `f5f9316f1af4a4b1b7082730867f4207c9f304c1` |

각 단계에서 `apply --cached --check` 성공 → 적용 → `git diff --cached --name-only` 목록이 patch 의 `diff --git` 목록과 동일함을 확인(`staged-*.txt`, `patchfiles-*.txt`) → 커밋. 커밋 메시지에 카드, patch 이름, sha256, `Refs #278` 포함. 커밋 후 `git diff --cached` 0건.

## 3. 커밋 후 확인

| 항목 | 결과 |
|---|---|
| 작업 트리 불변 | 시작 전에 기록한 dirty 파일 100개의 SHA-256 이 커밋 후에도 100개 모두 동일(변경 0, 제외 항목 없음). 단 HEAD 가 바뀌어 `git status` 의 추적 수정 행은 29 → 21 로 줄었다(patch 로 커밋된 내용과 같은 파일이 더는 수정으로 보이지 않음. 파일 바이트는 변하지 않음) |
| HEAD 단독 트리 = 기대 상태 | `git archive HEAD` 를 새 폴더 `D:\xpe-h274k` 에 풀어 `expected-states-v2.json` 의 24개 파일과 비교: 바이트 불일치 0 |
| 빌드 CalibrationChecks / E2E(컴파일만) / SelfCheck | 종료 코드 0 / 0 / 0, 경고 22 / 0 / 1, 오류 0 |
| CalibrationChecks | 종료 코드 0, PASS 6개, `failures=0`(고정 보고서 0개인 새 출력 폴더에서 실행) |
| SelfCheck | 종료 코드 0, "Lifetime scenarios passed (29 scenarios)", "GUI-S0 self-check passed" |
| 남은 프로세스 | `dotnet build-server shutdown` 후 dotnet/msbuild/컴파일러/시험 0개 |

## 4. 기준 상태 (`baselines.json`, 두 가지를 따로)

(1) 원래 480 보존: 480항목 중 디스크에서 사라진 것 0. 435는 여전히 status 에 있고 45는 커밋(2-1·2-2·이번 3개)으로 status 에서 빠졌다. 바이트 삭제는 없다.
(2) 현재 501 스냅샷: 빌드 입력 438개 sha256 모두 동일(변경 0, 누락 0). 501행 중 456 유지, 45 커밋으로 이동, 그 외 사유로 사라진 행 0. 스냅샷 이후 새 status 행 154개(주로 `.moai/reports/lane-pre/QA-A-274/` 산출물).

## 5. 한계

- 이전 보고서(2-5, 2-6)의 한계가 그대로다: Lane C FlaUI E2E 미실행, ACL 거부 시험은 이 PC 에서만 관측, 보존 복사 실패 경로 미시험, Mock 백엔드만.
- 로컬 커밋 3개는 push 되지 않았다(`origin/dev/preprocess` 보다 앞선 커밋 8개). push·merge 는 리더가 한다.
- 임시 폴더 `D:\xpe-h274e` … `D:\xpe-h274k` 는 남아 있다(리더 지시가 있을 때 정리).

## 6. 증거 파일 메모

`.gitignore` 가 `.json` 을 무시하므로 인용한 JSON 은 `.json.txt` 사본으로 커밋한다: `patches/expected-states-v2.json.txt`, `step2-6/baselines.json.txt`, `step2-7/baselines.json.txt`, `step2-7/dirty-sha-before.json.txt`(원본 JSON 은 로컬에만 있고 내용은 사본과 같다).
