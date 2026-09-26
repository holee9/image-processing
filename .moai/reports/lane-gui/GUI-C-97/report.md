# GUI-C-97 보고 — GUI 픽셀 연쇄: 처리 단계 계약 설계 판독 (#180, #173)

- 판독 기준:
  - GUI 는 `dev/gui` `a580905` 에서 읽었다. main `716dbeb` 와 비교해 `gui/` 차이는 없다.
  - 모듈·문서는 `origin/main` `716dbeb` 에서 읽었다.
- **코드 변경 없음.** 설계 선택지와 영향만 적는다. 결정은 리더가 한다.
- 인용 사본은 `build/c97-spec.md`(SPEC-XPE-GSVG), `build/c97-gsvg_api.h` 에 두었다.

## 0. 지금 GUI 의 픽셀 흐름 (출발점)

| 사실 | 위치 |
|---|---|
| 불러오기: `LoadRawImage` → `SourceImage`, `ProcessedImage`, `ActiveImageFrame` 설정 | `MainWindowViewModel.cs:938-945` |
| 디스플레이: `Settings.Snapshot()` 으로 입력을 고정한 뒤 `Task.Run` 에서 `ApplyDisplayPipeline(ActiveImageFrame, inputs)` 호출 | `:976-978` |
| Real 디스플레이 입력은 `rawFrame.RawPixels` 이고, 결과 프레임도 **같은 `RawPixels`** 를 들고 돌아온다 | `RealXpeBackend.cs:100-127`, `:174-185` |
| 따라서 디스플레이를 다시 실행해도 늘 원본 raw 에서 시작한다 | 위 두 줄의 결과 |
| 전처리 결과는 `ProcessedImage` 미리보기만 바꾼다. 픽셀(`result.Pixels`)은 어디에도 보관되지 않는다 | `MainWindowViewModel.cs:1030-1046` |
| 픽셀을 나르는 유일한 필드는 `LoadedImageFrame.RawPixels` 다(이름이 곧 "원본") | `Models/LoadedImageFrame.cs:15` |
| `PreprocessRunResult(Ran, Summary, Pixels, ProcessedPreview)` 에는 "요청됐지만 안 됨" 과 "됐지만 변화 없음" 을 가르는 필드가 없다 | `Models/PreprocessRunResult.cs:14` |

**지금 구조에서 GSVG 결과를 디스플레이 입력으로 쓰려면** 둘 중 하나가 필요하다.
- 결과를 `RawPixels` 에 넣는다 — 이름이 거짓이 되고, REQ-GSVG-022 의 "원본"이 어느 필드인지 흐려진다.
- 디스플레이 입력을 가리키는 별도 필드를 만든다.

## 1. `IXpeBackend` 계약 초안 — 두 안

두 안 모두 공통 모델 위에 선다.

```csharp
// 공통 모델 (두 안 모두)
public sealed record RawImage(ushort[] Pixels, int Width, int Height, string LoadId);   // 불변, 원본 보존

public enum StageStatus { NotRequested, Applied, AppliedNoChange, RequestedNotApplied }

public sealed record StageOutcome(
    string StageId,                 // "gsvg", "preprocess" …
    StageStatus Status,
    ushort[]? Pixels,               // Applied/AppliedNoChange 일 때만. 입력과 별도 배열
    string? Reason,                 // RequestedNotApplied 의 이유(모듈 오류 코드 + 경보 문구)
    string SettingsKey);            // 캐시·오래됨 판정용 정규화 설정

// 디스플레이는 "연쇄의 마지막 결과" 에서 시작한다
public sealed record ChainResult(RawImage Raw, IReadOnlyList<StageOutcome> Stages)
{
    public ushort[] DisplayInput =>
        Stages.LastOrDefault(s => s.Pixels is not null)?.Pixels ?? Raw.Pixels;
}
```

- **Status 네 값**:
  - `AppliedNoChange` 가 필요한 이유: gsvg 는 격자가 검출되지 않으면 **XPE_OK 를 돌려주고 dst 를 다시 쓰지 않는다**(`gsvg_api.h:121-123`). 오류가 아니므로 "적용됨" 과 같은 반환값이다. 이것을 "적용됨" 으로 보여 주면 HAZ-GUI-005 와 같은 모양(요청과 실제의 불일치가 보이지 않음)이 된다.
  - 판별 수단: 모듈 API 에는 "격자 없음" 을 알리는 반환 신호가 없다. 입력과 출력을 비교하거나 경보 큐를 읽어야 하는데, 경보 큐에 해당 경보가 있는지는 **확인하지 않았다**.
- **실패 시 원본으로 되돌리기(REQ-GSVG-024)**:
  - 모듈은 실패하면 dst 에 원본 픽셀을 남긴다(`gsvg_api.h:124-127`, 제자리 처리 포함).
  - GUI 에서는 `Pixels = null`, `Status = RequestedNotApplied` 로 두어 `DisplayInput` 이 앞 단계(또는 raw)로 떨어지게 하고, 경보를 남긴다.

### 안 A — 단계별 메서드

```csharp
public interface IXpeBackend
{
    // 기존 멤버 유지 …
    StageOutcome ApplyGridCorrection(ReadOnlyMemory<ushort> input, int width, int height,
                                     GsvgSettings settings, ReadOnlyMemory<byte> fieldMask = default);
    bool SupportsGridCorrection { get; }
    LoadedImageFrame ApplyDisplayPipeline(ReadOnlyMemory<ushort> input, LoadedImageFrame frame, AppSettings settings);
}
// 순서·보관·폴백은 뷰모델이 한다:
//   var g = backend.ApplyGridCorrection(raw.Pixels, …); var input = g.Pixels ?? raw.Pixels; backend.ApplyDisplayPipeline(input, …)
```

- 장점:
  - 기존 `RunPreprocessing` / `SupportsPreprocessing` 과 같은 모양이다(`IXpeBackend.cs:37-40`).
  - Mock·Fault 래퍼(`FaultInjectingBackend.cs:71-72`)에 메서드 하나씩만 더하면 된다.
- 단점:
  - 순서와 폴백 규칙이 뷰모델에 흩어진다(현재 1590줄).
  - lane 이 둘이면(#173) 같은 오케스트레이션을 두 번 써야 한다.
  - 단계가 늘 때마다 인터페이스가 바뀐다(`@MX:ANCHOR`, `IXpeBackend.cs:5-6`).

### 안 B — 단계 목록 실행

```csharp
public sealed record StageRequest(string StageId, JsonObject Config);   // 설정은 단계별 정규화 객체

public interface IXpeBackend
{
    // 기존 멤버 유지 …
    IReadOnlySet<string> SupportedStages { get; }
    ChainResult RunChain(RawImage raw, IReadOnlyList<StageRequest> stages, CancellationToken ct);
}
// 디스플레이: backend.ApplyDisplayPipeline(chain.DisplayInput, …)
```

- 장점:
  - 순서·보관·폴백·캐시가 한 곳(`RunChain` 구현 또는 공용 `ChainRunner`)에 모인다.
  - lane 마다 `StageRequest` 목록만 다르다(#173).
  - 단계를 추가해도 인터페이스가 바뀌지 않는다.
- 단점:
  - 문자열 단계 ID 와 `JsonObject` 설정은 컴파일러가 검사하지 않는다. 키 오타가 조용히 기능을 끄는 gsvg 함정(`gsvg_api.h:61-66`)이 GUI 층에서 한 번 더 생길 수 있다. 단계별 설정 레코드를 두고 직렬화를 한 곳에서 하면 줄어든다.
  - `ChainRunner` 를 Real·Mock 이 공유하지 않으면 두 벌이 된다.

### 공통 판단거리

- **디스플레이 입력 분리**: `ApplyDisplayPipeline` 이 `rawFrame.RawPixels` 를 읽는다(`RealXpeBackend.cs:100-127`). 두 안 모두 입력 픽셀을 별도 인자로 받게 바꿔야 한다. Mock 도 같다(`MockXpeBackend.cs:119-160`).
- **보관 비용**: 3072² uint16 은 약 18 MB 다. raw + GSVG 결과 + (전처리 결과) + 디스플레이 float 버퍼(`RealXpeBackend.cs:127`, 약 36 MB)가 동시에 살아 있다.
- **마스크 의존**: main 에 `xpe_gsvg_process` 의 마스크 변형이 들어왔다(`gsvg_api.h:197-237`, QA-B-96). 필드 마스크는 enhance_advanced 의 콜리메이션 검출에서 온다고 적혀 있다. 이것을 쓰면 연쇄에 **GSVG 앞의 마스크 단계**가 생긴다. 안 B 는 단계 결과로 마스크를 넘기는 자리를 따로 정해야 한다(`StageOutcome` 에 픽셀 외 산출물).

## 2. 전처리를 연쇄에 넣을지 (결정은 리더)

| 영향 | 넣는다 | 넣지 않는다 (지금처럼 별도 명령) |
|---|---|---|
| 화면 | 전처리 결과가 GSVG·디스플레이의 입력이 된다. 교정된 영상에 VOI 가 적용되어 보인다 | 전처리 결과는 지금처럼 미리보기로만 보인다(`MainWindowViewModel.cs:1043`). VOI 없이 최소~최대로 늘인 모습(`:1044` 주석). GSVG 는 raw 에 적용된다 |
| 입력 형식 | gsvg 는 uint16 검출기 DN 을 받는다(C-94 판정). 전처리 출력이 같은 형식이면 들어맞는다. 전처리 출력 형식(`PreprocessRunResult.Pixels` 는 `ushort[]`)은 형식상 맞다 | 해당 없음 |
| 전처리 kVp | `GuiPreprocessRunner.cs:97` 이 kVp 70 을 하드코딩한다. GSVG 의 `vg_kvp` 는 사용자 입력이다. 연쇄 안에서 두 값이 다르면 한 영상에 두 kVp 가 쓰인다 → 하나의 설정으로 합쳐야 한다 | 두 값이 따로 있어도 서로 만나지 않는다 |
| Mock | `SupportsPreprocessing=false`(`MockXpeBackend.cs:307`)라 연쇄의 전처리 단계는 늘 `RequestedNotApplied` 다. Mock E2E 는 이 상태를 보게 된다 | Mock E2E 는 지금과 같다 |
| 시험 | 기존 전처리 시험(미리보기 교체, `PREPROCESS_NOT_RUN` 경보)의 기대가 바뀐다. 연쇄 순서 시험이 필요하다 | GSVG 연쇄 시험만 추가하면 된다 |
| 교정 모드 7종(#182) | 연쇄가 생기면 다시 켤 후보가 된다. 다만 교정 모드를 읽는 네이티브 경로는 여전히 없다(C-95) | 미적용 유지 |

## 3. #173 (2-lane) 과의 관계

- 두 lane 은 **같은 raw 에 다른 단계 목록**을 적용한 두 `ChainResult` 로 표현된다. 안 B 는 그대로 올라간다(`RunChain` 두 번). 안 A 는 뷰모델 오케스트레이션을 lane 별로 복제해야 한다.
- 현재 lane 선택 UI(`AlgorithmBar.xaml`)는 C-95 에서 비활성이다. 선택값 `LaneAAlgorithm` / `LaneBAlgorithm`(`AppSettings.cs` "Production v1.2" / "Candidate v1.4")을 **단계 목록 프리셋의 이름**으로 쓰면 다시 켤 자리가 생긴다.
- 뷰포트는 원본/처리 두 영상을 받는다(`ViewportShell.xaml:20-23`). 2-lane 이면 "원본" 자리에 lane A 결과를 넣을지, 새 속성을 둘지를 정해야 한다. 옛 `LaneAImage` / `LaneBImage` 바인딩은 아무도 값을 넣지 않아 #172 를 만들었다(`ViewportShell.xaml:15-19` 주석).
- 캐시 키(6절)에 lane 을 넣을 필요는 없다. 같은 raw·같은 단계 목록이면 결과가 같다.

## 4. #171 통제와 #182 연결 조사 시험에 새 설정이 들어가는 방법

| 통제 | 지금 | 연쇄가 생기면 |
|---|---|---|
| 렌더 입력 스냅샷(#171 ②) | 디스플레이 입력만 `Settings.Snapshot()`(`MainWindowViewModel.cs:977`) | 연쇄 전체가 **같은 스냅샷**을 써야 한다. GSVG 1.5 s 사이에 사용자가 값을 바꿔도 결과와 표시가 어긋나지 않게 |
| 오래됨 ①(설정 변경) | `DisplayInputsDiffer` 가 디스플레이 입력 목록을 손으로 나열한다(`:410-423`). 교정 모드 7종이 들어 있지만 처리에 쓰이지 않는다(C-95) | GSVG 설정 전부를 추가해야 한다. 손 목록이라 빠뜨리기 쉽다 → 단계별 `SettingsKey` 비교로 바꾸는 안이 있다 |
| 오래됨 ③(실행 실패) | `StalePipelineFailed`(`:996`) | 단계 실패는 "실패" 가 아니다. 원본으로 폴백해 **표시는 현재**다. 별도 표시("GSVG 요청됨 — 적용 안 됨: <이유>")가 필요하다. 오래됨 배너와 섞으면 두 뜻이 한 문구에 들어간다 |
| HUD | VOI 만 보인다(`SetRenderedVoi`, `:337`) | 단계 상태(적용 / 변화 없음 / 요청됐으나 안 됨)를 HUD 나 상태 줄에 보여야 한다. 분리 뷰어 HUD 도 같이(C-80) |
| 증거 묶음 / 보고서 | `applied = DisplayPipelineApplied`(`:811`) | 단계별 Status·Reason·SettingsKey 를 남긴다 |
| **#182 연결 조사 시험** | 진입점: `LoadRawImage`, `ApplyDisplayPipeline`, `RunPreprocessing`(`SettingsProcessingConnectionTests.cs:53`). **문자열 리터럴 안의 읽기는 처리로 치지 않는다** | ① 새 메서드(`ApplyGridCorrection` 또는 `RunChain`)를 진입점 목록에 넣어야 한다. ② **GSVG 설정 JSON 을 보간 문자열로 만들면 시험이 그 읽기를 보지 못한다** → 모든 GSVG 설정이 "미연결" 로 빨강이 된다. 설정 객체를 직렬화하거나(`settings.X` 가 코드로 읽힘) 시험 규칙을 바꿔야 한다. ③ 설정을 화면에 묶으면 시험이 자동으로 수집한다. 연결 전에는 미연결 목록에 넣어야 한다 |

## 5. GSVG 설정 표면 — 기본값 후보와 출처

| 설정 | 모듈 키 | 모듈 기본값 | 기본값 후보 | 출처 |
|---|---|---|---|---|
| 켜기(없음/억제/가상, 서로 배타) | `grid_suppression`, `virtual_grid` | 둘 다 false. 둘 다 true 면 init 실패 | **없음** | `gsvg_api.h:71-76`, `:109-112` |
| 격자비 | `vg_grid_ratio` | 없음(필수) | 10 | 벤치·시험 설정(`test_virtual_grid.cpp:794`, `:679`, `:711`). REQ-GSVG-016 은 6/8/10/12(`c97-spec.md:188-191`) |
| 표 경로 | `vg_table_path` | 없음(필수) | **제품용 표가 저장소에 없다**. 아래 참고 | `gsvg_api.h:78-86` |
| kVp (사용자 입력) | `vg_kvp` | 없음(필수) | 없음(입력 필수로 둘 것) 또는 70 | 전처리가 70 을 하드코딩(`GuiPreprocessRunner.cs:97`). 벤치는 80. 표의 kVp 축은 60/80/100/120(`tools/mcsim/tables/grid_water_victre.csv`) |
| 화소 간격 | `vg_pixel_pitch_mm` | 없음(필수) | 0.14 또는 0.139 | 전처리 메타데이터 0.14(`GuiPreprocessRunner.cs:97`). 헤더 예시·벤치 0.139(`gsvg_api.h:84`, `test_virtual_grid.cpp:794`). **두 값이 다르다** |
| 공기 신호 | `vg_air_signal` | 없음(필수) | 60000 | 헤더 예시·벤치(`gsvg_api.h:85`). 검출기 측정 출처는 찾지 못했다 |
| 반복 수 | `vg_iterations` | 없음(필수), 1..100 | 3 | 헤더 예시·벤치(`gsvg_api.h:86`). 범위는 `gsvg.cpp:259-261` |
| (선택) 피라미드 단계·이득, 잡음 k | `vg_pyramid_levels` / `_gain`, `vg_denoise_k` | 없으면 꺼짐 | 꺼짐 | `gsvg_api.h:88-94` |
| (선택) 필드 마스크 | 마스크 API 인자 | 없음 | 없음 | `gsvg_api.h:197-237` |

**표 경로에 관한 사실**
- 로더는 네 절(`[kernels] [wet] [grid] [spr_cap]`)이 든 **파일 하나**를 읽는다(`virtual_grid.h:40-53`).
- 저장소의 표:

| 파일 | 형식 | 비고 |
|---|---|---|
| `modules/gsvg/tests/data/virtual_grid_synthetic_table.csv` | 이 형식 | 이름 그대로 합성 시험용 |
| `tools/mcsim/tables/*.csv` | 절별 **별도 파일** | 그대로는 로드할 수 없다 |

- `tools/mcsim/tables/grid_water_victre.csv` 는 격자비마다 24행(격자 주파수 2 × 두께 3 × kVp 4)이다. 로더는 `[grid]` 절의 **중복 격자비를 거부한다**(`virtual_grid.cpp:216`). 이 표를 쓰려면 행을 하나로 줄이는 규칙(주파수·두께·kVp 선택)이 필요하다.
- 헤더는 이 표가 "simulation-based, not calibrated" 라고 적는다(파일 1행).

**격자비 목록 — 선택지**

| 안 | 내용 | 장점 | 단점 |
|---|---|---|---|
| (a) 표 파일의 `[grid]` 절에서 읽기 | 합성 표에는 6/8/10/12/**100** 이 있다 | 표와 UI 가 어긋날 수 없다(표에 없는 비를 고르면 init 이 아니라 process 에서 `XPE_ERR_CONFIG_INVALID`, `virtual_grid.cpp:611-612`) | GUI 가 표 형식을 파싱해야 한다(모듈 파서는 내부 심볼, `virtual_grid.h:26-27`). 시험용 100 같은 값이 목록에 나온다 |
| (b) 4값 고정(6/8/10/12) | REQ-GSVG-016 그대로 | 단순하다 | 표에 없는 비를 고를 수 있다. 그러면 처리 때 실패하고 원본으로 폴백한다 → "요청됐으나 안 됨" 표시가 반드시 필요하다 |

## 6. 성능과 캐시

- 3072² 에서 약 1.5 s 는 리더가 준 값이다. 이 레인에서 측정하지 않았다.
  - REQ-GSVG-019 요구는 1.0 s 이고, 상태는 "Not measured"(`c97-spec.md:219-226`)다.
  - 측정 시험은 `test_virtual_grid.cpp:781`(시간만 기록)에 있다.
- **캐시해도 되는 조건** — 결과가 같으려면 아래가 모두 같아야 한다.
  1. 입력 픽셀: raw 의 식별자(`LoadId` + 파일 경로 + 크기·수정시각, 또는 픽셀 해시). 앞 단계가 있으면 앞 단계의 캐시 키
  2. 정규화한 단계 설정: 모듈에 넘기는 JSON 과 같은 값. 부동소수 표기를 고정한다
  3. **표 파일 내용**: 경로가 같아도 파일이 바뀔 수 있으므로 내용 해시(또는 크기 + 수정시각)
  4. 필드 마스크를 쓰면 그 마스크의 키
  5. 모듈 버전(`xpe_gsvg_version`) — DLL 교체 뒤 재사용 금지
  6. 백엔드 종류 — Mock 결과를 Real 로 재사용 금지
- **캐시하면 안 되는 것**:
  - `RequestedNotApplied` 결과는 캐시하지 않거나 짧게만 둔다. 표 파일을 고친 뒤 다시 시도해야 하기 때문이다.
  - `AppliedNoChange` 는 캐시해도 된다.
- **VOI 만 바뀐 경우**: 연쇄를 다시 돌리지 않고 디스플레이만 다시 돌린다. 이것이 캐시의 주 이득이다(W/L 조정이 1.5 s 를 기다리지 않음).
- **메모리**: 결과 하나에 약 18 MB 다. raw 당 단계별 1개(최근 1개)로 제한하는 안이 단순하다.

## 7. 미검증

- gsvg 가 "격자 없음" 일 때 경보 큐에 무엇을 남기는지 보지 않았다. `AppliedNoChange` 를 입출력 비교 없이 판별할 수 있는지 모른다.
- 3072² 1.5 s 는 측정하지 않았다(리더 제공 값).
- 두 계약안을 컴파일해 보지 않았다(스케치).
- `ReadOnlyMemory` 로 P/Invoke 에 넘길 때 복사가 생기는지 보지 않았다.
- enhance_advanced 의 콜리메이션 검출이 GUI 에서 호출 가능한지(내보낸 함수, 입력 형식) 보지 않았다.

## 8. 잔여 위험

- **키 오타 함정**: 설정 JSON 키 오타는 모듈이 조용히 기능을 끄는 경로다(`gsvg_api.h:61-66`). GUI 가 JSON 을 만들면 "켜기를 요청했는데 적용 안 됨" 이 오류 없이 생긴다. 초기화 뒤 적용 여부를 읽을 수단이 모듈에 없다.
- **kVp 두 값**: 전처리 70 고정과 GSVG 사용자 입력이 공존하면 한 영상에 두 값이 쓰인다.
- **화소 간격 두 값**: 0.14 와 0.139 는 산란 커널 반경(cm)에 직접 들어간다.
- **제품용 표 부재**: 합성 시험 표를 기본값으로 두면 임상적으로 의미 없는 보정이 "적용됨" 으로 표시된다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 계약 두 안 비교 | GUI-C-97 |
| 전처리 포함 여부 영향 | GUI-C-97 → 리더 결정 |
| #173 관계 | GUI-C-97 |
| #171 / #182 반영 방법 | GUI-C-97 |
| GSVG 설정 기본값·출처, 격자비 목록 선택지 | GUI-C-97 → 리더 결정 |
| 캐시 조건 | GUI-C-97 |
| 구현 | 다음 카드 |
| 제품용 GSVG 표 / 화소 간격·kVp 단일화 | 새 카드 후보 (리더 판단) |
