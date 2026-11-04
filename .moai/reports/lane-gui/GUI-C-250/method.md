[2026-10-17 16:15 KST] GUI-C-250 방법과 예측 (실행 전에 커밋, Refs #273)

카드: IAP-CALIB-001 1부와 빠른 시작을 문서대로 따라 cyan_test 로 2판 결함 맵 보정 세트를 만들고, 앱에서 영상 한 장을 판 1 세트와 판 2 세트로 각각 보정해 비교한다. 문서와 실제가 다른 곳을 전부 적는 것이 산출이다. 기준: dev/gui 336d9282(main 병합 후).

## 1. 입력

| 항목 | 값 |
|---|---|
| 문서 | `docs/calibration/IAP-CALIB-001_Image_Acquisition_Protocol.md` 1부 §1.2~1.4, `gui/ImageProcTest/help/quick-start.ko.html` (dev/gui 336d9282 의 파일 그대로) |
| 도구·DLL | dev/preprocess `c19e98a0dfd2b538292b8d79211f16406d6c8aa3` 의 CI 실행 38992249313 산출물을 개인 폴더 `D:\workspace-github\xpe-data\gui-250-dlls` 에 받았다(실행 결론 cancelled: clang-tidy 취소, 나머지 잡은 success). 249 와 같은 스크래치 다운로더(main 성공 push 검사만 뺀 복사본). 아티팩트 ID·ZIP SHA-256 은 보고서에 적는다 |
| 원시 프레임 | `D:\workspace-github\image-processing\tests\test_data\cyan_test\calfiles\` (이 워크트리에는 없다). 다크 `Dark_08~14`, 게인용 `Bright_23~28`, 결함 맵용 `Bright_17~28`·`Bright_31~36`(18장), 문서 1.3 의 검증된 예와 같다 |
| 보정할 영상 | `...\cyan_test\Bright_Foot phantom_55kv_200ma_8mas(SID_110)_03.raw` (GUI-C-247·249 와 같은 한 장) |
| 판 1 세트 | `D:\workspace-github\xpe-data\calib-cyan-v3` (읽기만) |

## 2. 순서 (문서의 어느 절을 어떤 순서로)

1. IAP 1.3 의 명령 형식으로 목록 파일 3개(다크, 게인용, 결함 맵용)를 만든다. 개인 폴더 `D:\workspace-github\xpe-data\gui-250-set-inputs\` 에 둔다.
2. IAP 1.3 의 명령에 `--bpm-v2 --bpm-margins 6,2,1,1` 만 더해 `xpe_calib_fixture_gen.exe` 를 1회 실행한다. IAP 1부에는 이 두 옵션이 없다. 옵션은 `--help` 와 카드에서 찾았다(문서 공백으로 기록). 만료는 빠른 시작이 쓰는 `--expiry-ms 4102444800000`(IAP 는 값을 정하지 않음).
3. `manifest.json` 의 `defect_format` 을 인용한다.
4. IAP 1.4·빠른 시작 절차대로 앱을 띄워(UIA, 개인 DLL 폴더) 영상 한 장을 판 2 세트로, 이어서 판 1 세트로 보정한다. 상태줄·알림을 그대로 적는다. 같은 창에서 File > Save Corrected Image (float32 .raw) 로 보정 결과를 저장하고 PNG(16-bit) 도 저장한다. 화면 캡처도 시도한다.
5. 두 float 결과를 파이썬으로 비교한다: 바뀐 화소 수, 위치(촬영 영역 안·밖, 결함 위치), 크기.
6. 따라가며 문서와 실제의 차이를 목록으로 적는다(명령, 옵션, 문구, 화면, 경로).

## 3. 예측 (결과가 아니다)

생성:
1. `Get-CiNativeDlls.ps1` 은 main 성공 push 실행이 아니면 거부하므로 문서 그대로는 병합 전 산출물을 못 받는다(이미 스크래치 복사본으로 받았다).
2. 생성은 종료 코드 0 으로 끝나고 네 파일이 생긴다. `defect.xcal` 의 헤더 version 은 2 이다.
3. `--bpm-v2` 는 "첫 다크 프레임을 건너뛴다"고 도움말에 쓰여 있고, IAP 는 사용자가 목록에서 첫 장을 직접 빼라고 한다. 둘을 함께 따르면 `Dark_08` 이 한 번 더 빠져 offset 이 6장(09~14)으로 만들어질 것이다. 이전 cal-254(목록 07~14, 건너뜀 포함)와 offset 이 달라질 것이고, 결함 평면의 수도 cal-254 의 2,353 과 조금 다를 것이다(같으면 우연).
4. 영역 밖 화소는 여백 6,2,1,1 에서 계산상 30,699 개다: 9,437,184 − (3072−7)×(3072−3) = 30,699. 줄 그룹 수는 0 일 것이다(cal-254 가 0).
5. 생성 시간은 문서의 "약 7분" 근처일 것이다.

앱:
6. 판 1·판 2 모두 `preprocess=Applied`, `3 stages (offset · gain · defect)`, 보정 단계 경고 없음(노출 편차 알림 1건은 영상 때문에 남는다). 빠른 시작이 말하는 경고 `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT` 와 `XPE_WARN_CALIB_SESSION_UNSPECIFIED` 는 실제 1단계 세트에서 나온다고 하는데, 이 세트에서는 안 나올 것이다(GUI-C-247 에서 안 났다).
7. 화면 캡처는 GUI-C-247 에서 창 안쪽이 전부 흰색이었던 읽기 한계가 그대로일 가능성이 높다. 그러면 앱의 PNG 저장(표시용)을 화면 대신 쓴다.

차이 수치:
8. 판 2 와 판 1 의 offset 이 달라 거의 모든 화소가 조금씩 다를 것이다. 그래서 "바뀐 화소 수"만으로는 결함 맵의 효과가 안 보이고, 결함 맵 효과는 결함 위치(두 맵의 합집합) 안에서의 큰 차이로 따로 읽는다. 촬영 영역 밖 가장자리는 아직 별도 구현이 없다면(QA-A-255) 판 1 과 같은 방식으로 보정돼, 차이가 offset 변화 크기에 머물 것이다.

## 4. 단언하지 않는 것

- 보정 결과가 옳다는 것(정답이 없다). 두 세트가 같은 보정을 낸다는 것.
- 화소 동등성의 증거로 EI/DI 가 같다는 점을 쓰지 않는다.

## 5. 실행 범위

세트 생성 1회, 앱 처리 2회(판 2 먼저, 판 1 다음), 한 번에 하나, 실행 전후 가용 메모리 기록, 남은 프로세스 0 확인, UIA 패턴과 창 사각형 화면 복사만 쓴다(전역 입력 없음). 전체 스위트는 돌리지 않는다.
