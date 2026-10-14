[2026-10-14 16:44 KST] [xpe-gui] GUI-C-242 보고 — W15 를 승인된 U8 기준(프록시 중앙값 ≤ 1500 ms)으로 (Codex #194 발견 1, Refs #251)

푸시하지 않았다. 기준 `548f3e94`. 변경: 시험 2개 파일(`QuickStartWalkthroughScenarios.cs` 의 W15, `ScreenPictureObserver.cs` 에 판정 함수와 단위 시험). 제품 코드 변경 없음. 가용 메모리 5800 MB 로 시작해 5975 MB 로 끝났고 남은 앱/testhost 0 (`memory_before.txt`, `memory_after.txt`).

## 1. 판정

- **화면이 읽히든 아니든 프록시 5회 중앙값 ≤ 1500 ms 를 단언한다.** 이전에는 화면 관측 불가일 때 `proxy[2] <= 3000` 만 단언했다(그 줄 삭제). 화면이 **읽히는** 환경에서는 화면 중앙값 ≤ 3000 ms 를 **별도로** 단언한다(그대로; 이 기계에서는 한 번도 그 경로가 돌지 않았다 — 236c 보고서와 같다).
- 승인 근거: main `5537f4e5` (milestone-3 U8 종료점 결정, 리더 2026-10-14): 종료점은 처리 프리뷰 할당 뒤 두 번째 합성 틱(프록시), 판정은 한도의 절반인 1500 ms. 커밋 메시지·문서 표를 읽어 확인했다.

## 2. 판정 함수 분리와 경계 시험

`U8Verdict.OfProxyMedian(5개 값)` (`ScreenPictureObserver.cs`): 정확히 5개가 아니면 `ArgumentException`, 중앙값 ≤ `ProxyLimitMs`(1500)이면 통과. 단위 시험 `U8VerdictTests` 4건(앱·데이터 불필요, 모든 러너에서 돎): **중앙값 1500 → 통과, 1501 → 실패**; 평균·최댓값이 아니라 중앙값이 결정(느린 첫 열기 하나는 통과, 빠른 이상치가 느린 실행을 구하지 못함); 열기 순서 무관; 4개·6개는 거부. 9/9 통과(`unit_tests.txt`: 새 4건 + 기존 `ScreenSettleTests` 5건).

## 3. 출력·주석 문구

- W15 의 문서 주석: "판정은 화면 값" 문장을 승인 규칙으로 교체(프록시 중앙값 ≤ 1500 이 판정, 화면은 읽힐 때 추가로 ≤ 3000), 제목에 242·승인 규칙 표기.
- 실행 출력: `U8 verdict (approved: proxy median of five <= 1500 ms): median 1130 ms -> PASS. The end point is the second composition tick (a proxy); how many frames later the picture is really presented is NOT measured here (unmeasured estimate).` — "실제 제시까지 몇 프레임"은 **이 환경에서 미측정 추정**이라고 출력에 명시.
- 실데이터 실행(`w15_run.txt`): 프록시 1278 / 1139 / 1111 / 1130 / 1089 ms, **중앙값 1130 ms → PASS**(≤ 1500). 화면은 이번에도 5회 모두 관측 불가(흰 화면), 줄 `U8 BY THE SCREEN: NOT MEASURED`. 이 기계의 관찰값이다(관측기가 한 코어를 쓰는 상태, 생성 보정 세트 + 리더의 CI DLL 폴더).

## 반증 (`falsify.txt`, 복원 바이트 동일, 복원 뒤 초록)
- 팔 A: 한도를 옛 3000 으로 → `TheMedianOfFive_At1500_…` 와 `TheMedian_NotTheMean…` 빨강(이번 발견의 결함 자체).
- 팔 B: 한도를 이 기계의 중앙값(≈1200)보다 낮은 1000 으로 → W15 빨강: `the proxy median 1213 ms exceeds the approved 1000 ms (U8, main 5537f4e5)`. 즉 W15 는 화면 관측 불가 환경에서도 판정을 강제한다.

## 한계
- 프록시는 렌더 직전 이벤트라 그림이 실제로 제시된 시각의 증거가 아니다(승인된 대리 종료점). 제시까지의 지연은 측정하지 못했고 추정조차 하지 않았다.
- 같은 `QuickStartWalkthroughScenarios.cs` 의 **W12**(`Run Preprocessing` 한 프레임 벽시계, 3000 ms)는 다른 측정(GUI-C-234)이라 이 카드에서 바꾸지 않았다. 필요하면 별도 카드.
- CI 러너에서 실행하지 않았다. CI 의 W15(34 s)는 리더가 알린 값이며, 그 러너의 프록시 중앙값은 이 카드에서 읽지 못했다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 1 프록시 중앙값 ≤ 1500 단언(화면 무관) + 화면 읽힐 때 ≤ 3000 별도 | GUI-C-242 |
| 2 판정 함수 분리, 경계 1500 통과·1501 실패 | GUI-C-242 |
| 3 출력·주석·보고서 문구, 미측정 추정 명시 | GUI-C-242 |
