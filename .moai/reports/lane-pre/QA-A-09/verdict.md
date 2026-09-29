# QA-A-09 — Doxygen 그룹 정의 누락 수정 (#114, Class A)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-09   **Refs**: #114

## 1. 주장 (Claim)

`@ingroup xpe_preprocess` 에 대응하는 `@defgroup` 이 없어 Doxygen 이 경고를 냈고,
`WARN_AS_ERROR = FAIL_ON_WARNINGS` 때문에 exit 1 이 됐다.
`mode_selector.h` 에 `@defgroup xpe_preprocess` 블록 1개를 추가해 해소했다.

## 2. 증거 (Evidence)

### 2.1 변경 diff — 1파일, 주석 블록 1개

```
$ git diff --stat
 modules/preprocess/include/xpe/preprocess/mode_selector.h | 9 +++++++++
 1 file changed, 9 insertions(+)
```

배치 위치: 파일 최상단 `@file` 블록 직후, `#ifndef` 직전.
참조 형식은 `modules/common/include/xpe/common/xpe_types.h:39-45` 의 `@defgroup xpe_common`.

**`@{` / `@}` 는 의도적으로 넣지 않았다.** `xpe_common` 은 블록 형태(`@{`…`@}`)로
멤버를 묶지만 `xpe_preprocess` 는 멤버가 개별 `@ingroup` 으로 참여한다.
`@{` 만 넣고 `@}` 를 빠뜨리면 새 경고가 생긴다. 실측:

```
$ grep -c '@{' modules/preprocess/include/xpe/preprocess/mode_selector.h
0
$ grep -c '@}' modules/preprocess/include/xpe/preprocess/mode_selector.h
0
```

### 2.2 그룹 정의/사용 대조 (저장소 전체)

```
$ grep -rhoP '@ingroup\s+\K\S+' --include=*.h --include=*.hpp --include=*.c --include=*.cpp modules/ | sort -u
xpe_ai
xpe_common
xpe_dicom
xpe_enhance_advanced_internal
xpe_preprocess

$ grep -rhoP '@defgroup\s+\K\S+' ... | sort -u
xpe_common
xpe_dicom
xpe_preprocess          <- 이번에 추가
```

### 2.3 남은 미정의 2건이 경고를 내지 않는 이유 (Doxyfile 스코프 실측)

`xpe_ai`, `xpe_enhance_advanced_internal` 은 정의가 없지만 **Doxygen 입력 범위 밖**이다.

| 미정의 그룹 | 사용 파일 | 제외 근거 (Doxyfile) |
|---|---|---|
| `xpe_ai` | `modules/ai/include/xpe/ai/ai_api.h`, `ai_worker_protocol.h` | `INPUT` (27-33행) 에 `modules/ai/include` 가 **없음** |
| `xpe_ai` | `modules/ai/src/*.cpp`, `ai_ipc_bridge.h` | `INPUT` 에 `src/` 없음 + `FILE_PATTERNS = *.h` (35행) |
| `xpe_enhance_advanced_internal` | `modules/enhance_advanced/include/xpe/enhance_advanced/internal.h` | `EXCLUDE_PATTERNS = *internal*` (39행) 에 매칭 |

## 3. baseline 귀속

- 트리: `dev/preprocess` @ `040d88f` (작업 전)
- `git merge-base --is-ancestor HEAD origin/main` → **YES**
- `git rev-list --count --left-right origin/main...HEAD` → `9  0` (로컬 고유 커밋 0건)
- `git diff --stat HEAD origin/main -- modules/preprocess/include modules/common/include/xpe/common/xpe_types.h docs/help/doxygen/` → **빈 출력**
  → 대상 파일이 `origin/main` 과 동일하므로 merge 선후와 무관하게 수정 내용이 같다.

## 4. Gaps (미검증)

- **로컬 `doxygen` 실행 못 함 — 도구 부재.** 카드 지시대로 설치하지 않았다. 실측:
  - `command -v doxygen` → 없음
  - `where doxygen` (cmd) → 파일 없음
  - `C:\Program Files\doxygen*`, `C:\Program Files (x86)\doxygen*`, chocolatey/scoop shim → 전부 없음
  → **exit 0 / 경고 0 은 검증되지 않았다.** CI `Documentation Generation` 이 최종 판정한다.
- `git merge main` **미실행** — `CLAUDE.local.md` 가 사용자 승인 대기로 금지. 사용자에게 상신함.
- 이 수정이 CI 실패의 **유일한** 원인인지는 미검증. Doxygen 을 못 돌렸으므로
  다른 경고가 남아 있을 가능성을 배제하지 못한다.

## 5. 잔여 위험

- `WARN_IF_UNDOCUMENTED = YES` + `EXTRACT_ALL = NO` 조합에서, 새 `@defgroup` 이 문서화되지
  않은 멤버를 새로 노출시켜 **다른 경고를 유발할** 가능성. `@brief` 를 붙여 그룹 자체는
  문서화했으나, 그룹 페이지에 딸려 들어오는 항목까지는 확인하지 못했다.
- 카드의 근거 서술 정정: 카드는 다른 모듈이 "`@ingroup` 을 안 써서 경고 없음"이라 했으나
  실제로는 **쓰고 있고**, 경고가 없는 이유는 Doxyfile 입력 범위 밖이기 때문이다(§2.3).
  결론(추가 금지)은 같지만 근거가 다르다 — 향후 `modules/ai/include` 를 `INPUT` 에
  추가하면 그 순간 CI 가 다시 깨진다.
