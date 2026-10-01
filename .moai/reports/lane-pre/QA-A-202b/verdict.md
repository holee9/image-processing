# QA-A-202b — Codex #23 보류 2건: 파이프라인 세 진입점의 예외 가드, 설정 숫자 표기의 호환성

기준 커밋 `9abae87c`(QA-A-204 1/2) 위. 감사 원문은 `evidence/00_codex23_audit_original.md` 에 그대로 복사해 두었다(메인의 outbox 는 다음 회신이 덮어쓴다). 증거는 `evidence/` (번호 순).

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| ① | `xpe_preprocess_pipeline`, `_ex`, `_batch` 는 본문 전체가 하나의 예외 가드 안에 있다: `bad_alloc`→`XPE_ERR_OUT_OF_MEMORY`, 그 밖→`XPE_ERR_PROCESSING_FAILED`. 메모리 부족으로 실패한 호출은 영상을 건드리지 않았고 메타데이터·교정 저장소·고스트 이력이 호출 전과 같다. 배치는 실패한 프레임을 그 프레임의 실패로 보고 다음 프레임으로 넘어간다(프레임은 "온전히 처리됨" 아니면 "그대로"). | 빨강 `03_red_oom.txt`, 초록 `06_green_oom.txt`, 반증 `arm_noguard_*`, `arm_nometarestore_*`, `arm_noframeguard_*` |
| ② | 설정 숫자는 앞 공백(공백·탭·개행·수직 탭·폼 피드·캐리지 리턴)과 단일 앞 `+` 를 전처럼 받고, 숫자 뒤에 남는 글자는 거부한다. 정수 칸의 `1e2`·`2.0` 은 거부, 실수 칸의 `1e2` 는 수락. 경계는 세 파이프라인 진입점과 `xpe_ghost_create` 에서 표로 고정했다. | 빨강 `02_red_tests.txt`, 초록 `05_green_tests.txt`, 반증 `arm_nolead_*`, `arm_tail_*` |

## 2. 무엇을 바꿨나

- `pipeline.cpp`: 세 정의를 파일 안 구현(`pipeline_impl`, `pipeline_ex_impl`, `pipeline_batch_impl`)으로 바꾸고, 같은 이름의 수출 함수를 파일 끝에 감싸개로 둔다.
  - 단일 프레임 둘: 진입 때 메타데이터를 복사해 두었다가, `bad_alloc` 예외 **또는** 단계가 `OUT_OF_MEMORY` 코드를 돌려준 경우 복원한다(단계들은 자기 플래그를 진행하며 세우고, 영상은 맨 끝에 쓰므로 실패 시 영상은 그대로인데 플래그만 남는다).
  - 배치: 프레임마다 자체 가드(예외 또는 `OUT_OF_MEMORY` 코드 → 그 프레임의 메타데이터 복원, 다음 프레임 계속), 바깥에 설정 읽기·교정 적재용 가드 하나.
  - 메모리 부족 이외의 오류 코드는 지금까지의 동작을 그대로 둔다(끝난 단계의 플래그가 남음). 아래 6절 참고.
- `xpe_strict_parse.hpp`: `skip_leading()` 추가(할당 없음). 규칙은 아래 4절.
- `preprocess_api.h`: 네 함수의 반환값 설명에 표기 규칙(`xpe_preprocess_pipeline`), OOM 보장 추가.
- 시험: `test_config_strict_parse.cpp` 의 경계 표 둘(`...PipelineEntryPoints...`, `...GhostCreate...`), `test_oom_injection.cpp` 의 `OomPipeline` 셋. 스윕에 호출별 추가 검사 인자(`extra`)를 더해 저장소가 비워지기 전에 영상·메타데이터·저장소·고스트 이력을 비교한다.

## 3. 빨강 → 초록

- 빨강(구현 전): 경계 표 둘이 빨강(앞 공백·앞 `+` 수락 행이 `CONFIG_INVALID`). 스윕: `pipeline_ex` 는 "an exception left the C ABI function when allocation #1 failed", `pipeline` 은 #18. 배치는 그 실행에서 프로세스가 비정상 종료했다(아래 6절의 버퍼 문제와 겹친 실행이라 원인은 가르지 못함).
- 초록: 경계 표 포함 `ConfigStrictParse` 8 통과, `OomPipeline` 3 통과. 스윕 점수: `pipeline_ex` 15, `pipeline` 32, `pipeline_batch` 45 (설정 문자열 읽기와 단계 버퍼 모두 주입 범위에 들어간다).
- 통제: 각 시험은 스윕 전에 깨끗한 실행이 성공하고 영상이 바뀌고 offset·gain·defect(·ghost) 플래그가 서는 것을 단언한다(배치는 ghost 없이 — 고스트 이력은 프레임 순서에 의존하므로 실패한 프레임 뒤의 프레임 결과가 참조와 달라진다).

## 4. 설정 숫자 표기 — 최종 규칙 (api-spec 용 한 단락)

> **설정 JSON 의 숫자.** 설정 문자열의 숫자(파이프라인의 `detectorTempC`·`binningMode`, 고스트의 `tier`·`alpha1`·`tau1`·`alpha2`·`tau2`·`tier2Threshold`·`nlcscBeta`)는 선택적 앞 공백(공백, 탭, 개행, 수직 탭, 폼 피드, 캐리지 리턴), 선택적 단일 `+`(다른 부호가 바로 뒤따르지 않을 것), 그리고 값의 나머지를 모두 채우는 십진수로 이루어진다. 정수 칸은 정수만, 실수 칸은 유한한 수(소수점과 지수 허용)를 받는다. 값의 일부만 숫자인 경우(`25 `, `2x`), `+-1`·`++1`·`+ 1`, `nan`·`inf`, 16진수, `int`/`float`/`double` 범위를 넘는 값은 `XPE_ERR_CONFIG_INVALID` 이며 이때 영상·메타데이터·교정 저장소·핸들은 바뀌지 않는다. 빈 값은 키가 없는 것으로 보아 기본값을 쓴다. 변환은 로케일과 무관하다(`1,5` 는 거부).

경계 표 (시험이 단언하는 기대값; `수락` = 호출이 `XPE_OK`, `거부` = `XPE_ERR_CONFIG_INVALID`; 값 앞뒤의 `␠` 는 공백, `⇥` 는 탭):

| 값 | 실수 칸 (`detectorTempC`, `alpha1`) | 정수 칸 (`binningMode`, `tier`) |
|---|---|---|
| `␠+25` / `␠+2` | 수락 | 수락 (`␠+2`) |
| `+2` | 수락 | 수락 |
| `␠0.5` | 수락 | — |
| `␠4` | — | 수락 |
| `⇥3` / `⇥2` | 수락 | 수락 |
| `-5.5` / `-1` | 수락 | 수락 (`-1`) |
| `1e2` | 수락 | **거부** |
| `2.0` | — | **거부** |
| `25␠`, `2␠` (뒤 공백) | 거부 | 거부 |
| `+-1`, `++1`, `+␠1` | 거부 | 거부 |
| `nan`, `inf`, `-inf` | 거부 | 거부 (`nan`, `inf`) |
| `1e999` / `99999999999999999999` | 거부 | 거부 (`99999999999999999999`) |
| `0x10` / `0x4` | 거부 | 거부 |
| `1,5`, `1.5.2` | 거부 | — |
| `1e` | — | 거부 |

## 5. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | **전체 타깃** `BUILD_EXIT=0` (`21_build_final.txt`) |
| preprocess DLL 시험 | 770 실행 / 762 통과 / 8 건너뜀(원래 건너뛰던 8건), 섞기도 동일 (`24`, `25`) |
| preprocess OOM exe | 21 통과 (`26_pre_oom.txt`) |
| common OOM exe / common 기존 시험 | 8 통과 / 69 통과 |
| `ctest -N` | 908 (직전 903) (`27_ctest_n.txt`) |
| 수출 이름 | preprocess 48 차이 없음, common 16 차이 없음 |
| 헤더 문서 / 프리셋 | 0 findings / 12 of 12, 종료 0 |

## 6. 반증 (한 번에 하나, 전체 빌드 후 두 시험 실행 파일 모두 실행, 복원 뒤 `cmp` 동일)

| 팔 | 손상 | 빨개진 시험 (그 외 초록) |
|---|---|---|
| noguard | 세 진입점이 예외를 바꾸지 않음(배치의 프레임별 가드는 남김) | `PipelineEx...`, `Pipeline...` ("an exception left the C ABI function when allocation #1 / #18 failed"). 배치는 프레임 가드가 남아 있어 초록 |
| nometarestore | OOM 인데 메타데이터를 복원하지 않음 | `PipelineEx...`, `Pipeline...` ("frame 0 is neither as it was nor fully processed (meta flags 00000010, was 00000000)") |
| noframeguard | 배치의 프레임별 가드 제거 | `PipelineBatch...` (같은 메시지) |
| nolead | 앞 공백·앞 `+` 를 다시 거부 | 경계 표 둘 |
| tail | 값 전체 소비 검사 제거 | 경계 표 둘 + 이전 카드의 거부 시험 셋 (뒤 글자 입력 포함) |

## 7. 새로 발견한 것 (리더에게 — 제품 결함 후보, 이 카드에서 고치지 않음)

`pipeline_core` 의 온도 보상 단계(`pipeline.cpp:139`)는 `std::memcpy(stage1Data.data(), img->data, img->dataSize)` 를 `width*height` 개 `uint16` 크기의 버퍼에 한다. 호출자가 `dataSize` 를 `width*height*2` 보다 크게 주면(예: 이득 단계의 float 출력이 들어갈 만큼, 즉 `width*height*4`) 버퍼를 넘어 쓴다. **관측**: 이 카드의 OOM 시험 영상을 `dataSize = N*sizeof(float)` 로 만들었더니 프로세스가 힙 손상(종료 코드 `-1073740940` = 0xC0000374)으로 죽었고(`evidence/10_probe_build.txt`, `11_probe_exit.txt`, `12_probe_output.txt`), `N*sizeof(uint16_t)` 로 돌리면 정상이다. **읽기**: 같은 함수의 마지막 복사는 `min(img->dataSize, 최종 단계 dataSize)` 로 제한하고 헤더 주석은 "호출자가 float 크기를 확보해야 한다" 고 적는다 — 그 계약대로 큰 `dataSize` 를 주면 앞쪽 복사가 넘친다. **미확인**: 실제 호출자(GUI 의 `GuiPreprocessRunner`)가 어떤 `dataSize` 를 넘기는지는 보지 않았다. 별도 카드 후보.

## 8. 미검증 (Gaps)

- 메모리 부족 이외의 오류(예: 결함 맵 없음)에서는 끝난 단계의 플래그가 남고 영상은 그대로다. 이번에는 메모리 부족에만 복원을 적용했다 — 같은 비일관성이 다른 오류 코드에도 있으나 기존 동작이라 손대지 않았다.
- 배치 시험은 두 프레임이고 고스트 없이 돌린다. 고스트 핸들을 쓰는 배치에서 한 프레임이 실패했을 때 이후 프레임의 고스트 결과가 달라지는 것은 시험하지 않았다(제품 주석이 이미 "배치의 고스트는 순차" 라고 경고한다).
- `pipeline`(교정 경로 있음)의 교정 저장소 비교는 같은 내용의 파일을 다시 적재하므로 값 요약(맵 내용·크기)으로 비교한다. 적재 시각·세션 id 는 비교하지 않는다.
- `ci-preprocess` 구성 하나에서만 돌렸다. Linux/GCC 미검증.

## 9. 잔여 위험

- 단계가 `OUT_OF_MEMORY` 코드를 돌려주는 경우에도 메타데이터를 복원하므로, 단계가 메타데이터의 다른 필드(플래그 이외)를 부분적으로 고치다 실패하는 경우도 함께 되돌려진다 — 의도한 동작이다.
- `xpe_strict_parse.hpp` 의 앞 공백 집합은 6개 고정 문자다(로케일 `isspace` 가 아님). 이전 `std::stof` 는 로케일에 따라 다른 공백도 받았을 수 있다.
