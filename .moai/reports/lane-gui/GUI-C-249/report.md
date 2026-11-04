[2026-10-17 05:58 KST] GUI-C-249 보고서 — 결함 맵 형식 판 2 를 앱이 받는지 (SRS FUNC-039 앱 쪽, Refs #273)

기준: dev/gui 7df31965(main 414c91ad 병합) 위의 새 커밋. 푸시하지 않는다. 방법과 예측은 실행 전에 f3fc29b2 로 커밋했다(그 커밋의 첫 줄 시각 05:31 은 `date` 로 읽지 않은 오기였고 이번 커밋에서 05:22 로 고쳤다). 실행 범위는 카드대로 앱 처리 2회와 손상 파일 3회(대조군 1회, 반증 1회 추가)이며 전체 스위트는 돌리지 않았다.

## 1. 결과 (결론)

**판 2 세트와 구판 세트가 이 앱에서 각각 적재에 성공했고, 손상 파일 세 가지는 사유가 보이는 오류로 거부됐다. 앱 코드 변경은 필요 없었다.** 두 세트의 결함 평면은 서로 다르므로(Codex #218 의 직접 대조: 비영 화소 2,353개 대 134개, 바이트가 다른 위치 2,461개; offset·gain 파일은 두 세트가 같다) "같은 보정을 한다"거나 FUNC-039 전체가 끝났다는 뜻이 아니다(GUI-C-249b 에서 정정). 예측 7항목이 모두 맞았다(한 항목은 예측보다 좋았다: 잘린 파일도 사유 문구가 나왔다).

| 실행 | 입력 | 결과 | 처리 | 화면에서 읽은 것 |
|---|---|---|---|---|
| A | cal-254 (판 2) | 통과 13 s | `preprocess=Applied` | HUD `3 stages (offset · gain · defect)`, 알림은 `Exposure deviation: DI=11.00` 1건과 INFO 1건뿐 |
| B | calib-cyan-v3 (구판) | 통과 13 s | `preprocess=Applied` | A 와 같은 HUD·알림 |
| D1 | 잘린 파일 | 거부 확인 | `preprocess=RequestedNotApplied` | `XPE_ERR_XCAL_TRUNCATED: the file is 100000 bytes but its header declares 214232 (config 410 + payload 213670 + the header): it ends early`, 줄: `Loading defect calibration failed (-9)` |
| D2 | 미지원 판 번호(3) | 거부 확인 | `RequestedNotApplied` | `XPE_ERR_XCAL_VERSION: file version 3 (type 2) is not supported: this reader reads version 1 (every type) and version 2 (defect maps only)`, `failed (-4)` |
| D3 | 줄 기록 범위 밖(행 5000) | 거부 확인 | `RequestedNotApplied` | `XPE_ERR_XCAL_EXTENSION: line 0: lines 5000..5000 are outside the frame (it has 3072 lines of this orientation)`, `failed (-4)` |
| D4 | 대조군(같은 줄 기록, 행 100) | 통과 | `preprocess=Applied` | 알림 serious 0 |

- 입력 영상: `tests/test_data/cyan_test/Bright_Foot phantom_55kv_200ma_8mas(SID_110)_03.raw` (GUI-C-247 과 같은 한 장).
- A 와 B 의 화면 지표는 `uncalibrated EI = 3143.86, DI = 11` 로 같게 표시됐으나 화소 동등성의 증거가 아니다(영상을 비교하지 않았고 두 세트의 결함 평면은 다르다). 표시 창 값도 소폭 다르다(Codex 가 읽은 값: A C3667.694/W1129.825, B C3667.693/W1129.824).
- **판 2 의 영역·줄·덩어리 정보는 앱이 쓰지 않는다.** 앱은 보정 폴더를 네이티브 모듈에 넘기기만 하고 xcal 을 직접 읽지 않는다. 그래서 처리는 "무시하고 결함 평면으로 보정"이 된다. 촬영 영역 밖 처리(보간 원천에서 제외, 제자리 유지, 자르지 않음)는 QA-A-255 의 몫이라 이번에 확인하지 않았다.
- 대조군 D4 의 의미: D3 과 D4 는 줄 기록의 행 번호(5000 과 100)만 다르다. D4 가 통과하므로 D3 의 거부는 줄 기록 구성의 다른 오류가 아니라 범위 때문이다.
- 거부된 세 경우에서 앱은 죽거나 멈추지 않았고(UIA 읽기 계속 가능), 거부 시 `enhance_basic=Applied` 는 그대로 나온다. 즉 보정이 거부돼도 향상 단계는 보정 없는 영상에 대해 돌아간다. 이것이 의도인지는 이번 카드 범위 밖이라 판단하지 않는다.

## 2. 입력 DLL (판 2 를 읽는 빌드)

- CI 실행 **38961608579**, 브랜치 dev/preprocess, headSha **260cef183e5cc8628af17efbd20bb855ebb439af**.
- **CI 실행 결론은 failure**(doxygen·cppcheck·clang-tidy 게이트 빨강, preprocess-tests 시간 초과 취소). DLL 산출은 빌드 단계가 성공해 올라온 산출물(`xpe-ci-common/preprocess/post/dicom-binaries`)이다.
- **빨강들과 이번 결과의 관계 판단:** 빨강은 문서·정적 분석·시험 시간 초과이고 산출 DLL 의 빌드를 막지 않았다. 앱이 이 DLL 로 판 2 를 읽고 손상 사유를 낸 것을 직접 관측했으므로 이번 결과는 그 빨강과 관계없다. 다만 preprocess-tests 가 취소돼 이 DLL 에 대한 모듈 시험 결과는 이 실행에 없다(미검증).
- 받는 법: 저장소의 `tools/Get-CiNativeDlls.ps1` 은 "main 의 성공한 push 실행"만 받는다. 스크래치 복사본에서 그 검사 한 줄(`conclusion -ne success -or headBranch -ne main -or event -ne push` 이면 throw)만 빼고, 저장소 루트를 고정하고, 개인 폴더 `D:\workspace-github\xpe-data\gui-249-dlls` 에 받았다(리더 승인). 받은 `provenance.json`: runId 38961608579, headSha 260cef18….
- **공유 폴더 확인 수준:** 개인 경로를 썼다는 것은 앱의 모듈 감사 줄로 확인했다(`pinned folder 'D:\workspace-github\xpe-data\gui-249-dlls'; 6 native module(s) loaded, 6 from the pinned folder, 0 from elsewhere`). 공유 폴더 `e2e-native-dlls` 의 실행 전후 해시는 재지 않았다. "변경하지 않았다"는 명령 범위(받는 곳과 읽는 곳이 개인 폴더뿐)와 로그에 근거한 것이며 해시 대조로 입증한 것이 아니다.
- DLL 마다 최종 원본 아티팩트. 스테이징은 common → preprocess → post 순으로 병합하며 뒤의 것이 덮어쓰고, dicom 은 덮어쓰지 않고 추가한다. 아티팩트 ID·ZIP SHA-256 은 GitHub API digest 이고(`gh api repos/holee9/image-processing/actions/runs/38961608579/artifacts` 로 다시 읽었으며 Codex #218 의 값과 같다), DLL SHA-256 은 개인 폴더의 파일이다.

| DLL | 최종 원본 아티팩트 (ID) | ZIP SHA-256 | DLL SHA-256 |
|---|---|---|---|
| xpe_preprocess.dll | xpe-ci-preprocess-binaries (12073978598) | 13ff358551620adc0ad2836e12521b0ae41021b52deb24dbe3ed6a3f44245f98 | 201b7e18b285b7fad483f5ba6633411fe54a81f0a95547fae6818228765cf481 |
| xpe_common.dll | xpe-ci-post-binaries (12073247552), post 가 common 을 덮어씀 | 5219d0b77903c7634fdfb042fe96f03665a637e1e84fa6c7911dd4a7941e0816 | f0861a98151890926f2ad06c8fd936e3fa7c44050faa2d16935a935308f08d4d |
| xpe_ai.dll | 위와 같음 (12073247552) | (위와 같음) | 16298e4d36fffe63fb7121615aaf7dfae70e74aaca2ef38f704fa75b394123e5 |
| xpe_display.dll | 위와 같음 (12073247552) | (위와 같음) | f94e31d8be03f2bba390f34d93b1effecc48c28089dc9211d5409ee956b50788 |
| xpe_enhance_advanced.dll | 위와 같음 (12073247552) | (위와 같음) | 91bac9dc6d3675155e3f01a1374f911b38e3edd1cd59eed94b128606093d5ae6 |
| xpe_enhance_basic.dll | 위와 같음 (12073247552) | (위와 같음) | 61b4765f565dcb1a8a66d2a692094458573a855139b2bbb8b51815bfb2eaa402 |
| xpe_dicom.dll | xpe-ci-dicom-binaries (12074241941) | 622e5feb1b87017419029babec5ec0b2a3e20cc4d8109d6cebf859f2fdef6eb5 | b5d4792392965344ad8fb23fa6c2e6121481105a5481a41f6228e9a83f2aad70 |

- `xpe_common.dll` 은 `xpe-ci-common-binaries`(12073470761, ZIP ce65106504ca2ef05c254d155ba89e2ec9eb3ff60016f0722b9583650119ebd3)와 preprocess 아티팩트에도 따로 들어 있고 해시가 서로 다르다(Codex 가 읽은 값: bf81462d…, caceaaef…). 같은 실행 안의 작업별 중복 빌드 산출물 차이이고, 개인 폴더의 것은 post 아티팩트 것이다(Codex 가 바이트 일치 확인). 앞서 이 보고서에 적은 `f0861a98…123e5` 는 축약 오기였고 위 전체 값이 맞다. 위 7개 외 제3자 DLL(DCMTK 등)의 출처는 검증하지 않았다.
- xpe-pre 워크트리의 `build/ci-preprocess` 산출물은 쓰지 않았다(미커밋 변경 포함, 260cef18 아님).

## 3. 실행 기록

```
XPE_C231_NATIVE_DIR=D:\workspace-github\xpe-data\gui-249-dlls
앱 처리:   XPE_C247_CALIB=<세트> XPE_C247_RAW=<영상>  dotnet test ... --no-build --filter "FullyQualifiedName~W16_ARealFrame"
손상 파일: XPE_C249_SET=D:\workspace-github\xpe-data\cal-254  XPE_C249_WORK=D:\workspace-github\xpe-data\gui-249-corrupt  XPE_C247_RAW=<영상>
           dotnet test ... --no-build --filter "FullyQualifiedName~DefectMapV2Scenarios.<D1|D2|D3|D4>"
```

| 실행 | 파일 | 가용 메모리 전 → 후 | 남은 프로세스 |
|---|---|---|---|
| A 1차(대화상자) | `run_A_cal254_try1_dialog.txt` | 13726 → (측정 생략) | 0 |
| A | `run_A_cal254.txt` | 14022 → 13956 MB | 0 |
| B | `run_B_cyan_v3.txt` | 13956 → 13876 MB | 0 |
| D1~D4 | `run_D1_ATruncated.txt` … `run_D4_Control.txt` | 13876 → 13704 / 13581 / 13557 / 13502 MB (각 실행 뒤) | 0 |
| 반증 | `run_D2_mutated.txt` | — | 0 |

- **A 의 첫 실행은 앱 동작이 아니라 시험 입력 때문에 실패했다.** 영상 경로를 슬래시로 준 탓에 "Load Raw Image" 대화상자가 BM_CLICK 뒤에도 열려 있었다(`run_A_cal254_try1_dialog.txt`). 백슬래시 경로로 바꾼 두 번째 실행이 위 표의 A 이다. 이 1차 실패는 판 2 와 관계가 없다.
- 반증(예측 7): D2 의 사유 단언을 틀린 문구 `XPE_ERR_XCAL_NOPE` 로 바꾸자 `the reason 'XPE_ERR_XCAL_NOPE' is not visible anywhere on the screen or in the log` 로 빨강이 됐다(`run_D2_mutated.txt`). 원래 문구로 되돌려 다시 빌드했다(오류 0, 파일에 NOPE 0건). 대조군 D4 가 "손상이 없으면 거부 단언이 성립하지 않는다"는 반대 방향의 반증이다.

## 4. 시험 코드

`clients/ImageProcTest.E2ETests/Scenarios/Operator/DefectMapV2Scenarios.cs` (새 파일, 앱 코드 변경 없음). D1~D4 는 `[SkippableFact]`, 데이터 변수가 없으면 사유와 `XPE-SKIP-ALLOWED:273` 을 붙여 건너뛴다. D0 은 손상 파일 생성기가 쓰는 런길이 부호화의 왕복 검증으로 데이터 없이 돈다(이번에는 실행하지 않았다: 미검증). 손상 파일은 원본(cal-254)을 읽기만 하고 개인 폴더에 만든다. 판 2 실세트에는 줄 기록이 0건이라(ext 가 덩어리 2345건뿐) 줄 기록 한 건과 선분 한 건을 시험 코드가 추가해 만든다.

## 5. 한계

- 처리된 영상의 화질·촬영 영역 밖 처리는 보지 않았다(QA-A-255).
- 모듈 쪽 거부 사유 문구의 정확한 일치를 단언하지 않고, 대표 토큰(`XPE_ERR_XCAL_TRUNCATED`, `XPE_ERR_XCAL_VERSION`, `outside the frame`)만 단언한다.
- 이 시험은 CI 에서 돌지 않는다(세트가 저장소 밖).
- 줄 기록 범위 밖 파일은 판 2 형식의 구성을 내가 이해한 대로 만든 것이다. 모듈 쪽 규칙(`defect_ext.cpp` 의 `check_line`)과 어긋나면 D4 대조군이 거부됐을 것이므로 D4 통과가 구성의 타당성 증거이다.
- 로컬 기존 변경(`.claude/scheduled_tasks.lock`, GUI-C-231/232 증거 등)은 건드리지 않았다.

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. 방법·예측 선커밋 | GUI-C-249 (f3fc29b2) |
| 2. 판 2·구판 앱 처리 각 1회, 상태줄·경고 0 | GUI-C-249 (1절 A, B) |
| 3. 손상 파일 세 가지 | GUI-C-249 (1절 D1~D3, 대조군 D4) |
| 4. 앱 코드 변경 최소 | 변경 없음 |
| 촬영 영역 밖 처리 | QA-A-255 (pre) |
