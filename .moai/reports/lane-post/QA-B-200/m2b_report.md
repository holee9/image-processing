# QA-B-200 M2b — E1: 언샤프 마스킹이 REQ-ENH-018·021 대로 동작 (#251)

카드: `.moai/lanes/post/inbox/QA-B-200.md` 리더 결정(M1 수신 후) 선택지 가. M2a·M2a2 는 별도 커밋이다.

## 0. 결과 요약

| 항목 | 내용 |
|---|---|
| 결함 | `xpe_edge_enhance` 가 과도 상승을 `원본 ± amount×threshold` 로 양쪽에서 잘라, 에지 대비가 얼마든 샤프닝된 화소가 정확히 `amount×threshold` 만큼만 움직이고 `threshold = 0` 이면 아무것도 바꾸지 않았다(QA-B-199 E1, M1 재현). |
| 수정 | 과도 상승 상한을 REQ-ENH-021 대로 `max(원본×2, 원본 + amount×threshold)` 로 한다. 하한 클램프는 없앴다(§2 판단 사항). `edge_enhance.cpp` 4줄 추가·3줄 삭제. |
| 시험 | `test_edge_enhance_formula.cpp`(M1 의 `test_repro_qa_b_200.cpp` 를 이름 변경해 `DISABLED_` 를 풀고 3개 추가) 6개. |
| 반증 | 5팔 모두 터졌고 원본은 팔마다 바이트 단위로 복원됨. |
| 검증(관측) | ci-post ctest(CI 필터) 1360개 통과·0 실패(이전 1354 + 새 6), `xpe_enhance_basic_tests.exe` 필터 없이 156개 통과, 에지 성능 시험 통과. |

## 1. M1 에서 확정된 결함과 수정 전후

M1 재현(수정 전): 대비 100·1000 의 계단 에지에서 SPEC 은 밝은 쪽 에지 화소를 +20.0 · +200.1 움직이는데 모듈은 둘 다 +5(= amount 0.5 × threshold 10). `threshold = 0` 은 SPEC 이 400.3 을 움직이는데 모듈은 0.

수정 후(`m2b_run_formula.txt`):

| 상황 | SPEC(독립 참조 블러) | 모듈 |
|---|---|---|
| 기본 임계값(10), 에지 대비 1000, amount 1 — 최대 변화 | 400.26(`threshold 0` 행) | 397.92 |
| 대비 100, amount 0.5 — 밝은 쪽 에지 화소 | +20.013 | +19.896 |
| 대비 1000, amount 0.5 — 밝은 쪽 에지 화소 | +200.131 | +198.959 |
| 비율 1000 대 100 | 10.00 | 10.00 |
| 어두운 base 10 · 밝은 쪽 10010, amount 5 — 밝은 쪽 에지 화소 | 20020.0(= 2×원본) | 20020.0 |
| 어두운 점 30 (base 0), amount 5, threshold 10 | 80.0(= 원본+amount×threshold, 2×원본 = 60 보다 큼) | 80.0 |

모듈과 참조의 약 0.3~0.6 % 차이는 참조 블러(여기서 쓴 분리형 가우시안, 반경 ⌈4σ⌉, 경계 복제)와 모듈 자신의 블러 커널이 다르기 때문이며, 시험의 허용 오차(15 %)는 이 차이만을 위한 것이다. 결함은 수십 배 차이였다.

## 2. 판단이 필요한 한 가지 — 하한 클램프를 없앴다

REQ-ENH-021 은 "과도 상승(overshoot)"의 상한만 정한다. 옛 코드는 하한(`원본 − amount×threshold`)도 잘랐는데, SPEC 에 근거가 없고 결함의 절반이었으므로(어두운 쪽도 `amount×threshold` 만큼만 움직임) 같이 없앴다. 결과로 어두운 쪽 에지 화소는 `원본 + amount×(원본−블러)` 로 내려가며 **음수 값이 나올 수 있다**(대비 1000 에서 −199 로 내려감을 시험이 관측). 음수 화소가 하류에서 어떻게 처리되는지(uint16 변환에서 0 으로 자르는지)는 이 커밋에서 확인하지 않았다. 대안은 `max(0, …)`-식 하한을 두는 것인데, SPEC 에 없는 새 규칙이라 정하지 않았다. **리더 판단 사항**: SPEC 그대로 하한 없음(지금), 아니면 새 하한 규칙을 SPEC 에 먼저 적고 구현.

## 3. 시험 (`test_edge_enhance_formula.cpp`)
6개. 기대값은 모듈이 아닌 이 파일의 독립 참조 블러와 SPEC 문구에서 계산한다.
- 대조군: 기본 임계값에서 에지가 실제로 샤프닝된다(시험 도구가 샤프닝을 볼 수 있음).
- `threshold = 0` 이 에지를 샤프닝한다(REQ-ENH-020 이 0 을 허용).
- 샤프닝 크기가 에지 대비에 비례한다(대비 100·1000 의 비율 10).
- 과도 상승이 `2×원본` 으로 제한된다(밝은 쪽 화소가 `원본+amount×threshold` 가 아닌 `2×원본` 에 붙음) + 모든 화소가 REQ-ENH-021 상한 이하.
- 어두운 점이 `원본+amount×threshold`(2×원본 보다 큼)에 붙는다 — 두 항 중 큰 쪽이 실제로 `max` 로 고르는 경우를 둘 다 시험.
- 차이가 임계값 미만인 화소는 그대로다(단계 8, threshold 10 → 변화 0; 참조도 0 이라는 전제 단언).

## 4. 반증 (`m2b_arms_out.txt`, 구동기 `m2b_arms_driver.py.txt`)

| 끈 방어 | 빨개진 시험 |
|---|---|
| 옛 양쪽 `amount×threshold` 클램프로 되돌림 | threshold 0, 대비 비례, 과도 상승 제한(3) |
| 상한에서 `원본×2` 항 제거 | 대비 비례, 과도 상승 제한 |
| 상한에서 `amount×threshold` 항 제거 | 어두운 점 |
| 상한 클램프 전부 제거 | 어두운 점, 과도 상승 제한 |
| 임계값 게이트 제거(`abs_diff >= 0`) | 임계값 미만 화소, 기존 `EnhanceBasicParameterDependency.EdgeEnhance_EveryParameterReachesTheOutput` |

처음 한 번의 실행에서 2개 팔은 팔 자체가 컴파일되지 않았고(쓰이지 않는 변수 경고가 오류로 처리됨) 한 팔은 어떤 시험도 터지지 않았다("상한에서 `amount×threshold` 항 제거" — 어두운 점 시험이 없었다). 그래서 시험을 하나 더하고 팔을 고쳐 5팔 전체를 시험 파일 최종본으로 다시 돌렸다. 위 표는 그 재실행 결과다.
**터뜨리지 못한 변형**: 게이트를 `>=` 에서 `>` 로 바꾸는 팔은 구성하지 않았다. 임계값 0 에서는 차이가 0 인 화소의 샤프닝 결과가 원본과 같아 출력이 같고, 양수 임계값에서는 차이가 임계값과 *정확히* 같은 경우에만 갈리는데 블러된 영상에서 그 일치를 만들 수 없다. 동등 변이로 본다(검증하지 않은 것 아님: 이 판단은 추론이다).

## 5. gui 영향 목록 (리더·xpe-gui 에 사실로 전달)
`clients/` 와 `gui/` 를 검색해 `edge_enhance`/`xpe_edge_enhance` 를 호출하는 파일과, 해시·기준선을 쓰는 테스트를 대조했다.
- 실제 네이티브 `xpe_edge_enhance` 를 호출하는 앱 코드: `clients/ImageProcTest/Services/NativeEnhanceBasicPreviewService.cs`, `.../PInvokeWrappers/XpeEnhanceBasicWrapper.cs`, `.../Diagnostics/XpeEnhanceBasicReadinessProbe.cs`, `gui/ImageProcTest/Services/EnhanceBasicStage.cs`, `gui/ImageProcTest/Services/Native/{GuiEnhanceNative,XpeEnhanceBasicInterop}.cs`, `gui/ImageProcTest/Services/PipelineOrchestrator.cs`.
- `enhance` 와 해시를 함께 언급하는 테스트: `clients/ImageProcTest.IntegrationTests/Functional/BaselineDeterminismTests.cs`(가짜 배열의 해시, 같은 입력 두 번의 결정성 비교), `.../BaselineExecutionTests.cs`(증거 파일에 해시 필드가 있는지 길이 64 만 확인), `clients/ImageProcTest.E2ETests/Scenarios/Smoke/AutomationReportBackendTests.cs`(실제 모듈이 있을 때만 도는 기준선 한 건, `BaselineOutputSha256` 가 비어 있지 않은지).
- **enhance_basic 출력의 픽셀 해시나 기준선 값을 저장해 두고 그 값과 비교하는 테스트는 이 검색에서 찾지 못했다.** 따라서 이 변경으로 갱신할 저장된 기준선은 찾지 못했다.
- 한계: 위는 `Grep`/`grep` 으로 만든 목록이다. 해당 .NET 테스트를 돌려 보지는 않았고, 문자열 검색에 걸리지 않는 방식으로 기대값을 저장한 테스트가 있다면 놓쳤을 수 있다. `tests/e2e_post_pipeline`(내 소유)은 ci-post ctest 안에서 통과했다.

## 6. Gap / 잔여 위험
Gap
- 하류(display·dicom 쓰기)가 음수 화소를 어떻게 다루는지 관측하지 않았다(§2).
- .NET gui/clients 테스트를 돌리지 않았다(§5).
- 모듈 자신의 블러 커널은 노출되지 않아 정확한 기대값 비교는 못 했다(참조와 약 0.5 % 이내 일치로만 확인).
- 임계값에서 정확히 같은 경우(`>=` 대 `>`)는 시험하지 않았다(§4).

잔여 위험
- 샤프닝 강도가 이전보다 훨씬 커졌다(5 → 대비에 비례하는 수십~수백). 실제 임상 영상에서 후광 크기와 정량 지표(예: 노이즈, 에지 폭)가 어떻게 변하는지는 실영상으로 보지 않았다. 기본값(amount 0.5, threshold 10)에서 에지 근처 화소는 대비의 약 20 % 가 더해진다.
- 에지 성능 시험은 통과했다(상한 연산이 두 비교에서 `max` 한 번으로 바뀐 것뿐). 이전 대비 속도 차이는 측정하지 않았다.
