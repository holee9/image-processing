# GUI-C-233b 보고 — Codex #171 보류 4건 (Refs #251)

커밋(dev/gui): 4번 `359a12b8`(3d0a3fbc 병합)·`9f207ff6`(docs 행) — 이미 push됨. 1번 `2e0ae2d6`. 2·3번은 이 보고서와 함께 커밋.

## 1. HUD·설정 패널은 실제 적용한 창을 보여 준다
- `LoadedImageFrame.AppliedVoi`(모드·중심·폭·자동 여부): Real 은 `xpe_voi_auto_window` 가 정한 값(수동이면 설정값), Mock 은 근사 창 값을 싣고, VM 이 `RenderedVoi*` 를 그것으로 채움. 싣지 않았는데 자동 창이면 설정의 32768/65535 대신 비움.
- `DisplayInputsDiffer` 에 `VoiWindowAuto` 추가; 자동일 때는 쓰이지 않는 수동 값의 변화를 stale 로 보지 않음.
- SelfCheck 시나리오 10 (`selfcheck_hud.txt`, exit 0): 자동 창의 HUD = 적용값(1234.5/987/LinearExact, 설정은 32768/65535/Linear) / 자동→수동 전환은 stale, 다시 그리면 수동값 / **자동 창 호출이 실패하면** 화면의 그림과 HUD 는 그대로, 상태 줄 `Display pipeline failed: xpe_voi_auto_window failed (-1)` 와 `DISPLAY_PIPELINE_FAILED` 알림.
- 반증: `SetRenderedVoi` 를 설정값으로 되돌리면 시나리오 10 빨강(`selfcheck_mutation_hud_from_settings.txt`).
- 한계: 실제 Native 에서 HUD 가 적용값을 보이는지는 UI 로 관측하지 않았습니다(가짜 backend 기준). Real 의 `AppliedVoi` 는 코드 경로만 확인.

## 2. 232d — 경고 코드 단언 제거, 실제 러너로 한 번 더
- 가짜 문장은 `xpe_preprocess_pipeline_out failed (-1).` 만 (러너가 실제로 쓰는 문장); `XPE_WARN_*` 는 앱이 읽지 않으므로 단언하지 않음.
- 새 시험 `TheRealRunner_ReceivingInvalidInputFromTheRealModule_FailsTheBaseline_WithTheCallAndTheCode`: **실제 `GuiPreprocessRunner` + 실제 xpe_preprocess.dll**(`GuiPreprocessRunner.cs` 를 통합 시험에 링크; 걸림돌이던 `PreprocessRunResult.ProcessedPreview`(WPF 형식)는 어디서도 쓰이지 않아 삭제). 3072×3072 프레임을 1024×1024 맵(`XPE_C231_SETS\ok1024`)에 넣으면 모듈이 -1 을 돌려주고, 러너 문장이 Baseline 에 들어가 Baseline 이 Fail·DICOM 0건, 상태와 JSON 에 `preprocess` 와 `xpe_preprocess_pipeline_out failed (-1)`. 데이터가 없으면 건너뜀(CI 에서는 건너뜀 — 환경 변수 있을 때만 발견되는 시험이 아니라 일반 SkippableFact 이므로 CI 의 '설명 없는 건너뜀' 게이트 영향 여부는 확인 못 함).
- 한계: "가짜 네이티브"(스텁 DLL)는 만들지 않았습니다 — 이 레인은 네이티브를 빌드하지 않습니다. 대신 **진짜 모듈**이 -1 을 내는 입력을 썼습니다.

## 3. 한 프로세스에 xpe_common 두 개 — 원인 규명과 정리 (flaky_two 흡수)
- **원인**: 통합 시험 csproj 의 `CopyXpeDllsForTests` 가 `build/ci-common/bin`(9월 26일 빌드)의 xpe_common·xpe_preprocess·spdlog·fmt 를 시험 출력 폴더(`bin/Debug/net8.0`)로 복사해 둔다. XPE_NATIVE_DIR 을 다른 폴더로 지정해 돌리면, 로케이터가 **앱 폴더를 배타적 절단보다 먼저** 제시하므로 어떤 시험은 출력 폴더의 복사본을, 다른 시험은 XPE_NATIVE_DIR 을 적재한다 → 한 프로세스에 두 인스턴스. 어느 쪽을 먼저 적재하느냐가 시험 클래스 실행 순서에 달려 간헐적.
- **재현(반증)**: 출력 폴더에 오래된 복사본 4개를 두고 XPE_NATIVE_DIR 로 돌리면 `NativeCommonSingleInstanceTests` 가 `xpe_common.dll at …\c233_native\… and …\bin\Debug\net8.0\xpe_common.dll` 로 빨강(2회 중 1회, 순서 의존과 일치; `falsification_stale_copies_in_bin.txt`). 복사본을 지운 상태에서는 통합 전체 4회 연속 통과.
- **정리**: csproj — `XPE_NATIVE_DIR` 이 설정돼 있으면 그 폴더가 모듈의 유일한 출처이므로 복사하지 않고 출력 폴더의 오래된 복사본을 **삭제**(설정 안 됐으면 기존 스테이징 그대로). 부수 효과: 이전에는 `xpe_preprocess` 를 출력 폴더의 9월 복사본이 몰래 해결해 주던 시험이 있었음(새 시험이 그걸 드러냄 — 새 시험은 공유 로더로 XPE_NATIVE_DIR 에서 적재).
- **가드**: `NativeCommonSingleInstanceTests` 의 "한 모듈이 두 경로" 검사에 spdlog.dll·fmt.dll 을 추가하고, 메시지에 흔한 원인과 해결(XPE_NATIVE_DIR 을 설정하고 다시 빌드)을 넣음.
- **GainPolyClamp 빨강**: 이번 정리 뒤 통합 전체를 5회(+정리 전 6회) 돌려 재현되지 않았음. 원인으로 의심하나 **증명하지 못했습니다** — 앞의 두 번 실패는 같은 출력 폴더 혼재 상태에서 나왔다는 정황뿐.

## 4. 결합 검증 준비
- `3d0a3fbc` 병합(충돌 없음), troubleshooting.md 행 교체(7/7 통과). push 완료(리더). P10/P11 골든은 CI 결과·산출물로 측정해 다음 카드에서.

## 미검증·참고
- 통합 945 통과/0 실패/2 건너뜀(임시 DLL 폴더 기준, 출처 UNPROVEN 은 233 보고서와 같음).
- 234 의 미완 작업은 로컬 side branch `wip/gui-c-234`(342b5591)에 있음.
