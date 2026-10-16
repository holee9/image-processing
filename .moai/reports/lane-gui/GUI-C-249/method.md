[2026-10-17 05:31 KST] GUI-C-249 방법과 예측 (실행 전에 커밋, Refs #273)

카드: 결함 맵 형식 판 2 를 앱이 받는지 확인한다(SRS FUNC-039 앱 쪽). 기준: dev/gui 7df31965(main 414c91ad 병합). 실행 전이라 아래 예측은 결과가 아니다.

## 1. 입력과 근거 요건

| 항목 | 값 |
|---|---|
| 판 2 세트 | `D:/workspace-github/xpe-data/cal-254/` (offset·gain·defect·manifest). defect.xcal 헤더 version=2, type=2, 3072×3072, 214232 바이트(압축) |
| 구판 세트 | `D:/workspace-github/xpe-data/calib-cyan-v3/` (defect.xcal version=1, 9437336 바이트) |
| 원시 영상 | GUI-C-247 과 같은 팬텀 영상 한 장(`tests/test_data/cyan_test/Bright_Foot phantom_55kv_200ma_8mas(SID_110)_03.raw`) |
| 판 2 를 읽는 DLL | dev/preprocess `260cef18` 의 CI 실행이 끝난 뒤 그 실행의 네이티브 DLL 세트를 Get-CiNativeDlls 로 **개인 폴더** `D:\workspace-github\xpe-data\gui-249-dlls` 에 받는다. 공유 폴더는 읽지도 지우지도 않는다. CI 실행 번호·headSha·sha256 은 보고서에 적는다 |
| 사전 확인 | xpe-pre 워크트리의 빌드 산출물은 쓰지 않는다(DLL 수정 시각 05:18 이 커밋 시각 05:15 보다 늦고 미커밋 변경이 있어 260cef18 이 아니다). 리더가 CI 산출물 방식으로 정했다 |

## 2. 실행 (한 번에 하나, UIA 만, 실행 전후 가용 메모리 기록, 남은 프로세스 0 확인)

앱 처리 2회는 기존 `QuickStartWalkthroughScenarios.W16_ARealFrameCalibrationSet_IsUsedByTheApp_WithoutWarnings` 를 그대로 쓴다(시험 코드 변경 없음). 환경변수만 바꾼다.

```
XPE_C231_NATIVE_DIR=D:\workspace-github\xpe-data\gui-249-dlls  XPE_C247_CALIB=<세트>  XPE_C247_RAW=<영상>
dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj --no-build --filter "FullyQualifiedName~W16_ARealFrame"
```

| 실행 | 세트 |
|---|---|
| A | cal-254 (판 2) |
| B | calib-cyan-v3 (구판), 같은 DLL |

손상 파일 3종은 새 E2E 시나리오 한 개(손상 파일을 시험 코드가 개인 폴더에 만든다)로 각각 한 번 적재한다. 원본은 cal-254 의 세 파일을 읽기만 하고, 개인 폴더 `D:\workspace-github\xpe-data\gui-249-corrupt\<종류>\` 에 복사한 뒤 defect.xcal 만 고친다.

| 종류 | 만드는 법 |
|---|---|
| 잘린 파일 | defect.xcal 을 앞 100,000 바이트에서 자른다(헤더 152 바이트는 온전, 본문 부족) |
| 미지원 판 번호 | 헤더 4~7 바이트(version)를 2 에서 3 으로 바꾼다 |
| 줄 기록 범위 밖 | 압축을 풀어 확장 블록의 첫 줄 기록(orientation 0=행)의 index 를 5000(높이 3072 초과)으로 바꾸고, config‖payload 의 sha256 을 다시 계산해 헤더에 넣은 뒤 같은 방식으로 다시 압축한다 |

## 3. 확인 항목과 기대 결과 (예측)

앱 처리 (A, B 공통):
1. 상태줄 `preprocess=Applied`, HUD `3 stages (offset · gain · defect)`, 로그의 `Preprocess:` 줄에 offset→gain→defect.
2. 알림은 GUI-C-247 과 같은 기준: ALERT WARN/ERROR 중 `Exposure deviation:`(처리된 영상의 노출 지수 알림, 보정 단계가 아님)을 뺀 것이 0건. INFO 는 허용.
3. 예측: **A 도 B 와 같은 결과(통과)**. 앱은 xcal 을 직접 읽지 않고 폴더를 네이티브 모듈에 넘기므로, 판 2 의 촬영 영역 평면·줄 기록·덩어리는 앱 코드에서 쓰이지 않는다. 처리는 "무시하고 결함 평면으로 보정"이 된다. 촬영 영역 밖 처리는 QA-A-255 몫이라 이번에 확인하지 않는다. 앱 쪽 코드 변경은 필요 없다고 예측한다.
4. 반증 조건: A 에서 `failed`, 3단계 미충족, serious 알림이 1건이라도 있으면 예측이 틀린 것이고 원인을 적는다.

손상 파일 (각 한 번):
5. 세 경우 모두 `preprocess=Applied` 가 되지 **않고**, 로그에 ALERT ERROR 가 남고, 앱은 죽거나 멈추지 않는다.
6. 사유 문구 예측: 미지원 판 번호는 `XPE_ERR_XCAL_VERSION` 과 "file version 3 ... is not supported" 가 보인다. 줄 기록 범위 밖은 `XPE_ERR_XCAL_<종류>` 문구에 범위 사유가 보인다. **잘린 파일은 불확실**: 오류 코드(IO_FAILED 또는 CONFIG_INVALID)만 나오고 사유 문구가 없을 수 있다. 문구가 없으면 "사유가 보이는 오류"가 아니므로 결함으로 기록하고, 고칠 곳(앱이 코드를 번역하는지, 모듈이 문구를 주는지)을 가려 최소로 고친다.
7. 반증: 시험의 사유 단언을 의도적으로 틀린 문구로 바꿔 빨강이 되는지, 손상 대신 원본 판 2 를 넣으면 같은 시험이 빨강이 되는지 확인한다.

## 4. 단언하지 않는 것

- 처리된 영상이 판 2 에서 구판과 화소 단위로 같다고는 말하지 않는다(촬영 영역 처리는 QA-A-255).
- 이 시험은 CI 에서 돌리지 않는다(세트가 저장소 밖). 보고서 인용만 한다.
