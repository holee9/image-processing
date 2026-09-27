# QA-A-85 — 기계 없는 성능 수치 전수 (SPEC-XPE-P1A 전체 + preprocess docs)

**카드**: `.moai/lanes/pre/inbox/QA-A-85.md` · **브랜치**: `dev/preprocess` · **HEAD**: `c27aeaf` (리더의 A-84 정정 포함) · **파일 변경 없음, 재측정 없음**

행 번호는 모두 `c27aeaf` 기준이다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | 11개 파일에서 성능·계수 후보 **114줄**을 기계적으로 뽑고, 문단·절 단위로 읽어 분류했다. 114줄 전부가 정확히 한 분류에 들어갔음을 스크립트로 확인했다 |
| C2 | **A(기계 있음) 22줄 + 경계 3줄**, 모두 `spec.md` 의 측정 서술이다 |
| C3 | **B(기계 없음, 출처는 추적됨) 11줄** — 전부 `spec.md` 안. 인용된 보고서가 모두 존재하고 수치가 그 안에 있으며, **그 보고서들에는 기계가 적혀 있다**. 후보 밖에서 1건 더(`spec.md:281`) |
| C4 | **B+C(기계 없음, 유도 근거도 없음) 목표 수치 31줄** — 55 / 95 / 500 ms 계열과 그 변형이 7개 파일에 흩어져 있다 |
| C5 | **`acceptance.md:587` PERF-007 이 `spec.md:195` 에서 폐기된 `< 35ms / < 12ms` 를 그대로 들고 있다** |
| C6 | **`spec.md:560` 은 표의 출처를 `XPE-ALG-001` 로 적지만, 그 문서(12,498줄)에 `55 ms`·`95 ms` 가 없다** (대조군: 같은 명령이 ms 값 86줄을 찾음) |
| C7 | **카드가 "있었다"고 한 `1830/1830` 이 `plan.md:435` 에 아직 있다.** `acceptance.md:612` 에서만 제거됐다 |
| C8 | 제 첫 검색 패턴이 C7 을 **놓쳤다**("parity" 단어). 넓힌 패턴으로 다시 돌려 찾았다 |
| C9 | `modules/preprocess/docs/test_suite_report.md`, `quality-report.md`, `security-fixes-report.md` 에는 성능 수치가 없다 |

---

## 2. 증거 (Evidence)

### 방법

1. **후보 추출** — `a85_scan.py`(스크래치패드). 패턴: `숫자 + ms/µs/ns/fps/Hz/GB/s/MB/s/sec`, `숫자x`·`숫자×`, `숫자x faster/slower/over/away/cheaper`, `숫자/숫자`, `숫자% faster/speedup/overhead`. 각 줄에 대해 **같은 문단**(빈 줄·빈 `>` 줄로 구분)과 **앞 6줄·뒤 3줄**에 기계 단어가 있는지 표시.
   기계 단어: `this machine`, `development machine`, `개발 기계`, `이 기계`, `i7`, `12700`, `P-core`, `E-core`, `CI runner`, `runner`, `windows-latest`, `Golden Cove`, `Gracemont`.
2. **자동 표시는 분류가 아니다.** 114줄 중 88줄이 "문단·주변에 기계 없음"으로 표시됐지만, `spec.md` 의 인용 블록은 **절 머리글**(예: "Measured on the shipped implementation, this machine (QA-A-61):")이 표 두세 문단 위에 있어 자동 표시가 놓친다. 그래서 `spec.md:209-300` 은 **통째로 읽고** 절 단위로 분류했다.
3. **C 는 찾아본 뒤에만.** 기계 없는 측정값은 인용 카드의 보고서를 열어 수치가 있는지 확인했다. 목표 수치는 `.moai/reports/lane-pre/`, `.moai/specs/SPEC-XPE-P1A/`, `docs/` 전체에서 유도 근거를 찾았고, 명시된 출처(`XPE-ALG-001`)는 원문을 열었다.

### 대조군 — 패턴이 눈멀지 않았는가

첫 패턴(`ms|fps|x|×|숫자/숫자 checks|tests|passing`)으로 파일별 계수 후, **카드가 존재했다고 한 값**으로 확인:

```
$ grep -rn "405/405\|1830/1830" .moai/specs/SPEC-XPE-P1A/ modules/preprocess/docs/
acceptance.md:612:> **`1830/1830` removed 2026-09-16 (QA-A-75, #160).** ...
plan.md:435:- BP-SIMD addendum (1830/1830 parity) passing (REQ-P1A-040)
spec.md:447:> **The dispatch override never existed, under four different names.** ...
```

**`plan.md:435` 는 첫 패턴의 계수 5줄 안에 없었다** — `1830/1830 parity` 는 `checks|tests|passing` 바로 앞이 아니라서다. 패턴을 `\b\d+/\d+\b` 로 넓혀 다시 돌렸고(후보 114줄), 그 안에 들어왔다.

### 분류 결과 요약

`a85_classify.py` 가 114줄 각각에 분류를 붙이고, 미분류 0 · 중복 0 · 스캔에 없는 키 0 을 확인했다.

| 분류 | 줄 수 | 위치 |
|---|---|---|
| A — 기계 있음 | 22 | `spec.md` 195·197·199·200·201·206·208·215·216·238·239·240·241·249·258·261·263·275·276·279·287·299 |
| 경계 (앞 문단의 기계 표기에 기대는 캡션·파생값) | 3 | `spec.md` 218·243·247 |
| **B — 기계 없음, 출처 추적됨** | **11** | 아래 표 1 |
| **B+C — 기계 없음, 유도 근거 없음 (목표)** | **31** | 아래 표 2 |
| **C — 근거 없는 통과 계수** | **2** | 아래 표 3 |
| 성능 아님 (제외) | 45 | 아래 §제외 |
| **합계** | **114** | |

후보 밖에서 통째로 읽다가 찾은 B 1건(`spec.md:281`)은 위 합계에 넣지 않았다.

### 표 1 — B: 기계 없음, 출처 보고서 있음

| 위치 | 원문(발췌) | 출처 카드 | 보고서에 수치 | 보고서의 기계 |
|---|---|---|---|---|
| `spec.md:222` | "the current measured value (732.1 ms, QA-A-55) plus ~10% ... spread is 17-23%" | QA-A-55 | `732.1` 있음 | "이 기계" |
| `spec.md:245` | "The probe reported global sigma at 26.0 ms on 12 threads (5.52x)" | QA-A-58 | `26.0`·`5.52` 있음 | "이 기계" |
| `spec.md:269` | "3072x3072 single thread went **692.7 -> 163.7 ms**, putting the SPEC target 2.7x away instead of 11.5x" | QA-A-65 / A-66 | `692.7` 있음 | i7-12700 |
| `spec.md:289` | "one of two selections took 70% of the time (97.0 ms) while ... 14.2 ms — 6.8x cheaper" | QA-A-67 | `97.0`·`14.2` 있음 | i7-12700 |
| `spec.md:293` | "branching \| signed differences \| 28.9 ms" | QA-A-67 | `28.9` 있음 | i7-12700 |
| `spec.md:294` | "branchless \| signed differences \| 5.6 ms" | QA-A-67 | (같은 표) | i7-12700 |
| `spec.md:295` | "**branching** \| **absolute deviations** \| **4.4 ms**" | QA-A-67 | (같은 표) | i7-12700 |
| `spec.md:297` | "1 KB 26.8 ms vs 256 KB 27.3 ms ... (186.5 -> 188.8 ms after adding it)" | QA-A-67 | `26.8`·`188.8` 있음 | i7-12700 |
| `spec.md:313` | "30,704 of 9,437,184 pixels (0.33%), 2.149 ms ... gate sample spread of 8%" | QA-A-68 | `30,704` 있음 | i7-12700 |
| `spec.md:317` | "0.23% ... about 1.4% at width 512 ... roughly 58x slower ... 30,704 -> 12,284 ... 2.149 -> 0.985 ms" | QA-A-69 | `12,284` 있음 | i7-12700 |
| `spec.md:475` | "giving up **692.7 -> 163.7 ms** on the 3072x3072 detection (QA-A-65, bit-identical)" | QA-A-65 | `692.7` 있음 | i7-12700 |

**후보 밖의 B 1건 — `spec.md:281`**: "the regression band (AVX2 forced off, a real rebuild) is 7.060 - 7.154". 단위가 없는 비율이라 자동 후보에 없었고, `:209-300` 을 통째로 읽다가 찾았다. 같은 문장 앞부분의 정상 대역(1.251-1.720)에는 "across P-core, E-core, and CI" 로 기계가 있지만 **회귀 대역에는 기계가 없다.** 출처는 QA-A-66 이며, 보고서 수치 대조는 하지 않았다.

**요지(사실)**: 표 1 의 11줄은 모두 `c27aeaf` 이전부터 인용 카드 번호를 달고 있고, 그 보고서의 머리에 기계가 적혀 있다. **SPEC 문장으로 옮기면서 기계가 떨어졌다.**

### 경계 3줄 — 판단이 필요한 곳

| 위치 | 원문 | 왜 경계인가 |
|---|---|---|
| `spec.md:218` | "Memory is not the constraint (1.16 ms at a measured 40.7 GB/s)" | 바로 앞 표의 머리글(`:211` "QA-A-56, this machine")과 같은 인용 블록의 다음 문단. 출처 A-56 에 `40.7` 있음, 기계 i7-12700 |
| `spec.md:243` | "Whole-detection measured total at 16 threads: **102.3 ms**." | 바로 앞 표의 머리글(`:234` "Measured on the shipped implementation, this machine (QA-A-61)")에 이어지는 캡션 |
| `spec.md:247` | "Even at saturation the detection sits **1.7x** over the 60 ms target, against the 1.44x the probe suggested" | 위 표(A)와 A-58 프로브(B)에서 파생된 배수. 문단 자체에는 기계 없음 |

### 표 2 — B+C: 기계 없음, 유도 근거 없음 (목표 수치)

| 위치 | 원문 | 비고 |
|---|---|---|
| `spec.md:138` | "**Performance**: < 55ms for 3072x3072 UINT16 frame (scalar); < 15ms (AVX2)" | offset |
| `spec.md:152` | "**Performance**: < 55ms for 3072x3072 UINT16 frame (scalar); < 15ms (AVX2)" | gain |
| `spec.md:170` | "**Performance**: < 95ms for 3072x3072 UINT16 frame (scalar); < 30ms (AVX2)" | defect |
| `spec.md:564-567` | 표 "Offset < 55ms / < 15ms · Gain < 55ms / < 15ms · Defect < 95ms / < 30ms · Full Pipeline < 500ms / < 100ms" | 머리글 `:560` "**출처: XPE-ALG-001**, 3072x3072 UINT16 기준" — 아래 참조 |
| `acceptance.md:390` | "And total processing time < 500ms (scalar) or < 100ms (AVX2)" | AC-PIPE-001 |
| `acceptance.md:581-586` | PERF-001~006: 55/15, 55/15, 95/30, 500/100, 50/N/A, 200/80 ms | 표 머리글 "Target (Scalar) \| Target (AVX2)" |
| **`acceptance.md:587`** | "**PERF-007 \| Runtime detection (Hampel) \| 3072x3072 U16 \| < 35ms \| < 12ms**" | **`spec.md:195` 가 2026-09-12 에 폐기한 수치.** 프레임 형식도 `U16` (`spec.md:220` 은 검출 경로가 FLOAT32 라고 적음) |
| `plan.md:468` | "A4 \| Performance target 달성 (< 55ms offset/gain) \| Benchmark test" | |
| `research.md:147` | "Performance (3072x3072 baseline): Offset < 55ms, Gain < 55ms, Defect < 95ms, pipeline < 500ms scalar / < 100ms AVX2" | |
| `research.md:215` | "Processing time (baseline path, bilinear): < 60ms for 3072x3072 with typical 0.1% defect density" | **같은 연산(defect)인데 `acceptance.md:583`·`spec.md:170` 은 < 95 ms** |
| `research.md:220` | "Processing time: < 35ms for 3072x3072 (scalar), < 12ms (AVX2)" | `spec.md:220` 이 이 줄을 "no cited source" 로 기록 |
| `research.md:338` | "Performance budgets met (3072x3072 Offset < 55ms, Gain < 55ms, Defect < 95ms)" | |
| `spec-compact.md:250-252` | "offset correction completes in <55ms / gain ... <55ms / defect ... <95ms" | AC-PER-001 |
| `spec-compact.md:260` | "And SIMD achieves >2x speedup over scalar" | AC-PER-002 |
| `tdd-progress-report.md:176-178` | "Offset correction: <55ms / Gain correction: <55ms / Defect correction: <95ms (3072x3072)" | 제목 "Performance (Target)" |
| `simd-parity-harness.md:174-176` | "Throughput 0.5 (2x AVX2)" / "0.5 (2x AVX2)" / "1 (2x AVX2)" | AVX-512 intrinsic 표. 측정 아님 |
| `research.md:181` | "precompute `1/G(x,y)` and multiply instead of divide (3-5x faster on modern CPUs). Reference: Intel AVX-512 vs AVX2 study (Intel Dev Guide 2023)" | **외부 문헌 인용** — 출처는 있으나 기계 없음 |

줄 수: `spec.md` 7, `acceptance.md` 8, `plan.md` 1, `research.md` 5, `spec-compact.md` 4, `tdd-progress-report.md` 3, `simd-parity-harness.md` 3 = **31**.

#### 유도 근거를 찾은 기록

`55 ms`·`95 ms`·`500 ms` 를 `.moai/reports/lane-pre/`, `.moai/specs/SPEC-XPE-P1A/`, `docs/` 전체에서 검색(`< 55ms` 같은 목표 서술 자체는 제외):

```
.moai/reports/lane-pre/QA-A-41/verdict.md:148  "MAD 의 두 번째 정렬 + 복사 — 155 ms (21.9%)"   ← 무관(155)
docs/ai-module/...                              1500ms, 3000ms, 2500ms, 500ms(DLL 로드)       ← 무관(AI 모듈)
docs/archive/superseded/...                     2500ms                                       ← 무관
```

**55 / 95 의 유도 근거는 이 범위에 없다.**

`spec.md:560` 이 지목한 출처 `docs/post-processing/xpe/XPE-ALG-001_Unified_Algorithm_Development_Specification.md` (12,498줄):

```
$ grep -nE "55 ?ms|95 ?ms|500 ?ms|15 ?ms|30 ?ms|35 ?ms|12 ?ms" <ALG-001>
1052:| 처리 시간 (3072×3072) | < 30ms | 단일 코어 |          ← §3.0.9, 비선형성 LUT 검증 기준
1097:// Vectorized subtraction with floor-at-zero (SRS-PERF-001: ≤500ms)   ← xpe_offset_correct 주석
4132:| CPU 처리 | < 500 ms / 3K×3K |
4277:| GPU 추론 | < 15 ms |
... (그 외 500·30·15 ms 는 다른 알고리즘)

대조군: grep -cE "[0-9] ?ms" <ALG-001>  → 86
```

- **`55 ms`, `95 ms` 는 ALG-001 에 없다.** 대조군이 같은 파일에서 ms 값 86줄을 보므로, 명령이 눈먼 결과가 아니다.
- ALG-001 의 `xpe_offset_correct` 주석은 **≤ 500 ms (SRS-PERF-001)** 이다. `spec.md:138` 의 offset 목표는 **< 55 ms** 이다.
- ALG-001 에서 기계 단어(`i7|xeon|ryzen|reference machine|기준 하드웨어|측정 환경` 등)는 0줄이다. `단일 코어` 는 스레드 조건이지 기계가 아니다.

### 표 3 — C: 근거 없는 통과 계수

| 위치 | 원문 | 사실 |
|---|---|---|
| **`plan.md:435`** | "- BP-SIMD addendum (1830/1830 parity) passing (REQ-P1A-040)" | 같은 수치가 `acceptance.md:612` 에서 "**`1830/1830` removed 2026-09-16 (QA-A-75, #160).** The number named a harness that does not exist" 로 제거됨. `plan.md` 쪽은 남아 있다 |
| `simd-parity-harness.md:421` | "**Total test cases across all architectures: 1830 × 3 = 5490**" | 계획 문서. 이 harness 경로(`modules/preprocess/tests/simd/`)는 `acceptance.md:612` 가 존재하지 않는다고 기록 |

### 제외 — 성능이 아닌 후보 45줄

| 종류 | 위치 | 줄 수 |
|---|---|---|
| TRUST 점수 `NN/100` | `quality-report.md` 5·15~19·214~218 | 11 |
| 테스트 통과 수 `89/90` | `spec.md` 23·606·617, `progress.md` 6·22·35, `research.md` 51·103·393 | 9 |
| 버전 `1.14.x` 등 | `plan.md` 40~43, `research.md` 122·377 | 6 |
| 식별자 슬래시 `PRE-02/03/06`, `001/002/003`, `021/022` 등 | `progress.md` 39·60, `simd-parity-harness.md` 467·476, `tdd-progress-report.md` 123, `research.md` 23·234 | 7 |
| 커버리지 분수 | `tdd-progress-report.md` 164·165 | 2 |
| 레인 폭 `16x uint16` | `simd-parity-harness.md` 40~42 | 3 |
| 정확도(성능 아님) | `acceptance.md` 604 (NMSE ≥ 10x), `research.md` 207 (NMSE 14.2x) | 2 |
| 통과 기준(목표 계수) | `spec.md` 497 ("100/100 parity checks succeed per operation") | 1 |
| 이미 제거 기록 | `acceptance.md` 612, `spec.md` 447 | 2 |
| `spec.md` 측정 서술의 재언급·게이트 설명 (A 절 안의 비성능 문장) | `spec.md` 151·230 | 2 |

`89/90` 은 성능이 아니라서 제외했지만, **`405/405` 와 같은 모양의 "통과 수"** 라는 점은 적어 둔다. 4월 시점 값이고 현재 ctest 는 643 이다. 출처 실행은 찾지 않았다.

---

## 3. baseline 귀속 (Baseline-attribution)

- 후보 추출: `a85_scan.py` 출력 `a85_scan.txt` (19,124 바이트, 후보 114줄). 분류 검증: `a85_classify.py` → 미분류 0, 스캔에 없는 키 0, 합계 114.
- 분류: `sed -n` 으로 문맥을 읽은 결과. `spec.md:209-300` 은 행 번호를 붙여 통째로 읽었다.
- B 출처 확인: 인용 카드 9개(`QA-A-55/56/58/61/65/66/67/68/69`)의 `verdict.md` 존재 확인 + 수치 문자열 `grep -l`.
- 보고서의 기계: 각 `verdict.md` 에서 `i7-12700|이 기계|...` 첫 매치.
- XPE-ALG-001: 원문 grep, 대조군 86줄.
- **재측정 없음. 파일 변경 없음.**

---

## 4. 미검증 (Gaps)

- **단위 없는 비율·대역은 자동 후보에서 빠진다.** `spec.md:281` 의 `7.060 - 7.154` 는 통째로 읽다가 찾았다. `spec.md:209-300` 밖의 단위 없는 비율은 찾지 않았다.
- **표 1 의 `:281` 회귀 대역은 A-66 보고서와 대조하지 않았다.**
- **경계 3줄은 판단을 넣지 않았다.** 카드 기준("같은 문단 앞뒤에 기계가 있으면 A")을 엄격히 적용하면 셋 다 B, 절 단위로 읽으면 A 다.
- **`89/90` 의 출처 실행은 찾지 않았다**(성능 아님).
- **`XPE-ALG-001` 의 `SRS-PERF-001` 원문(SRS 문서)은 열지 않았다.** 500 ms 가 어느 기계 기준인지는 모른다.
- **`research.md:215` 의 < 60 ms 와 `acceptance.md:583` 의 < 95 ms 가 어느 쪽이 먼저인지(이력)는 보지 않았다.**
- **`modules/preprocess/` 의 소스 주석 안 성능 수치는 범위 밖이다**(카드 범위는 `docs/` 만).

---

## 5. 잔여 위험 (Residual-risk)

- **`acceptance.md:587` 의 PERF-007 이 폐기된 35/12 ms 를 들고 있는 한**, acceptance 를 기준으로 읽는 사람은 A-84 에서 고친 것과 반대되는 목표를 본다. 같은 SPEC 안에서 `spec.md:195`(≤ 60 ms, 개발 기계)와 `acceptance.md:587`(< 35 / < 12 ms, 기계 없음)이 함께 서 있다.
- **`spec.md:560` 의 "출처: XPE-ALG-001" 은 출처가 있는 것처럼 보이게 한다.** 원문에 55·95 가 없으므로, 출처 표기 자체가 추적을 막는다.
- **같은 연산에 서로 다른 목표 둘**(defect: `research.md:215` < 60 ms, `acceptance.md:583`·`spec.md:170` < 95 ms).
- **`plan.md:435` 의 `1830/1830`** — `acceptance.md` 에서만 지워진 것이 A-78 의 "한 구역만 고치고 전체가 정리됐다고 읽는" 형태와 같다.
- **이 보고서의 초안에서 요약 수치를 손으로 맞추다 틀렸다** — 후보를 113 으로 읽었고(실제 114), B+C 를 20·32 로 적었으며(실제 31), 합계를 맞추려고 **실재하지 않는 제외 행("기타 9")을 넣었다.** 최종본은 114줄 각각에 분류를 붙인 스크립트 결과로 바꿨다. 표를 손으로 요약하면 같은 일이 다시 날 수 있다.

---

Refs #144
Refs #160
