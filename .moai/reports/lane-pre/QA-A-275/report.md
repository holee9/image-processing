[2026-10-21 17:24 KST] [xpe-pre]

# QA-A-275 — 증거 patch 의 줄 끝 공백으로 인한 text-lint 실패 해소

## 1. 원인

CI `39561227782` 의 `text-lint` 는 추적되는 `.txt` 파일의 줄 끝 공백을 오류로 본다(`tools/ci/Test-TrackedTextFiles.ps1`). 증거 커밋 `6cdf1195` 의 `*.patch.txt` 에는 원본 소스의 줄 끝 공백이 문맥 줄로 그대로 들어 있어 46건(2-4 v2 37, 2-5 v1 4, 2-5 v2 5)이 걸렸다. 공백을 지우면 patch 와 해시가 깨진다.

## 2. 변경 (`.moai/reports/lane-pre/QA-A-274/patches/` 한정, 검사 규칙·제품 코드 무변경)

- 추적에서 제거한 파일 5개: `2-3-c1a`, `2-4-b1`, `2-4-b1.v2`, `2-5-dualwrite`, `2-5-dualwrite.v2` 의 `.patch.txt`.
- 추가한 파일 6개: 같은 바이트를 gzip(mtime=0, 결정적) 후 base64(76자 줄)로 인코딩한 `*.patch.gz.b64.txt` 5개, 원본 sha256 목록 `SHA256SUMS.txt`.
- `.gitignore` 가 `.moai/reports` 아래에서 `md`·`txt` 만 추적하므로 `.gz` 대신 `.gz.b64.txt` 로 했다. base64 줄에는 공백·제어 문자가 없다.
- 원본 `.patch.txt` 5개는 작업 트리에서도 지웠다(적용 시험 시 저장소 안에 untracked 로 남아 검사에 걸리지 않게 하기 위함). 바이트는 아래 해시 검증으로 보존되며, 지우기 전 사본을 세션 임시 폴더에 두었다.

복원: `python -c "import base64,gzip,sys;sys.stdout.buffer.write(gzip.decompress(base64.b64decode(open(sys.argv[1]).read())))" <이름>.patch.gz.b64.txt > <이름>.patch.txt`

## 3. 해시 확인 (커밋 대상 index 의 blob 을 풀어 sha256 계산)

| patch | sha256 | SHA256SUMS 일치 |
|---|---|---|
| 2-3-c1a | `e2fab163…b7452` | 일치 |
| 2-4-b1 (v1) | `1c4a75b2…b9ed` | 일치 |
| 2-4-b1.v2 | `13cb71da…d9f` | 일치 |
| 2-5-dualwrite (v1) | `43880776…cae9` | 일치 |
| 2-5-dualwrite.v2 | `066930ba…8845f` | 일치 |

Reviewer #261 이 인용한 세 값(2-3, 2-4 v2, 2-5 v2)이 풀었을 때 그대로 나온다.

## 4. 검사 (로컬, CI 와 같은 스크립트)

- `powershell -File tools/ci/Test-TrackedTextFiles.ps1`(스크립트가 `-Encoding UTF8` 로 명시 읽음), index 기준 전체 추적 파일: 종료 코드 0, "Tracked text file validation passed."
- 주입 대조: 한글 뒤 줄 끝 공백이 든 `.txt` 하나를 index 에 넣고 같은 스크립트를 실행 → 종료 코드 1, 해당 파일 1건 검출. 이후 index 와 작업 트리에서 제거(`git status` 로 확인).

## 5. 한계

- 로컬에는 PowerShell 7(`pwsh`)이 없어 Windows PowerShell 5.1 로 돌렸다. 스크립트가 UTF-8 을 명시하므로 디코더는 CI 와 같다. CI 의 실제 결과는 push 후 확인해야 한다.
- 이 변경 이전에 추적에 들어간 다른 증거(2-3~2-7 보고서 등)는 이 스크립트를 통과했다(위 0건). untracked 증거 일부에는 UTF-8 이 아닌 파일이 있어 추적에 넣을 때 따로 확인해야 한다.
- push 하지 않았다.
