# QA-B-217 — 출하 DLL 의 실제 내보내기 함수 수 측정 (api-spec 개수 표 정정용, 측정만)

제품 코드·빌드 변경 없음. 측정 대상은 main `e421c1ff` push CI(run 38223364628)의 산출물이다.

## 결론

| 질문 | 답 |
|---|---|
| DLL 8개 내보내기 | **134** 이름 (모듈 간 중복 0) |
| 공개 헤더 선언(`XPE_API`/`GSVG_API`) | **122** |
| 차이 12 | `xpe_ai` 시험 훅 6 (`xpe_ai_test_set_*`) + `xpe_preprocess` 의 C++ 장식 이름 6 (`?read_xcal_file@@…` 등). 134 = 122 + 6 + 6 |
| docs §0 표(합 110) | 헤더 목록과도 내보내기 목록과도 **안 맞는다** — 헤더 기준으로 13개 빠지고 1개(`xpe_crc32`, 철회됨) 남아 있다 |
| docs §4 표(합 80) | 내보내기·헤더 어느 쪽과도 안 맞는 행이 6개(preprocess·basic·advanced·display·dicom·gsvg). §4 는 §0 보다 더 낡았다 |
| 39행 "106 names" | §0 표의 합(110)과도 안 맞는 산문. 어느 시점의 값이다 |
| 권고 | "개수" 는 **공개 헤더 선언 수 = 122** 한 가지로 정의하고, 내보내기 수와의 차이(훅 6, 장식 이름 6)는 한 단락으로 설명한다 |

## 1. 산출물 출처와 sha256

run 38223364628, 커밋 `e421c1ff15ef74a5865d58d9d58741d9a8b413c2`, 워크플로 "XPE CI Pipeline", push, 성공. 도구: `dumpbin /exports`(MSVC 14.44 의 것, 이름 읽기에는 컴파일러 버전이 영향 없다).

| DLL | artifact (id) | sha256 |
|---|---|---|
| xpe_common, xpe_enhance_basic, xpe_enhance_advanced, xpe_display, xpe_ai, gsvg | `xpe-ci-post-binaries` (11735264032) | `dll_sha256.txt` |
| xpe_preprocess | `xpe-ci-preprocess-binaries` (11735825787) | `dll_sha256.txt` |
| xpe_dicom | `xpe-ci-dicom-binaries` (11735247658), `bin/` 아래 | `dll_sha256.txt` |

이 artifact 들은 **시험 빌드**(RelWithDebInfo + BUILD_TESTS)다. 별도의 "출하(Release)" 네이티브 DLL artifact 는 이 실행에 없다(아래 Gaps).

## 2. 표 (분석 스크립트 `analyze217.py.txt`, 결과 `analysis_tables.txt`)

| DLL | 내보내기 | 헤더 선언 | 내보내기에만 있음 | 헤더에만 있음 |
|---|---:|---:|---|---|
| xpe_common | 16 | 16 | - | - |
| xpe_preprocess | 55 | 49 | C++ 장식 이름 6: `read_xcal_file`, `write_xcal_file`, `write_xcal_file_ex`, `validate_xcal_header`, `validate_xcal_session_field`, `xcal_bytes_per_pixel` | - |
| xpe_enhance_basic | 10 | 10 | - | - |
| xpe_enhance_advanced | 9 | 9 | - | - |
| xpe_display | 8 | 8 | - | - |
| xpe_ai | 17 | 11 | `xpe_ai_test_set_after_verify_hook`, `…_before_file_read_hook`, `…_before_state_delete_hook`, `…_log_capture`, `…_mutex_held_hook`, `…_worker_path_hook` | - |
| xpe_dicom | 11 | 11 | - | - |
| gsvg | 8 | 8 | - | - |
| **합계** | **134** | **122** | | |

대조군(두 목록이 같은 이름을 실제로 찾는가): 8개 DLL 모두 양쪽이 찾은 공통 이름이 16·49·10·9·8·11·11·8개로 0 이 아니다 — 도구가 아무것도 못 읽어서 "차이 0" 이 나온 경우가 아니다. 전체 내보내기 134개 이름은 서로 다른 DLL 사이에 겹치지 않는다.

내보내기에만 있는 이름의 성격:
- **xpe_ai 시험 훅 6**: 정의 6곳 모두 컴파일 조건 안에 있다(5개 `#ifdef XPE_AI_TEST_HOOKS`, 1개 `#ifdef XPE_AI_TEST_LOG_CAPTURE`). `release` 프리셋은 `XPE_AI_TEST_HOOKS=OFF`, 옵션 기본값은 BUILD_TESTS 를 따른다(`ai_test_hook_guards.txt`). 따라서 출하 빌드의 xpe_ai 는 헤더 수 11 과 같을 것으로 **추정**한다(출하 구성 DLL 은 만들어 보지 않았다).
- **xpe_preprocess 장식 이름 6**: 내부 헤더 `modules/preprocess/src/xcal_reader.hpp`(공개 헤더 아님)가 C++ 함수에 `XPE_API` 를 붙여 내보내기 표에 새어 나온다. C ABI 가 아니다. 의도한 노출인지는 판단하지 않았다(제안: 별도 카드로 `XPE_API` 제거 여부 판단).

## 3. 두 사본의 표가 무엇과 맞는가 (`analysis_tables.txt` 하단)

`docs/project/api-spec.md` (origin/main):

| DLL | §0 개수 | §4 개수 | 내보내기 | 헤더 | §0 은 | §4 는 |
|---|---:|---:|---:|---:|---|---|
| xpe_common | 16 | 16 | 16 | 16 | 맞음 | 맞음 |
| xpe_preprocess | 45 | 18 | 55 | 49 | 헤더에 없는 `xpe_crc32`(철회, QA-A-152) 포함, 헤더에 있는 5개 누락: `xpe_calib_generate_nonlin_lut`, `xpe_calib_load_nonlin_lut`, `xpe_calib_unload_nonlin_lut`, `xpe_preprocess_is_initialized`, `xpe_preprocess_pipeline_out` | 18 은 어느 것과도 안 맞음 |
| xpe_enhance_basic | 8 | 8 | 10 | 10 | `…_get_max_threads`, `…_set_max_threads` 누락 | 8 (같은 2개 누락) |
| xpe_enhance_advanced | 7 | 7 | 9 | 9 | `…_get_max_threads`, `…_set_max_threads` 누락 | 7 (같은 2개 누락) |
| xpe_display | 8 | 6 | 8 | 8 | 맞음 | 6 (2개 누락) |
| xpe_ai | 11 | 11 | 17 | 11 | 헤더와 맞음 (내보내기와는 시험 훅 6 차이) | 헤더와 맞음 |
| xpe_dicom | 11 | 10 | 11 | 11 | 맞음 | 10 (1개 누락) |
| gsvg | 4 | 4 | 8 | 8 | `xpe_gsvg_get_max_threads`, `…_set_max_threads`, `…_process_ex`, `…_process_masked` 누락 | 4 (같은 4개 누락) |
| 합 | 110 | 80 | 134 | 122 | | |

읽는 법: §0 은 "헤더를 훑어 만든 표" 라고 스스로 밝히는데, 생성 이후 헤더에 더해진 함수(스레드 수 get/set 6개, gsvg `_ex`·`_masked`, 보정 비선형 LUT 3개 등)를 따라가지 못했다. §4 는 더 오래된 값이다(QA-B-39 시점 등). 두 표 모두 "내보내기" 를 센 것이 아니라 "헤더를 센 값이 낡은 것" 이며, 시험 훅은 어느 표에도 없다.

`.moai/project/api-spec.md` 사본: §0·§4 모두 위와 같고, 추가로 `xpe_ai` 가 10 (`xpe_ai_worker_state` 누락), `xpe_dicom` §0 이 10 (`xpe_dicom_version` 누락), §4 합계 79.

## 4. 정정 문구 초안

`patch_draft_api-spec_counts.txt` — 두 사본 각각에 대해 (파일 / 찾을 줄 전체 / 바꿀 줄 전체). 표 행은 헤더 측정 목록에서 생성했다(손으로 맞추지 않음). 내용: §0 행 4~5개(preprocess 49, basic 10, advanced 9, gsvg 8, .moai 사본은 ai 11·dicom 11 추가), §4 행의 개수, 합계 122, 39행 "106 names" → "122 names", 그리고 "무엇을 세는가" 단락 한 개(훅 6, 장식 이름 6, 134 = 122 + 6 + 6). `docs/`·`.moai/project/` 는 리더 소유라 직접 고치지 않았다.

## 5절 요약 (verification-claim-integrity §3)

**Claim**: 위 결론 표. **Evidence**: 이 폴더의 `exports_*.txt`(dumpbin 원본), `analysis_tables.txt`, `ai_test_hook_guards.txt`, `dll_sha256.txt`, 스크립트 `analyze217.py.txt`·`make_patch217.py.txt`.
**Baseline-attribution**: run 38223364628(`e421c1ff`)의 artifact 에 `dumpbin /exports`, 같은 커밋의 헤더를 `git show e421c1ff:<경로>` 로 읽음. api-spec 은 `origin/main` 의 두 사본(이 측정 시점에 가져온 것).
**Gaps (미검증)**:
- 출하(Release) 구성으로 만든 DLL 은 보지 않았다. xpe_ai 가 출하에서 11 이 된다는 것은 소스의 컴파일 조건과 프리셋 값에서 읽은 추정이다.
- 헤더 파싱은 정규식이다(`XPE_API`/`GSVG_API` 다음 함수 이름). 대조군은 통과했고 내보내기와 이름이 모두 일치하지만, 매크로로 만든 선언이나 다른 내보내기 매크로가 있었다면 놓친다 — 내보내기에만 있는 이름이 위 12개뿐인 것이 그런 누락이 없다는 반증이다.
- xpe_preprocess 장식 이름 6개가 의도된 것인지는 판단하지 않았다.
- api-spec 의 본문 §5 이하(함수별 절)가 내용상 맞는지는 이 카드 범위가 아니다.
**Residual-risk**: 헤더가 바뀔 때마다 표가 다시 낡는다. 표를 손으로 유지하지 말고 이 스크립트처럼 생성하는 쪽이 안전하다(제안: 문서 점검 단계로 CI 에 넣을지는 리더 판단).
