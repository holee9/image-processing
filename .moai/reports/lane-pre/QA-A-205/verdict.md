# QA-A-205 — 파이프라인 온도 보상 단계의 힙 손상 (#234)

기준 커밋 `098ae75b`. 카드: `.moai/lanes/pre/inbox/QA-A-205.md`. 증거는 `evidence/` (번호 순).

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| ① | 온도 보상 단계는 호출자가 말한 `dataSize` 가 아니라 치수가 가리키는 프레임(`width*height*2` 바이트)을 작업 버퍼로 복사한다. `dataSize` 가 그보다 크면(이득 단계의 float 결과를 되돌려 받으려면 호출자가 줘야 하는 크기) 더 이상 버퍼를 넘지 않는다. | 빨강 `04_red_run.txt`, 초록 `06_green_run.txt`, 반증 `arm_oldcopy_*` |
| ② | 0 이 아니면서 치수가 필요로 하는 크기(`width*height*2`)보다 작은 `dataSize` 는, 어떤 단계도 돌기 전에 `XPE_ERR_INVALID_INPUT` 이고 영상·메타데이터는 그대로다. (#123 계약: `dataSize` 0 은 "미지정, 치수를 믿는다".) | 빨강, 초록, 반증 `arm_nocheck_*` |
| ③ | 같은 형태(호출자의 크기로 고정 크기 버퍼에 복사)가 `modules/preprocess`·`modules/common` 에 또 있지는 않다 — 보호 없이 남은 것은 이것 하나였다. | `evidence/07_copy_audit.md` (63행 전수 분류) |
| ④ | 실제 호출 경로에서는 터지지 않았다: 저장소에서 `xpe_preprocess_pipeline*` 를 부르는 코드는 `modules/preprocess` 의 시험뿐이고, 클라이언트·GUI(`clients/`, `gui/`)는 파이프라인 진입점을 부르지 않으며 개별 단계 함수를 치수에 딱 맞는 `dataSize` 로 부른다. | `evidence/08_callers.txt` |

## 2. 무엇을 바꿨나

- `pipeline.cpp`(`pipeline_core`)
  - 맨 앞에 `inputBytes = pixelCount * sizeof(uint16_t)` 와 거부 검사(`img->dataSize != 0 && img->dataSize < inputBytes` → `XPE_ERR_INVALID_INPUT`).
  - 1단계 복사 길이를 `img->dataSize` 에서 `inputBytes` 로.
- 앞단의 거부 여부(카드 1번): 기존에는 `pipeline_core` 에 이 검사가 없었다. **관측**: 읽기 검증을 켠 구성에서는 구현 전에도 작은 `dataSize` 가 거부되었고(둘째 시험의 해당 행이 구현 전에도 초록), 그 단계를 끈 구성에서는 거부되지 않았다(구현 전 빨강 9건 — 온도 보상이 켜져 있으면 뒤 단계의 입력 설명자는 작업 버퍼라서 호출자의 작은 `dataSize` 를 아무도 보지 못하고, 복사가 짧아 나머지는 0 인 프레임이 조용히 처리된다). **읽기**: 온도 보상도 끈 구성이면 오프셋 단계(`offset_correct.cpp:166`)가 거부한다(실행하지는 않았다). 지금은 첫머리에서 거부한다. 지금은 첫머리에서 거부한다.
- `test_pipeline_stage_values.cpp`: 주석 하나(온도 보상 단계를 끈 이유가 이 결함이었다는 설명)를 사실에 맞게 고쳤다. 시험 동작은 그대로다.
- 시험 (`test_oom_injection.cpp`, OOM 실행 파일): 이 실행 파일의 할당기에 **버퍼 뒤 256바이트 감시 영역**을 더했다(켠 동안 할당된 블록만, 주소 표로 기록, 해제 때 검사; 머리글이 없어 다른 모듈이 할당한 블록은 영향이 없다). 새 시험 둘:
  - `TheFrameCopiedIsTheOneTheDimensionsDescribeWhateverSizeTheCallerClaims`: 세 진입점 × `dataSize = N*4`(통제), `N*4+13`, `N*2` — 정상 종료, 버퍼 초과 0, `dataSize` 이상 바이트는 건드리지 않음, 결과는 프레임 크기 호출의 결과(앞부분)와 같음. 먼저 감시 장치가 쓰기-초과를 실제로 보고하는지 통제로 단언한다.
  - `AClaimSmallerThanTheFrameIsRefusedBeforeAnythingIsDone`: 세 진입점 × 두 구성(읽기 검증 켬 / 끔) × `dataSize = N*2-1, N*2-2, 1` — `INVALID_INPUT`, 영상·메타데이터 불변.

## 3. 빨강 → 초록

- 빨강 (`04_red_run.txt`): 첫 시험은 감시 영역이 변조되었음을 보고("a claim of 77 bytes overran a buffer" 등, 세 진입점 모두). 둘째 시험은 읽기 검증을 끈 구성에서 아홉 가지(3 진입점 × 3 크기)가 실패 — 구현 전에는 작은 `dataSize` 가 거부되지 않았다.
- 같은 결함의 이전 관측: QA-A-202b 에서 이 크기를 쓰는 시험 영상이 감시 장치 없이 힙 손상(종료 코드 `-1073740940` = 0xC0000374)으로 프로세스를 죽였다 (`QA-A-202b/evidence/10_probe_build.txt`, `11_probe_exit.txt`, `12_probe_output.txt`).
- 초록: 두 시험 통과 (`06_green_run.txt`).

## 4. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | 전체 타깃 `BUILD_EXIT=0` (`21_build_final.txt`) |
| preprocess DLL 시험 | 772 실행 / 764 통과 / 8 건너뜀(원래 건너뛰던 8건), 섞기도 동일 (`24`, `25`) |
| preprocess OOM exe | 23 통과 (`26_pre_oom.txt`, 직전 21) |
| common OOM exe / common 기존 | 8 / 69 통과 |
| `ctest -N` | 912 (직전 910) |
| 수출 이름 | preprocess 48, common 16 불변 |
| 헤더 문서 / 프리셋 | 0 findings / 12 of 12, 종료 0 |

## 5. 반증 (한 번에 하나, 복원 뒤 `cmp` 동일)

| 팔 | 손상 | 빨개진 시험 |
|---|---|---|
| oldcopy | 복사 길이를 `img->dataSize` 로 되돌림 | 첫 시험 하나만 ("a claim of 77 bytes overran a buffer" 세 진입점). 이 실행은 프로세스가 죽지 않고 단언으로 잡혔다(감시 영역 256바이트가 초과분 45바이트를 흡수) |
| nocheck | 맨 앞 거부 검사 제거 | 둘째 시험 하나만 (읽기 검증을 끈 구성의 아홉 가지) |

## 6. 새로 확인한 것 (리더에게, 이 카드에서 고치지 않음)

1. **이미 알려졌으나 우회된 결함이었다.** `test_pipeline_stage_values.cpp` 의 주석이 "온도 보상은 `img->dataSize` 바이트를 `W*H` uint16 버퍼에 복사하고 이 영상은 float 크기라서 끈다" 고 원인까지 적고 있었다. 결함을 제품에서 고치는 대신 시험 구성에서 피한 것이다. 이번에 그 주석을 고쳤다.
2. **`dataSize = N*2`(입력 크기)로 이득 단계가 켜져 있으면 출력 float 프레임의 앞 절반만 쓰이고 `img->format` 은 FLOAT32 로 바뀐다.** 마지막 복사가 `min(img->dataSize, 최종 단계 크기)` 이기 때문이다(`pipeline.cpp` 복사-되쓰기 구간). 이 카드의 시험이 그것을 "앞부분이 같다 + 그 이후는 건드리지 않았다" 로 고정했을 뿐 오류로 만들지는 않았다. 올바른 결과를 받으려면 호출자가 `N*4` 이상을 줘야 하며, 이제 그 호출이 안전하다. 부족한 크기를 `XPE_ERR_BUFFER_TOO_SMALL` 로 거부할지는 계약 결정이다.
3. **`dataSize = 0`** 은 이제 입력을 치수대로 읽지만(복사 길이가 `inputBytes`), 마지막 복사는 `min(0, ...)` 로 아무것도 쓰지 않으면서 `img->format` 은 갱신하고 `XPE_OK` 를 돌려준다(코드를 읽어 얻은 것이며 실행으로 확인하지는 않았다).

## 7. 미검증 (Gaps)

- 힙 검사는 디버그 CRT·페이지 힙이 아니라 이 실행 파일 안의 감시 영역이다. 초과가 감시 영역(256바이트)을 넘어서는 크기에는 신호가 약해질 수 있고(그 경우 프로세스가 죽는 것으로 드러난다), 다른 모듈이 할당한 블록은 보지 않는다.
- 감시 영역은 `operator new` 로 할당된 블록만 본다. `malloc` 으로 직접 할당된 버퍼는 보지 않는다(이번 경로의 버퍼는 모두 `std::vector`).
- 복사 전수 검사는 `modules/preprocess/src` 와 `modules/common/src` 만 대상으로 했다. 다른 모듈은 보지 않았다.
- `ci-preprocess` 구성 하나에서만 돌렸다. Linux/GCC 미검증.

## 8. 잔여 위험

- 맨 앞 거부 검사는 파이프라인 입력을 `uint16` 로 가정한다(`inputBytes = 픽셀 수 * 2`). float 입력 영상은 `dataSize` 가 그 이상이라 통과하지만, 형식과 `dataSize` 의 일치는 이 검사가 아니라 각 단계의 형식 검사에 맡긴다.
- 위 6절 2·3은 이 카드의 범위 밖이어서 열려 있다.
