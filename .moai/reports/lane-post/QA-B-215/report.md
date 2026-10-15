# QA-B-215 — 같은 소스의 enhance_basic 이 빌드마다 다른 수치를 낸다 (3단계 U4, #251)

측정·조사 카드. 제품 코드는 바꾸지 않았다. 바꾼 것은 측정 하니스 시험 하나(`DISABLED_StageDump_215`, `tests/e2e_post_pipeline/test_m2_measure.cpp`)뿐이다.

## 결론

| 질문 | 답 |
|---|---|
| 소스가 다른가 | **아니다.** CI 가 만든 시점(`39f9727a`)과 이 브랜치의 `modules/enhance_basic` 트리 해시가 같다(`a641739943ee…`, `source_identity.txt`) |
| 원인 | **컴파일러 버전 차이로 읽힌다.** CI 는 Visual Studio 18 의 MSVC 14.51.36231, 로컬은 VS 2022 의 14.44 (`ci_post_build_job_log_head.txt`, `dumpbin_headers_*.txt`). 같은 CMake 프리셋(`ci-post`)·같은 소스인데 `/arch:AVX2 /fp:fast` 로 컴파일된 양방향 필터 출력이 다르다. 14.29 와 14.44 는 서로 바이트까지 같고, 14.51 만 다르다 |
| 처음 갈라지는 단계 | **(6) 양방향 잡음 제거.** (5) 로그는 바이트 동일, (6) 은 화소 56.9% 가 다르고 최대 7 ulp(상대 5.8e-7) |
| 스레드 수 의존 | **없다.** 같은 DLL 에서 스레드 1·4·20·자동이 단계마다 같은 해시. 결정성 결함이 아니다 |
| common·spdlog·fmt 교체 | **수치에 영향 없다.** 이 하니스에서 CI 의 세 DLL 을 섞어도 네 단계와 최종 uint16 해시가 그대로다. gui 의 97.22% 혼합 구성은 재현하지 못했다(아래 4절) |
| 크기 | 최종 uint16 에서 9431047/9437184 동일(99.935%), 최대 차 384, 다른 화소 6137개(0.065%). gui 가 낸 숫자와 일치한다 |

## 1. 컴파일 조건 대조

| 항목 | post 로컬 | CI | 출처 |
|---|---|---|---|
| 소스 | `modules/enhance_basic` 트리 `a641739…` | 같음 (`39f9727a`) | `source_identity.txt` |
| 프리셋 | `ci-post` (RelWithDebInfo) | `cmake --preset ci-post` | `.github/workflows/ci.yml` |
| 옵션 | `/O2 /Ob1 /arch:AVX2 /fp:fast /EHsc -std:c++17 -MD` | 같은 CMakeLists 에서 나옴(명령행 자체는 CI 로그에 출력되지 않아 직접 관측하지 못했다) | `ninja_commands_enhance_basic_local.txt` |
| 컴파일러 | MSVC 14.44.35207, 링커 14.44 | **MSVC 14.51.36231, 링커 14.51** (VS 18 Enterprise) | `dumpbin_headers_post.txt` / `_ci.txt`, CI 로그 |
| 코드 크기(.text) | 0x18000 | **0x57000** (약 3.6배) | `dumpbin_headers_*.txt` |
| 선택 규칙 | 로컬 vcvars | `tools/ci/Use-MsvcDevShell.ps1` 의 `vswhere -latest` — **고정되지 않음** | 스크립트 |

CI 의 DLL 은 이 PC 의 같은 런타임에서 돌렸다. 두 DLL 의 차이는 **컴파일(·정적으로 묶인 라이브러리) 때** 생긴 것이지 실행 환경이 아니다.

변형 빌드(같은 `ninja -t commands` 명령행에서 한 가지만 바꿈, 스크립트 `variant215.py.txt`):

| 변형 | 단계별 해시·최종 uint16 | 의미 |
|---|---|---|
| `asis` (명령행 그대로 수동 재빌드) | preset 빌드와 **동일** | 대조군: 수동 재빌드가 빌드를 정확히 재현함 |
| `msvc1429` (MSVC 14.29, 링커 14.29 확인) | preset 빌드와 **동일** | 14.29 와 14.44 사이엔 차이가 없음 |
| `fpprecise` (`/fp:precise`) | 세 번째 결과. (5) 로그부터 5.1% 화소가 다름 | `/fp:fast` 의 SVML 경로가 정밀 모드와도 다르다. 어느 쪽도 "정답" 이 아니라 구현이 정한 값 |
| CI 14.51 DLL | 위 셋과 **다름** | 14.51 에서 `/fp:fast` 코드 생성(또는 정적으로 묶이는 SVML 구현)이 바뀐 것으로 읽힌다 |

양방향 필터는 화소마다 `std::exp` 를 부르는 범위 가중치 루프(`noise_reduce.cpp` 147–149행)를 `/fp:fast` 에서 SVML `vexpf8` 로 벡터화한다(소스 주석 18–36행에 의도로 적혀 있다). 로그 단계도 SVML 인데 14.44 와 14.51 에서 같으므로, 갈라지는 지점은 `exp` 구현이나 누산 순서의 재배열 쪽이다. **어느 쪽인지(코드 생성 대 정적 SVML 구현)는 14.51 컴파일러가 이 PC 에 없어 가르지 못했다.**

## 2. 처음 갈라지는 단계 (post 대 CI, `compare_post_vs_ci_enh.txt`)

| 단계 | 다른 화소 | 최대 차 | 최대 ulp |
|---|---|---|---|
| (5) 로그 | 0 (0%) | 0 | 0 |
| (6) 양방향 | 5,367,490 (56.9%) | 0.0017 | 7 |
| (7) CLAHE | 3,133,948 (33.2%) | 1.58 | 6,754 |
| (8) USM | 3,145,167 (33.3%) | 7.02 | 41,002 |
| 최종 uint16 | 6,137 (0.065%) | 384 | — |

증폭 (`amplification_post_vs_ci.txt`): 양방향 차이는 전부 0.01 미만(0.001 초과 3,635개)인데, CLAHE 뒤에는 0.1 초과 4,624개, 1 초과 3,835개다. 작은 부동소수 차이가 CLAHE 에서 화소 수천 개의 값을 크게 움직인다. 공간적으로 몰려 있지 않고 흩어져 있다(행 2383가지·열 2362가지). 이 형태는 "히스토그램 칸 경계에 걸린 화소가 이웃 칸으로 넘어간다" 와 맞지만, **칸 넘김 자체를 직접 보지는 않았다**(가설).

최종 영상의 크기(`diff_magnitude_post_vs_ci.txt`): RMSE 1.68, PSNR 91.8 dB, 다른 6137개는 전부 차이 64 이상이고 128 이상은 35개, 256 이상은 15개. 8비트 렌더(`>>8`)로는 1563개(0.017%)가 다르고 최대 차 2다. 자동 창은 중심이 같고 폭만 1198.49561 대 1198.49634 로 달랐다.

## 3. 스레드 수 (`matrix_hashes_run1.txt`)

| DLL | 스레드 1 / 4 / 20 / 자동 |
|---|---|
| post | 네 단계 해시 전부 같음 (`b6f5783b…`, `4f31e563…`, `29347ead…`) |
| CI | 1 / 4 / 자동 전부 같음 (`0f5c29d7…`, `2c06a31…`, `4fa88ed8…`) |

두 번 반복(`matrix_full_chain_2runs.txt`)도 같은 해시였다. 병렬 누산 순서에 따른 비결정성은 없다.

## 4. common·spdlog·fmt 교체

| 구성 | 최종 uint16 해시 |
|---|---|
| post 전부 | `a5a96773…` |
| post + CI 의 fmt·spdlog·common | `a5a96773…` (**동일**) |
| CI enhance_basic 만 | `fa6c1004…` |
| CI 의 fmt·spdlog·common·enhance_basic | `fa6c1004…` (**CI enhance 만과 동일**) |

enhance 네 단계와 표시 단계(post display)의 수치는 common·spdlog·fmt 에 의존하지 않는다. 두 빌드의 `modules/common` 차이도 로그 주석·`XPE_FLAG_CORRECTION_BYPASSED` 정의 한 줄뿐이고 수치 코드가 없다(`git diff 39f9727a HEAD -- modules/common`).

gui 가 본 "post enhance+display 에 CI common/spdlog/fmt 를 섞으면 97.22%, 최대 65" 는 이 하니스로 재현되지 않았다. 원인을 찾지 못했으므로 **재현 실패와 미해결**로 적는다. 가능한 설명은 하나 있으나 확인하지 못했다: GUI-C-233b 가 "한 프로세스에 두 `xpe_common` — 출력 폴더의 오래된 복사본" 을 밝혔으므로, 그 혼합 실험에서 실제로 어느 `xpe_enhance_basic.dll` 이 적재됐는지(프로세스 모듈 목록과 해시)를 확인해야 한다.

## 5. 처방 제안

요구 문구 초안 `draft_REQ-ENH_determinism_v1.txt`. 요약:

1. **결정성 범위는 "같은 DLL" 이다.** 같은 DLL·같은 입력·같은 매개변수는 스레드 수와 무관하게 비트 동일(이 카드가 관측). 소스가 같아도 컴파일러·옵션이 다르면 동일성은 주장하지 않는다. 214d 에서 자동 창에 적은 것과 같은 선이다.
2. **기준 영상의 출처를 DLL 로 적는다.** 2단계 기준 영상(m2-post-v2)은 post 로컬 빌드(14.44)로 만들었다. 출하는 CI 빌드이므로, 판정 근거 영상은 출하 DLL 로 다시 만들거나, 만든 DLL 의 컴파일러 버전과 sha256 을 영상에 붙인다. 2단계 판정 항목(구조, 값 범위, 시간, 누수)은 차이 크기(PSNR 91.8 dB, 8비트 렌더 최대 차 2)에 비춰 옮겨지는 것으로 읽히지만, 비트 단위로 적은 항목이 있으면 옮겨지지 않는다.
3. **비트 동일 대조(U4)는 앱과 모듈을 같은 DLL 로 돌린다.** GUI-C-234 가 CI 결합 DLL 한 벌로 다시 재면 닫힌다. 이 카드의 하니스 결과로는 같은 DLL 에서 결정적이므로 일치가 예상된다(관측은 아님).
4. **CI 의 컴파일러가 고정되지 않았다.** `vswhere -latest` 라서 러너 이미지가 올라가면 소스가 같아도 출하 수치가 바뀐다. 선택지: 도구 모음 버전을 고정하거나, 빌드 기록에 컴파일러 버전을 남기고 릴리스마다 기준 영상을 그 DLL 로 다시 만든다. 이건 `tools/ci/`·워크플로 변경이라 리더 소유다 — 제안만 한다.
5. **코드 쪽 선택지(제안만, 이번엔 안 고침).** 컴파일러가 달라도 같은 수치를 원하면 `/fp:fast` 를 없애야 하는데, `/fp:precise` 도 다른 수치를 내고 소스 주석은 약 8배 처리량을 이유로 `/fp:fast` 를 택했다(속도는 이번에 재지 않았다). 비용 대비 효과가 불분명하므로 지금은 권하지 않는다.

## 5절 요약 (verification-claim-integrity §3)

**Claim**: 위 결론 표. **Evidence**: 이 폴더의 `source_identity.txt`, `dumpbin_headers_*.txt`, `ci_post_build_job_log_head.txt`, `dump_*.txt`, `matrix_*.txt`, `compare_*.txt`, `diff_magnitude_post_vs_ci.txt`, `amplification_post_vs_ci.txt`, `variant_*_build.bat.txt`.
**Baseline-attribution**: 입력 `xpe-data/m1-baseline-v2/wrist_lat_3072x3072_corrected_v2_f32le.raw`, CI DLL 은 `xpe-data/ci-dlls-39f9727a`(provenance 의 runId 37591480261), post 는 `build/ci-post/bin`(소스 HEAD `3d0a3fbc`). 시험은 같은 실행 파일 복사본에 DLL 만 바꿔 돌렸다.
**Gaps (미검증)**:
- MSVC 14.51 이 이 PC 에 없어, 차이가 컴파일러 코드 생성 때문인지 정적으로 묶인 SVML 때문인지 가르지 못했다. "컴파일러 버전" 은 가장 좁힌 관측(14.29=14.44≠14.51)이지 내부 원인이 아니다.
- CI 가 쓴 실제 컴파일 명령행은 로그에 없어서, 옵션이 같다는 것은 "같은 CMake 에서 나온다" 는 추정이다.
- 앱(WPF) 프로세스에서의 적재는 보지 않았다. gui 의 혼합 실험(97.22%)은 재현하지 못했다.
- CLAHE 칸 넘김 가설은 직접 확인하지 않았다.
- 이 하니스의 입력은 전처리 뒤 기준 영상이다. 앱 안 전처리 출력과 같다는 것은 gui 의 99.935% 일치(같은 숫자가 나옴)로 뒷받침될 뿐 별도로 비교하지 않았다.
**Residual-risk**: CI 이미지가 갱신되면 출하 수치가 다시 바뀔 수 있다(고정 없음). 기준 영상이 출하 DLL 과 다른 채로 비트 동일을 요구하는 항목이 남아 있으면 계속 어긋난다.
