# QA-B-213 — 2단계 체크리스트 P1~P7 측정 보고서

작성: Lane B (xpe-post), 브랜치 `dev/postprocess`. 측정 카드이며 제품 코드는 바꾸지 않았다. 실패가 있어도 고치지 않고 기록했다.

## 결론

**통과 7 / 실패 0 / 미구현·기준 없음 0** — 단, 아래 두 단서를 붙여서 읽어야 한다.

1. **P2 는 "중앙값 기준 통과"** 이다. SPEC 문구는 호출마다 "within N ms" 인데 통계량을 정하지 않았다. 중앙값은 7단계 모두 한도 안이지만, 반복 21회 중 CLAHE·USM·Presentation 은 일부 반복이 한도를 넘었다(표 P2). 호출 단위로 엄격히 읽으면 이 세 단계는 실패다. 어느 쪽으로 읽을지는 리더 판정 사항이다.
2. **P7 은 "산출물 생성" 으로 통과** 이다. "후처리 기본이 실영상에서 쓸 만한가" 는 사용자 육안 판정이며 아직 이루어지지 않았다.

| # | 항목 | 판정 | 핵심 측정값 | 증거 |
|---|---|---|---|---|
| P1 | 기본 체인이 끝까지 돈다 | 통과 | 7단계 모두 `XPE_OK`, 출력 전부 유한, 최종 uint16 0..65535 | `p1_chain.txt` |
| P2 | 단계별 시간 | 통과(중앙값) | 아래 표 | `p2_times_run1..3.txt` |
| P3 | EI/DI | 통과 | EI=219.82, DI(HAND)=+3.42 → 경고 1건 | `p1_chain.txt` P3 절 |
| P4 | USM 과도 강조 제한 | 통과 | 상한 위반 0, 음수 출력 0 (amount 0.5 와 5.0 모두) | `p1_chain.txt` P4 절 |
| P5 | VOI 범위 / Presentation 규칙 | 통과 | VOI 출력 [0,1] 밖 0건, NaN·inf → `INVALID_INPUT` 이고 버퍼 불변 | `p1_chain.txt` P5 절 |
| P6 | 100 프레임 누수 | 통과 | 기울기 +0.0023 MiB/프레임, 양성 대조군 +1.006 | `p6_leak.txt`, `p6_memory_100_frames.csv.txt` |
| P7 | 사람이 볼 PNG | 통과(산출) | 16비트 PNG 8장 + SHA-256 | `xpe-data/m2-post-v1/` |

## 입력과 환경

- 입력: `m1-baseline-v2/wrist_lat_3072x3072_corrected_v2_f32le.raw`. 이 파일의 SHA-256 을 이번 실행 뒤 `sha256sum` 으로 직접 계산해 `d152e8eb…fdfd4`(README 의 값)와 일치함을 확인했다(`input_sha256.txt`). 통계: min 78.105, max 3711.779, mean 2198.175, p01 139.760, p99 2833.780 ADU, 비유한 0. p01·p99 는 pre 의 README 가 적은 값과 소수 셋째 자리까지 같다.
- 하니스: `tests/e2e_post_pipeline/test_m2_measure.cpp`, 커밋 `bbf1470d`. `DISABLED_` 시험 3개라 CI 의 ctest 에는 들어가지 않는다.
- 빌드: `cmake --build --preset ci-post --target test_e2e_post_pipeline --parallel 2` (RelWithDebInfo, MSVC, 공유 라이브러리). 명령 원문 `_build.bat.txt`, `_run.bat.txt`.
- 기계: 하드웨어 스레드 20, `xpe_enhance_basic_get_max_threads()` 요청값 0(자동). **로컬 개발 기계이며 다른 세션들이 동시에 돌고 있었다.** 시간은 로컬값이다.

## 기준 문구 (SPEC 본문에서 인용)

`.moai/specs/SPEC-XPE-P1B-ENH/spec.md`, `SPEC-XPE-P1B-DISP/spec.md` 에서 가져왔다.

- REQ-ENH-006 로그 ≤ 15 ms · REQ-ENH-012 노이즈 ≤ 100 ms · REQ-ENH-017 CLAHE ≤ 50 ms · REQ-ENH-022 USM ≤ 20 ms (각각 "WHILE processing a 3072x3072 float32 image … SHALL complete … within")
- REQ-DISP-008 Modality ≤ 20 ms · REQ-DISP-016 VOI ≤ 16 ms · REQ-DISP-028 Presentation ≤ 25 ms
- REQ-ENH-021: 오버슛은 `max(original * 2.0, original + amount * threshold)` 로 클램프, 출력 화소는 0 미만이 아니다(2026-10-03 하한 추가)
- REQ-ENH-023/023a/024/026: `EI = K_cal * (mean / S0_reference)`, `K_cal = 100.0` 은 미교정 기본값, `DI = 10 log10(EI / EIT)`, DI 가 [-3, +3] 밖이면 WARNING 경보
- REQ-DISP-012: VOI 출력은 `[minOut, maxOut]` 안 · REQ-DISP-021: 유한 값은 [0,1] 로 클램프, NaN·±inf 는 `XPE_ERR_INVALID_INPUT` 이고 버퍼 불변 · REQ-DISP-033: 호출 범위를 넘어 사는 힙 할당 없음

**SPEC 이 값을 정하지 않아 내가 고른 것** (지어낸 기준이 아니라 측정 조건이다):
- 로그 `normFactor = 1000` — REQ-ENH-001 은 식만 정한다. 기존 E2E 시험(`test_e2e_full_pipeline.cpp`)이 쓰는 값을 따랐다.
- VOI 창 — LINEAR, Modality 출력의 1~99 분위수(center 2793.1208, width 1366.7012), 출력 [0,1]. 창을 정하는 규칙이 SPEC 에 없어 카드 지시대로 영상 분위수를 썼다.
- Presentation LUT — 1024칸 선형 램프, `gsdfEnabled = 0`(GSDF 는 2단계 범위 밖, #248).
- P6 판정 문턱(프레임당 기울기·시작 대비 증가분)은 SPEC 에 없다. 이 보고서는 문턱을 선언하지 않고 측정값과 대조군을 함께 적는다.

## P1 — 체인

로그 → 노이즈(bilateral) → CLAHE → USM → Modality → VOI → Presentation, 각 함수의 헤더 기본값. 단계 후 통계(로그 영역 값 = 1000·log10(ADU+1)):

```
after log              min=1898.2052 max=3569.6990 mean=3269.1015 p01=2148.4780 p99=3452.5190 nonFinite=0
after noise_bilateral  min=1935.3500 max=3556.1650 mean=3269.1015 p01=2148.4895 p99=3452.0901 nonFinite=0
after clahe            min=1935.5490 max=3556.1650 mean=3252.9333 p01=2110.2471 p99=3476.4695 nonFinite=0
after usm              min=1935.5490 max=3556.1650 mean=3252.9412 p01=2109.7703 p99=3476.4714 nonFinite=0
after modality         (USM 과 동일, identity)
after voi              min=0.0000 max=1.0000 mean=0.8374 nonFinite=0
Presentation uint16    min=0 max=65535, 0 인 화소 95027(1.01%), 65535 인 화소 105813(1.12%), bitsAllocated=16, dataSize=18874368
```

## P2 — 단계별 시간 (ms, 3072², 같은 프로세스, 반복 21회: 첫 회 + 이후 20회)

각 반복은 해당 단계의 입력(앞 단계 출력 스냅샷)에서 시작한다. 세 번 실행했다. 반복 단위 원값은 `p2_times_run*.txt` 에 있다.

| 단계 | 한도 | 중앙값(3회 실행) | 최대(3회 실행) | 한도 초과 반복(실행별, 21회 중) |
|---|---|---|---|---|
| 로그 | 15 | 5.11 – 5.25 | 6.4 – 7.1 | 0 · 0 · 0 |
| 노이즈(bilateral) | 100 | 36.4 – 37.6 | 38.4 – 43.8 | 0 · 0 · 0 |
| CLAHE | 50 | 45.5 – 47.2 | 50.7 – 54.5 | 3 · 4 · 2 |
| USM | 20 | 19.0 – 19.8 | 20.9 – 22.5 | 6 · 3 · 8 |
| Modality | 20 | 3.7 – 4.0 | 4.3 – 5.0 | 0 · 0 · 0 |
| VOI | 16 | 10.7 – 11.2 | 12.0 – 14.4 | 0 · 0 · 0 |
| Presentation | 25 | 23.3 – 23.6 | 25.6 – 27.8 | 3 · 3 · 2 |

관찰: USM 중앙값이 한도 20 ms 의 1~5% 안쪽에 붙어 있다(19.0–19.8). CLAHE 중앙값은 45.5–47.2 ms 로 한도 50 에 가깝다. 최소값(CLAHE 43.9–44.6, USM 18.3–18.4, Presentation 22.5–22.9)은 모두 한도 안이다. 한도 초과가 로컬 부하 때문인지 코드 때문인지는 이 측정으로 가를 수 없다(아래 Gaps).

## P3 — EI/DI

입력 영상(보정된 ADU, 로그 전)에 `xpe_calc_exposure_index`:

```
bodyPart="HAND"   rc=0 EI=219.817505 DI=+3.420623 → 경보 1건 severity=1 "Exposure deviation: DI=3.42 (outside [-3.0, +3.0] range)"
bodyPart=""       rc=0 EI=219.817505 DI=+0.410323 → 경보 0건 (EIT 기본값 200)
bodyPart="CHEST"  rc=0 EI=219.817505 DI=+0.410323 → 경보 0건
```

EI 가 부위와 무관하고 DI 만 부위에 따라 달라지는 것은 REQ-ENH-023 정정(#154)이 규정한 모양 그대로다. 범위 안/밖 대조를 위해 같은 실영상에 상수를 곱한 사본도 돌렸다(×0.25 → DI −2.60 경보 0, ×0.5 → +0.41 경보 0, ×2 → +6.43 경보 1, ×4 → +9.44 경보 1). 범위를 벗어난 세 경우에만 경보가 났다.

**이 값의 의미에 대한 한계**: `K_cal = 100` 은 미교정이고(REQ-ENH-023a) `S0_reference = 1000` 은 ALG-SPEC 기준선이다. 이 영상의 촬영 조건은 알 수 없으므로, DI = +3.42 가 "과노출" 이라는 임상적 판정은 이 측정이 뒷받침하지 않는다. 확인된 것은 "값이 나오고, 범위 밖에서 경보가 난다" 까지다.

## P4 — USM

실영상의 USM 직전 영상에 USM 을 적용하고 REQ-ENH-021 의 상한을 화소마다 대조했다(`out > max(2·in, in + amount·10)` 이면 위반).

```
amount=0.5 (기본)  radius=2.0 threshold=10.0: 바뀐 화소 124019(1.31%), 상한 위반 0, 음수 출력 0, 클램프에 걸린 화소 0, max|out-in|=60.8052, max(out/in)=1.022352
amount=5.0 (범위 최대): 바뀐 화소 124019(1.31%), 상한 위반 0, 음수 출력 0, 클램프에 걸린 화소 0, max|out-in|=608.0530, max(out/in)=1.223516
```

단계별 실행과 USM 단독 재실행의 최대 변화량은 같다(60.8052). 가장 크게 바뀐 화소 둘레의 행 프로파일은 `p1_chain.txt` 에 입출력 값으로 있다(`2488.2/2495.1 2483.5/2496.3 2466.8/2484.9 …` 처럼 경계의 밝은 쪽에서 +7~+18, 어두운 쪽에서 −7~−15).

**한계**: 이 실영상에서는 amount 를 최대로 올려도 `out/in` 이 1.22 라 상한(2배)에 닿는 화소가 하나도 없다. 즉 "상한을 어기지 않는다" 는 확인되었지만 **상한 클램프가 실제로 작동하는 장면은 이 영상에서 관측되지 않았다.** 클램프의 작동 자체는 이 측정의 범위 밖이다.

## P5 — VOI와 Presentation

```
VOI 출력 범위 [0.0, 1.0]: min=0.000000 max=1.000000 below=0 above=0 nonFinite=0
Presentation 출력: min=0 max=65535
주입 대조 (실영상 VOI 출력 사본에 화소 주입. 주입은 보고서에 밝힌 인위 조작이다):
  NaN 을 [0] 에 주입    → rc=-1 (Invalid input parameter), 버퍼 바이트 불변=1, 형식은 float32 그대로=1
  +inf 를 [2] 에 주입   → rc=-1 (Invalid input parameter)
  -5.0 과 7.0 을 주입(유한) → rc=0, out[0]=0 (=lut[0]), out[1]=65535 (=lut[1023])
```

REQ-DISP-012 와 REQ-DISP-021 이 적은 동작과 일치한다. 이 절의 로그에 보이는 `[error] xpe_apply_presentation_lut failed: code -1` 두 줄은 위 NaN·+inf 주입의 기대된 거부 로그다.

## P6 — 100 프레임 누수

100 프레임 각각이 입력 복사 → 7단계 → 해제를 거치고, 프레임이 끝날 때마다 프로세스 비공개 메모리(PrivateUsage)를 잰다(VOI 창은 첫 프레임으로 고정하여 분위수 정렬이 메모리 계열에 섞이지 않게 했다).

```
[real chain]                    private MiB: 첫 프레임=37.29  프레임11-20 중앙값=37.43  프레임91-100 중앙값=37.59  끝-시작=0.164 MiB  기울기(프레임21-100)=0.00234 MiB/프레임
[control: 1 MiB/frame injected] private MiB: 첫 프레임=38.61  프레임11-20 중앙값=53.70  프레임91-100 중앙값=134.18  끝-시작=80.477 MiB 기울기(프레임21-100)=1.00597 MiB/프레임
최대 작업 집합(peak working set) = 133.4 MiB (관찰값; 후처리에는 SPEC 한도가 없다)
```

양성 대조군은 같은 판정기를 **실제로** 프레임당 1 MiB 를 새게 만든 계열(touch 한 페이지를 붙들고 있음)에 적용한 것으로, 판정기가 누수를 보면 기울기 1.006 을 낸다는 것을 보여 준다. 실제 체인의 기울기는 2.4 KiB/프레임이다. 이 크기가 "누수 없음" 이라는 문턱은 SPEC 이 정하지 않았다 — 위 두 값을 나란히 놓는 것이 이 보고서의 판정 근거이다(대조군의 약 1/400).

## P7 — PNG

`D:/workspace-github/xpe-data/m2-post-v1/` 에 16비트 PNG 8장, `sha256.txt`, `README.txt`(명령·매개변수·SHA). `00_input_adu.png` 는 pre 가 만든 `…DISPLAY_ONLY_p1-p99_linear_16bit.png` 와 SHA-256 이 같다(`b97741bb…a3fa9`) — 하니스의 입력 읽기와 창 계산이 pre 의 것과 일치한다는 독립 대조다. `05_modality` 는 `04_usm` 과 바이트가 같다(Modality 가 identity 이므로 당연함). 01~06 은 각 단계 고유의 1~99 분위수 창이라 파일 간 밝기를 비교할 수 없다. 07 만 실제 체인 출력이다.

## 배경 기울기 관찰 (고치지 않음)

배경 공기 두 곳(300×300 화소, 표시 PNG 를 보고 눈으로 고른 상자: 왼쪽 위 150..450, 오른쪽 아래 2600..2900)의 평균(`p1_background_gradient.txt`):

| 영역 | 오른쪽아래 / 왼쪽위 | 차이 |
|---|---|---|
| 입력 ADU | 1.4248 | +834.8 ADU |
| 로그 영역 | 1.0467 | +153.9 |
| CLAHE 후(로그 영역) | 1.0494 | +159.0 |
| 최종 uint16 | — | 53270.9 → 60898.6 (+7627.7, 출력 범위의 11.6%) |

- **CLAHE 는 기울기를 줄이지 않았다**: 두 상자의 차이가 로그 영역에서 153.9 → 159.0 으로 같거나 약간 커졌다.
- **VOI 창에 대한 영향**: 입력의 기울기(로그 영역 153.9)는 VOI 창 폭(1366.7)의 0.113배이다. 배경이 최종 영상에서 53271 → 60899 로 밝기 차이가 남고, 두 상자 모두 65535 포화 화소는 0% 이다. 포화한 화소(전체의 1.12%)는 이 상자들 밖에 있다. 창을 정하는 분위수에는 공기와 사지가 섞여 있으므로, 배경 기울기는 창의 위치와 폭에 영향을 주는 입력이다(정량 분리는 하지 않았다 — Gaps).
- 육안(축소 미리보기 기준): CLAHE 출력 이후 팔의 오른쪽 가장자리를 따라 밝은 띠가 보인다. 사용자 육안 검토 대상이다.

## 5절 요약 (verification-claim-integrity §3)

**Claim**: 위 표의 P1~P7 판정.
**Evidence**: 이 폴더의 `p1_chain.txt`(체인·P3·P4·P5), `p2_times_run1..3.txt`(P2), `p6_leak.txt` + `p6_memory_100_frames.csv.txt`(P6), `p1_background_gradient.txt`, `p7_png_list.txt` 의 원문 출력. 위 코드 블록은 그 원문에서 옮긴 것이다.
**Baseline-attribution**: 하니스 `bbf1470d`, `ci-post`(RelWithDebInfo) 빌드, 입력 `d152e8eb…fdfd4`. 기준 문구는 SPEC `1.3.x` 본문에서 인용.
**Gaps (미검증)**:
- 영상은 손목 측면 한 장뿐이다. 다른 부위·선량에서의 거동은 측정하지 않았다.
- P2 의 초과가 기계 부하 탓인지 코드 탓인지 가르지 못했다. 같은 코드를 부하가 없는 기계(CI 러너)에서 재면 가를 수 있다. CI 의 중앙값은 리더가 CLAHE 37 ms 라고 적은 바 있어 로컬보다 낮다 — 이 보고서는 CI 값을 측정하지 않았다.
- USM 상한 클램프의 작동은 실영상에서 관측되지 않았다(P4).
- EI/DI 의 절대 크기(K_cal, S0_reference)는 교정되지 않아 임상 의미를 판정할 수 없다(P3).
- 배경 기울기가 CLAHE·VOI 창에 주는 영향은 한 영상의 두 상자 평균으로 본 관찰이며, 기울기를 제거한 영상과의 대조 실험은 하지 않았다(금지 사항).
- NLM 노이즈 감소, GSDF, 고급 처리, GSVG, AI 는 범위 밖(마일스톤 문서의 "뒤로 미룬다")이라 측정하지 않았다.
- 하니스에 P2 의 반복별 원값 출력을 나중에 덧붙였다. 체인(P1·P3·P4·P5)과 P6 출력은 그 덧붙임 이전 빌드에서 얻었고, 해당 시험의 소스는 덧붙임 전후로 같다. P2 는 덧붙인 빌드로 3회 다시 돌렸다.
**Residual-risk**: P2 판정은 기계 부하에 따라 달라질 수 있다. P6 의 "누수 없음" 은 100 프레임·한 입력·Windows PrivateUsage 기준이며, 프로세스 밖 자원(핸들, GPU)은 보지 않았다.
