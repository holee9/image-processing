# QA-B-140 (#162) — 시험이 순서에 기댔고, **그 옆에 제품 결함이 있었습니다**

## 1. 주장 (Claim)

1. **(b) 먼저: 빨강은 정확히 1건입니다.** 12개 바이너리 675 시험을 한 프로세스씩
   돌려 셌습니다. **빙산이 아니었습니다.**
2. **(a) 방향은 둘 다입니다.** 시험이 "자기가 첫 번째로 돈다" 고 가정한 것이 맞고,
   **동시에 제품 결함이 있었습니다** — 리더가 추측하신 "억제가 새는" 것과는 다른
   자리입니다.
3. **억제 자체는 설계대로입니다.** 서명이 무효 키 **이름 집합**이고, `texture_gain` 의
   평면 철자와 `mfp` 중첩 철자는 **같은 집합**이라 **같은 한 문장**을 법니다. 두 번
   말할 이유가 없습니다.
4. **제품 결함은 그 옆에 있었습니다.** `multiscale_process` 가 호출을
   `if (levels <= 3)` 로 감싸, **헬퍼의 "올바른 설정이 기억을 지운다" 는 계약이 실행될
   기회 자체가 없었습니다.**
5. **실제 앱 영향**: 3단+`texture_gain` 으로 경고 → 4단으로 고침 → 다시 3단.
   **그 스레드는 영영 조용합니다.**

## 2. (b) 게이트가 못 보는 것 — 먼저 잰 수

고치기 **전**, 12개 바이너리를 각각 한 프로세스로:

| 바이너리 | 시험 수 | 결과 |
|---|---|---|
| `gsvg_tests` | 159 | 통과 |
| `test_xpe_enhance_advanced` | 237 | **1건 실패** |
| `xpe_enhance_basic_tests` | 113 | 통과 |
| `test_xpe_common` | 69 | 통과 |
| `test_display_*` (7개) | 85 | 통과 |
| `test_e2e_post_pipeline` | 12 | 통과 |
| **합계** | **675** | **1건 실패** |

같은 시점 `ctest ci-post` 는 **675/675 초록**이었습니다.

**이 하나였습니다.** 이 작업의 크기를 정하는 수이고, 작습니다.

## 3. (a) 무엇에 기대고 있었나 — 코드

### 억제 규칙

`warn_inert_keys_once`(`enhance_advanced_helpers.cpp`)는 **present 한 무효 키 이름을
정렬해 이어 붙인 서명**을 스레드별로 기억하고, 같으면 침묵합니다.

`texture_gain` 은 평면(`{"num_levels":3,"texture_gain":2}`)이든 중첩
(`{"mfp":{"num_levels":2,"texture_gain":2}}`)이든 서명이 `texture_gain\x1f` 로
**동일**합니다. 그래서 **두 번째로 도는 시험이 침묵을 봅니다.**

`InertKey_TextureGainAtThreeLevelsWarns`(`:301`)가 먼저 돌고
`InertKey_NestedTextureGainWarns`(`:313`)가 뒤에 돌았습니다.

### 그리고 제품 결함 — **리셋이 실행될 기회가 없었습니다**

헬퍼는 복구 계약을 갖고 있습니다:

```cpp
if (present.empty()) {
    lastWarned.clear();   // same reason as above: let the next one be heard
    return;
}
```

미지 키 쪽도 같은 계약을 갖고, 그쪽 주석이 이유를 적습니다 — *"a caller that fixes its
typo and then reintroduces it would stay silent the second time, and the warning would be
worth less than it looks."*

**그런데 무효 키 쪽 호출자가 그 경로를 막고 있었습니다:**

```cpp
if (levels <= 3) {                       // <-- 게이트
    warn_inert_keys_once(..., kInert, 1, "mfp", ..., s_lastInert);
}
```

`levels > 3` 이면 **헬퍼에 도달하지 않습니다.** 그러면 `present.empty()` 분기가 돌지
않고 기억이 남습니다. 미지 키 쪽은 호출이 **무조건**이라 이 문제가 없습니다 — **같은
계약, 한쪽만 실행 가능**했습니다.

**실제 앱 시나리오** (시험 밖):

1. `{"num_levels":3, "texture_gain":2}` → 경고 1회
2. `{"num_levels":4, "texture_gain":2}` → 조용 (옳음). **그러나 기억은 그대로**
3. 다시 `{"num_levels":3, "texture_gain":2}` → **조용.** 조언이 영구히 사라집니다

> 리더가 지목하신 메모리 *"정리 코드가 다음 측정을 오염시켰다"* 와 **같은 자리는
> 아니었습니다** — 여기서는 정리 코드가 아니라 **호출 게이트**가 복구 경로를 막았습니다.
> 다만 성질은 같습니다: 프로세스 전역 상태를 **끄는 쪽으로만** 건드렸습니다.

## 4. 고친 것

| 파일 | 고침 | 왜 |
|---|---|---|
| `multiscale_process.cpp` | 호출을 **무조건**으로, **목록 길이**를 값에 따라 (`levels<=3 ? 1 : 0`) | 빈 목록은 건너뛸 이유가 아니라 **실제 상태** |
| `enhance_advanced_helpers.cpp` | `inertCount == 0` 에서 그냥 반환하지 않고 **기억을 지움** | 값에 따라 목록을 만드는 호출자가 리셋을 건너뛸 수 없게 — 계약을 **한 곳**에 |
| `test_config_warning_once.cpp` | `SetUp` 이 **제품 자신의 리셋 경로**로 기억을 비움 (시험 전용 훅 없음) | 시험이 순서에 안 기대게 |
| `test_config_warning_once.cpp` | 억제 규칙을 **고정하는 시험** 추가 | 규칙이 바뀌면 **거기서** 빨강 — 다른 데서 순서 의존으로 새지 않게 |

### 새 시험이 대조군이기도 한 이유

`InertKey_SameInertSetIsSaidOnceAcrossDifferentConfigs` 는 네 단계를 **한 시험**에
담습니다: ① 첫 설정 경고 → ② 같은 집합의 **다른 설정**(중첩 철자) 침묵 →
③ 무효가 아닌 설정이 기억을 비움 → ④ **다시 경고**.

①이 없으면 ②의 침묵은 "경고가 고장났다" 와 구별되지 않고, ④가 없으면 억제가
**영구 침묵**인지 알 수 없습니다. 넷이 함께여야 *"중첩도 경고한다"* 가 **순서 덕분이
아님**을 보입니다.

## 5. 반증

제품 고침을 되돌렸습니다(게이트 복원). `===BUILD=0===` 확인 후:

```
test_config_warning_once.cpp(379): error: Expected equality of these values: ...
the warning did not recover -- a caller that fixes the config and then re-breaks it
would never be told again
```

**정확히 그 단언이** 빨강입니다. 복원하니 초록으로 돌아왔습니다(`_build3.log`).

시험 쪽만 고치고 제품을 안 고쳤다면 이 단언이 **계속 빨강**이었을 것입니다 — 그것이
방향을 "둘 다" 로 고른 근거입니다.

## 6. 검증 — 두 게이트 모두

```
===BUILD=0===                     (_build3.log)
ctest ci-post: 676/676 통과       (_verify.log, ===CTEST=0===)
한 프로세스 전체: 12 바이너리, 0 실패   (_inprocess.sh)
```

고치기 전 한 프로세스 전체는 **1건 실패**였습니다(§2). 시험이 하나 늘어
`enhance_advanced` 는 237 → 238 입니다.

## 7. 리더께 — CI 에 넣을 것 (워크플로는 리더 소유)

**ctest 를 대체하지 않습니다. 더합니다.** 별도 프로세스 실행의 격리는 그대로 값입니다.

기존 ci-post 작업에서 `ctest` **다음에** 붙일 단계입니다. 빌드가 이미 있으므로 비용은
시험 실행 시간 한 번(로컬 측정 **약 2분 20초**, 대부분 `gsvg_tests` 88초)입니다.

```yaml
      - name: In-process suite run (ctest cannot see order-dependent state)
        shell: bash
        working-directory: modules/gsvg   # gsvg_tests reads phantoms by relative path
        run: |
          # gtest_discover_tests registers each TEST as its own ctest entry, so
          # ctest starts a FRESH PROCESS per test. Any defect that depends on
          # state carried between tests in one process -- a thread_local, a
          # call_once, a static cache -- is invisible to ctest by construction.
          # QA-B-140 found one that way: ctest 675/675 green, one red here.
          fail=0
          for exe in "$GITHUB_WORKSPACE"/build/ci-post/bin/*; do
            [ -f "$exe" ] && [ -x "$exe" ] || continue
            case "$exe" in *.pdb|*.ilk|*.lib|*.dll|*.exp) continue ;; esac
            name="$(basename "$exe")"
            if out="$("$exe" 2>&1)"; then
              echo "ok   $name"
            else
              fail=$((fail + 1))
              echo "FAIL $name"
              printf '%s\n' "$out" | grep -E '^\[  FAILED  \] [A-Za-z]' | sed 's/^/     /'
            fi
          done
          echo "in-process: $fail binary(ies) failed"
          [ "$fail" -eq 0 ]
```

**검산해 둔 것**: 글롭 하나만 씁니다. 처음에 `*.exe`·`test_*`·`*_tests` 를 나란히
썼더니 윈도우에서 **같은 파일을 두 번 방문해 12개를 22개로 셌습니다.**

참고 구현이 `_inprocess.sh` 에 있습니다(보고서 디렉터리, gitignore 대상). 워크플로에
인라인으로 넣는 쪽을 권합니다 — 스크립트의 영구 거처(`tools/`)가 리더 소유이고,
이 블록은 짧습니다.

**새 이슈 필요 여부**: 제 판단으로는 **불필요합니다.** 결함도 게이트 보강도 `#162`
범위 안이고 거기 보고했습니다. 게이트 보강을 따로 추적하고 싶으시면 열어 주십시오.

## 8. baseline 귀속 · 미검증 · 잔여 위험

**귀속**: 트리 `dev/postprocess`, `origin/main d4d3727` 병합 후. 위 §2 의 "고치기 전"
수치는 `b271dc3` 직전 트리에서, §6 은 그 이후 트리에서 측정했습니다.

**미검증**

- **미지 키 쪽에도 같은 형태의 구멍이 남아 있습니다**: `warn_unconsumed_keys_once` 는
  `json == nullptr` 에서 **기억을 지우지 않고** 반환합니다. 주석이 *"defaults are an
  ordinary call, not a mistake"* 라 **의도된 선택**으로 읽히지만, 오타 설정 → 기본값
  호출 → 같은 오타 설정이 침묵하는지는 **재지 않았습니다.** 무효 키 쪽은 같은 이유로
  `json == nullptr` 를 그대로 두었습니다(형제와 동일하게).
- **스레드 축**은 안 봤습니다. 억제는 `thread_local` 이라 스레드마다 한 번씩 말합니다.
  스레드 풀에서 프레임이 스레드를 옮겨 다니면 같은 조언이 여러 번 나옵니다 — 설계
  의도인지 확인 안 했습니다(`ConcurrentThreadsDoNotEraseEachOthersWarning` 은 서로
  지우지 않는 것만 봅니다).
- 한 프로세스 실행은 **gtest 기본 순서** 하나만 봤습니다. `--gtest_shuffle` 로는 안
  돌렸습니다 — 다른 순서에서 또 나올 수 있습니다.

**잔여 위험**

- 새 시험은 **알림 문자열**(`"texture_gain"`, `"no effect"`)에 결합돼 있습니다. 문구가
  바뀌면 눈멉니다. ①의 존재 단언이 함께 빨강이 되므로 **조용하지는 않습니다.**
- `SetUp` 의 리셋은 제품 경로를 쓰므로, 리셋 경로가 고장나면 시험도 함께 고장납니다.
  이는 의도한 것입니다 — 시험 전용 훅이었다면 **제품이 고장나도 초록**이었습니다.

---

Refs #162, #145
