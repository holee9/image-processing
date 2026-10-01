# QA-B-181e — 화소로 들어오는 float→int 변환 7곳과 `/fp:fast` 아래의 유한성 검사 (#233)

Refs #233

이 카드는 **측정과 보고**가 목적이다. 제품 코드는 고치지 않았다. 바꾼 것은 시험 한 줄(§6)뿐이다.

## 0. 먼저 읽을 것 (요약)

| 질문 | 답 | 근거 |
|------|----|------|
| 181d 의 `std::isfinite` 검사가 출하 `/fp:fast` 코드에 남아 있는가 | **남아 있다.** 세 파일 모두 `call _fdclass` 가 `/fp:fast` 와 `/fp:precise` 에서 같은 개수(3·4·2)로 컴파일된다 | `disassembly_isfinite_survives_fp_fast.txt` |
| 그럼 `/fp:fast` 는 문제가 없는가 | 검사는 남지만 **범위 비교의 의미가 바뀐다.** 같은 `x <= 0` 이 `/fp:fast` 에서는 NaN 을 거부하고 `/fp:precise` 에서는 통과시킨다. 그래서 NaN 만 넣는 시험은 검사를 지워도 초록일 것으로 읽힌다 (디스어셈블 기준 추론이며 그 반증은 재지 않았다, §6) | §3, §4 |
| 화소 7곳이 NaN/∞ 로 크래시하는가 | **크래시 없음, 오류 코드도 없음(rc=0).** 대신 값이 조용히 틀린다 | §1 |
| 그런 화소가 실제로 생기는가 | **생긴다.** 유한한 화소 100 하나가 `xpe_log_inverse` 를 거쳐 ∞ 가 되고, 이어서 `xpe_contrast_enhance` 가 rc=0 으로 영상 전체를 비유한 값으로 만든다 | §2 |
| 가드 비용 | 3072² CLAHE 에서 약 +1.3 ms (46.5 → 48.2 ms, +3%) | §5 |

판단은 리더의 몫이다. 아래는 그 판단에 필요한 측정이다.

## 1. 주장 1 — 7곳은 크래시하지 않지만 NaN/∞ 를 조용히 삼킨다

**Evidence** (`probe_matrix_raw.txt`, 출하 `/fp:fast` 빌드, 호출을 `__try/__except` 로 감쌈)

| 위치 | 입력 | 결과 |
|------|------|------|
| `contrast_enhance.cpp:71`, `:111` | 화소 하나 NaN | rc=0, 그 화소 출력 2103 (정상 범위 값처럼 보임). `/fp:precise` 에서는 같은 입력이 51.56 |
| 〃 | 화소 하나 +∞ / −∞ | rc=0, **영상 4096 화소 전부 비유한** |
| 〃 | 화소 하나 ±1e10, ±3e38 | rc=0, 입력 범위를 벗어난 값이 3825~4095 화소에 퍼짐 |
| 〃 | −3e38 과 3e38 (둘 다 유한) | rc=0, 4096 화소 전부 비유한 (`tmax − tmin` 이 float 을 넘침) |
| `hough_transform.cpp:55` | NaN, −∞, ±1e10, ±3e38 | rc=0, 대조군과 같은 상자 |
| 〃 | **+∞** | rc=0, **상자가 전체 영상으로 바뀜 (대조군과 다름)** |
| `display_internal.h:67` ← `presentation_lut.cpp:46`, `modality_lut.cpp:62` | NaN, ±∞, ±1e10, ±3e38 | rc=0. 표 모드와 표현 LUT 모두 NaN→0, +큰값→끝 칸, −큰값→0 으로 포화 |
| 체인: modality LINEAR → presentation | slope 또는 intercept 가 NaN/+∞ | modality rc=0 (NaN/∞ 가 그대로 이미지에), presentation rc=0 (NaN→0, +∞→65472) |
| `gsvg.cpp:497` | 가상 그리드 설정에서 `vg_pyramid_gain` 1e30 / 1e308 | 초기화 rc=0, 처리 rc=0. 1e30 은 65535 로 포화한 화소 1934개, 1e308 은 0 이 3348개·65535 가 147개 (값은 유한하게 포화) |
| `grid_dwt.cpp:511` | 전부 0, 전부 65535, 무작위, 0/65535 번갈아, 4 주기 줄무늬 | rc=0, 출력은 0..65535 안 |

`vg_denoise_k` 1e308 은 출력에 영향이 없었다 (24008~28633, 컨트롤 24008~28720).

**Baseline-attribution** — 명령: `build/g181e-probe-fast/probe181e.exe fast <표 경로> <그룹>` 을 contrast / hough / display / gsvg 그룹별로 실행. 컨트롤(NaN 없는 입력)도 같은 실행에서 나왔다 (hough 컨트롤 상자 `(30,30)-(97,97)`, gsvg 컨트롤 출력 24008~28720). 프로브 원본은 세션 scratchpad 에 있고 저장소에는 넣지 않았다.

**Gaps**
- `/fp:precise` 팔은 **contrast 와 reach 그룹만** 돌렸다. hough 는 enhance_advanced, display 는 enhance_basic 밖, gsvg 도 밖이라 그 플래그의 영향을 받지 않는다 (`/fp:fast` 는 `xpe_enhance_basic` 한 곳뿐, §4). 이 세 곳을 `/fp:precise` 로 다시 재지는 않았다.
- 화소 하나만 바꾼 입력이다. 여러 화소·영역이 비유한인 경우는 재지 않았다.
- `gsvg.cpp:497` 은 NaN 화소를 **직접** 넣는 경로가 이 프로브에 없다 (입력이 uint16 이다). 위 행은 설정 값으로 중간 계산을 극단으로 몰아 본 것이다.
- `grid_dwt.cpp:511` 도 입력이 uint16 이라 NaN 을 직접 넣을 수 없다. 극단 입력 5종만 봤다.

## 2. 주장 2 — 비유한 화소는 입력 계약만으로는 도달 불가가 아니다

모듈 입력이 uint16→float 이면 원래는 유한하다. 그러나 **공개 API 한 번을 거치면 ∞ 가 된다.**

**Evidence** (`probe_matrix_raw.txt`, `reach` 그룹, 출하 빌드)

```
reach  control: pixel 2.0, normFactor 1        log_inverse rc=0 nonfinite=0     contrast rc=0 nonfinite=0 of 4096
reach  finite pixel 1000, normFactor 1         log_inverse rc=0 nonfinite=1     contrast rc=0 nonfinite=4096 of 4096
reach  finite pixel 100,  normFactor 1         log_inverse rc=0 nonfinite=1     contrast rc=0 nonfinite=4096 of 4096
reach  finite pixel 38.5, normFactor 1         log_inverse rc=0 nonfinite=0     contrast rc=0 nonfinite=0 of 4096
```

- `log_transform.cpp:51-54` 는 `exp(px * scale_inv) - 1` 이라 `px/normFactor` 가 약 38.5 를 넘으면 float 이 ∞ 로 넘친다. `normFactor` 가 작거나 로그 영상 값이 클수록 쉽다.
- 이 ∞ 를 받은 `xpe_contrast_enhance` 는 오류 없이 영상 전체를 비유한 값으로 바꾼다.
- 같은 모양의 두 번째 경로: `xpe_apply_modality_lut` 의 LINEAR 모드에서 `slope` 1e38 이나 NaN/∞ (표 1, 체인 행).

전처리 쪽에서 읽은 것: `gain_correct.cpp` 는 게인을 유한·범위 안인지 검사한다(`is_valid_gain`), `binning_correct.cpp:39` 와 `ghost_correct.cpp` 는 비유한 입력에 `XPE_ERR_PROCESSING_FAILED` 를 돌려준다. 이 경로들은 비유한 화소를 **만들지 않는다.**

**Gaps**
- `temp_compensate.cpp` 의 `exp` (double)와 `xpe_calib_generate_gain.cpp` 의 다항식은 읽지 않았다. 비유한 출력을 낼 수 있는지 모른다.
- ci-post 프리셋은 `BUILD_PREPROCESS=OFF` 라 전처리 모듈은 이 세션에서 실행하지 않았다. 위 전처리 문장은 **코드 읽기**일 뿐 실행 증거가 아니다.
- 실제 임상 데이터에서 `log_inverse` 로 이런 값이 만들어지는지는 모른다. 확인한 것은 "공개 API 로 만들 수 있다" 까지다.

## 3. 주장 3 (카드 항목 4) — `/fp:fast` 에서도 181d 의 유한성 검사는 컴파일되어 남는다

**Evidence** (`disassembly_isfinite_survives_fp_fast.txt`)

방법: `ninja -t commands` 로 얻은 출하 컴파일 명령 그대로(`/O2 /Ob1 /arch:AVX2 /fp:fast …`)에 `/FAs` 만 더해 `noise_reduce.cpp`, `edge_enhance.cpp`, `contrast_enhance.cpp` 를 컴파일했고, `/fp:precise` 로도 한 번 더 컴파일했다.

```
/fp:fast  noise_reduce: call _fdclass = 3     /fp:precise noise_reduce: 3
/fp:fast  edge_enhance: call _fdclass = 4     /fp:precise edge_enhance: 4
/fp:fast  contrast    : call _fdclass = 2     /fp:precise contrast    : 2
```

`std::isfinite` 는 `_fdclass` 호출로 컴파일되고 결과 `> 0` (NaN·∞ 분류)이면 거부 분기로 간다. `/fp:fast` 에서 최적화기가 지워 버리지 않았다.

**그러나** 같은 파일의 범위 비교는 모드마다 다르게 컴파일된다.

```
/fp:fast    s <= 0  ->  vcomiss s,0 ; jbe reject    (비교 불가 → CF=ZF=PF=1 → jbe 가 NaN 을 거부)
/fp:precise s <= 0  ->  vcomiss 0,s ; jae reject    (피연산자 순서가 바뀌어 jae 는 NaN 을 통과)
```

이 차이가 §4 의 실물 증거(`log_inverse`)와 §6 의 시험 설계를 정한다.

**Baseline-attribution** — 위 명령과 출력이 이 실행에서 나왔다. 컴파일러는 `cl.exe` MSVC 14.44, 소스는 이 작업 트리의 HEAD `e0827103`.

**Gaps**
- 대안(`#pragma float_control(precise, on)`, 비트 패턴 검사)은 **제안만** 했고 컴파일·측정하지 않았다. 카드가 "고치지 말고 보고만" 이다.
- 디스어셈블한 것은 세 파일의 **목적 파일**이다. 링크 후 DLL(`dumpbin /disasm`)은 보지 않았다. LTCG/`/GL` 이 켜져 있지 않은 것은 컴파일 명령(`/GL` 없음)으로 확인했다.
- `gsvg.cpp`, `ai.cpp`, `display` 의 181d 검사는 `/fp:fast` 모듈이 아니라서 보지 않았다.

## 4. 주장 4 (카드 항목 5) — `/fp:fast` 를 쓰는 곳은 `xpe_enhance_basic` 한 곳이다

**Evidence**
- `git grep -n -E "fp:fast|fp:precise|ffast-math"` (CMake 파일·프리셋): 일치는 `modules/enhance_basic/CMakeLists.txt:51` 한 줄. `target_compile_options(xpe_enhance_basic PRIVATE /arch:AVX2 /fp:fast)`.
- 도입: 커밋 `b9ef9722` (2026-04-16, drake.lee, "feat(enhance_basic): SPEC-XPE-P1B-ENH 전체 구현 완료 — 67/67 테스트 통과"). `git log -S"fp:fast"` 와 `git blame` 으로 확인.
- 사유: 같은 파일의 주석이 적어 두었다. `/arch:AVX2` 와 `/fp:fast` 가 함께 있어야 SVML 자동 벡터화가 `exp`/`log` 에 걸리고 log_transform 과 bilateral 루프에서 약 8배 처리량이 난다 (SPEC-XPE-P1B-ENH §3.1).
- 따라서 **문서화된 성능 선택**이다. 사고로 생긴 플래그가 아니다.
- 7곳 중 `/fp:fast` 모듈에 있는 곳은 contrast 2곳뿐이다. 나머지 5곳(hough, display 2곳, gsvg, grid_dwt)은 전역 `/arch:AVX2` 만 받는다 (`cmake/Platform.cmake:10`, `modules/preprocess/CMakeLists.txt:79` 도 `/fp:fast` 없음).

**실물 증거 — 같은 API 검사가 플래그에 따라 갈린다** (`probe_matrix_raw.txt`, `reach` 그룹)

```
reach  pixel 2.0, normFactor NaN   /fp:fast    : log_inverse rc=-1 (거부)
reach  pixel 2.0, normFactor NaN   /fp:precise : log_inverse rc=0, nonfinite px=4096 (영상 전체 NaN)
```

`xpe_log_inverse` 의 `if (normFactor <= 0.0f) return XPE_ERR_INVALID_INPUT;` 는 `/fp:fast` 에서 NaN 을 우연히 거른다. 이 검사는 NaN 을 막으려고 쓴 것이 아니다. **출하 빌드에서는 안전하고, 플래그를 바꾸는 순간 안전하지 않다.** 이 NaN 인자는 181d 의 census(33행) 에 없다. int 변환이 아니라 NaN 전파라서 그 census 의 범위 밖이었다.

**Gaps**
- `log_transform`(정방향)의 `normFactor` 검사는 같은 모양이지만 재지 않았다.
- `/fp:fast` 가 SVML 을 켜는 데 **필요한지**는 이번에 다시 재지 않았다. 주석의 8배는 인용이다.

## 5. 주장 5 (카드 항목 3) — 화소 단위 가드의 비용은 3072² 에서 약 +1.3 ms

**Evidence** (`guard_cost_3072_alternating.txt`)

대상: 7곳 중 가장 뜨거운 contrast 히스토그램 루프(`contrast_enhance.cpp:71`). `int bin = (int)((v−tmin)*scale)` 를 NaN 안전한 `f >= 0 ? (f < NUM_BINS ? (int)f : NUM_BINS-1) : 0` 로 바꾼 DLL 을 임시로 빌드했다. 3072×3072, CLAHE 8×8 타일, 회마다 8회의 중앙값, 번갈아 4회.

| 회 | 출하 | 가드 | 차 |
|----|------|------|----|
| 1 | 46.14 ms | 47.69 ms | +1.55 |
| 2 | 46.90 ms | 48.77 ms | +1.87 |
| 3 | 45.84 ms | 46.46 ms | +0.62 |
| 4 | 48.48 ms | 49.73 ms | +1.25 |

네 회 모두 가드 쪽이 느렸다. 차의 범위는 +0.6~+1.9 ms (약 +1~4%), 중앙값은 약 +1.4 ms.

소스는 측정 후 원복했고 파일이 바이트 단위로 같음을 단언했다. 출하 DLL 해시 `a7165732dd47`, 가드 DLL `a20fb78f5dfc`.

**Baseline-attribution** — 두 DLL 은 같은 시험 exe(`time181e.exe`)를 같은 폴더 구성으로 번갈아 실행했다. 기계 상태가 코드 차이로 읽히지 않게 순차가 아닌 교대 방식이다.

**Gaps**
- 한 곳(contrast 히스토그램)만 쟀다. 나머지 6곳의 가드 비용은 재지 않았고, 이 루프가 "가장 뜨겁다" 는 것은 **코드를 읽은 판단**이지 프로파일 측정이 아니다.
- 가드가 "NaN 을 0 번 칸으로 보낸다" 는 정책은 측정용이다. NaN 을 0 으로 보낼지 거부할지는 리더 결정이다 (카드 본문).
- 이 가드는 타일 `tmax − tmin` 이 ∞ 가 되는 문제(§1 의 +∞ 행)를 막지 못한다. 그 문제는 타일 단위의 `isfinite(range)` 검사로 막아야 하고, 그 비용은 O(타일 수) 라 무시할 만하지만 재지 않았다.

## 6. 주장 6 (카드 항목 6) — CI 도 같은 `/fp:fast` 로 시험한다. 변별력 있는 입력은 +∞ 이다

**Evidence**
- `.github/workflows/ci.yml` 의 ci-post 잡은 `cmake --preset ci-post` 후 `cmake --build --preset ci-post` 이고, `/fp:fast` 는 **타깃 단위** 옵션이라 프리셋과 무관하게 `xpe_enhance_basic` 에 항상 붙는다. 그래서 CI 도 출하와 같은 설정이다.
- 문제는 시험 쪽이다. 기존 `BilateralRefusesANaNRangeSigma` 는 NaN 만 넣는다. 그런데 `/fp:fast` 에서는 `sigma_range <= 0` 이 NaN 을 거부하는 코드로 컴파일되므로(§3), **`!std::isfinite(sigma_range)` 를 지워도 이 시험은 초록일 것이다.** 이는 디스어셈블에서 읽은 추론이다. NaN 만 넣은 원래 시험에 대해 검사를 지우고 돌려 본 반증은 **재지 않았다.** 아래 반증은 +∞ 줄을 더한 시험에 대한 것이다.
- +∞ 는 `<= 0` 이 어느 모드에서도 거짓이다. 그래서 `isfinite` 가 유일한 방어이고, 시험이 정말 그 검사를 가리게 된다.

반증 (`arm_fast_sigma_range_plus_inf_summary.txt`):

```
baseline (finiteness check present), /fp:fast:            build_ok=True red_tests=0
sigma_range finiteness check removed, /fp:fast:           build_ok=True red_tests=1
    ExceptionGuard.BilateralRefusesANaNRangeSigma
```

시험에 `RunBilateral(3.0f, kInf)` 한 줄을 더했다 (`modules/enhance_basic/tests/test_exception_guard.cpp`). 검사가 있으면 초록, 지우면 빨강이다. 이 상태에서 CMake 와 소스는 원복했고 바이트 일치를 단언했다.

**제안 (코드는 안 고쳤다)**
1. 유한성 시험마다 **+∞ 사례**를 짝지운다. NaN 은 `/fp:fast` 에서 범위 비교에 가려진다.
2. 검사를 비교로만 쓰는 곳(예: `log_inverse` 의 `normFactor <= 0`)에는 `std::isfinite` 를 명시해 플래그에 의존하지 않게 한다. 지금은 `/fp:fast` 가 우연히 막고 있다.
3. 비유한 입력을 받는 모듈 함수마다 `/fp:precise` 로 한 번 더 도는 빌드 구성이 있으면 이런 가려짐이 CI 에서 드러난다. 이번 §4 의 `normFactor` NaN 은 그 구성에서만 잡힌다. 비용은 enhance_basic 한 타깃의 추가 빌드·시험 시간이다.

**Gaps**
- 다른 NaN 전용 시험(noise_reduce 의 sigma_space, edge_enhance 의 amount/radius, contrast 의 clip_limit)이 `/fp:fast` 에서 검사를 지워도 초록인지는 **재지 않았다.** `edge_enhance` 와 `noise_reduce` 의 디스어셈블이 `_fdclass` 뒤에 `vcomiss`+`jb/ja` 를 붙인 것까지만 확인했다.
- CI 로그에서 `/fp:fast` 가 실제 컴파일 명령에 붙었는지는 보지 않았다. CMake 소스와 로컬 `ninja -t commands` 로만 확인했다.

## 7. Residual-risk

- 측정은 이 한 기계의 MSVC 14.44, 한 CPU 이다. 다른 컴파일러 버전이 `vcomiss` 순서를 다르게 낼 수 있다. 그래서 "`/fp:fast` 가 NaN 을 우연히 막는다" 는 설명을 보장으로 읽으면 안 된다.
- 변환 결과는 정의되지 않은 동작이다. 오늘 보인 값(2103, 51.56 등)은 이 빌드의 관찰이고 다음 빌드에서 바뀔 수 있다. 실제로 `/fp:fast` 와 `/fp:precise` 사이에서 이미 바뀐다.
- `log_inverse → contrast` 경로가 실제 파이프라인에서 쓰이는지 확인하지 않았다 (§2 Gaps).

## 8. 이 카드가 바꾼 것

| 파일 | 내용 |
|------|------|
| `modules/enhance_basic/tests/test_exception_guard.cpp` | `BilateralRefusesANaNRangeSigma` 에 +∞ `sigma_range` 한 줄과 사유 주석 |
| `.moai/reports/lane-post/QA-B-181e/` | report.md, 디스어셈블 목록, 반증 요약, 가드 비용, 프로브 원본 출력 |

제품 코드(`modules/*/src`)는 바꾸지 않았다.
