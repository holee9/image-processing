# QA-A-51 — 실프레임 검증 하네스 (데이터 도착 전 제작)

사용자 결정(2026-09-12): **"실제 촬영 영상을 확보해 검증한 뒤 결정."**
이 카드는 **준비**다. 운영 코드도 기본값도 바꾸지 않았다.

추가/수정 파일
- `modules/preprocess/tools/xpe_real_frames.cpp` (신규, 실험 TU)
- `modules/preprocess/tools/xpe_struct_frames.cpp` (`--dump` 추가)
- `modules/preprocess/CMakeLists.txt` (신규 타깃 `xpe_real_frames`)
- 문서 `.moai/reports/lane-pre/QA-A-51/HOWTO.md` (§3)

---

## §1. Claim — 주장

| # | 주장 |
|---|---|
| C1 | 실프레임 입력 경로를 만들었다 — **raw(8/16/32비트)** 와 **DICOM**(런타임 `LoadLibrary`). 형식·비트·치수를 프레임마다 출력하고, **처리 못 하는 파일은 이름과 사유를 찍고** 건너뛴다 |
| C2 | **잴 수 있는 것과 없는 것을 코드에서 분리**했다. 실프레임에는 `FP`·`TPR` 을 쓰지 않고 `flagged` 만 쓴다. 합성 주입은 `INJECTED-ON-REAL` 로 별도 표기 |
| C3 | **사전 판정 기준을 숫자로 고정**해 소스에 넣고 **매 실행 첫 화면에 출력**한다 (R1 3.0배 · R2 1.0e-2 · R3 5.0배 · R4 · R5 p10~p90) |
| C4 | 자체 검증 통과 — A-47/A-49 모사 7종을 **이 도구 자신의 입력 경로**로 다시 읽어 QA-A-50 표와 대조, float32 편차 ≤0.005, 불일치 0 |
| C5 | uint16 경로의 편차(최대 0.048)는 **MAD 양자화 계단(1.0484 ADU)** 으로 설명된다 — 이건 실제 검출기 데이터에도 그대로 적용되는 성질이다 |
| C6 | **내가 만든 사전 기준 R5 가 잘못 설계되어 있었고, 실데이터가 오기 전에 스모크 실행이 잡아냈다.** 고친 근거와 경위를 소스와 이 보고서에 남겼다 |
| C7 | 반증: 추정기를 일부러 깨뜨리면 자체 검증이 7/7 MISMATCH 로 잡는다 — 통과가 의미를 갖는다 |
| C8 | 재측정 605/605, 69/69, 경고 0 |

---

## §2. Evidence — 증거

### 2-1. 자체 검증 (카드 3항)

`xpe_struct_frames --dump <dir>` 가 모사 7종을 **uint16 raw 와 float32 raw 양쪽으로** 쓴다.
uint16 은 검출기가 실제로 주는 형태이고, float32 는 QA-A-50 이 실제로 잰 것이다.
두 벌을 두는 이유가 바로 아래 결과다.

명령: `xpe_real_frames --selftest .moai/reports/lane-pre/QA-A-51/frames`
(전문 `a51-selftest.log`)

```
  frame       trueSig  |            float32             |             uint16
                       |     Bm  ref     d      Bm4  ref     d |     Bm  ref     d      Bm4  ref     d
  uniform     10.000 |  1.00x  1.00x +0.000   1.00x  1.00x -0.000 |  1.05x  1.00x +0.048   1.05x  1.00x +0.048
  scatter     17.845 |  0.99x  0.99x +0.005   0.99x  0.99x +0.005 |  1.00x  0.99x +0.009   1.00x  0.99x +0.009
  edge        12.000 |  1.39x  1.39x +0.002   1.39x  1.39x +0.002 |  1.40x  1.39x +0.008   1.40x  1.39x +0.008
  lines       12.000 |  1.00x  1.00x +0.001   1.00x  1.00x +0.001 |  0.96x  1.00x -0.039   0.96x  1.00x -0.039
  checker2    12.000 |  2.05x  2.05x +0.003   2.05x  2.05x +0.002 |  2.01x  2.05x -0.041   2.01x  2.05x -0.041
  checker8    12.000 |  1.15x  1.15x +0.003   1.15x  1.15x +0.003 |  1.14x  1.15x -0.014   1.14x  1.15x -0.014
  diag        12.000 |  2.66x  2.66x -0.001   1.00x  1.00x +0.000 |  2.62x  2.66x -0.039   0.96x  1.00x -0.039

  frames read 7/7, mismatches 0
  SELF-TEST PASS
```

**float32 열이 판정선이다.** QA-A-50 이 잰 것과 같은 데이터를, 독립적으로 구현한 추정기가
이 도구 자신의 raw 입력 경로로 읽어 **편차 0.005 이하**로 재현한다. diag 열이 특히 결정적이다 —
Bm 2.66x / Bm4 1.00x 가 그대로 나온다. 값이 우연히 같을 수 있는 자릿수가 아니다.

**uint16 열의 편차는 하네스 결함이 아니라 데이터의 성질이다.** 정수 데이터의 차분 MAD 는
정수이므로, 추정값은 `MAD × 1.4826 × 0.70711 = MAD × 1.0484` 의 **계단값만 가질 수 있다.**
σ 에 대한 비율로 쓰면 허용폭은 `1.0484 / trueSigma` — σ=10 에서 0.105, σ=12 에서 0.087 이다.
관측된 최대 편차 0.048(uniform, σ=10)은 그 절반이다. 이 허용폭은 **관측값을 보고 맞춘 것이
아니라 유도한 것**이며, 검출기가 uint16 을 주는 한 실프레임에도 그대로 적용된다.

> 처음 쓴 허용폭은 근거 없는 0.03 이었고 4건이 불일치로 떴다. 숫자를 늘리는 대신 원인을
> 찾았고, 원인이 양자화라는 것이 float32 경로가 편차 0.005 로 통과하는 것으로 증명된다.

### 2-2. 입력 경로 end-to-end (카드 1항)

명령(스모크): `--raw 1024x1024:16 --in edge.raw --in diag.raw --in nosuch.tif --inject`
(전문 `a51-smoke.log`)

```
== edge.raw ==
  source      : raw
  dims        : 1024 x 1024   bitsStored 16   format UINT16 (little-endian)   pixels 1048576
  local sigma : p01 2.965  p10 6.672  p50 14.085  p90 29.652  p99 44.478  max 142.330
  global sigma: A(value-MAD) 1974.8231   Bm(shipped) 16.7737   Bm4 16.7737
  -- shipped   (a=0.80, cap off) --
     floor 13.4190  cap 0.0000   flagged 1627   flaggedRate 1.5516e-03   1% ok
     flagged by local-sigma decile (d1=quietest .. d10=noisiest):
            161     213     200     297     493     190      70       3       0       0
     top-decile enrichment 0.00x
  -- candidate (a=0.95, b=1.20) --
     floor 15.9350  cap 20.1284   flagged 598   flaggedRate 5.7030e-04   1% ok
             42      51      38      74     128     140      70      14      17      24
     top-decile enrichment 0.40x
```

처리 못 하는 파일은 **조용히 넘어가지 않는다**:

```
SKIPPED  .../nosuch.tif
         reason: unrecognised extension 'tif' -- expected .dcm/.dicom (DICOM) or .raw/.bin/.img (+ --raw)
```

DICOM 은 **링크가 아니라 런타임 `LoadLibrary("xpe_dicom.dll")`** 다. `ci-preprocess` 프리셋이
`BUILD_DICOM=OFF` 로 구성되므로(`CMakePresets.json:94`) 링크 의존을 걸면 이 도구가 **레인이
쓰는 바로 그 프리셋에서 빌드 불가**가 된다. DLL 이 없거나 심볼이 없으면 그 사유를 그대로 찍는다.

### 2-3. 고정한 측정 항목 (카드 2항)

| 잴 수 있는 것 | 근거 |
|---|---|
| 표시 화소 수 `flagged`, 비율 `flaggedRate` | 정답이 없어도 셀 수 있다 |
| **위치 분포** — 국소 σ 십분위별 표시 수 + 최상위 십분위 집중도 | A-47 의 "구조에 몰리는가"를 정답 없이 재는 형태. 실프레임엔 구조 oracle 이 없으므로 프레임 **자기 국소 σ 십분위**로 대체했다. d10 = 가장 시끄러운 10%(경계·조직 질감이 여기 온다) |
| 전역 σ(A/Bm/Bm4) 대 국소 σ 분포(p01..max) | A-47 §1 형식 |
| 설정 간 차이(출고 vs 후보) | 같은 프레임에서 직접 대조 |
| SPEC 1% 상한 | SPEC 이 정상 촬영에 대해 주장하는 값이므로 정답 없이 판정된다 |

| 잴 수 **없는** 것 | 이유 |
|---|---|
| TPR | 결함 위치 정답이 없다. 출력에 아예 없다 |
| FPR(문자 그대로) | 표시된 화소가 진짜 결함일 수 있다. 그래서 `FP` 라는 말을 실프레임에 쓰지 않는다 |

`--inject` 는 **별개 측정**이다. 실프레임 잡음 위에 합성 결함을 얹어 회수율을 재고, 출력마다
`INJECTED-ON-REAL (… this is NOT a measurement of real defects)` 를 붙인다.

### 2-4. 사전 판정 기준 — 숫자 (카드 2항)

소스 상수이고 매 실행 첫 화면에 찍힌다(`a51-smoke.log` 첫 16줄).

```
  R1 REJECT candidate   if flagged(candidate) > 3.0x flagged(shipped)      on any frame
  R2 REJECT candidate   if flaggedRate(candidate) >= 1.0e-02 (SPEC ceiling) on any frame
  R3 REJECT cap b=1.2   if topDecileEnrich(candidate) >= 5.0x AND
                           topDecileEnrich(shipped)    <  5.0x             on any frame
  R4 ACCEPT candidate   if on EVERY frame: flagged(candidate) <= flagged(shipped)
                           AND flaggedRate(candidate) < 1.0e-02
                           AND topDecileEnrich(candidate) < 5.0x
  R5 Bm4 usable         if on EVERY frame: Bm4 <= Bm AND
                           p10(localSigma) <= Bm4 <= p90(localSigma)
  otherwise INCONCLUSIVE -- more frames, not a relaxed threshold.
```

각 숫자의 근거(소스 주석에도 동일하게 있음):

- **3.0배** — A-50 이 잰 모든 모사에서 후보 설정은 출고 설정보다 표시 수가 **적었다**(uniform
  108→6, edge 1695→630, lines 5126→2333, checker8 22→3). 실프레임에서 3배 이상 **늘어난다면**
  그건 측정 산포가 아니라 질적 역전이다
- **1.0e-2** — SPEC 자신의 상한(`sum(defectMap) ≤ W*H*0.01`, XPE-ALG-001 §9.8 / REQ-P1A-013)
- **5.0배** — A-47/A-50 의 구조 집중도는 0.00x~7.97x 였고, `lines` 는 **두 설정 모두에서**
  7.9x 근처였다. 구조만으로도 그 값이 나온다는 뜻이므로, 규칙은 **후보만 5.0을 넘고 출고는
  안 넘을 때** 발동한다 — 즉 cap 이 만들어낸 군집일 때만
- **R4 를 REQ-P1A-013 의 1e-5 FPR 로 잡지 않은 이유** — 정답 없는 프레임에서 그 요구를 판정할
  수 없고, A-40 이 이미 미달로 기록했다. 실질 결정은 **"오늘 출고되는 것보다 나쁘지 않은가"**
  이므로 그것을 기준으로 삼았다. 이건 기준 완화가 아니라 **판정 가능한 명제로의 교체**이며,
  이 문장 자체가 사전에 고정되어 출력에 찍힌다

### 2-5. 재측정

```
100% tests passed, 0 tests failed out of 605     (build/ci-preprocess)
100% tests passed, 0 tests failed out of  69     (build/ci-common)
```
`a51-ctest-pre.log`, `a51-ctest-common.log`. 전체 빌드 exit 0, `grep -ci warning` = **0**
(`a51-full-build-pre.log`, `a51-full-build-common.log`).
`tools/ci/Test-TrackedTextFiles.ps1` → `Tracked text file validation passed.`

---

## §3. 실행 문서 (카드 4항)

`.moai/reports/lane-pre/QA-A-51/HOWTO.md` — 이 카드도 보고서도 안 읽고 돌릴 수 있게 썼다.
핵심 한 줄:

```powershell
./build/ci-preprocess/bin/xpe_real_frames.exe --in frame01.dcm --in frame02.dcm --inject
# raw 라면
./build/ci-preprocess/bin/xpe_real_frames.exe --raw 3072x3072:16 --in frame01.raw --inject
```

문서가 담은 것: 빌드 명령, DICOM 이 안 읽힐 때의 두 가지 해결책, 출력 각 줄을 읽는 법,
**잴 수 없는 것(TPR·문자 그대로의 FPR)을 먼저 못 박는 절**, 실데이터 전에 자체 검증을 돌리는
절차, 증거 로그 남기는 법.

---

## §4. 반증 — 빌드 결과 포함

**가설**: "자체 검증이 통과했다는 사실에는 정보가 있다" — 즉 이 자체 검증은 **깨진 하네스를
실제로 잡아낼 수 있다.** 통과만 확인하고 끝내면, 아무것도 못 잡는 검사도 똑같이 통과한다.

**조작**: `estDiffMadMin4` 의 짝 보정 `k` 를 `0.70710678f` → `1.0f` 로 바꿨다(√2 보정 제거).

**빌드 결과** (`a51-falsify-build.log`):
```
[1/2] Building CXX object modules\preprocess\CMakeFiles\xpe_real_frames.dir\tools\xpe_real_frames.cpp.obj
[2/2] Linking CXX executable bin\xpe_real_frames.exe
BUILD_EXIT=0
```
컴파일·링크가 실제로 다시 돌았다 — 낡은 바이너리가 결과를 위장한 경우가 아니다.

**결과** (`a51-falsify.log`): **7/7 MISMATCH**, `SELF-TEST FAIL`.
float32 열이 전부 정확히 √2(=1.414)배로 뜬다 — uniform 1.00x→1.41x, lines 1.00x→1.41x,
checker2 2.05x→2.90x. 오차의 크기까지 예측대로다.

**판정**: 자체 검증은 깨진 하네스를 잡는다. §2-1 의 PASS 는 내용이 있는 통과다.

**복원 확인**: `a51-restore-build.log` exit 0(컴파일+링크 재실행), 자체 검증 `mismatches 0`,
`SELF-TEST PASS` 복귀.

---

## §5. 내가 만든 기준 하나가 틀렸고, 데이터 오기 전에 잡혔다

이 카드의 존재 이유를 그대로 보여준 사건이라 따로 적는다.

**처음 쓴 R5**: `|Bm4 − 국소 σ 중앙값| / 국소 σ 중앙값 ≤ 0.25`.

스모크 실행에서 모사 `diag` 프레임이 **R5 실패**로 떴다. 그런데 diag 의 참 σ 는 12.0 으로
**알려져 있고**, Bm4 는 11.53 — **맞는 값**이다. 틀린 쪽은 비교 대상이었다: 국소 σ 중앙값이
20.76 으로, 3×3 국소 MAD 자체가 구조에 부풀려져 있었다. **규칙이 추정기를 "정답과 일치한다는
이유로" 기각하고 있었다.**

**고친 R5**: `Bm4 ≤ Bm` 그리고 `p10(국소 σ) ≤ Bm4 ≤ p90(국소 σ)`.
전역 추정값은 그 프레임 **자신의 국소 σ 띠 안**에 있어야 한다 — 띠 밖이면 프레임의 잡음에
없는 무언가를 재고 있다는 뜻이다. 정답이 필요 없고, 차원도 없다.

**이 수정이 정당한 이유**: 실프레임은 아직 하나도 없다. 기준을 고친 근거는 **참값이 알려진
모사 프레임**이며, 그 참값이 규칙의 오설계를 증명했다. 데이터를 보고 기준을 맞춘 것이 아니라,
**기준을 데이터가 오기 전에 시험해 본 것**이다. 이 경위는 소스 주석(`T4` 항)에도 남겼다 —
조용히 숫자만 바꾸면 다음 사람은 원래 규칙이 무엇이었는지 알 수 없다.

---

## §6. Baseline-attribution — 무엇에 대고 쟀나

- **자체 검증 기준선**: QA-A-50 §1 표의 비율(소스 `refs[]` 에 하드코딩, 출력에 `ref` 열로 표시).
  A-50 로그에서 옮겨 적은 값이며, 같은 생성기·같은 seed(uniform 20260911, 나머지 20260912)
- **입력 데이터**: 이번 턴에 `xpe_struct_frames --dump` 로 새로 생성. A-50 이 쓴 메모리 내
  프레임과 같은 생성 코드이되 파일을 거친다 — 그 왕복 자체가 검증 대상이다
- **양자화 허용폭**: `RUNTIME_DETECTION_MAD_SCALE × 0.70710678` 로 **런타임에 계산**한다.
  상수를 손으로 적지 않았으므로 헤더가 바뀌면 허용폭도 따라간다
- **테스트**: 이번 트리에서 실행. 운영 소스는 이 카드에서 한 줄도 바뀌지 않았다

---

## §7. Gaps — 관측하지 않은 것

1. **실프레임은 한 장도 없다.** 이 카드는 준비이고, 하네스는 **모사 프레임으로만** 돌려봤다.
   실제 파일에서 처음 드러날 문제(패딩, 리틀/빅 엔디안, 서명 있는 픽셀, ROI 잘림, 12비트가
   16비트 컨테이너에 담긴 경우 등)는 전혀 검증되지 않았다
2. **DICOM 경로는 한 번도 실행되지 않았다.** `ci-preprocess` 에 `xpe_dicom.dll` 이 없어서
   `LoadLibrary` 실패 경로만 확인했다. **정상 경로(open→read_image→close)는 미검증**이다
3. **DICOM ABI 3개를 손으로 다시 선언했다.** `xpe_dicom` 이 시그니처를 바꾸면 링크 오류가 아니라
   **잘못된 픽셀**로 나타난다. 실프레임 첫 실행 때 치수·값 범위를 눈으로 확인해야 한다
4. **3072² 에서 돌려보지 않았다.** A-50 기준 1024² 전역 σ 만 ~30 ms, 검출은 그보다 훨씬 크다.
   실제 프레임 크기에서의 실행 시간은 미측정
5. **십분위 집중도라는 지표 자체가 검증된 적 없다.** A-47 의 구조 oracle 을 정답 없이 대체한
   것이고, "국소 σ 상위 십분위 = 구조"라는 전제는 모사에서조차 확인하지 않았다
6. **R1~R5 기준 자체가 실데이터에서 유용한지는 모른다.** R5 처럼 또 오설계일 수 있다.
   §5 는 그런 일이 한 번 실제로 있었다는 기록이다
7. 운영 `ComputeGlobalSigma` 교체, 기본값 변경, 데이터 확보 — 모두 범위 밖(카드 지시)

---

## §8. Residual-risk — 남는 위험

1. **정상 프레임 3~5장으로는 결론이 안 날 가능성이 높다.** 판정이 `INCONCLUSIVE` 로 나오는 것이
   정상적인 결과이며, 그때 기준을 낮추자는 압력이 생긴다. 출력에 "more frames, not a relaxed
   threshold" 를 박아둔 이유다
2. **"정상 프레임"에 실제 결함 화소가 들어 있다.** 검출기에는 원래 죽은 화소가 있고, 표시된
   화소 중 몇이 진짜인지 구분할 방법이 이 하네스에는 없다. 표시 수가 많다는 것이 곧 과검출은
   아니다 — 이 혼동이 가장 그럴듯한 오판 경로다
3. **합성 주입 회수율을 TPR 로 읽을 위험.** 이름과 출력 문구로 막아놨지만, 표를 옮겨 적는
   과정에서 라벨이 떨어져 나가면 그대로 오해가 된다
4. **uint16 양자화가 σ 추정을 ±1.05 ADU 계단으로 만든다.** σ 가 작은(예: 5 ADU 이하) 저선량
   프레임에서는 상대 오차가 20%를 넘는다. 이 하네스는 그 사실을 드러내지만 보정하지는 않는다
5. **하네스가 통과한다는 것이 기준이 옳다는 뜻은 아니다.** §4 는 하네스가 **자기 구현 오류**를
   잡는다는 것만 보였다. 측정 항목 선택이 틀렸을 가능성은 그대로 남는다

---

Refs #151 #148 #143
