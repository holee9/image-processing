# QA-A-98 — `[wet]` 표, `[grid]` 확인, 가상 그리드 독립 검증용 MC 영상 (#180)

**카드**: `.moai/lanes/pre/inbox/QA-A-98.md` · **브랜치**: `dev/preprocess` · **커밋**: `5c6ea0c`(도구), `2f61814`(표·영상·README) — 미푸시 · **병합 기준**: `6026cb0` · **환경**: WSL2 `Ubuntu-24.04`, CUDA 12.9, RTX 4070 Ti

| 항목 | 상태 |
|---|---|
| (1) `[wet]` | **만듦** — `tools/mcsim/tables/wet_water_csi600.csv` (+ `_fit.csv`) |
| (2) `[grid]` | **멈춤** — MC-GPU v1.3·PCD 파생판에 그리드 모델 없음 |
| (3) 검증 영상 | **만듦** — `tools/mcsim/phantoms/`(계단·경사, 80×80, 236 KB) |

post 레인 파일(`modules/gsvg/src/virtual_grid.h`, `modules/gsvg/tests/data/virtual_grid_synthetic_table.csv`)은 `git show origin/main:<path>` 로 **읽기만** 했다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | `[wet]` 구획 문법: `[wet]` 줄 다음 머리 `kvp,w0,a,b`, 식 `mu(t) = w0 - a*t/(1+b*t)` [1/cm], `L = -ln(P/I0) = mu(t)*t`, `#` 줄·빈 줄 무시(`virtual_grid.h:14, 40-48`). 만든 표의 열이 이와 같다 |
| C2 | `[wet]` 7개 kVp 맞춤: 투과 오차 최대 **0.68 %**(70 kVp), 두께 되찾기 오차 최대 **0.031 cm**, RMS ≤ 0.011 cm |
| C3 | MC 의 L 과 MC 없이 계산한 L(스펙트럼 × 물 표 × CsI 표)의 차이 최대 **0.0127**(60 kVp, 30 cm), 상대 0.16 % 이하 |
| C4 | `[grid]`: MC-GPU v1.3 과 PCD 파생판 소스에 반산란 그리드 모델이 없다. v1.3 소스 머리말에 "does not simulate … the anti-scatter grid" 가 있다. **(2)는 여기서 멈췄다.** VICTRE v1.5b 에 Day & Dance 해석 모델이 있다는 사실만 적는다 |
| C5 | 검증 영상: 계단·경사 물 팬텀, SDD 100 cm 넓은 조사(검출기 면 30×30), 80 kVp, 1차·전체·공기·경로 길이를 따로 저장. 팬텀당 1.0e10 선원 광자 |
| C6 | 영상 점검: 계단 평탄부에서 1차 L 이 `[wet]` 곡선(경로 길이 대입)과 대체로 0.01 안쪽으로 맞는다. 계단 경계·경사 계단에서는 최대 0.09 / 0.055 차이(화소 안 두께 혼합) |
| C7 | 계단 팬텀의 30 cm 단은 **조사야 밖**이다(발산으로 검출기 ±15 cm 가 팬텀에서 더 좁다). 영상에 보이는 계단은 5/10/15/20/25 cm |

---

## 2. 증거 (Evidence)

### C1 — post 레인 구획 문법 (읽기만)

`git show origin/main:modules/gsvg/src/virtual_grid.h` 발췌:

```
14: *    mu(t) = W0 - a*t/(1+b*t), L = -ln(P/I0) = mu(t)*t  (US 7,907,697 B2)
40:// One text file, four sections. Lines starting with '#' and blank lines are
41:// ignored. Each section starts with its [name] line followed by a CSV header;
44://   [kernels]   thickness_cm,kvp,model,a1,s1,a2,s2[,a3,s3,a4,s4]...
46://   [wet]       kvp,w0,a,b          mu(t) = w0 - a*t/(1+b*t)   [1/cm], t in cm
47://   [grid]      ratio,tp,ts         primary / scatter transmission of the grid
48://   [spr_cap]   thickness_cm,kvp,max_spr
```

`VgSettings`: `kvp`, `gridRatio`, `pixelPitchMm`, `airSignal`(I0 [DN]), `iterations` — 모두 필수. 출력은 65535 에서 잘린다(`clippedHigh`).
합성 시험 표(`virtual_grid_synthetic_table.csv`)의 `[kernels]` 머리에는 도구 CSV 의 추가 열이 그대로 있고 "they are ignored" 라고 적혀 있다.

### C2, C3 — `[wet]`

명령: `wet_curve.py --out-dir /root/mcsim/wet --table tables/wet_water_csi600.csv --errors tables/wet_water_csi600_fit.csv --commit 5c6ea0c`

방법: 연필빔, PCD 파생판, 2×2 cm 검출기, 비산란 광자만, CsI 600 µm(2.5 keV 빈). 두께 1–30 cm 1 cm 간격, 두께당 1e8. I0 = 공기 1 cm 슬랩(밀도 0.0012). 맞춤은 t = 0(L = 0) 포함 최소제곱, w0·a·b ≥ 0.

| kVp | w0 | a | b | 투과 오차 최대 | 두께 오차 최대 (cm) | 두께 RMS (cm) | MC−해석 L 최대 | 계수 잡음 최대 |
|---|---|---|---|---|---|---|---|---|
| 60 | 0.320332 | 0.00607909 | 0.0672916 | 0.40 % | 0.017 | 0.009 | 0.0127 | 0.57 % |
| 70 | 0.2932 | 0.00467562 | 0.0594096 | 0.68 % | 0.031 | 0.011 | 0.0067 | 0.43 % |
| 80 | 0.274991 | 0.00410444 | 0.059599 | 0.45 % | 0.021 | 0.007 | 0.0056 | 0.36 % |
| 90 | 0.261244 | 0.00348084 | 0.0567083 | 0.43 % | 0.021 | 0.007 | 0.0031 | 0.31 % |
| 100 | 0.251253 | 0.00298762 | 0.0532508 | 0.27 % | 0.013 | 0.006 | 0.0030 | 0.28 % |
| 110 | 0.243533 | 0.00260241 | 0.0499952 | 0.28 % | 0.014 | 0.006 | 0.0035 | 0.26 % |
| 120 | 0.23805 | 0.0023782 | 0.0480295 | 0.33 % | 0.017 | 0.005 | 0.0042 | 0.24 % |

MC 대 해석 L 예(1 / 10 / 20 / 30 cm):

| kVp | MC | 해석 |
|---|---|---|
| 60 | 0.3172 / 2.8384 / 5.3697 / 7.8001 | 0.3177 / 2.8372 / 5.3686 / 7.7875 |
| 80 | 0.2731 / 2.4924 / 4.7518 / 6.9229 | 0.2733 / 2.4904 / 4.7506 / 6.9199 |
| 120 | 0.2370 / 2.2193 / 4.2762 / 6.2649 | 0.2372 / 2.2184 / 4.2764 / 6.2608 |

표 머리에 "simulation-based, not calibrated", US 7,907,697 화소별 보정 필요, 가정 전부, 도구 커밋 `5c6ea0c` 를 적었다.

### C4 — `[grid]` 모델 가능성

`grid_capability.txt`(스크립트 출력):

```
MCGPU/MC-GPU_v1.3.cu                         grid-matches=1     (183행 설명문 하나)
MCGPU/MC-GPU_kernel_v1.3.cu                  grid-matches=0
MCGPUv1.3_PCD_scatterMode/MC-GPU_v1.3_PCD.cu grid-matches=1     (같은 설명문)
MCGPUv1.3_PCD_scatterMode/MC-GPU_kernel_v1.3_PCD.cu grid-matches=0
VICTRE_MCGPU/MC-GPU_v1.5b.cu                 grid-matches=20
VICTRE_MCGPU/MC-GPU_kernel_v1.5b.cu          grid-matches=7
183: *   anti-scatter grid, a bow-tie filter or a curved detector (flat-panel detector only).
```

(검색어: `antiscatter|anti-scatter|grid_ratio|septa`, 대소문자 무시)

VICTRE v1.5b(`30b5e66cf547`, 빌드하지 않음):
- `MC-GPU_kernel_v1.5b.cu:1735` "Analytical model of a 1D focused antiscatter grid based on the work of Day and Dance [Phys Med Biol 28, p. 1429-1433 (1983)]"
- `:1493-1496` 광자마다 그리드 투과 확률로 흡수 여부를 뽑는다("scatter or fluorescence in the grid not simulated")
- 입력(예제): 격자비, 주파수, 스트립 두께, 스트립·중간재의 **평균 에너지 평균 자유 경로 한 값**
- 입력 형식은 v1.3 과 다르다(A-95 보고서 참고).

### C5–C7 — 검증 영상

명령: `phantom_images.py --kind {air,step,wedge} --launches 100 --out /root/mcsim/phantoms` 세 번, 이어서 `export_phantoms.py … --export tools/mcsim/phantoms`.

| 팬텀 | 선원 광자 | 1차 광자 수 조사야 최소 | 상대 잡음 최대 | 중심 1차 | 중심 전체 |
|---|---|---|---|---|---|
| air | 10,001,280,000 | 1,724,391 (중앙 60×60 화소, json 값) | — | 7.1887 | — |
| step | 10,001,280,000 | 1988 | 2.2 % | 0.12669 | (json) |
| wedge | 10,001,280,000 | 2777 | 1.9 % | 0.10882 | (json) |

(값 단위: 화소당·이력당 CsI 흡수 에너지 [eV]. step·wedge 의 최소는 `export_phantoms.py` 가 조사야에서 가장자리 한 화소를 뺀 영역으로 센 값이다. json 의 `primary_counts_min_in_field` 는 중앙 60×60 화소 기준이라 더 크다(3692 / 3709).)

공기 영상: 조사야 안 최소/최대 = 중심의 0.9415 / 1.0026 배, 공기 산란/1차 = 0.00171.

중심 행(2행 평균) 점검 발췌 — 계단:

| x (cm) | 경로 (cm) | L MC | L `[wet]` | 차 | SPR |
|---|---|---|---|---|---|
| −14.2 | 5.05 | 1.3077 | 1.3079 | −0.0001 | 0.376 |
| −11.0 | 7.95 | 1.9827 | 2.0090 | −0.0263 | 0.875 (경계) |
| −6.2 | 10.02 | 2.4957 | 2.4964 | −0.0007 | 1.086 |
| −1.4 | 15.00 | 3.6330 | 3.6365 | −0.0035 | 2.223 |
| 0.2 | 19.99 | 4.7391 | 4.7494 | −0.0103 | 5.717 |
| 6.6 | 22.29 | 5.1948 | 5.2540 | −0.0592 | 4.682 (경계) |
| 13.0 | 25.20 | 5.8875 | 5.8884 | −0.0009 | 4.261 |

조사야 전체 |L MC − L `[wet]`|: 계단 중앙값 0.0048 / 최대 0.0881, 경사 중앙값 0.0133 / 최대 0.0554. 전체 표는 `phantom_check.txt`.

같은 두께 단 안에서도 SPR 이 위치에 따라 변한다(20 cm 단: x 0.2 cm 에서 5.72, x 5.0 cm 에서 3.59). 이웃 단의 두께가 산란에 영향을 주는 것으로 보이며, 원인은 따로 확인하지 않았다.

저장소 파일: `tools/mcsim/phantoms/{step,wedge}_80kVp_{primary,total,air,thickness}.f32`(각 25,600 B) + `.json`(각 약 1.4 KB), 합 236 KB. json 에 `dn_scale = 50000 / air_center` 와 `VgSettings` 에 넣을 값(kvp 80, pitch 4.0 mm, airSignal 50000)을 적었다. WSL 원자료 668 KB(`/root/mcsim/phantoms`).

---

## 3. baseline 귀속

- 모든 수치는 이번 카드의 WSL 실행(`/root/mcsim/wet`, `/root/mcsim/phantoms`)에서 나왔다. 도구 `5c6ea0c`(wet), `2f61814`(팬텀).
- post 레인 문법은 `origin/main` `6026cb0` 시점의 파일이다.
- 증거: `.moai/reports/lane-pre/QA-A-98/`(`phantom_check.txt`, `grid_capability.txt`, `wsl/` 에 팬텀 json·입력·첫 로그와 kVp 별 스펙트럼).

---

## 4. 미검증 (Gaps)

- **`[grid]` 표 없음**(멈춤). VICTRE 빌드·실행은 하지 않았다.
- 검증 영상을 **가상 그리드로 실제 돌려 보지 않았다**(post 레인 몫, 코드 읽기만).
- 영상은 80 kVp 한 가지, 물만, 두께 변화는 x 방향만. PMMA·다른 kVp 는 만들지 않았다.
- `[wet]` 의 t = 0 기준(I0)은 공기 1 cm 를 통과한 값이다. 팬텀 영상의 공기 영상은 공기 30 cm 를 통과한 값이다(두 I0 의 정의가 다르다).
- `thickness` 는 화소 중심을 지나는 광선 하나의 경로다. 화소 안 평균이 아니다.
- 경사 팬텀 점검 차(최대 0.055)의 원인은 0.5 cm 계단과 화소 크기 혼합으로 보았으나 따로 확인하지 않았다.
- 가상 그리드 입력 형식(행·열 방향, 부호)과 내 파일의 방향이 맞는지는 post 레인이 확인해야 한다.
- **push 하지 않았다.**

---

## 5. 잔여 위험 (Residual-risk)

- 영상·표 모두 **보정 전 시뮬레이션**이다. 가상 그리드의 오차를 이 영상으로 재면 "시뮬레이션 안에서의 오차"다. 실측 대비 0.6–0.93 배일 수 있는 산란 크기(A-95)는 그대로다.
- 커널 표와 `[wet]` 표, 검증 영상은 같은 도구·같은 가정(CsI 수직 입사, 물 표, 스펙트럼)을 공유한다. 영상은 합성곱 모델과는 독립이지만, 물리 가정까지 독립은 아니다.
- 1차 영상 잡음 최대 약 2 %. 두께가 큰 곳의 되찾은 1차를 비교할 때 이 잡음이 차이에 섞인다.
- **`5c6ea0c`, `2f61814` 는 미푸시다.**

---

Refs #180
