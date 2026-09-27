# GUI-C-101 보고 — GSVG 를 연쇄 단계로 붙였다 (#180)

- 커밋: `dev/gui` `e876865` (GSVG), `0e6a60a` (오류코드 미러, 별건) — 둘 다 미푸시, main `cedce22` 위
- 증거: `build/e2e-c101/`
- 네이티브: `build/ci-c72f0e8/bin` (CI run 35275125717, head `c72f0e8`) — 3절 참고

## 1. 구현

| 항목 | 내용 |
|---|---|
| P/Invoke | `Services/Native/XpeGsvgInterop.cs` — `xpe_gsvg_init` / `xpe_gsvg_process_ex` / `xpe_gsvg_shutdown`, `XpeGsvgResult`(24바이트, `structSize` 설정), 사유 열거형 |
| DLL 탐색 | `GuiNativeLibraryResolver` 에 `gsvg.dll` 추가 — `xpe_display` 와 같은 모듈 정책 |
| 실행기 | `Services/Native/GuiGsvgRunner.cs` — 설정 JSON 은 `GsvgConfig` 레코드(키는 `JsonPropertyName` 상수)를 직렬화해서 만든다. 보간 문자열이 아니다(C-97 위험 (b)) |
| 연쇄 | `ProcessingChainPlan` 이 `[preprocess, gsvg]` 를 순서대로 낸다. 디스플레이는 그 뒤다 |
| Mock | `gsvg=RequestedNotApplied` — "Grid correction requires the native backend (gsvg.dll)." |
| 설정 6종 | `GsvgMode`(None/GridSuppression/VirtualGrid 3택1), `GsvgTablePath`, `GsvgGridRatio`, `GsvgGridFrequencyPerCm`, `GsvgAirSignal`, `GsvgIterations` |
| 표시 | 상태줄·분석 패널의 연쇄 상태, **적용 안 된 단계의 사유 포함**, HUD(뷰포트 `ChainStatus`), 오래됨 비교(GSVG 설정 6종 + 화소 간격), 보고서 3종(`gsvgMode`·격자비·선밀도·화소 간격 추가) |
| kVp | 비활성이 아니라 "연결 대기 — #180" 표시(`ExposureKvpPendingMark`). 입력은 계속 쓸 수 있다 |

### 사유 → 상태 (리더 승인안 그대로)

| 사유 | 실행기 반환 | 상태 |
|---|---|---|
| 0 `APPLIED` | `Ran=true` + 픽셀 | Applied (러너가 픽셀 비교) |
| 3 `NO_GRID_DETECTED` | `Ran=true` + 픽셀 | AppliedNoChange (입력과 같으므로) |
| 1 `NOT_CONFIGURED`, 2 `IMAGE_TOO_SMALL`, 4 `GRID_NOT_IN_SUBBANDS`, 5 `VG_REFUSED` | `Ran=false` + 사유 | RequestedNotApplied |
| 알 수 없는 값 | `Ran=false` + 사유 | RequestedNotApplied |

- 별도 분기 없이 기존 러너 규칙으로 떨어진다(C-100 §3.1 그대로).

### 표 파일

- 결정대로 **설정 + 기본값은 gsvg.dll 옆**이다. 설정이 비면 `GuiGsvgRunner.DefaultTablePath()` 가 해석된 `gsvg.dll` 경로 옆의 `vg_table_water_csi600_victre.csv` 를 쓴다.
- **설치 규칙이 없으므로**(`modules/gsvg/CMakeLists.txt` 에 `install()` 없음) 이번에는 저장소의 제품 표를 스테이징 디렉터리에 손으로 복사했다: `build/ci-c72f0e8/bin/vg_table_water_csi600_victre.csv` (23,102 바이트). CI 산출물에는 표가 들어 있지 않다.

## 2. 측정 결과

### 사용자가 보는 동작 (Native, `build/ci-c72f0e8/bin`)

| 시나리오 | 연쇄 상태 | 그린 픽셀 해시 |
|---|---|---|
| GSVG 끔 | `gsvg=NotRequested; display input=raw` | `9e58cd6029598dfa` |
| 억제(Suppress) | `gsvg=AppliedNoChange; display input=chain` | `9e58cd6029598dfa` (같음) |
| 가상 격자, 비 10 | `gsvg=Applied; display input=chain` | `2c37695466f7fd46` |
| 가상 격자, 비 6 | `gsvg=Applied` | `9f1f675fb6c7ab0b` |
| 가상 격자, 비 7(표 밖) | `gsvg=RequestedNotApplied` + `reason=VirtualGridRefused (5) … restoredOriginal=1, code=-4` | `9e58cd6029598dfa` (원본) |
| 표 경로가 없는 파일 | `gsvg=RequestedNotApplied` + `The virtual-grid table was not found: D:\no\such\table.csv` | 원본 |
| Mock, 억제/가상 | `gsvg=RequestedNotApplied` | 원본 |

- **합성 시험 영상에는 격자가 없다.** 그래서 억제 모드가 `AppliedNoChange` 다. 이것이 C-97 위험 (a)가 말한 경우이고, 이제 "적용됨" 과 구별된다.
- **kVp 70 은 거부되지 않았다** (카드 4의 확인 항목). 제품 표의 kVp 축은 60/80/100/120 인데 70 으로 가상 격자가 `Applied` 였다. post 레인이 보고한 보간이 실제로 동작한다는 관측이다.
  - 다만 **보간 결과가 옳은지**는 이 레인이 판단할 수 없다. 축 위 값과 비교하지 않았다.

### 시험

| 시험 | 내용 |
|---|---|
| C-03 (이론 2건) | 모드별로 끄고 켰을 때, 상태가 `Applied` 면 픽셀이 달라야 하고 아니면 같아야 한다 |
| C-04 | 표 파일이 없으면 거부 + 사유 + 원본 유지 |
| C-05 | 3택1 배타 — 하나를 고르면 나머지 둘이 꺼진다 |
| C-06 | 격자비 10 ↔ 6 은 서로 다른 픽셀을 그린다 |
| C-07 | 표에 없는 격자비(7)는 거부되고 원본이 남는다 |
| U-05 | kVp 의 "연결 대기 — #180" 표시가 화면에 있고, 입력은 활성이다 |
| `PendingConnectionEntries_AreBoundAndCarryAnIssue` | 연결 대기 항목은 바인딩돼 있고 이슈 번호가 있으며 미연결·화면 상태와 겹치지 않는다 |
| `Plan_FollowsTheSettings` | 단계 순서가 `preprocess, gsvg` 이고 각 단계가 자기 설정을 따른다 |
| `GsvgMode_UnknownValueBecomesNone` | 알 수 없는 모드는 None |

### 반증

| 약화 | 결과 | 파일 |
|---|---|---|
| `GuiGsvgRunner.Interpret` 가 사유를 무시하고 늘 `Applied` 를 돌려주게 함 (재빌드 `BUILD_EXIT=0`) | **C-07 실패** — `The chain status never showed 'gsvg=RequestedNotApplied'; it reads '… gsvg=AppliedNoChange …'` | `f-always-applied.txt` |

- 복원 뒤 재빌드 `BUILD_EXIT=0`.
- 이 반증은 C-07(표 밖 격자비)에서만 잡힌다. 표 없음(C-04)은 네이티브 호출 전에 걸러지므로 영향을 받지 않는다.

### 전체 실행

| 실행 | 결과 | 파일 |
|---|---|---|
| 빌드 | `GUI=0`, `SC=0`, `E2E=0`, `INT=0`, 경고 0 | `build-final.txt` |
| 통합 | 실패 0 / 통과 239 / 건너뜀 1 | `full-int2.txt` |
| Mock E2E 전체 | 실패 0 / 통과 108 / 건너뜀 6 | `full-mock.txt` |
| **Native E2E 전체** | **실패 0 / 통과 114 / 건너뜀 0** | `full-native.txt`, `full-native.trx` |

- Mock 의 건너뜀 6건은 네이티브 전용 경우(C-02, C-04, C-06, C-07, IB-02, NativeProvenance)다.

## 3. 네이티브 산출물을 다시 스테이징했다

- 기존 `build/ci-776292b/bin` 의 `gsvg.dll` 에는 `xpe_gsvg_process_ex` 가 **없다**. 측정된 오류: `Unable to find an entry point named 'xpe_gsvg_process_ex' in DLL 'gsvg.dll'`.
- CI run **35275125717**(head `c72f0e8`, 성공)의 산출물을 `build/ci-c72f0e8/bin` 에 스테이징했다(`Stage-NativeArtifacts.ps1`, `stage.txt`). 로컬 빌드는 하지 않았다(#98).
- **이 사건 자체가 계약 확인이다**: 낡은 DLL 은 조용히 통과하지 않고, 사유가 화면에 나타났다("gsvg threw: Unable to find an entry point…"). 상태 문구에 사유를 넣은 덕분에 보였다.

## 4. 별건 커밋 — 오류코드 미러 (`0e6a60a`)

리더 요청으로 `XPE_ERR_INVALID_CALIB_DATA = -17` 을 C# 미러 두 곳에 넣었다. **지금은 빨강 3건이다(측정).**

| 실패 | 이유 | 언제 풀리나 |
|---|---|---|
| 파리티 2건 (`Copy_DeclaresNoCodeAbsentFromHeader`) | main 헤더에 아직 -17 이 없다. 시험은 헤더↔미러를 **양방향**으로 본다 | pre 의 헤더가 main 에 들어오면 |
| 메시지 1건 (`ErrorString_ForAllDefinedCodes…`) | 스테이징된 DLL 의 `xpe_error_string` 이 -17 을 모른다("Unknown error") | -17 을 아는 CI 산출물을 스테이징하면 |

- 즉 **미러가 먼저 들어가도, 헤더가 먼저 들어가도 그 사이 구간은 빨강이다.** 헤더와 미러가 같은 배치로 main 에 들어오면 빨강 구간이 없다. 병합 순서는 리더 판단이다.

## 5. 미검증 / 잔여 위험

- **HUD 표시**: 뷰포트 HUD 에 연쇄 상태를 그리도록 했지만, HUD 는 그려진 텍스트라 자동화로 읽을 수 없어 **단언하지 않았다**. 화면으로도 확인하지 않았다(스크린샷 없음).
- **kVp 보간의 정확성**: 70 이 거부되지 않는 것만 봤다. 60/80 사이 보간 결과가 옳은지는 보지 않았다.
- **선밀도 60 /cm**: 리더의 가정이다. 근거가 없다. 40 과 60 중 어느 것이 장비의 격자인지 모른다.
- **공기 신호 60000**: 모듈 헤더의 예시값이다. 검출기 근거는 없다.
- **표 설치 규칙**: 없다. 지금은 스테이징 디렉터리에 손으로 복사한 상태이고, 배포본에서 표가 어디 있을지는 정해지지 않았다.
- **vignette**: gain map 을 넘기지 않으므로 늘 0 이다. 비네팅 보정은 GUI 에서 쓸 수 없다.
- **성능**: 1024² 시험 영상에서만 돌렸다. 3072² 시간(REQ-GSVG-019)은 재지 않았다.
- **마스크**: 콜리메이션 필드 마스크를 넘기지 않으므로 모듈이 핸들당 한 번 경고를 남긴다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| GSVG 단계, 순서, 사유→상태, 폴백·경보 | GUI-C-101 |
| 구조화된 설정 JSON | GUI-C-101 |
| 표 경로 (설정 + DLL 옆), 기본값 6종 | GUI-C-101 |
| kVp 70 확인 | GUI-C-101 — 거부되지 않음 |
| 켜고 끄기·격자비·표 없음·배타·반증 시험 | GUI-C-101 |
| 연결 대기 범주 + 화면 표시 | GUI-C-101 |
| 오류코드 미러 | 별건 커밋 `0e6a60a` — 병합 순서 판단 필요 |
| 표 설치 규칙 | 리더 |
| HUD 단언, 선밀도·공기 신호 근거, 3072² 성능 | 새 카드 후보 |
