# QA-A-17 — XpeImageBuffer.dataSize 크기 정합성 가드 + 헬퍼 정비 (#123)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-10   **Refs**: #123
**baseline**: 로컬 `main` `b3e4f17` 병합 → `304d8d7` (leader 가 알린 `9457de9` 는 이미 지난 값)

## 1. 주장 (Claim)

api-spec 의 `XpeImageBuffer.dataSize on input` 계약 3절을 preprocess 에 발효시켰다.
(a) 기존 헬퍼가 `dataSize == 0` 을 거부하던 것을 계약대로 통과시키고,
(b) 보정 3함수의 **입력** 크기 가드를 새로 넣어 ASan 이 관측한 읽기 오버플로를 없앴으며,
(c) 두 테스트 디렉터리의 손수 만든 버퍼를 전수 값 초기화했다.

## 2. 증거 (Evidence)

### 2.1 RED — ASan 이 관측한 읽기 오버플로 (`asan-red.log`)

```
[ RUN      ] OffsetCorrectTest.UndersizedInputBufferIsRejected
==20920==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x12673f8a5180
READ of size 16 at 0x12673f8a5180 thread T0
SUMMARY: AddressSanitizer: heap-buffer-overflow
  modules\preprocess\src\offset_correct.cpp:151 in `anonymous namespace'::offset_correct_float_avx2
```

`W*H/2` 만 할당한 버퍼에 `width×height` 를 선언하고 `xpe_offset_correct` 를 부른 결과.
AVX2 커널이 16바이트 단위로 할당 밖을 읽었다 — QA-B-18 과 같은 부류.

빌드: `build/asan-a17` (신규 디렉터리, 기존 트리 삭제하지 않음), `/fsanitize=address /Zi`,
`XPE_WARNINGS_AS_ERRORS=OFF`, 나머지 캐시 변수는 `ci-preprocess` 프리셋과 동일.

### 2.2 GREEN — 가드 후 (`asan-green.log`)

```
[       OK ] OffsetCorrectTest.UndersizedInputBufferIsRejected (2 ms)
[       OK ] OffsetCorrectTest.InputDataSizeZeroIsUnspecifiedAndAccepted (1 ms)
[       OK ] OffsetCorrectTest.InputDataSizeLargerThanDimensionsIsAccepted (1 ms)
[  PASSED  ] 3 tests.
```

### 2.3 ASan 전체 스위트 — 0건 (`asan-full.log`)

```
[==========] 293 tests from 35 test suites ran. (129699 ms total)
[  PASSED  ] 285 tests.
$ grep -c "AddressSanitizer" asan-full.log
0
```

8건은 사전 존재하던 SKIPPED, 1건 DISABLED. **AddressSanitizer 보고 0건.**

### 2.4 헬퍼 계약 위반 — 추론이 아니라 실측 (`probe17.log`)

`xpe_preprocess_internal.h:154` `xpe_buffer_has_format` 이 `dataSize == 0` 을 거부했다.
수정 전 실측:

| 계약 절 | 수정 전 |
|---|---|
| `dataSize == 0` → 미지정, 통과 | **FAILED** |
| `0 < dataSize < 필요바이트` → `INVALID_INPUT` | OK (이미 성립) |
| `dataSize > 필요바이트` → 통과 | OK (이미 성립) |

이 헬퍼는 preprocess **7파일 15곳**에서 쓰인다 (`binning_correct`, `ghost_correct`,
`nonlinearity_correct`, `readout_validate`, `temp_compensate`, `xpe_defect_gen`,
`xpe_verify_metrics`) — 한 줄 수정으로 15곳이 동시에 계약을 따른다.

### 2.5 손수 만든 버퍼 전수 목록 (`scan-declarators.log`)

`modules/common/tests` + `modules/preprocess/tests`:

| 항목 | 값 |
|---|---|
| 선언자 총계 | **206** (단일 189 + 배열 17; 다중 선언 `a, b, c` 는 분해 계수) |
| 수정 전 미초기화 | **30** |
| 수정한 줄 | 26 (다중 선언 때문에 줄 < 선언자) |
| 수정 후 재스캔 | `state counts: Counter({'zeroed': 206})` / **RAW 0건** |

leader 가 전달한 함정 2종(다중 선언·배열)을 스캐너에 반영했다. **첫 스캐너는
`XpeImageBuffer buf{};` 를 미초기화로 오탐(111건)** 했고, 고친 뒤 30건으로 확정했다.
고치기 전 숫자를 보고했다면 그대로 틀렸다.

### 2.6 가드 위치 표

| 진입점 | 입력 원소 | 가드 |
|---|---|---|
| `xpe_offset_correct` (offset_correct.cpp) | `uint16_t` | 신규 (ASan RED→GREEN) |
| `xpe_gain_correct` (gain_correct.cpp) | `uint16_t` | 신규 |
| `xpe_defect_correct` (defect_correct.cpp) | `float` | 신규 |
| 15곳 (7파일) | 각 포맷 | `xpe_buffer_has_format` 경유 — 계약 준수로 전환 |

공개 헤더 변경 없음, common 신규 export 없음 (REQ-P0-008 16개 유지).

### 2.7 정규 빌드 재실측 (`ctest17c.log`)

```
100% tests passed, 0 tests failed out of 355
Total Test time (real) =  25.83 sec
```

349 → 355 는 이번에 추가한 6건(readout 3 + offset 3).

## 3. baseline 귀속

로컬 `main` `b3e4f17` 병합 후 트리(`304d8d7` → 작업). 모든 수치는 이번 실행에서 관측.
ASan 트리는 `build/asan-a17` 신규 생성 — 기존 `build/ci-preprocess` 는 건드리지 않았다.

## 4. Gaps (미검증)

- **`xpe_gain_correct` / `xpe_defect_correct` 가드는 테스트로 확인하지 못했다.**
  두 테스트 파일이 `modules/preprocess/CMakeLists.txt` 에서 주석 처리돼 있다
  (`test_gain_correct.cpp` "uses old 2-arg API", `test_defect_correct.cpp` 동일).
  가드는 offset 과 같은 형태로 넣었으나 **실행으로 관측한 바 없다.**
- **아직 가드가 없는 진입점**: `runtime_detection.cpp`, `xpe_calib_generate_gain.cpp`,
  `xpe_calib_generate_offset.cpp`. 코드를 읽어 후보로 분류했을 뿐 **오버플로를 관측하지
  않았다** — 결함 주장이 아니라 미확인 영역으로 남긴다.
- **출력 버퍼는 손대지 않았다.** `offset_correct.cpp:300` 등은 `output->dataSize < 필요`
  를 `BUFFER_TOO_SMALL` 로 거부하며 `0=미지정` 을 따르지 않는다. 계약 절 제목이
  "on **input**" 이고 카드도 "이미지 **입력** 진입점"이라 범위 밖으로 두었다. 판단 필요.
- C# 통합 테스트·CI 미검증 (Lane C 소유).
- `ci-preprocess` 단일 구성. ASan 도 같은 구성 1회.

## 5. 잔여 위험

- 가드는 `dataSize` 가 **정직하게 채워졌을 때만** 보호한다. 호출자가 실제보다 큰 값을
  넣으면(계약상 "과대 허용") 여전히 오버플로가 난다 — 계약이 그렇게 정한 결과다.
- `dataSize == 0` 경로는 계약대로 검사를 건너뛰므로, 레거시 호출자는 이번 변경으로
  **아무 보호도 얻지 못한다.** 보호는 필드를 채우는 호출자에게만 발생한다.
- 헬퍼 한 줄 완화가 15곳에 동시 적용됐다. 그중 `dataSize=0` 을 의도적으로 거부하던
  곳이 있었다면 이번에 함께 느슨해졌다 — 그런 의도가 있었는지는 확인하지 못했다.
