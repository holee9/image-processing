# GUI-C-80 보고 — 분리 뷰어의 ①②③ (#171)

커밋(dev/gui, 미푸시): 0552ba7

## 1. 주장

1. 수정 전, 분리 뷰어는 같은 영상을 표시하면서도 오래됨 표시와 HUD를 보여 주지 않았다. C-79에서 소스를 읽고 적은 판독을 실행으로 확인했고, 대조군은 같은 실행의 메인 창 표시다.
2. 분리 뷰어에 HUD와 오래됨 배너를 붙였다. 둘 다 메인 창과 같은 뷰모델 속성(`RenderedVoiCenter/Width/Mode`, `PreviewStaleReason`, `IsPreviewStale`)에 바인딩했고, 창마다 따로 계산하지 않는다.
3. W-25(편집 후 미적용)와 W-26(주입한 렌더 실패)이 이를 고정한다. 분리 뷰어 쪽만 끄면 이 두 시험만 실패한다.

## 2. 증거 (이 디렉터리)

- `observe.txt`, `observe26.txt`: 수정 전 실행.
  - W-25: 메인 창 표시는 `STALE — display parameters changed…`, 분리 뷰어 표시는 `''`, 분리 뷰어 HUD 없음. 분리 뷰어 텍스트는 `Mode=SwipeVertical, Zoom=Fit, …` 한 줄뿐이다.
  - W-26: `calls=2`, 분리 뷰어 영상 v1 유지, 메인 창 표시는 `STALE — the display pipeline failed…`, 분리 뷰어 표시는 `''`.
  - `observe.txt`의 W-26 첫 실패는 NullReference였다. 분리 뷰어가 열린 상태에서 Apply 메뉴 항목을 찾지 못한 시험 도구 문제이고, `observe26.txt`가 도우미 수정 후 다시 관측한 결과다.
- `green.txt`: 수정 후 W-15, W-16, W-20~26 모두 통과(통과 9).
- `falsify-banner.txt`: 분리 뷰어 배너를 끄면 통과 7, 실패 2(W-25 ①, W-26 ③).
- `falsify-hud.txt`: 분리 뷰어 HUD를 끄면 통과 8, 실패 1(W-25, 메시지 `C=(no HUD)` ②).
- `full-e2e.txt`: Mock 전체 결과. 실패 0, 통과 77, 건너뜀 2(`WindowReacquireTests.ReadableWindow_IsKept_AndLeavesNoNote`, `NativeProvenanceTests`는 Mock이라 건너뜀).
- `full-int.txt`: 통합 결과. 실패 0, 통과 209, 건너뜀 1.
- 빌드: `dotnet build clients/ImageProcTest.slnx`에서 경고 0, 오류 0.

## 3. 기준선 귀속

이 세션에서 dev/gui 0552ba7 트리로 측정했다(origin/main 이 HEAD 를 포함). 수정 전 관측은 같은 트리에서 제품 코드 변경만 없는 상태로 했고, 반증은 해당 한 줄만 주석 처리한 작업본으로 했다.

## 4. 미검증

- **Native 백엔드:** 이번 카드에서는 실행하지 않았다.
- **배너 반증:** 배너 하나가 ①과 ③을 함께 담당하므로, 배너를 끄면 W-25와 W-26이 같이 실패한다. 공유 상태를 쓰라는 카드 지시의 결과이며, 두 시험 모두 분리 뷰어 시험이다. ①과 ③ 자체의 구분은 C-79의 반증(W-22/W-23)이 맡는다.
- **화면 위치:** 분리 뷰어 HUD와 배너가 화면에서 뷰포트와 겹치는 위치는 픽셀로 확인하지 않았다(캡처가 보이지 않는 하네스). 자동화 트리에서 보이는지만 확인했다(`IsOffscreen=false` 필터).
- **`WindowReacquireTests` 건너뜀:** 원인을 조사하지 않았다. C-79 Native 실행에서도 같은 시험이 NotExecuted였다.
- **CI:** 결과 없음(push는 lead 몫).

## 5. 잔여 위험

- `ApplyDisplayPipeline` 도우미는 첫 시도에서 마우스 클릭, 이후 시도에서 확장 패턴을 쓴다. 겹친 창이 클릭을 가로챈다는 원인은 추정이다. 확인한 것은 확장 패턴으로 바꾼 뒤 통과했다는 것뿐이다.
- 분리 뷰어 HUD는 코드로 만든 TextBlock이라 XAML의 HUD와 서식이 한 벌 더 있다. 값의 원천은 같지만 표기가 어긋날 수 있다.
- 분리 뷰어 viewport 버전은 창마다 자기 교체 횟수를 센다(메인 v3 / 분리 v2). 두 창의 버전 번호를 서로 비교하면 안 된다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 분리 뷰어 ①②③ | GUI-C-80 (이 카드) |
| 2-lane | #173 |
