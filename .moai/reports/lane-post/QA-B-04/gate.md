# QA-B-04 — enhance_basic 헤더 누락 심볼 정합

- card: QA-B-04 / branch: dev/postprocess / sha: 3d4988b
- 측정일: 2026-08-31

## 결론: 코드 변경 없음. 카드의 전제가 사실과 다르다.

카드는 "`xpe_enhance_basic_version` 이 DLL 은 정상 export 하는데 **공개 헤더에 선언이 없다**"는
전제로 작업 1~3을 지시한다. **실측 결과 이 전제는 성립하지 않는다.**

공개 헤더에 이미 선언되어 있고, export 수와 헤더 선언 수는 **이미 일치**한다.
따라서 작업 2(헤더에 선언 추가)는 불필요하고, 작업 3(export 제외)은 하면 안 된다.

| 비교 축 | 수 | 일치 |
|---|---|---|
| DLL export | 8 | — |
| 공개 헤더 `XPE_API` 선언 | 8 | **일치** |
| `api-spec.md` 기재 | 7 | **−1 불일치** |

**실제 격차는 헤더가 아니라 문서에 있다.** 그리고 그 문서는 Lane B 소유가 아니다.

## Evidence (verbatim)

### E1. DLL export 8개
```
$ dumpbin /exports build\ci-post\bin\xpe_enhance_basic.dll
           8 number of functions
           8 number of names

  xpe_calc_exposure_index      xpe_contrast_enhance
  xpe_edge_enhance             xpe_enhance_basic_version
  xpe_log_inverse              xpe_log_transform
  xpe_noise_estimate_sigma     xpe_noise_reduce
```

### E2. 공개 헤더 선언 8개 — `xpe_enhance_basic_version` 포함
```
$ grep -o "XPE_API[^(]*(" modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_api.h
XpeErrorCode xpe_calc_exposure_index      XpeErrorCode xpe_contrast_enhance
XpeErrorCode xpe_edge_enhance             XpeErrorCode xpe_log_inverse
XpeErrorCode xpe_log_transform            XpeErrorCode xpe_noise_estimate_sigma
XpeErrorCode xpe_noise_reduce             const char*  xpe_enhance_basic_version

$ grep -c "XPE_API" modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_api.h
8
```

set diff = 공집합. E1 과 E2 는 **완전히 같은 8개 심볼**이다.

### E3. 작업 1 — 공개 API 로 의도된 것이 맞다 (동종 심볼 대조)
카드 작업 1 이 요구한 "다른 모듈의 동종 심볼과 대조":

| 모듈 | 공개 헤더의 version 심볼 |
|---|---|
| common | `XPE_API const char* xpe_version(void)` |
| preprocess | `XPE_API const char* xpe_preprocess_version(void)` |
| **enhance_basic** | **`XPE_API const char* xpe_enhance_basic_version(void)`** |
| enhance_advanced | `XPE_API const char* xpe_enhance_advanced_version(void)` |
| ai | `XPE_API const char* xpe_ai_version(void)` |
| display | `XPE_API const char* xpe_display_version(void)` |
| dicom | (공개 헤더에 version 선언 없음) |

7개 모듈 중 6개가 `<모듈>_version()` 을 공개 API 로 노출한다. enhance_basic 은 그 관례를
정확히 따르고 있다. **의도된 공개 API 가 맞다** → 작업 3(export 제외)은 관례를 깨는 오답이다.
(dicom 만 예외인데, 이는 dicom 쪽 누락으로 보이며 이 카드 범위 밖이다.)

### E4. 실제 불일치 지점 — api-spec.md
```
$ grep -n "xpe_enhance_basic.dll" .moai/project/api-spec.md
158:| xpe_enhance_basic.dll | 7 | includes `xpe_calc_exposure_index` moved from enhance_advanced |
```
§7 의 함수 절은 7개(7.1~7.7)만 기술하며 `xpe_enhance_basic_version` 절이 없다.

파일이 **두 벌** 존재하고 서로 다르다:
```
$ find . -name 'api-spec.md' -not -path './build/*'
./.moai/project/api-spec.md
./docs/project/api-spec.md
$ diff -q .moai/project/api-spec.md docs/project/api-spec.md
Files ... differ
```
158행은 두 벌 모두 `7` 로 동일하다.

### E5. 무회귀 확인
```
$ ctest --test-dir build\ci-post -R "EnhanceBasic|ExposureIndex|Noise|Contrast|Edge|Log"
100% tests passed, 0 tests failed out of 105
```
코드를 변경하지 않았으므로 회귀 여지가 없으나, 합격 조건 확인을 위해 실행했다.

## 소유 경계 — Lane B 가 고칠 수 없다

수정 대상은 `api-spec.md` 이고, 두 사본 모두 main 소유다.

- `.moai/project/api-spec.md` → lane-sessions.md §1: main 이 `.moai/` 소유
- `docs/project/api-spec.md` → CODEOWNERS: `/docs/ @holee9`, "Shared — modify only on main"

QA-B-01 S2 분류에서 이 건을 "문서 지연"으로 나눈 판단이 여기서 확인된다.
코드는 정상이고 문서만 낡았으며, 그 문서는 레인 밖이다.

## 합격 조건 대조

| 조건 | 판정 | 근거 |
|---|---|---|
| export 수 == 공개 헤더 선언 수 | **충족** | 8 == 8 (E1, E2). 조치 없이 이미 성립 |
| ctest 무회귀 (74/74 유지) | **충족** | 105/105 GREEN (E5). 코드 무변경 |
| 판단 근거가 gate.md 에 남을 것 | **충족** | E1~E4 |

## Gaps (미검증)

- **api-spec.md 두 사본의 diff 내용 미확인.** 158행이 같다는 것만 확인했고 전체 차이는 보지 않았다.
  두 벌이 갈라져 있다는 사실 자체가 별개 문제일 수 있으나 main 소유라 조사하지 않았다.
- **dicom 의 version 심볼 부재 미조사.** E3 에서 7개 중 dicom 만 예외임을 관측했으나
  누락인지 의도인지 판단하지 않았다. 이 카드 범위 밖이다.
- **QA-B-01 서브에이전트 기술의 재확인.** 당시 보고는 "DLL export 8 vs api-spec.md 문서 7"
  로 **정확했다**. 카드가 이를 "헤더 누락"으로 옮겨 적으면서 전제가 어긋났다.
  헤더는 처음부터 8개였다.

## Residual risk

- 이 카드는 코드를 바꾸지 않았으므로 런타임 위험이 없다.
- 다만 문서가 7 로 남아 있는 한, api-spec.md 만 보는 소비자(P/Invoke 바인딩 작성자 등)는
  `xpe_enhance_basic_version` 의 존재를 모른다. 실질 영향이 없지는 않으며, main 의 조치가 필요하다.
