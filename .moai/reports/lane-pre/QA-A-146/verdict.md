# QA-A-146 (#209) — 36 MB 스냅숏: 분기가 아니라 **삭제**입니다

판정: 카드 §2 의 세 선택지 중 어느 것도 아닌 네 번째 — **in-place 를 허용한 채로 복사를 통째로 제거**.
근거는 §3 반증이 카드의 예상과 반대로 나왔기 때문이며, 그 결과가 이 보고서의 핵심입니다.

---

## §1 전제 확인 (조치 전)

| # | 전제 | 확인 | 결과 |
|---|---|---|---|
| 1 | 이웃 조회가 전부 `source` 에서 나온다 | `defect_correct.cpp` 전수 | **참**. `src` 를 직접 읽는 곳은 `memcpy(dst, src, …)` (`:167`) 하나뿐 |
| 2 | in-place 계약이 문서화돼 있지 않다 | `preprocess_api.h` 주석·`.moai/specs/`·`.moai/docs/`·`docs/` 전수 | **참**. 대조군: 같은 헤더의 `xpe_calib_generate_nonlin_lut` 은 인자 단위까지 적혀 있음 |
| 3 | 실제 in-place 호출자가 없다 | 아래 | **거짓 — 존재합니다** |

### §1-3 호출처 전수

검색 범위: 저장소 전체, 확장자 무관, `build/` 와 `.moai/reports/` 의 로그만 제외.

| 호출처 | 형태 | 앨리어싱 |
|---|---|---|
| `modules/preprocess/tests/test_integration.cpp:100` | `xpe_defect_correct(&buf.gainBuf, &buf.gainBuf, &buf.meta)` | **예** |
| `modules/preprocess/tests/test_integration.cpp:131` | 같음 | **예** |
| `modules/preprocess/src/pipeline.cpp:263` | 별도 벡터로 스테이징 | 아니오 |
| `clients/.../GuiPreprocessRunner.cs:155` | `ref gainOut, ref defectOut` (서로 다름) | 아니오 |
| `modules/preprocess/tests/test_defect_correct.cpp` 다수 | `img` → `output` | 아니오 |

따라서 카드 §2 표의 "있음" 행 — 분기 + "in-place 허용" 명시 — 이 1차 조치였고, 실제로 그렇게 구현했습니다.
**그 뒤 §3 반증이 이 판단을 뒤집었습니다.**

---

## §3 반증 — 갈리지 않았고, 그것이 답이었습니다

카드: *"안 갈리면 그 시험이 in-place 를 실제로 태우지 않은 것입니다."* 이 가능성을 먼저 배제했습니다.

**시험은 in-place 를 실제로 태웁니다.** `DefectCorrectTest.InPlaceMatchesOutOfPlace` 는 같은 버퍼를
입력·출력으로 넘겨(`xpe_defect_correct(&buf, &buf, …)`) 돌린 뒤, 별도 버퍼로 돌린 결과와 **원소 단위로**
비교합니다. 커버하는 경로는 셋입니다 — 군집 3×3 median(인접 결함 2개), 고립 화소 4근방 평균,
그리고 **꽉 찬 3×3 블록**(중심의 4근방이 전부 결함이라 `xpe_interpolate_pixel` 의 r=1..3 링 대체 경로가
돈다 — 이 함수가 가진 가장 넓은 읽기 반경).

**대조군**: 스냅숏을 런타임-거짓으로 완전히 무력화.

```cpp
static volatile bool kFalsifyNoSnapshot = true;
const bool aliases = !kFalsifyNoSnapshot && (src < dst + n) && (dst < src + n);
```

`volatile` 이라 컴파일러가 접지 못하고, `/WX` 도 통과합니다(`BUILD_EXIT=0`, DLL 타임스탬프 20:33:42 로 갱신 확인 — 낡은 바이너리가 아닙니다).

결과: **14/14 초록.** 링 경로를 포함해도 초록.

### 왜 갈리지 않는가 — 정적 근거

읽는 집합과 쓰는 집합이 **서로소**입니다.

- **쓰기**: 결함 맵이 표시한 화소에만 (`dm[idx] != 0` 인 위치, `defect_correct.cpp:219`)
- **읽기**: 표시되지 **않은** 화소에서만
  - `median_filter_cluster`: `if (defectMask[idx] == 0)` (`defect_correct.cpp:96`)
  - `xpe_interpolate_pixel`: `try_add` 의 `if (defectMask[idx] == 0)` — 4근방과 r=1..3 링 **양쪽 모두** (`helpers.cpp:30`)
  - `count == 0` 최종 대체는 자기 자신(`pixels[y*width+x]`)을 읽지만, 그 위치의 쓰기는 같은 문장의 대입이라 읽기가 먼저입니다

그래서 앨리어싱이든 아니든 이웃 조회가 정정된 값을 볼 수 없습니다. 스냅숏이 막던 위험은 **처음부터 존재하지 않았습니다.**

### 복사 자리를 대신하는 것은 주석이 아니라 시험입니다

위 불변식은 *현재 커널의 성질*입니다. 앞으로 어떤 커널이 결함 이웃을 읽게 되면 in-place 가 **조용히** 깨집니다 —
카드가 지적한 "빨개지지 않는 종류". `InPlaceMatchesOutOfPlace` 가 그때 빨개집니다. 소스 주석에 이 역할을 명시했습니다.

두 번째 시험 `OutOfPlaceStillCorrectsWithoutTheSnapshot` 은 반대쪽 대조군입니다 — 보정 자체를 지워도
위 쌍은 "둘 다 똑같이 틀린 값"으로 통과하므로, 4근방 평균을 **입력에서 직접 계산해** 대조합니다.

> 이 대조군은 처음 작성했을 때 빨갰고, **원인은 제 시험이었습니다.** `i % 37` 램프에서 결함 화소의
> 4근방 평균이 원래 값과 우연히 같아(1009 대 1009) "값이 바뀌었다" 단언이 성립하지 않았습니다.
> 결함 화소를 눌린 값(5.0f)으로 두어 고쳤습니다. 제품은 처음부터 옳았습니다.

---

## §2 조치 — 계약 문구 제안

구현은 in-place **허용**입니다. 계약 문구는 리더 소유(`.moai/specs/`)이므로 제안만 드립니다.

> `xpe_defect_correct` 는 `input->data == output->data` (부분 겹침 포함)를 허용한다.
> 결과는 겹치지 않는 호출과 비트 단위로 같다. 근거는 보정이 결함 화소에만 쓰고 정상 화소에서만
> 읽기 때문이며, 이 불변식은 `DefectCorrectTest.InPlaceMatchesOutOfPlace` 가 지킨다.

헤더 주석(`preprocess_api.h`)은 제 소유지만 이번 커밋에는 넣지 않았습니다 — SPEC 문구가 정해진 뒤
같은 표현으로 넣는 편이 어긋날 위험이 적습니다. **리더 판단을 기다립니다.**

---

## §4 이득 — 같은 실행에서 귀속

i7-12700, 3072×3072 FLOAT32, 결함 밀도 0.1 %, RelWithDebInfo, 워밍업 1회 + 5회 중 최소값.
같은 바이너리·같은 세션에서 스냅숏만 되살려 baseline 을 재측정했습니다(`#204` 의 18.45 ms 는 다른
세션 값이라 귀속에 쓰지 않았습니다).

| | 최소 | 5회 |
|---|---|---|
| 스냅숏 있음 (baseline) | **18.72 ms** | 18.85 / 18.72 / 19.90 / 19.86 / 21.27 |
| 스냅숏 제거 (현재) | **10.82 ms** | 11.19 / 11.42 / 11.05 / 11.13 / 10.82 |

**절감 7.90 ms (42 %).** `REQ-P1A-012` 예산 45 ms 대비 여유 **34.2 ms (4.16배)**.
`#204` 가 세운 목표는 그대로 두어도 됩니다 — 압박이 더 줄었을 뿐입니다.

측정 하네스는 임시 파일(`test_zz_a146_bench.cpp` + CMake 한 줄)로 만들었고 **되돌렸습니다**
(`grep -c` 로 두 파일 모두 0건 확인).

---

## 검증

| 항목 | 결과 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| 전체 ctest | **756 / 756** (직전 754 + 신규 시험 2건) |
| 한 프로세스 기본 순서 | `ran=687`, 실패 0 |
| shuffle seed 1 / 2 / 9 | 각각 `ran=687`, 실패 0 |
| 위생 위반 | 0 |
| in-place 호출자 회귀 | `test_integration.cpp:100`·`:131` 포함 초록 |

## 미검증 / 잔여 위험

- **부분 겹침 in-place 는 시험하지 않았습니다.** 서로소 논증은 부분 겹침에도 그대로 성립하지만,
  실제로 어긋난 오프셋으로 호출해 보지는 않았습니다. 그런 호출자도 없습니다.
- 측정은 이 기계(i7-12700) 한 대뿐입니다. 절감의 성격(메모리 대역폭 + 페이지 폴트)상 기계마다
  비율은 달라집니다. 다만 **절감이 음수가 되는 경로는 없습니다** — 순수하게 일을 덜 하는 변경입니다.
- 링 대체 경로는 3×3 블록 하나로만 태웠습니다. r=2, r=3 까지 가는 더 큰 블록은 시험하지 않았습니다.

🗿 MoAI

---

# QA-A-146b — `:167` memcpy 자기복사 가드 (리더 지적)

리더 확인 중 남은 항목: `std::memcpy(dst, src, …)` 에 `dst != src` 가드가 없었습니다.

## 왜 지금 문제가 되는가

전에는 "우연히 in-place 로 부르는 곳이 있다" 였습니다. 이번 변경이 in-place 를 **명시적 계약으로
올리므로**, `memcpy` 의 비중첩 요구를 어기는 자기복사(UB)를 계약이 보증하게 됩니다. 이 툴체인에서
동작하는 것과 표준이 보장하는 것은 다릅니다 — ASAN/UBSAN 이나 다른 구현에서 터질 자리입니다.

조치: `if (dst != src) std::memcpy(...)`. 사유를 소스 주석에 적었습니다. 부수 효과로 in-place 경로에서
프레임 복사 한 번이 더 사라집니다.

## 반증 — 그 분기를 실제로 타는가

`else` 에 센티넬을 넣어 skip 경로가 실행되는지 직접 확인:

```cpp
if (dst != src) { std::memcpy(dst, src, n * sizeof(float)); }
else            { dst[0] = -12345.0f; }   // control
```

```
test_defect_correct.cpp(140): error: Expected equality of these values:
    Which is: 1000
    Which is: -12345
[  FAILED  ] DefectCorrectTest.InPlaceMatchesOutOfPlace
```

**in-place 호출이 실제로 skip 분기를 탑니다.** 가드가 죽은 코드가 아닙니다. (`BUILD_EXIT=0`)

곁가지 확인: 같은 실행에서 `FullPipelineSmallImage` 는 이 센티넬에도 **초록**입니다 — `rc` 만 보기
때문이며, QA-A-146 본문에서 "in-place 호출자가 있어도 그 시험은 값을 안 본다" 고 적은 것과 같은 사실입니다.

## 검증 (가드 적용 후)

| 항목 | 결과 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| 전체 ctest | **756 / 756** (`CTEST_EXIT=0`) |
| 한 프로세스 네 순서 (기본 + seed 1/2/9) | 각각 `ran=687`, `PASSED 679`, 실패 0 |

## 미검증

- in-place 경로의 추가 절감은 재측정하지 않았습니다. 3072² 프레임 `memcpy` 1회분(`#204` 분해에서
  3.04 ms)이 빠지지만, in-place 호출자는 현재 시험뿐이라 제품 성능 수치로 쓸 곳이 없습니다.
- 부분 겹침(`dst != src` 이지만 범위가 겹침)은 여전히 `memcpy` 를 탑니다. 그런 호출자는 없고,
  계약 문구가 부분 겹침까지 허용한다면 `memmove` 또는 범위 비교로 올려야 합니다 — **리더 판단 필요**.

🗿 MoAI
