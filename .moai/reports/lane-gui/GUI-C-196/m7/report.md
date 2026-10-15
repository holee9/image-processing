# GUI-C-196 M7 — 표시 단계 float 중간값의 비유한 수

증거: `falsification_arms.txt`(이 폴더). M6 보고서가 한계로 남긴 칸("표시 단계의 float 중간값은 비유한을 세지 않는다")을 닫는다.

## 변경

| 파일 | 내용 |
|---|---|
| `Services/BaselineDisplayStage.cs` (새, WPF·네이티브 없음) | 표시 단계의 규칙. 입력은 픽셀과 백엔드뿐이고 **설정을 받지 않는다**(시그니처가 증거). modality LUT 뒤와 VOI LUT 뒤, 즉 아직 float 인 두 지점에서 비유한을 센다. 하나라도 있으면 그 자리에서 **멈추고**(오염된 영상에 다음 LUT 를 돌리지 않는다) 픽셀 없이 수를 돌려준다. `ComposeRun` 이 체인이 전부 적용됐을 때만 표시를 돌리고 그 수를 `BaselineSingleRun.NaNInfCount` 에 넣는다 |
| `Services/Native/GuiBaselineDisplayNative.cs` (새) | 같은 네이티브 함수·같은 순서(alloc → modality → VOI → presentation → 16비트 읽기)로 부르는 `IBaselineDisplayBackend` 구현 |
| `Services/RealXpeBackend.cs` | `RunBaselineDisplay` 를 지우고 `RunBaselineOnce` 가 `BaselineDisplayStage.ComposeRun` 을 호출 |
| `Services/BaselineExecution.cs` | 증거 JSON 의 `nonFiniteByStageRun1` 에 `display=<수>` 항목 추가(`nanInfCount` 합산은 M6 의 `BaselineRunner` 가 이미 `run.NaNInfCount` 를 더한다) |
| `gui/ImageProcTest/README.md` | 판정 기준 (3)에 표시 단계의 두 지점을 명시, `nonFiniteByStageRun1` 설명, "표시 단계 float 는 세지 않는다"는 한계 문장을 "프레젠테이션 LUT 뒤 출력은 16비트라 비유한을 담을 수 없어 그 앞의 float 만 검사한다"로 교체(197b 문장 정정) |

**프레젠테이션 LUT 뒤는 세지 않는다**: 그 출력은 16비트 버퍼라 NaN/Inf 가 표현될 수 없다(그래서 그 앞의 두 지점이 검사 가능한 float 의 전부다).

## 시험

- `BaselineReviewFixTests` +7: 정상 시 LUT 세 개가 고정 매개변수로 순서대로 호출되고 수 0·픽셀 길이 일치 / modality 뒤 주입 → 수·픽셀 없음·**VOI 이후가 호출되지 않음** / VOI 뒤 주입 → 수·픽셀 없음·프레젠테이션 미호출 / 거절된 LUT → 함수 이름과 코드가 든 예외 / 체인의 한 단계가 거절되면 표시를 열지도 않음 / **진짜 체인 러너·어댑터·`ComposeRun` 을 거친** 주입 → `Passed=false`·합 4(실행당 2)·"4 non-finite"·DICOM 미기록·JSON 의 `nanInfCount=4` 와 `display=2` / 주입 없이 같은 조립은 통과(대조).
- 네이티브 `BaselineDisplayNativeTests` 2건: 실제 `xpe_display.dll` 에서 표시 단계가 유한·결정적(두 번 동일)·영상을 바꿈, 그리고 **모듈 함수를 이전 구현과 같은 순서로 직접 부른 결과와 비트 단위로 같음**(이동이 출력을 바꾸지 않았다는 확인).
- M4 의 소스 대조 시험 `TheBaselineDisplay_ReadsNoUserDisplaySetting` 을 새 위치(스테이지에 `AppSettings` 없음, `RunBaselineOnce` 가 `ComposeRun` 호출)로 옮겼다.
- 결과: Functional 기본 환경 **518 통과·0 실패·5 건너뜀**, 네이티브 디렉터리 지정 환경 **522 통과·0 실패·1 건너뜀**. E2E `AutomationReportBackendTests`+`DeterministicBaselineScenarios` 기본 환경 **18 통과·0 실패·5 건너뜀**. 로컬 네이티브에서 A19(두 번 비트 동일, DICOM valid, 242 ms)·A20·A21·B01·B02(클릭→통과 줄 812 ms) 통과. SelfCheck·계약 러너 통과.
- 반증 8건(`falsification_arms.txt`), 전부 바이트 동일 복원: modality 뒤 세기 제거 / VOI 뒤 세기 제거 / 실행이 표시 수를 버림 / 실제 백엔드가 조립을 우회 / 증거 JSON 에서 `display` 항목 제거 / 오염된 영상에서 단계가 계속됨 / 거절된 단계 뒤에도 표시 실행 / 네이티브 VOI 출력 범위 변경(네이티브 등가 시험). 각각 대응 시험만 빨개졌다.

## 미검증·한계

1. 표시 단계의 비유한 주입은 **가짜 백엔드**로만 했다(실제 `xpe_display.dll` 에 NaN 을 넣어 보지는 않았다 — 입력이 uint16 이라 정상 경로에서는 만들 수 없다). 실제 모듈에서 확인한 것은 "정상 입력에서 수 0·결정적·직접 호출과 동일"까지다.
2. 검사는 두 시점(modality 뒤, VOI 뒤)이다. 모듈 내부 중간값은 볼 수 없다.
3. 검사마다 float 영상을 한 번씩 복사해 읽는다(1024²에서 눈에 띄는 비용은 관찰하지 못했고 전체 242 ms 안이다). 3072² 에서의 비용은 재지 않았다.
4. M5·M6 의 한계(CI Native 잡 미확인, 3072² 전체 시간, 비-949 코드 페이지 한글 경로)는 그대로다.
