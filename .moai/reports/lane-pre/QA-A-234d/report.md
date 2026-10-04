# QA-A-234d — Codex #139 보류 3건: clang-tidy 게이트

카드: QA-A-234d · Refs #253 · 제품 코드·`.clang-tidy` 변경 없음(게이트 스크립트·기준선·패치·보고서만). 패치는 `.txt`, `origin/main`(= `cdf85e39`)에 `git apply --cached --check` 통과.

## 0. 처리 요약

| # | 발견 | 처리 | 반증(증거) |
|---|---|---|---|
| 1 | stderr 로 나온 새 경고가 사라짐 | 진단을 **stdout·stderr 를 합쳐서** 모은다. ` warning: `/` error: ` 처럼 보이는데 파싱하지 못한 줄은 **실패**(조용히 버리지 않음) | stderr 로만 새 경고를 내고 0 으로 끝나는 대역 → `NEW … bugprone-stub-check` 3건, 종료 1 (`2_stub_stderr_new.txt`). 파싱 못 할 경고 줄을 stderr 로 내는 대역 → "a diagnostic-looking line could not be parsed" 실패 (`2_stub_stderr_weird.txt`) |
| 2 | 양성 대조가 종료 코드·파싱을 안 봄 | 대조는 **종료 코드 0 + `error:` 부재 + 대조 파일(`control.cpp`)의 `bugprone-empty-catch` 진단이 파싱되어** 나왔을 때만 통과 | 진단을 내고 종료 1 + 오류 → "exited 1 on the positive control" 실패 (`2_stub_ctrl_exit1.txt`). 다른 파일 이름의 진단만 내고 종료 0 → "did not report the empty catch planted in control.cpp" 실패 (`2_stub_ctrl_other_file.txt`) |
| 3a | basename 이 합쳐짐 | 키를 **저장소 상대 경로**(소문자, 슬래시)로. 다른 드라이브의 헤더는 상대 경로를 못 만들어 절대 경로가 되고 `modules/` 로 시작하지 않아 걸러진다 | 같은 basename 두 폴더(`modules/common/src/x.cpp` ↔ `modules/preprocess/src/x.cpp`)가 서로 다른 키, 개수도 따로 (`30_same_basename_unit.txt`, 키 함수를 직접 호출하는 단위 수준 증거) |
| 3b | 같은 점검·같은 메시지·같은 줄 텍스트가 한 파일에 반복되면 교체를 놓침 | 리더 결정대로 **명시된 한계로 수용**. 스크립트 머리 주석에 한계를 적었고, 기준선에 개수가 2 이상인 키가 있으면 실행 때마다 "note (accepted limit) … a swap among these can pass" 로 그 키를 출력한다(실패 아님). 지금 기준선에 그런 키는 **0개**(`35 baseline lines … 0 identities have a count of 2 or more`) | 개수 2 로 고친 기준선 사본으로 note 가 출력되는 것 확인 (`33_repeat_note.txt`) |
| 4 | 기존 반증 재실행 | 같은 개수 안의 교체 → NEW, 비정상 종료 → 실패 | 아래 §2 |

## 1. 스크립트 변경 (`patches/check_clang_tidy.py.txt`)

- `parse_diagnostics(out, where)`: stdout+stderr 합본에서 `PAT`(`경로:줄:열: warning: 메시지 [점검]`)로 모은다. 같은 진단이 두 번 찍혀도(경로·줄·열·점검이 같으면) 한 번으로 센다. `LOOKS_LIKE_DIAGNOSTIC` 에 걸리는데 `PAT` 로 파싱되지 않은 줄이 있으면 즉시 `fail`.
- `run_tidy(tidy, args, where)`: 대조와 모듈 실행이 **같은 함수**를 거친다 — 종료 코드 0, `FAILED`(": error: ", "Error while processing") 부재를 같은 방식으로 확인.
- `positive_control`: 위 함수로 실행한 뒤 `check == bugprone-empty-catch` 이고 파일 basename 이 `control.cpp` 인 진단이 파싱되어 나왔는지 확인.
- 키 `(점검, 상대 경로, 정규화한 메시지, 줄 텍스트)`; 기준선 줄은 탭으로 나눈 5칸(점검, 경로, 메시지, 줄 텍스트, 개수). 기준선은 새 키 형식으로 다시 만들었다: preprocess 35줄(경로가 `modules/preprocess/src/…`), common 0줄.

## 2. 반증 전체 (이번 트리, 이번 실행)

| 시나리오 | 기대 | 관측 | 증거 |
|---|---|---|---|
| 진짜 clang-tidy, 깨끗한 트리 | 통과 | common `0 findings, baseline 0`, preprocess `35 findings, baseline 35`, 종료 0 | `10_gate_clean.txt` |
| 대역: 통과 모드(변조 없음) | 진짜 도구와 같은 결과 | `0 findings over 3 files; baseline 0`, 종료 0 | (대역 투명성 확인) |
| stderr 로만 새 경고, 종료 0 | 실패(NEW) | `NEW: bugprone-stub-check modules/common/src/xpe_common.cpp … (baseline 0)` 등 3건, 종료 1 | `2_stub_stderr_new.txt` |
| stderr 의 파싱 못 할 경고 줄 | 실패 | `a diagnostic-looking line could not be parsed`, 종료 1 | `2_stub_stderr_weird.txt` |
| 대조에서 진단 후 종료 1 + error | 실패 | `clang-tidy exited 1 on the positive control`, 종료 1 | `2_stub_ctrl_exit1.txt` |
| 대조가 다른 파일의 진단만 출력, 종료 0 | 실패 | `did not report the empty catch planted in control.cpp`, 종료 1 | `2_stub_ctrl_other_file.txt` |
| 대조는 정상, 모듈 파일에서 종료 1 + stderr 만 | 실패 | `clang-tidy exited 1 on modules/common/src\xpe_common.cpp`, 종료 1 | `32_gate_crash_on_modules.txt` |
| 한 건 고치고 같은 파일에 다른 한 건 새로 만듦(개수 동일) | NEW | `below baseline … 1 -> 0` + `NEW: bugprone-narrowing-conversions … a234d_probe`, 종료 1. 복원 뒤 소스 diff 없음 | `31_gate_swap.txt` |
| 같은 basename, 두 폴더 | 따로 센다 | 키가 다르고 개수 `[1, 1]` (단위 수준) | `30_same_basename_unit.txt` |

대역 `stderr_new` 의 첫 시도는 배치 인자 위치가 `=` 때문에 밀려 실제로 새 경고가 만들어지지 않았고(종료 0, 지적 0) 그 "통과"는 반증이 아니었다. 마지막 인자를 반복문으로 얻도록 대역을 고쳐 다시 돌려 위 결과를 얻었다.

## 3. 한계와 남은 위험

- **수용한 한계(3b)**: 같은 점검·메시지·줄 텍스트가 한 파일에 둘 이상 있을 때, 하나를 고치고 같은 텍스트의 다른 곳에 만들면 통과한다. 지금 기준선에는 그런 키가 없다(0개). 앞으로 생기면 실행 출력의 note 가 알린다.
- 줄 텍스트 신분은 서식만 바꿔도 달라진다(NEW + below 한 쌍): 기준선 갱신이 필요하다(`--write-baseline`).
- `LOOKS_LIKE_DIAGNOSTIC` 은 ` warning: `/` error: ` 가 든 모든 줄을 본다. 소스 인용 줄(`  509 | … // error: …` 같은)이 우연히 그 문자열을 포함하면 헛 실패가 날 수 있다. 지금 트리에서는 나지 않았다.
- 컴파일 DB 가 비었을 때·파일의 명령이 DB 에 없을 때의 실패는 코드에 있으나 이번에도 실행으로 누르지 않았다(234c 와 같은 한계).
- 패치는 실제 CI 에서 돌지 않았다(러너의 clang-tidy·ASan 런타임·`choco install cppcheck` 는 첫 실행이 드러낸다).
