# TASK-GUI-VIEWER-001: 뷰어 기본 기능 구현 — Brightness / Contrast / Histogram

**문서 ID**: TASK-GUI-VIEWER-001  
**Lane**: C (GUI) — `dev/gui` 브랜치  
**대상 프로젝트**: `clients/ImageProcTest`  
**작성일**: 2026-04-20  
**상태**: 1차 구현 완료 / 모듈화 후속 작업 대기

---

## 1. 배경 및 목적

현재 `ImageProcTest.exe`는 원본(Raw) 영상과 처리 후 영상을 나란히 표시하는 비교 뷰를 목표로 한다.  
그러나 두 영상에 공통으로 적용해야 할 **기본 뷰어 조작 기능**이 없어 임상 비교가 불가능한 상태다.

### 핵심 설계 원칙

> **픽셀 데이터는 절대 수정하지 않는다.**  
> Brightness / Contrast / Window-Level 은 렌더링 파라미터(LUT)만 변경한다.  
> 원본 `float[]` 버퍼는 Read-Only로 유지된다.

이 원칙에 따라 해당 기능은 **DLL 모듈(xpe_display / xpe_preprocess 등)이 아닌 GUI 레이어**에서 직접 구현한다.

---

## 2. 구현 범위

### 2.1 포함 (In Scope)

| 기능 | 설명 | 우선순위 |
|------|------|---------|
| Window/Level 슬라이더 | WindowCenter(WC), WindowWidth(WW) 실시간 조절 | P1 |
| Brightness / Contrast 슬라이더 | WC/WW를 직관적 UI로 래핑 | P1 |
| 히스토그램 시각화 | 현재 표시 영상의 픽셀 분포 차트 | P1 |
| 원본↔처리 링크 토글 | 두 뷰포트의 W/L 동기/독립 전환 | P1 |
| Invert 토글 | 흑백 반전 (렌더 플래그, 픽셀 불변) | P2 |
| W/L 프리셋 | Chest PA / Bone / Soft Tissue 등 6종 | P2 |
| 자동 Window-fit | 픽셀 최소~최대 기반 자동 W/L 계산 | P2 |
| Zoom / Pan | 마우스 휠 줌, 드래그 팬 | P3 |

### 2.2 제외 (Out of Scope)

- CLAHE 실제 적용(픽셀 변환) → `xpe_display.dll` 담당
- 영구 저장 목적의 LUT 적용 → `xpe_display.dll` 담당
- DICOM Modality LUT / Presentation LUT → `xpe_display.dll` 담당

---

## 3. 아키텍처

### 3.1 새로 추가할 파일

```
clients/ImageProcTest/
├── ViewModels/
│   └── ViewportViewModel.cs          ← WC/WW/Invert 상태 + INotifyPropertyChanged
├── Controls/
│   ├── ViewportControl.xaml           ← WriteableBitmap 렌더 컨트롤
│   ├── ViewportControl.xaml.cs
│   ├── HistogramControl.xaml          ← 히스토그램 차트 (OxyPlot 또는 Canvas)
│   └── HistogramControl.xaml.cs
├── Services/
│   └── ViewportRenderService.cs       ← LUT 계산 + WriteableBitmap 픽셀 기록
└── Models/
    ├── ViewportRenderParams.cs        ← WC, WW, Invert, LutType
    └── HistogramData.cs               ← 256/4096 bin 히스토그램 데이터
```

### 3.2 ViewportRenderParams 구조

```csharp
public class ViewportRenderParams
{
    public float WindowCenter { get; set; } = 2048f;
    public float WindowWidth  { get; set; } = 4096f;
    public bool  Invert       { get; set; } = false;
    public LutType Lut        { get; set; } = LutType.Linear;
}

public enum LutType { Linear, Sigmoid }
```

### 3.3 렌더링 파이프라인

```
원본 float[] 버퍼 (Read-Only)
        │
        ▼
ViewportRenderService.Render(buffer, params)
        │  ← WC/WW LUT 적용 (in-place to byte[])
        ▼
WriteableBitmap.WritePixels(...)
        │
        ▼
WPF Image 컨트롤 (화면 표시)
```

### 3.4 비교 뷰 링크 모드

```
[원본 뷰포트]          [처리 뷰포트]
ViewportViewModel A ──링크──▶ ViewportViewModel B
                   (동기 모드: A 변경 시 B도 동일 파라미터 적용)
                   (독립 모드: A/B 각자 독립 파라미터)
```

---

## 4. 구현 상세

### 4.1 Window/Level LUT 공식 (선형 모드)

```csharp
// ViewportRenderService.cs
byte ApplyWindowLevel(float pixelValue, float wc, float ww)
{
    float lower = wc - ww / 2f;
    float upper = wc + ww / 2f;
    if (pixelValue <= lower) return 0;
    if (pixelValue >= upper) return 255;
    return (byte)((pixelValue - lower) / ww * 255f);
}
```

### 4.2 히스토그램 계산

```csharp
// HistogramData.cs
int[] ComputeHistogram(float[] pixels, int bins = 256)
{
    var hist = new int[bins];
    float min = pixels.Min(), max = pixels.Max();
    float range = max - min;
    foreach (var p in pixels)
    {
        int bin = (int)((p - min) / range * (bins - 1));
        hist[Math.Clamp(bin, 0, bins - 1)]++;
    }
    return hist;
}
```

### 4.3 W/L 프리셋 정의

| 프리셋 | WC | WW | 용도 |
|--------|----|----|------|
| Auto Fit | min+range/2 | range | 전체 범위 자동 |
| Chest PA | 2048 | 3000 | 흉부 정면 |
| Bone | 1200 | 2500 | 골격 |
| Soft Tissue | 2048 | 1000 | 연부 조직 |
| Lung | 2048 | 4096 | 폐 |
| Pediatric | 1800 | 2000 | 소아 |

---

## 5. UI 레이아웃

```
┌─────────────────────────────────────────────────────────┐
│  [원본]                    [처리 후]                      │
│  ┌──────────────────┐  ┌──────────────────┐             │
│  │                  │  │                  │             │
│  │   WriteableBitmap│  │   WriteableBitmap│             │
│  │                  │  │                  │             │
│  └──────────────────┘  └──────────────────┘             │
│                                                         │
│  [링크 토글 ●/○]  프리셋: [Chest PA ▼]  [Invert □]      │
│                                                         │
│  Brightness  ───────────●──────────── [  0 ]            │
│  Contrast    ──────────────●────────── [  0 ]            │
│  W/L Center  ───────────●──────────── [2048]            │
│  W/L Width   ──────────────●────────── [4096]            │
│                                                         │
│  히스토그램: ▁▂▃▅▇▇▅▃▂▁ (원본)  ▁▁▂▄▆▆▄▂▁▁ (처리)       │
└─────────────────────────────────────────────────────────┘
```

---

## 6. 작업 순서 (구현 체크리스트)

- [ ] `ViewportRenderParams.cs` 모델 클래스 작성
- [ ] `HistogramData.cs` 모델 클래스 작성
- [ ] `ViewportRenderService.cs` LUT 렌더링 서비스 작성
- [ ] `ViewportViewModel.cs` MVVM ViewModel 작성 (INotifyPropertyChanged)
- [ ] `ViewportControl.xaml/.cs` WriteableBitmap 컨트롤 작성
- [ ] `HistogramControl.xaml/.cs` 히스토그램 차트 및 W/L range handle 작성
- [ ] `MainWindow.xaml` 에 슬라이더 패널 및 뷰포트 컨트롤 통합
- [ ] 링크 토글 (동기/독립 모드) 구현
- [ ] W/L 프리셋 드롭다운 구현
- [ ] Invert 토글 구현
- [ ] 단위 테스트: `ViewportRenderService` LUT 경계값 검증

---

## 7. 의존성

| 항목 | 상태 | 비고 |
|------|------|------|
| OxyPlot.Wpf NuGet | 추가 필요 | 히스토그램 차트 (또는 직접 Canvas 구현으로 대체 가능) |
| xpe_display.dll | 불필요 | 픽셀 변환 없음 — GUI 레이어 자체 LUT |
| xpe_preprocess.dll | 불필요 | 뷰어 기능은 전처리 파이프라인 외부 |

---

## 8. 테스트 기준

| 테스트 | 합격 조건 |
|--------|---------|
| WW=4096, WC=2048 → 전체 범위 표시 | 최소값=0, 최대값=255 렌더 |
| WW=0 입력 | 예외 없이 WW=1 로 클램프 처리 |
| Invert 토글 | 픽셀 = 255 - 기존픽셀 |
| 원본 버퍼 불변성 | 슬라이더 조작 후 원본 float[] 값 동일 |
| 링크 모드 동기화 | A 뷰포트 WC 변경 시 B 뷰포트 동일 WC 적용 |
| 히스토그램 bin 합계 | = 전체 픽셀 수 |

---

## 9. 탭 책임 경계 및 후속 모듈화

viewer 기능은 Evaluation 탭의 주 콘텐츠로 유지한다. calibration 폴더/파일 선택, target raw 선택, algorithm chain 구성은 viewer 내부 책임이 아니다. 다만 Offset/Gain/Defect Apply/Skip 스위치는 영상 비교 중 즉시 재실행해야 하므로 Evaluation workbench에 둔다.

후속 작업은 `TASK-GUI-IA-001` 및 GitHub Issue #47에서 추적한다.

| 영역 | 책임 |
|------|------|
| Evaluation workbench | Offset/Gain/Defect Apply/Skip, 원본/적용 영상 비교, swipe, zoom, W/L, LUT, invert, histogram range, 사용자 visual review |
| Calibration setup | calibration folder/file role audit, raw 선택, algorithm chain 구성 |
| Metrics | detector-domain metric, stage latency, calibration load 결과 |
| Reports | evidence JSON/Markdown 저장 및 확인 |
| Diagnostics | DLL/export readiness, smoke test, native parameter range |

### 모듈화 기준

- `ViewportRenderService`는 픽셀 불변 렌더링 서비스로 유지한다.
- `ViewportViewModel`은 W/L, LUT, histogram, histogram range, zoom, swipe 상태를 소유한다.
- `ViewportControl` 또는 `EvaluationViewerPanel`은 XAML viewer를 소유한다.
- `MainWindow`는 탭 host와 전역 command routing만 담당하도록 축소한다.
- calibration setup에서 만든 `ActiveEvaluationContext`만 Evaluation viewer가 소비한다.

---

**연관 문서**: SRS-DISPLAY-001, docs/display/xpe-display-prd.md  
**Lane 담당**: Lane C (GUI) — `clients/ImageProcTest`  
**DLL 귀속 불필요**: 픽셀 변환 없으므로 IEC 62304 추적 대상 아님
