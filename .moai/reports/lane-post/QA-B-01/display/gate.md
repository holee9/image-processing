# QA Gate — display

- module: display
- build: build/ci-post (Ninja, RelWithDebInfo, /WX=ON)
- branch: dev/postprocess
- sha: e67c125
- measured: 2026-08-28

| # | Gate item | Result | Evidence |
|---|---|---|---|
| 1 | dumpbin /dependents 횡단 의존성 | PASS | 의존 DLL 5개 전부 시스템/CRT. xpe_* 모듈 의존 0건 (xpe_common 조차 없음 — 헤더 온리 사용) |
| 2 | GTest 100% GREEN | PASS | 55/55 (`100% tests passed, 0 tests failed out of 55`) |
| 3 | 메모리 누수 1000 프레임 | GAP | 1000-프레임 반복 생명주기 누수 테스트 미존재. 단일 스코프 `DisplayIntegration.NoLeak_PresLutReplacesBuffer` 1건만 존재 |
| 4 | /WX 0 warning | PASS | `_build_post.log` 전체 warning 라인 0건 (display 귀속 0건 포함) |
| 5 | P/Invoke ABI 심볼 수 일치 | MISMATCH | 헤더 6 = DLL export 6 (일치). 그러나 `docs/display/README.md`가 미존재 심볼 3개 기술 (delta +3) |
| 6 | CODEOWNERS 경계 | PARTIAL PASS / GAP | `/modules/display/ @holee9` 항목 존재 (PASS). 단 HEAD가 main의 조상이라 `main...HEAD` diff가 공집합 — 경계 침범 여부를 측정할 델타 자체가 없음 (GAP) |

## Evidence (verbatim)

### [1] dumpbin /dependents
```
> dumpbin /dependents build\ci-post\bin\xpe_display.dll
Dump of file build\ci-post\bin\xpe_display.dll
  Image has the following dependencies:
    VCRUNTIME140.dll
    api-ms-win-crt-math-l1-1-0.dll
    api-ms-win-crt-heap-l1-1-0.dll
    api-ms-win-crt-runtime-l1-1-0.dll
    KERNEL32.dll
```
xpe_* 모듈 의존 0건. 기대치("xpe_common 외 횡단 의존 없음")를 충족하며, xpe_common 링크조차 없음.

### [2] ctest (display 4개 실행파일: test_display_modality_lut / _presentation_lut / _voi_lut / _integration)
스위트 매핑은 `ctest -N` 전수 조회로 확인: ModalityLut(13) + PresentationLut(15) + VoiLut(18) + DisplayIntegration(9) = 55.
```
> ctest --test-dir build\ci-post -R "^(ModalityLut|PresentationLut|VoiLut|DisplayIntegration)\." --output-on-failure
...
55/55 Test #253: DisplayIntegration.VersionString_NotNull ...........................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 55

Total Test time (real) =   1.40 sec
```
전체 원본 출력: `tests.log`

### [3] 1000-프레임 누수 테스트 탐색
```
> grep -rniE '1000|leak|repeat|lifecycle|loop' modules/display/
modules/display/tests/test_display_integration.cpp:234:// REQ-DISP-033: No heap memory outliving function scope (no leaks in happy path)
modules/display/tests/test_display_integration.cpp:237:TEST(DisplayIntegration, NoLeak_PresLutReplacesBuffer) {
```
그 외 `1000` 매치는 전부 픽셀 값/윈도우 width 리터럴(예: `make_float32_image(4, 4, 1000.0f)`, `params.width = 1000.0f`)이며 프레임 반복 횟수가 아님.
1000-프레임 반복 생명주기 테스트는 **존재하지 않음**. 지시에 따라 신규 작성하지 않음.

### [4] /WX warning
```
> grep -icE 'warning' .moai/reports/lane-post/QA-B-01/_build_post.log
0
```
빌드는 `XPE_WARNINGS_AS_ERRORS=ON`으로 완료됨. 로그 전체 warning 0건이므로 display 귀속 warning도 0건.

### [5] P/Invoke ABI 심볼
DLL export (6개):
```
> dumpbin /exports build\ci-post\bin\xpe_display.dll
           6 number of functions
           6 number of names
    ordinal hint RVA      name
          1    0 00001028 xpe_apply_modality_lut
          2    1 00001073 xpe_apply_presentation_lut
          3    2 0000113B xpe_apply_voi_lut
          4    3 00001195 xpe_display_version
          5    4 00001190 xpe_gsdf_calibrate
          6    5 00001136 xpe_voi_preset_create
```
헤더 선언 (6개, `modules/display/include/xpe/display/display_api.h`):
```
> grep -rnE 'XPE_API' modules/display/include/
:145: XPE_API const char* xpe_display_version(void);
:173: XPE_API XpeErrorCode xpe_apply_modality_lut(...)
:199: XPE_API XpeErrorCode xpe_apply_voi_lut(...)
:212: XPE_API XpeErrorCode xpe_voi_preset_create(...)
:242: XPE_API XpeErrorCode xpe_apply_presentation_lut(...)
:268: XPE_API XpeErrorCode xpe_gsdf_calibrate(...)
```
**헤더 ↔ DLL: 6 = 6 MATCH.**

문서 `docs/display/README.md` 심볼 추출:
```
> grep -oE 'xpe_[a-z_]+' docs/display/README.md | sort -u
xpe_apply_modality_lut / xpe_apply_presentation_lut / xpe_apply_voi_lut
xpe_display_version / xpe_gsdf_calibrate / xpe_voi_preset_create        ← 6개 일치
xpe_gsdf_check_display_capability   ← 미존재
xpe_gsdf_set_display_params         ← 미존재
xpe_lut_auto_select                 ← 미존재
```
미존재 3개는 소스 전체에도 없음:
```
> grep -rn 'xpe_lut_auto_select\|xpe_gsdf_set_display_params\|xpe_gsdf_check_display_capability' modules/
(출력 없음)
```
문서상 문맥은 "미구현 예정"이 아니라 사용 예시/트러블슈팅 절차로 기술됨:
```
docs/display/README.md:171: xpe_lut_auto_select("chest") → "chest_pa"
docs/display/README.md:416: 3. `xpe_gsdf_set_display_params()` 호출로 파라미터 설정
docs/display/README.md:417: 4. `xpe_gsdf_check_display_capability()`로 검증
```
**문서 ↔ ABI: MISMATCH, delta = +3 (문서에만 존재).**

### [6] CODEOWNERS 경계
```
> cat CODEOWNERS
...
# Lane B: Postprocessing (Claude)
# Branch: dev/postprocess | Worktree: xpe-post
/modules/display/          @holee9
...
```
`/modules/display/` 소유 항목 존재 확인.
```
> git diff --stat main...HEAD -- modules/display
(출력 없음)
> git diff --name-only main...HEAD
(출력 없음)
> git rev-list --count main..HEAD
0
> git merge-base --is-ancestor HEAD main
YES
> git rev-parse --short main / HEAD
28ec75f / e67c125
```
HEAD(e67c125)가 main(28ec75f)의 **조상**이다. 즉 dev/postprocess의 작업물은 이미 main에 병합되어 있고, `main...HEAD` 대칭 차분이 공집합이므로 "자기 경로만 건드렸는지"를 판정할 델타가 존재하지 않는다.

## Gaps (미검증)
- **1000-프레임 누수**: 반복 생명주기 테스트가 없어 장시간 실행 누수를 전혀 측정하지 못했다. 기존 `NoLeak_PresLutReplacesBuffer`는 단일 호출 스코프만 검증한다.
- **경계 위반 이력**: `main...HEAD` 델타가 공집합이라 이번 브랜치 기준으로는 경계 침범 여부를 측정할 수 없었다. 병합 이전 커밋 범위를 따로 지정해야 측정 가능하다.
- **CODEOWNERS 강제력**: `.github/branch-protection.json.gtmpl`의 리뷰 강제 설정이 실제 활성화되어 있는지는 확인하지 않았다(원격 설정 조회 미수행).
- **동적 누수 계측 부재**: ASan/Dr.Memory 등 도구 기반 계측을 수행하지 않았다. 3번 판정은 "테스트 존재 여부" 근거일 뿐 "누수 없음" 근거가 아니다.
- **문서 3개 심볼의 의도**: 미존재 3개가 미구현 로드맵인지 문서 오류인지는 SRS/RTM을 대조하지 않아 판별하지 못했다.

## Residual risk
- 4096x4096 / 3072x3072 단발 테스트는 통과하나, 실제 임상 연속 촬영 시나리오의 누적 할당 패턴은 검증되지 않았다(3번 GAP 직결).
- `xpe_display.dll`이 xpe_common에 링크조차 하지 않는다는 것은 공통 타입을 헤더 온리로 소비한다는 뜻이다. 향후 xpe_common이 구현부를 갖게 되면 ODR/ABI 정합이 조용히 깨질 수 있다.
- 문서에 존재하지만 export되지 않는 3개 심볼은 P/Invoke 바인딩 작성자가 그대로 선언할 경우 런타임 `EntryPointNotFoundException`으로 나타난다 — 컴파일 타임에 잡히지 않는 유형의 결함이다.
- warning 0건은 이번 ci-post 빌드 구성(RelWithDebInfo/MSVC) 기준이다. 다른 컴파일러·구성에서의 경고는 측정 범위 밖이다.
