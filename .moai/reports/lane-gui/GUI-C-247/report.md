[2026-10-15 18:17 KST] GUI-C-247 보고서 — 실프레임 보정 세트(calib-cyan-v1)를 앱에서 쓴다 (1단계 B14, Refs #251)

기준 61c8ddba 위의 새 커밋. 푸시하지 않는다. 실행 범위는 카드대로 시나리오 1건 × 세트 2개(cyan-v1, 대조군 real-v2)이며, Native 전체·IntegrationTests 전체는 돌리지 않았다. 시험 프로세스는 한 번에 하나, 각 실행 뒤 남은 `ImageProcTest`/`testhost`/`xpe_ai_worker` 는 0이다.

## 1. 결과

| 세트 | 결과 | 처리 | 단계 | 알림 | 노출 지수 |
|---|---|---|---|---|---|
| `calib-cyan-v1` | **통과** (14 s) | `preprocess=Applied` | offset -> gain -> defect | WARN 1 (노출 편차만), INFO 1 | EI 3143.87, DI 11 |
| `calib-real-v2` (대조군) | 처리 성공, **경고 단언에서 실패(의도된 구별)** | `preprocess=Applied` | offset -> gain -> defect | WARN 3 (노출 편차 + 아래 2건), INFO 2 | EI 4111.11, DI 12.16 |

- 입력 프레임: `tests/test_data/cyan_test/Bright_Foot phantom_55kv_200ma_8mas(SID_110)_03.raw` (18874368 바이트 = 3072×3072×2).
- cyan-v1 의 알림 전문(`run_cyan_v1.txt`): `ALERT WARN NATIVE_ALERT: Exposure deviation: DI=11.00 (outside [-3.0, +3.0] range)` 와 `ALERT INFO REAL_DISPLAY_BACKEND_ACTIVE`. **보정 단계에서 나온 경고·오류는 0.**
- 대조군에만 나온 경고(`run_control_real_v2.txt`): `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT` (44053 화소, 0.467%), `XPE_WARN_CALIB_SESSION_UNSPECIFIED`. 같은 시나리오·같은 프레임인데 세트만 바꾸니 노출 지수와 경고 목록이 달라졌으므로, 시나리오는 지정한 보정 폴더를 실제로 읽는다.

## 2. 카드와 다르게 한 점·주의

- **"경고 0" 단언은 노출 편차 경고 하나를 이름으로 허용한다.** `Exposure deviation: DI=11.00` 는 처리된 영상의 노출 지수(범위 ±3 밖)에 대한 알림이고 보정 단계에서 나온 것이 아니다. 시험은 ERROR 와 "노출 편차 외 WARN" 이 0임을 단언하고 INFO 는 경고로 세지 않는다. 이 허용은 통과를 만들려고 고른 것이므로 알림을 전부 출력에 남겼다. 노출 편차 자체(팬텀이 기준 노출과 DI 11 만큼 다름)는 보정 세트의 결함이 아니며 이 카드가 판정하지 않는다.
- **"저장 결과의 단계 목록"은 저장 파일이 아니라 앱의 결과 기록으로 읽었다.** 단계 목록은 상태줄/로그의 `Preprocess: xpe_preprocess_pipeline_out (offset -> gain -> defect, input kept)` 줄과 HUD `Calib: 3 stages (offset · gain · defect)` 에서 읽었다. 보정 영상 저장(File > Save)은 하지 않았다.
- **처리된 화면 캡처는 쓸 수 없다.** `picture_calib-cyan-v1.png`(1560×920)는 창 사각형의 화면 복사인데 창 안쪽이 전부 흰색이다. 이 세션이 WPF 화면 내용을 읽지 못하는 기존 한계(`CopyFromScreen` 이 흰색)와 같은 현상이며, 시험은 창 안쪽이 균일하면 그렇게 출력한다. PNG 는 커밋하지 않았다(증거는 .md/.txt 만). 화면 캡처는 화면을 읽을 수 있는 환경(CI 러너 또는 사용자 데스크톱)에서 같은 시험을 `XPE_C247_PICTURE` 와 함께 돌리면 나온다.
- 이번 경로에서 처음 만든 캡처 도우미의 "변화 있음" 판정은 제목 줄에 속았다(첫 실행에서 "has variation" 이라고 출력). 안쪽만 보도록 고쳐서 다시 돌렸다.

## 3. 시나리오

`QuickStartWalkthroughScenarios.W16_ARealFrameCalibrationSet_IsUsedByTheApp_WithoutWarnings` (`[EnvGatedFact]`, `XPE_C231_NATIVE_DIR` 가 있을 때만 발견되므로 CI 의 Native E2E 잡에는 나타나지 않는다). 키·마우스 입력 없이 UIA 로 Open Raw 대화상자를 처리한다. 경로는 환경 변수로 받는다: `XPE_C247_CALIB`(기본 `D:\workspace-github\xpe-data\calib-cyan-v1`), `XPE_C247_RAW`(기본 위 팬텀), `XPE_C247_PICTURE`(캡처 경로). pre 가 `calib-cyan-v2` 를 내면 `XPE_C247_CALIB` 만 바꿔 한 번 더 돌린다(카드 3번, 리더가 알릴 때).

## 4. 실행 (명령은 모두 같은 형태)

```
XPE_C231_NATIVE_DIR=D:\workspace-github\xpe-data\gui-247-dlls  XPE_C247_CALIB=<세트>  XPE_C247_PICTURE=<png>
dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj --no-build --filter "FullyQualifiedName~W16_ARealFrame"
```

| 실행 | 결과 | 가용 메모리 전→후 | 남은 프로세스 |
|---|---|---|---|
| cyan-v1 (`run_cyan_v1.txt`) | 통과 14 s | 4861 → 4970 MB | 0 |
| real-v2 (`run_control_real_v2.txt`) | 경고 단언 실패(위 1절) | 4943 → 4715 MB | 0 |

- DLL: 개인 폴더 `D:\workspace-github\xpe-data\gui-247-dlls` (공유 `image-processing/build/e2e-native-dlls` 의 `*.dll` 과 `provenance.json` 을 **읽기만 해서 복사**, `xpe_preprocess.dll` sha256 일치 확인). 공유 폴더는 쓰지 않았다. 출처는 CI run 38509221829, headSha 6b8477ef.
- 앱의 네이티브 모듈 감사 줄: `pinned folder 'D:\workspace-github\xpe-data\gui-247-dlls'; 6 native module(s) loaded, 6 from the pinned folder, 0 from elsewhere`.

## 5. 한계

- 처리된 화면의 시각 확인은 못 했다(위). 처리 성공은 상태줄·HUD·로그 텍스트로 확인했다.
- 대조군 실행은 "경고 단언 실패"로 끝나므로 xunit 결과로는 빨강이다. 이것은 시나리오가 세트를 구별한다는 증거로 쓴 것이며, 대조군이 처리에 실패했다는 뜻이 아니다.
- 이 시나리오를 CI 에서 돌리는 설정은 만들지 않았다(보정 세트가 저장소 밖 `xpe-data` 에 있음).
- 로컬 기존 변경(`.claude/scheduled_tasks.lock`, GUI-C-231/232 증거 등)은 건드리지 않았다.

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. 시나리오 1건, cyan-v1, 처리 성공·경고/오류 0·단계 offset/gain/defect | GUI-C-247 (1·3절) |
| 2. 대조군 calib-real-v2 1회 | GUI-C-247 (1절) |
| 3. calib-cyan-v2 1회 | 리더가 알릴 때(미착수) |
| 산출: 화면 캡처 1장 | 얻지 못함(2절, 읽기 한계) |

## 6. 추가 실행: calib-cyan-v2 (카드 3번, 리더 요청 18:31)

- `XPE_C247_CALIB=D:\workspace-github\xpe-data\calib-cyan-v2`, 같은 프레임·같은 단언으로 1회(`run_cyan_v2.txt`): **통과** 14 s.
- `preprocess=Applied`, 단계 offset -> gain -> defect, HUD `3 stages (offset · gain · defect)`. ALERT 는 cyan-v1 과 같은 WARN 1(노출 편차 DI=11.00)+INFO 1 이고 보정 단계 경고·오류 0.
- 노출 지수 EI 3143.86 (v1 3143.87): 두 세트가 거의 같은 보정 결과를 냈다. 세트를 읽는지는 1절 대조군으로 확인했다.
- 가용 메모리 6628 -> 6603 MB, 남은 ImageProcTest/testhost/xpe_ai_worker 0.
- 화면 캡처는 같은 한계(창 안쪽 흰색)로 얻지 못했다.
