# GUI-C-102 (#180) — GSVG 연결의 남은 확인: 성능, 표시, 기본값 근거

## 1. 주장

| # | 주장 |
|---|---|
| C1 | 3072² 프레임에서 화면 갱신까지 걸린 시간을 모드별로 쟀고, GSVG 단계 몫과 GUI 몫을 분리했다 |
| C2 | 재적용 시 결과는 캐시되지 않는다 — 같은 설정으로 다시 적용해도 단계가 매번 다시 돈다 |
| C3 | HUD 는 이제 단언 가능하다. 그려진 문자열 자체를 뷰포트 자동화 피어가 노출한다 |
| C4 | 내보낸 `menu-command-report.json` 의 `processingChain.stages[].reason` 으로 GSVG 상태를 단언했다 |
| C5 | 선밀도 60 /cm 와 공기 신호 60000 의 영향은 **작다** — 평균 밝기 기준 0.28 / 0.53 (65535 척도) |
| C6 | gsvg 의 비네팅 보정은 GUI 경로에서 **꺼져 있다**. 그리고 그 사실은 보고서에서 보인다 |
| C7 | GUI 는 전처리 설정 JSON 을 **쓰지 않는다** (`xpe_preprocess_init(null)`). 중첩/평면 문제가 발생하지 않는다 |

## 2. 증거

### C1 — 성능 (P-01, Native, `build/ci-c72f0e8/bin`)

명령: `dotnet test … --filter "FullyQualifiedName~GsvgLargeFrameScenarios"`,
`XPE_E2E_BACKEND=Native`, `XPE_NATIVE_DIR=build/ci-c72f0e8/bin`. 원본: `build/e2e-c102/perf3.txt`, `perf4.txt`.

| 모드 | 1회차 총 / 단계 | 2회차 총 / 단계 | GUI 몫(총−단계) | post 모듈 단독 |
|---|---|---|---|---|
| 끔 | 2479 / 0 ms | 2080 / 0 ms | **2080 ms** | — |
| 격자 억제 | 2609 / 116 ms | 2515 / 100 ms | **2415 ms** | 493 ms |
| 가상 그리드 | 2022 / 31 ms | 2432 / 27 ms | **2405 ms** | 488 ms |

읽는 법 두 가지를 나눠 적습니다.

- **GUI 몫이 지배적이다.** 어느 모드든 화면 갱신까지 2.0~2.5 초이고, 그중 GSVG 단계는 27~116 ms 로 5 % 미만입니다. 나머지는 GSVG 와 무관한 공통 비용(원본 복제, 표시 파이프라인, WPF 비트맵 변환·렌더)입니다. **끔 모드도 2080 ms** 라는 것이 근거입니다.
- **단계 시간이 post 측정(493 / 488 ms)보다 작다.** 같은 크기의 프레임인데 4~18 배 빠릅니다. 이 차이는 **설명하지 못합니다**(§4 미검증). 측정 지점이 다를 가능성(제 계측은 `xpe_gsvg_process_ex` 호출 한 번을 감쌉니다), 빌드 구성 차이, 입력 영상의 성격 차이 중 무엇인지 확인하지 않았습니다. **"GUI 가 더 빠르다"고 읽지 마십시오** — 같은 것을 재고 있다는 것을 확인하지 않았습니다.
- REQ-GSVG-019 의 1.0 s 를 어느 쪽 숫자와 비교해야 하는지는 제가 정할 사항이 아닙니다. 단계만 보면 여유가 크고, 화면 갱신까지 보면 초과합니다.

### C2 — 캐시 없음 (P-02)

같은 설정으로 연속 3회 적용했을 때 단계 시간: **27 / 33 / 27 ms**. 0 ms 가 나오지 않으므로 매번 모듈을 다시 부릅니다. 캐시 조건(C-97 §6)은 GUI 경로에 구현되어 있지 않습니다. 재계산 빈도는 **적용 버튼을 누를 때마다 1회**이며, 설정이 바뀌지 않아도 같습니다.

### C3 / C4 — 표시 단언 (P-04, P-05)

P-04 가 읽은 그려진 HUD 문자열 전문:

```
SwipeVertical | zoom fit | pan 0,0 | swipe 50% | chain: preprocess=NotRequested, gsvg=Applied;
times: preprocess=0 ms, gsvg=26 ms; display input=chain
```

P-05 는 `menu-command-report.json` 을 실제로 내보내고 파싱해 단언했습니다: `processingChain.gsvgMode == "VirtualGrid"`, `stages[gsvg].status == "Applied"`, `elapsedMs > 0`, `reason` 에 `vignette=0` 포함. 7/7 통과.

### C5 — 기본값 민감도 (P-03)

| 설정 | 그려진 픽셀 해시 | 평균 밝기 | 기준 대비 Δ |
|---|---|---|---|
| 기준(60 /cm, 60000) | `36fc547e253b07f1` | 85.205 | — |
| 선밀도 40 /cm | `480be064a04022fb` | 84.926 | **0.279** |
| 공기 신호 45000 | `104dc69756a493b4` | 84.671 | **0.534** |

해시는 셋 다 다릅니다(출력이 실제로 바뀝니다). 다만 평균 밝기 변화는 0.28~0.53 으로, 8 비트 표시 척도에서 1 계단도 되지 않습니다.

### C6 — 비네팅

`GuiGsvgRunner.GsvgConfig` 에는 `vignette_correction` 키가 없습니다(§ 코드: `grid_suppression`, `virtual_grid`, `vg_*` 7개가 전부). 이득 맵도 넘기지 않습니다 — `xpe_gsvg_process_ex(handle, input, …, null, 0, null, 0, ref native)` (GuiGsvgRunner.cs:120). 모듈 기본값이 꺼짐이므로 **GUI 경로에서 비네팅 보정은 실행되지 않습니다**. 그리고 이 사실은 화면·보고서 양쪽에 보입니다: 단계 `reason` 문자열이 `vignette=0` 을 싣고(GuiGsvgRunner.cs:144), 그 문자열이 HUD 와 `menu-command-report.json` 에 그대로 실립니다. P-05 가 보고서 경로로 단언합니다.

### C7 — 전처리 설정 키 형태 (리더 질의 답)

GUI 는 전처리 설정을 JSON 으로 전달하지 않습니다. `xpe_preprocess_init(null)` (GuiPreprocessRunner.cs:61) 로 초기화하고, 보정 파일은 `xpe_calib_load_*(경로)` 로, 촬영 조건은 `XpeImageMetadata` **구조체**로 넘깁니다. 따라서 `panel.linear` 를 중첩으로 쓰는 코드는 없으며, 평면 키 규약과 충돌할 여지도 현재 없습니다. 참고로 GSVG 쪽에서 실제로 쓰는 설정 JSON 은 **평면 키**입니다(`grid_suppression`, `vg_kvp` …) — 같은 규약입니다.

### 빌드·시험

```
BUILD_EXIT=0   (dotnet build clients/ImageProcTest.E2ETests, error 0건)
Native  GsvgLargeFrameScenarios          : 총 7,  통과 7            (build/e2e-c102/perf4.txt)
Mock    GsvgLargeFrame+ProcessingChain   : 총 15, 통과 4, 건너뜀 11 (build/e2e-c102/mock1.txt)
IntegrationTests (전체)                   : 총 241, 통과 239, 실패 1, 건너뜀 1 (build/e2e-c102/int1.txt)
```

Mock 건너뜀 11건은 Native 전용 시나리오입니다(모의 백엔드에 GSVG 경로가 없음).

**실패 1건은 이 카드와 무관하며, 제 실행 환경 탓입니다.**
`ErrorCodeMappingTests.ErrorString_ForAllDefinedCodes_IsNonNullAndNotFallback(INVALID_CALIB_DATA)`
가 `xpe_error_string(-17)` 에서 `"Unknown error"` 를 받습니다. 소스에는 이미 있습니다 —
`modules/common/src/xpe_common.cpp:345` `case XPE_ERR_INVALID_CALIB_DATA: return "Calibration data out of valid range";`.
제가 스테이징해 둔 DLL 이 `c72f0e8` 시점 CI 산출물이라 #188 이 들어가기 전입니다. **코드 결함이 아니라 낡은 바이너리**이며,
네이티브를 다시 스테이징하면 사라질 것으로 봅니다 — 다만 **그 확인은 하지 않았습니다**(§4).

## 3. 기준 귀속

- 모든 성능·해시 수치는 이번 실행에서 `build/ci-c72f0e8/bin` (CI run 35275125717, head `c72f0e8`) 의 DLL 로 측정했습니다. 로컬 네이티브 빌드는 하지 않았습니다.
- post 레인 숫자(493 / 488 ms)는 카드에서 받은 값이며, 제가 재측정하지 않았습니다.
- 비교 대상 프레임은 `fixtures/gui-s0/raw/wrist_lat_3072x3072.raw` 하나입니다.

## 4. 미검증

- **단계 시간과 post 모듈 측정의 4~18 배 차이의 원인.** 같은 대상을 재고 있는지 확인하지 않았습니다. §2 C1 의 판단은 여기까지입니다.
- **GUI 몫 2.0 초의 내부 분해**(복제 / 표시 파이프라인 / 렌더 각각 얼마인지)는 재지 않았습니다. 끔 모드 2080 ms 로 "GSVG 와 무관하다"까지만 보였습니다.
- **비네팅이 켜졌을 때의 동작**은 확인하지 않았습니다. 켤 경로가 GUI 에 없어 대조군을 만들지 못했습니다 — 즉 `vignette=0` 단언은 "0 이 아닌 값도 나올 수 있다"는 대조 없이 통과합니다.
- 기본값 민감도는 **평균 밝기 한 지표**로만 쟀습니다. 국소 대비나 진단 품질에 미치는 영향은 판단하지 않았습니다.
- **새 DLL 로 다시 스테이징하면 `-17` 실패가 사라지는지** 확인하지 않았습니다. 소스에 문자열이 있다는 것까지만 봤습니다.
- 1회차/2회차 총 시간이 모드 간에 뒤집히는 구간이 있습니다(가상 그리드 1회차 2022 ms < 끔 2회차 2080 ms). 반복 횟수가 2회뿐이라 분산을 제거하지 못했습니다.

## 5. 잔여 위험

- **기본값 선택지.** 영향이 작으므로 **기본값 유지를 권합니다**(필수 입력으로 만들면 촬영 조건을 모르는 사용자가 막힙니다). 다만 §4 대로 평균 밝기 한 지표만 본 판단입니다. 값은 화면에서 편집 가능하게 남아 있으므로, 아는 사용자는 바꿀 수 있습니다. 필수 입력으로 돌릴지는 리더 결정 사항으로 남깁니다.
- **성능 게이트를 지금 잡으면 잘못된 기계에서 잡힙니다.** 이 숫자는 개발 기기 1대·반복 2회입니다. 게이트가 필요하면 CI 에서 먼저 돌려야 합니다(이전 1.91배 사례).
- P-05 는 앱이 실제로 파일을 쓸 때까지 10 초를 기다립니다. 느린 기계에서 시간 초과로 불안정해질 수 있습니다.

## 6. 이번에 찾아 고친 계측 결함 3건

측정을 신뢰하기 전에 계측 자체가 세 번 틀렸습니다. 기록해 둡니다.

1. **완료 신호가 영원히 오지 않았다.** 처음엔 "픽셀 해시가 바뀔 때까지" 기다렸는데, 반복 렌더는 같은 픽셀을 냅니다 — 62 초를 기다리다 실패. 이미지 **버전 번호**로 바꿨습니다.
2. **HUD 를 첫 `;` 에서 잘라 읽었다.** HUD 자체가 `;` 를 포함하므로 `times:` 가 있는데도 "없다"로 읽혔습니다.
3. **HUD 가 한 프레임 늦었다.** 피어가 노출할 문자열을 합성한 뒤에 `DrawHud` 가 돌아, 직전 프레임의 HUD 가 실렸습니다. `DrawHud` 를 먼저 부르도록 옮겼습니다.

## Card Cross-Check

| 이정표 | card |
|---|---|
| 성능 측정(P-01/P-02) | GUI-C-102 |
| 표시 단언(P-04/P-05) | GUI-C-102 |
| 기본값 근거(P-03) | GUI-C-102 |
| 비네팅 확인 | GUI-C-102 |
| GUI 몫 2.0 초의 내부 분해 | 새 카드 필요 |
| 단계 시간 vs post 측정 차이 규명 | 새 카드 필요 |
