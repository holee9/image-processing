# QA-B-207f — Codex #147 보류 2건: 정밀 모드 범위를 증명 전제에 맞추고, 최악 영상을 CI 가 재게

기준: `dev/postprocess` 929d6414 위. Refs #251. 증거 접미 `f_`.

## 1. 증명 전제와 컴파일 모드 (중간 → 고침)
**지적이 맞았다.** 5u 증명은 `R^ = fl(vmax - vmin)` 와 `inv = fl(4095 / R^)` 를 각각 IEEE 단일 반올림으로 센다. 그 두 계산은 `Binner` 생성자 안에 있었고 생성자는 `#pragma float_control(precise)` 영역 밖(= `/fp:fast`)이었다. pragma 는 함수 단위다.

**수정**: 그 연산들을 별도 함수 `precise_range` / `precise_inv` / `precise_inv_low` 로 빼 precise 영역에 두고 **noinline** 으로 표시했다(MSVC `__declspec(noinline)`). 생성자는 이 함수들을 호출만 한다. 필터의 나머지 연산(`v - vmin`, 곱, `floor`, 소수부)은 이미 precise 영역의 `filter_certifies` / `filtered_bin` / `fill_bins_filtered` 안에 있었고, 이 둘(`filtered_bin`, `fill_bins_filtered`)에도 noinline 을 붙였다. `filter_certifies` 는 precise 함수 안에서만 인라인된다(호출자의 모드를 따르는 것이 아니라 정의 시점의 precise 를 가진다; 시험은 /fp:fast 가 아닌 번역 단위라 어느 쪽이든 동일). 증명 주석에 "이 연산들은 precise 아래" 와 그 함수 목록을 적었다.

**컴파일된 코드에서 확인** (`f_disasm_summary.txt`, `dumpbin /DISASM` of `contrast_enhance.cpp.obj`, 배포 그대로의 `/fp:fast` 빌드):
| 함수 | 별도 본문 | 명령 수 | FMA | 연산 |
|---|---|---|---|---|
| `precise_range` | 예 | 4 | 0 | sub 1 |
| `precise_inv` | 예 | 3 | 0 | div 1 |
| `precise_inv_low` | 예 | 4 | 0 | div 1, mul 1 |
| `fill_bins_filtered` | 예 | 80 | 0 | sub 2, mul 1, floor 1 (= 증명이 세는 네 연산) |
- 생성자 안에서 `call ?precise_range`, `?precise_inv_low`, `?precise_inv` 로 **실제 호출**된다(인라인 안 됨). `fill_bins_filtered` 호출 5곳도 모두 call/jmp.
- 객체 전체의 FMA 22개는 `exact_bin`·`u320_to_double`(추정용)과 `xpe_contrast_enhance_impl` 의 블렌딩이며 구간 증명과 무관하다.
- `filtered_bin` 은 DLL 안에서 쓰는 곳이 없어(시험과 `Binner::bin` 전용) 본문이 목적 파일에 없다.
이것으로 "인라인되어 호출자의 fast 모드로 들어가지 않는다" 를 노인라인 + 호출 확인으로 보였다. 어셈블리의 구체 명령까지 읽은 것은 위 요약(연산 종류·FMA 유무)까지이다.

## 3. 207e 의 정확성·반증·속도가 그대로인가
| 항목 | 산출 | 결과 |
|---|---|---|
| ci-post 전체 | `b_ctest_f_post.txt` | 1451/1451 passed |
| 실제 DLL 대 Fraction (Codex 8x8 + 무작위 120) | `f_py_d.txt` | 0 differ |
| 207c 훑기 / 207b 좁은 범위 | `f_py_c.txt`, `f_py_b.txt` | 0 differ / 0 differ |
| 반증 F1 필터가 모든 곳에서 `floor(t^)` 신뢰 | `f_arm1_cpp.txt`, `f_arm1_py.txt` | C++ 5건 빨강, DLL Codex 56/64 빨강 |
| 반증 F2 정수 경로 끔 | `f_arm2_cpp.txt`, `f_arm2_py.txt` | C++ 2건 빨강, DLL 56/64 빨강 |
| 207e 대 207f 속도 (번갈아 2회, 3072²) | `f_perf_ab.txt` | 균일 소수 44~46 / 44~46, 화소 하나 0.0123 42~46 / 45~46, 정수 HU 50~53 / 50~54, 정수 HU+0.001 71~78 / 71~73 ms — 차이 없음 |

## 2. CI 가 최악 영상을 재게 (병합 조건 아님)
`ContrastEnhance.BenchmarkFreeze_Performance_REQ_ENH_017_Clahe3072` 에 `perf_measure::Measure` 를 영상별로 추가, **단언 없음**. 크기 열에 영상 이름: `3072x3072`(기존), `3072x3072 integer HU -1024..3071`, `3072x3072 decimals 0.0123..4000`, `3072x3072 integer HU -1024..3071 + one pixel 0.001`. 로컬 값(`f_cpp.txt`, 중앙값 7회): 48.0 / 53.2 / 46.3 / 73.5 ms. 출력의 비유한 값 검사는 영상마다 유지.

## Gaps
- 컴파일러가 `precise_*` 를 정말 IEEE 단일 연산(`subsd`, `divsd`)으로 냈다는 것은 위 명령 수·연산 종류로 보였으나 전체 어셈블리 줄을 하나하나 읽지는 않았다. 빠른 길 루프(`Binner::fill_bins`)가 /fp:fast 아래 어떻게 재배열됐는지는 증명이 요구하지 않아(곱 한 번씩과 정확 비교) 읽지 않았다.
- CI 에서의 값은 미관측(이 변경이 병합된 뒤 리더가 판단).

## Residual risk
- 다른 컴파일러(GCC/Clang)에서는 `float_control` pragma 가 없다. `noinline` 만으로는 `-ffast-math` 의 재결합을 막지 못할 수 있다. 이 모듈의 빌드는 MSVC 전용이다(`/fp:fast`, `/arch:AVX2` 가 MSVC 플래그).
