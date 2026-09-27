# QA-A-23 — 바이패스 JSON 불리언 + NOT_IMPLEMENTED 문자열 + test_pipeline_ex 등록 (#126, #120)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-10   **Refs**: #126 #120
**baseline**: `origin/main` `199e457`
**커밋**: `6067587`(1·2항) + 본 커밋(3·4항)

## 1. 주장 (Claim)

바이패스 플래그가 평범한 JSON 불리언을 무시하던 결함을 고치고, `-15` 문자열을 넣었다.
`PipelineExWithState` 문제는 **관측 결과 별개 결함이 아니라 위 바이패스 결함의 증상**이었고,
남은 크래시는 테스트의 소유권 전제 오류였다. `test_pipeline_ex.cpp` 14 케이스를 등록했다.

## 2. 증거 (Evidence)

### 2.1 1항 RED → GREEN (`a23-red.log`, `a23-green.log`)

`xpe_json_get_string`(`helpers.cpp:72`)이 값에 따옴표를 요구해
`{"bypassOffset": true}` 가 조용히 무시됐다. 내부 함수라 export 되지 않으므로 공개
진입점 `xpe_preprocess_pipeline` 으로 구동했다 — 결함이 호출자에게 보이는 지점이기도 하다.

| 케이스 | RED | GREEN |
|---|---|---|
| `JsonBooleanBypassFlagsAreHonoured` | **FAILED** | OK |
| `WhitespaceAroundBooleanIsTolerated` | **FAILED** | OK |
| `QuotedBypassFlagsStillHonoured` | OK | OK |
| `JsonBooleanFalseDoesNotBypass` | OK | OK |

따옴표 없는 스칼라를 구분자(`,` `}` `]` 공백)까지 읽는다. 중첩 객체·배열은 스칼라가
아니므로 빈 문자열을 유지한다 — 파서로 승격시키지 않았다.

### 2.2 2항 — 카드 질문에 답

`XPE_ERR_NOT_IMPLEMENTED`(-15) case 를 넣었다. **A-20 의 전 구간 순회 테스트는 이것을
잡고 있었다** — 그래서 A-20 이 `-15` 만 예외로 빼두어야 했다(당시 보고한 부수 관측).
이번에 그 예외를 제거했고 테스트가 통과한다. "자기 문자열" 단언은 약하지 않았다.

### 2.3 3항 — 관측 먼저 (카드 지시)

가설로 고치지 말라는 지시에 따라 순서대로 관측했다.

**관측 1 — 바이패스 수정만으로 `PipelineExWithState` 가 통과한다.**
1항 수정 뒤 재실행하니 이 케이스가 OK 로 바뀌었다(`a23-pex-green.log`).
A-20 보고서에 적은 가설("게인 단계의 float 결과가 최종 복사까지 도달하지 않는 것으로
보인다")은 **틀렸다**. 실제로는 바이패스 플래그가 무시돼 readout/temp/nonlinearity/
binning/ghost 스테이지가 전부 실행됐고, 그 결과가 최종 버퍼를 uint16 으로 되돌린 것이다.
가설을 근거로 파이프라인 배선을 고쳤다면 멀쩡한 코드를 바꿨을 것이다.

**관측 2 — 첫 테스트의 `-9`(IO_FAILED)는 남은 파일 때문이다.**
`[OBS]` 프로브로 매 SetUp 의 디렉터리 상태를 찍었다(`a23-obs.log`):

```
[OBS] tmp=C:\Users\...\xpe_pipeline_ex_test exists=1 fopen=1 errno=0
```

디렉터리는 존재하고 쓰기도 된다. `write_xcal_file` 은 `<path>.tmp` 를 쓰고
`std::rename` 하는데(`xcal_writer.cpp:163-206`), Windows 의 `std::rename` 은 **대상이
있으면 실패**한다. 앞선 실행이 중단되며 남긴 `offset.xcal` 이 원인이었다.
임시 디렉터리를 비우고 재실행하니 첫 테스트가 통과했다 — 관측으로 확인.

**관측 3 — 크래시는 double-free 다 (`a23-asan-batch2.log`).**

```
==12412==ERROR: AddressSanitizer: attempting double-free
  #5 PipelineExTest_BatchProcessesMultipleFrames_Test::TestBody  test_pipeline_ex.cpp:314
  #1 (freed by)                                                   test_pipeline_ex.cpp:312
```

A-20 에서 `:207` 하나를 지웠지만 같은 전제가 배치 테스트에 3곳 더 있었다(`:312`,
`:348`, `:391/393`). 구 API 는 `img.data` 를 새 버퍼로 갈아끼워 소유권을 넘겼고,
현재 구현은 호출자 버퍼에 쓴다.

**판정: 구현 결함 1건(바이패스 파싱, 수정) + 테스트 전제 오류 2건(소유권, 픽스처 정리).**

### 2.4 4항 — 등록 + 재실측

| 항목 | 결과 | 로그 |
|---|---|---|
| `PipelineExTest` | **14/14 통과** | `a23-pex-green.log` |
| `ci-preprocess` | `0 failed out of 422` | `a23-ctest4.log` |
| `ci-common` | `0 failed out of 63` | `a23-common.log` |
| ASan 전체 | 351 통과, **AddressSanitizer 보고 0건** | `a23-asan-full.log` |

408 → 422 (+14). CMake 의 A-15/A-20 유예 주석은 사유가 해소되어 제거했다.

## 3. baseline 귀속

`origin/main` `199e457` 트리. 모든 수치는 이번 실행 관측. ASan 은 `build/asan-a17`
(기존 트리 재사용, 삭제 없음).

## 4. Gaps (미검증)

- **새 스칼라 파서를 전수 검증하지 않았다.** 확인한 것은 불리언·숫자·따옴표 문자열·
  중첩 구조 거부 4종이다. 이스케이프가 든 문자열은 따옴표 경로라 기존과 같지만 케이스는 없다.
- 바이패스 플래그 8개를 **한꺼번에 켜는 방식**으로만 확인했다 — 개별 플래그 단위 검증이 아니다.
- `xpe_json_get_double` 과 같은 키를 두 추출기가 함께 읽는 지점이 있는지 전수 조사하지 않았다.
- **크래시 원인 규명은 배치 테스트 1건에 대해서만 했다.** 나머지 소유권 전제 3곳은 같은
  형태라는 판단으로 함께 고쳤고, 각각을 ASan 으로 따로 관측하지는 않았다(수정 후 전체
  ASan 0건으로 간접 확인).
- C# 통합 테스트는 실행하지 않았다.

## 5. 잔여 위험

- **파서가 관대해졌다.** 이제 `{"bypassOffset": tru}` 같은 오타도 "tru" 로 읽혀 조용히
  거짓이 된다. 이전에도 조용히 무시됐으므로 나빠지지는 않았으나, 값 검증은 여전히 없다.
- 바이패스 플래그가 **이제 실제로 동작한다**. 여태 무시되던 설정을 쓰던 호출자가 있었다면
  그 파이프라인의 동작이 이번 변경으로 달라진다 — 스테이지가 건너뛰어진다.
- 픽스처가 `fs::remove_all` 로 시작한다. 경로가 잘못 계산되면 지우는 범위가 넓어질 수
  있는 형태다(현재는 `temp_directory_path()/"xpe_pipeline_ex_test"` 고정).
