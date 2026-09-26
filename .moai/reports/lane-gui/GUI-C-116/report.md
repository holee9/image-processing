# GUI-C-116 — `offset` 자기 비교를 닫는다 (V-07)

## 1. 주장

1. C-112 가 남긴 마지막 자기 비교(`offset`)에 독립 대조가 **실재했고**, 그것을 써서 V-07 을 작성했다.
2. V-07 은 "요청한 pan" 과 "그려진 offset" 이 일치하는지를 본다.
3. 반증에서 **같은 실행 안에서** V-07 만 빨강, V-03·V-04 는 초록이었다.
4. 주입을 원복한 뒤 Mock E2E 전체가 초록이다.

## 2. 증거

### (a) 독립 대조가 존재한다 — 두 표현의 출처

lead 의 "(c) 독립 대조가 없으면 만들지 말고 멈추라" 분기는 **적용되지 않았다.**
다음 두 값은 같은 프레임에서 서로 다른 코드가 만든다.

```
gui/ImageProcTest/Controls/ImageComparisonViewport.cs:570
    HUD:  pan {PanX:0},{PanY:0}                       ← 의존 속성에서 직접

gui/ImageProcTest/Controls/ImageComparisonViewport.cs:543-544
    var x = ((ActualWidth - width) / 2.0) + PanX;     ← 그리기 사각형을 만든다
gui/ImageProcTest/Controls/ImageComparisonViewport.cs:342
    offset={imageRect.X + (imageRect.Width / 2.0) - (ActualWidth / 2.0)}   ← 그 사각형에서 역산
```

요청(HUD `pan`) 대 실측(그려진 `offset=`) — 배치가 맞는 동안 일치하고, 사각형이 요청을 따라가지 않으면 갈라진다.

### (b) 초록

```
V07 requested pan=90,70; drawn offset=90,70
```

### (c) 반증 — 같은 실행, 새 시험 빨강 + V-03·V-04 초록

주입: `ImageComparisonViewport.cs` 사각형 계산에 `PanX * 0.5` / `PanY * 0.5`

```
RED_EXIT=1   통과: 2   실패: 1
V07  빨강  asked to pan to 90,70 and the frame drew the image at 45,35
            — the image is not where the pan put it.
            … offset=45,35; … hud=… | pan 90,70 | …
V-03·V-04  초록
```

즉 기존 두 시험은 "절반 거리로 일관되게 그리는" 렌더러를 통과시킨다 — 이것이 자기 비교였다.

### (d) 원복 후 전체

```
MOCK_EXIT=0
통과!  - 실패: 0, 통과: 114, 건너뜀: 20, 전체: 134, 기간 4m 48s
```

## 3. baseline 귀속

- 주입 전 `ImageComparisonViewport.cs` 사본을 보관하고 그 파일로 복원, `git status` 에 gui 변경 0건.
- 초록/빨강 모두 이 워크트리·이 실행에서 측정.

## 4. 미검증 (Gaps)

- **V-06 의 한계를 그대로 물려받는다.** 두 읽기 모두 한 렌더 패스 안의 `PanX` 에서 출발하므로, 의존 속성 자체가 잘못 움직이면 둘이 같이 움직이고 V-07 은 초록으로 남는다. 두 값이 독립인 이유는 **오늘 코드가 그렇게 배치돼 있어서**이지, 무엇이 그것을 강제해서가 아니다. 이 문장은 시험 본문 주석에도 적었다.
- Native 백엔드에서는 돌리지 않았다(이 경로는 렌더 배치라 백엔드 무관).
- 허용 오차 1.0 px 의 타당성은 측정하지 않았다(정수 반올림 폭에서 잡은 값).

## 5. 잔여 위험

- 창 크기나 DPI 가 바뀌는 환경에서 `ActualWidth` 기준이 흔들리면 두 값의 대응이 달라질 수 있다. 현재 픽스처는 고정 크기로만 검증한다.

🗿 MoAI
