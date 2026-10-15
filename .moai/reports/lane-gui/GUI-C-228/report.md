# GUI-C-228 — 구현됨 33개의 단언 충분성 읽기 (#249 마지막 항목)

## 결론

- 33개 전부를 요구 문장과 단언 줄을 나란히 놓고 읽었다. 표: `sufficiency_table.txt` (인용은 스크립트가 파일·메서드·단언 정규식의 실재를 확인해야 생성된다).
- 시험이 약했던 곳은 3건이고 모두 앱 코드 없이 시험만 보강했다. 보강마다 약화 반증이 빨갛게 터진다.
- 요구 문구와 코드가 갈리는 곳 6건(B1~B6)은 시험 결함이 아니라 문구·모듈 결정 사항이라 고치지 않고 목록으로 남겼다. 목록은 `../GUI-C-227/spec_patch.txt` §6.
- 상태 수치(구현됨 33 · 부분 0 · 보류 3, 총 36)는 바뀌지 않는다. 스크립트로 단언했다: `check_totals_output.txt`.

## 보강 3건

| 요구 | 부족했던 점 | 추가한 단언 | 약화 반증 (파일) |
|------|-------------|-------------|------------------|
| REQ-004 | "ByValTStr · SizeConst 64 · CharSet.Ansi" 선언 자체를 보는 단언이 없었다(왕복·크기·오프셋의 효과로만 보였다) | `XpeImageMetadata_BodyPart_IsDeclaredAsByValTStr64_AndTheStructIsAnsi` | SizeConst 63 · LPStr · Unicode 세 변이 모두 새 시험이 빨강 (`falsification_004_*.txt`) |
| REQ-021 | "이전에 init 없음" 전제를 시험이 만들지 않았다. 앞선 시험이 남긴 초기화 상태에 기대고 있었다 | 시험 안에서 shutdown 후 `NOT_INITIALIZED` 로 전제를 확인 | 전제 만들기를 init 으로 바꾸면 빨강 (`falsification_021_*.txt`) |
| REQ-042 | 조각(가드, 진단 문구)만 따로 시험했고, 고정물이 x86 DLL 을 만났을 때 `IsAvailable=false` 와 경로를 말하는 `ResolvedPath` 로 합치는 부분은 안 돌았다 | `TheFixtureBootstrap_WithAnX86XpeCommon_IsUnavailable_AndItsResolvedPathNamesTheFile` (같은 x64 DLL 의 Machine 필드만 바꾼 사본) | 가드를 끄면 빨강 (`falsification_042_*.txt`) |

## 충분하지만 한계를 적어 둔 것

- 007: "폴백을 시험 실패로 본다"는 직접 단언하지 않는다. 형식의 부재만 단언한다.
- 008: 파일 심볼릭 링크 시험은 관리자 권한 환경에서만 돈다(로컬 건너뜀, CI 확인은 리더 몫).
- 020: 전용 프로세스가 아니라 고정물의 첫 생성 자체를 기록한다.
- 042: 진짜 x86 빌드가 아니라 Machine 필드를 바꾼 사본이다. 방향도 x64 호스트에 x86 DLL 하나뿐이다("vice versa" 없음).
- 043: ARM64 분기는 실행해 보지 못했다.
- 050, 052: AccessViolation 은 .NET 이 못 잡아 호스트 종료로만 드러난다. 번역 안 된 예외를 실제로 일으키는 입력은 공통 모듈 공개 함수에서 만들어지지 않는다.

## SPEC 579·656행

- 579행(AC-12): 아직 맞다. `ci.yml` 의 "Run C# integration tests" 는 필터 없이 프로젝트 전체를 돌리고, `Smoke` 라는 글자는 Native E2E 설명 주석 하나뿐이다. 로컬 측정(전체 865건 23~24초)을 덧붙이는 문구를 `spec_patch.txt` §5(a)에 넣었다. 게이트가 없다는 사실은 그대로다.
- 656행(AC 표): 주석은 맞지만 표 본문이 존재하지 않는 시험 클래스(`DllResolutionTests` 등 8개)를 가리킨다. 모든 클래스가 실재하도록 구현·상태 칸의 대체안을 `spec_patch.txt` §5(b)에 넣었다. AC-4 는 P/Invoke 16개 전부가 시험에서 호출되는지 스크립트로 확인했다(16/16).
- 미검증: AC-13 의 "9 cases" 와 AC-15(plan.md 등 산출물)는 기존 문구만 읽었고 다시 세지 않았다.

## 증거

| 항목 | 값 |
|------|----|
| 통합 시험 전체, 환경 변수 지정 | 실패 0 · 통과 864 · 건너뜀 1 · 전체 865 (`integration_full_suite.txt`) |
| 통합 시험 전체, `--no-build` 재실행 | 실패 0 · 통과 864 · 건너뜀 1 (`integration_full_suite_without_env.txt`) |
| 건너뜀 1건 | 파일 심볼릭 링크 시험(비관리자 셸) |
| 합계 단언 | `check_totals_output.txt` |
| 줄 끝 공백 점검 | 17개 파일 점검, 0건 |

## 미검증 / 잔여 위험

- 새 시험 3건은 Native 구성에서만 돌려 봤다. Mock 구성 CI 에서는 돌려 보지 못했다(통합 시험 프로젝트는 Mock 잡이 없다고 읽었으나 워크플로 전체를 다시 확인하지는 않았다).
- 약화 반증은 변이 5종이다. 선택은 요구 문장이 말하는 속성마다 하나씩이다. 다른 변이가 새 시험을 통과할 가능성은 배제하지 않았다.
- 시험 단언이 요구를 충족한다는 판정은 내가 읽은 결과다. 독립 검토(Codex #124 이후)는 아직 없다.
