# GUI-C-195 — 남은 비활성 메뉴 4건의 선행조건 재분류

보고서만이다. 코드는 바꾸지 않았다. 앱을 실제로 실행한 것은 §1 의 개수 확인뿐이다(Mock, 숨김 창, 자동화 보고서).

## 0. 결론 표

| 행 | 항목 | 지금 | 무엇이 | 비고 |
|---|---|---|---|---|
| 2 | `OpenDicomMenuItem` | **사용자 후순위** | (사실만 §3) | gui 의 운영 앱에는 DICOM 호출이 없다 |
| 9 | `RunDeterministicBaselineMenuItem` | **선행 필요** | ① 명령이 무엇인지 문서 셋이 서로 다르게 말한다(정의 결정). ② 정의가 정해져도 gui 에 `enhance_basic` 연동과 DICOM 쓰기 호출이 없다 | 모듈 쪽 재료는 준비됨. ①을 빼면 나머지는 전부 gui 레인 안의 일 |
| 18 | `QaConstancyMenuItem` | **사람 필요** | 기준(평탄) 영상, 항상성 허용 기준, SNR·균일도·결함 수를 재는 코드(어느 모듈도 수출하지 않음) | 152 의 "판정 기준 없음"은 일부 틀렸다(§5 정정) |
| 19 | `GsdfCalibrateMenuItem` | **사람 필요** | 실측 휘도 곡선(광도계)과 검증된 절차 | 문서가 정한 "휘도 입력 대화상자 → `xpe_gsdf_calibrate`"는 코드만으로 만들 수 있으나 §4 의 이유로 이 보고서는 "가능"으로 올리지 않는다 |

"지금 가능"인 행은 없다. 따라서 구현 범위 단락은 쓰지 않았다(행 9 에는 정의가 정해진 뒤의 최소 범위만 §2 끝에 조건부로 적었다).

## 1. 개수 확인 (대조군 포함)

- 4건이 맞다. XAML 에서 `IsEnabled="False"` 는 `MenuItem` 4개(115·362·441·446행): 이름은 `OpenDicomMenuItem`, `RunDeterministicBaselineMenuItem`, `QaConstancyMenuItem`, `GsdfCalibrateMenuItem`, 넷 다 `Command` 가 없다. 이름으로 센 값도 4, 정규식이 `MenuItem` 64개를 찾아냈고 `grep -c "<MenuItem"` 도 64 로 같다(`count_menu_items.txt`).
- 앱을 실제로 띄워 자동화 보고서를 읽으면 `DisabledFutureCommandCount = 4`, `UnimplementedMenuLeafCount = 4`(`MainWindow.xaml.cs` 의 이름 목록 쪽 수와 "명령 없는 비활성 잎" 쪽 수가 서로 독립으로 4 에서 만난다). 
- 152 의 21행을 이름으로 현재 상태에 대조하면 17행이 활성이고 4행만 비활성이다. 행 번호별 상태는 `c152_rows_now.txt`.

## 2. 행 9 — Deterministic Baseline

**무엇을 하는 명령인가: 문서 셋이 다르게 말한다.**

| 출처 | 말하는 것 |
|---|---|
| 메뉴 툴팁(`MainWindow.xaml:363`) | "Requires preprocess, enhance_basic, display, and dicom modules." |
| `XPE-GUI-MENU-001` §8 표(231행) | 소유 DLL `xpe_preprocess.dll`, 단계 1a, 활성 조건 "Same + **deterministic test mode active**" |
| `pipeline-spec.md` §4·§5.2 | 1b 단계는 `enhance_basic`·`display`·`dicom` 을 요구하는 "deterministic release baseline". 프레임 순서에 EI-0, 로그, 잡음 제거, 대비, 윤곽, 모달리티·VOI·표현 LUT, DICOM 쓰기가 들어 있다 |
| `product.md:183` | "Phase 1 total latency: deterministic baseline completes within 3000 ms" |
| `Preprocessing-E2E-Automated-Evaluation-Protocol.md` §5.7 | `DeterminismRMSE = RMSE(Y_run_1, Y_run_2)`, CPU 결정론 경로면 0 |

"deterministic test mode"는 저장소 전체에서 이 한 줄(`MENU-001:231`) 밖에 정의가 없다(`docs`·`.moai/specs`·`modules/*/include`·`gui`·`clients` 검색). 같은 이름이 "스펙의 1b 기준 경로 전체를 돌리는 명령", "전처리를 두 번 돌려 결과가 같은지 보는 시험"(`DeterminismRMSE`), "시험 모드가 켜졌을 때의 전처리" 셋으로 읽힌다. **무엇이 되어야 하는지는 이 보고서가 정하지 않는다.**

**지금 재료로 만들 수 있는가: 정의에 따라 다르고, 어느 쪽이든 gui 쪽 배선이 빠져 있다.**

| 필요한 것 | 모듈(수출) | gui 운영 앱(`gui/ImageProcTest`) |
|---|---|---|
| 전처리 | 있음 | 있음(체인 `preprocess`) |
| 표시 LUT 3종 | 있음 | 있음(`RealXpeBackend`) |
| `enhance_basic` 5함수(`xpe_log_transform`, `xpe_noise_reduce`, `xpe_contrast_enhance`, `xpe_edge_enhance`, `xpe_calc_exposure_index`) | 있음(헤더 확인, `exports_check.txt`와 같은 방법). 181f 로 비유한 입력은 거부됨 | **없음.** 체인 단계는 `preprocess`·`gsvg`·`ai_bone_suppress` 뿐(`StageIds`). 호출은 미참조 파일 `PipelineOrchestrator.cs` 에만 선언돼 있다. 실제 래퍼 `XpeEnhanceBasicWrapper` 는 별개의 옛 클라이언트 `clients/ImageProcTest` 에 있고 gui 의 csproj 가 연결하는 `clients` 파일 6개(로케이터·정책·`AlertDisplayFormatter`)에는 없다 |
| DICOM 쓰기(+검증) | 있음(`xpe_dicom_write`, `xpe_dicom_validate`) | **없음.** 호출은 옛 클라이언트 `NativePresentationExportService` 에만 있다 |
| 3000 ms 목표 대비 측정 | — | 체인 상태 문자열에 단계별 시간이 있으나 1b 경로 전체의 합은 없다(1b 경로가 없으므로) |

152 의 "선행 필요 `enhance_basic`·`dicom` 연동"은 지금도 맞다. 달라진 것은 선행이 모듈이 아니라 gui 쪽 연동만 남았다는 점이다(DICOM 읽기 엄격화와 `enhance_basic` 비유한 거부가 모듈에서 들어갔다). 읽기는 이 베이스라인 경로에 들지 않는다(스펙의 프레임 순서는 raw 에서 시작해 DICOM **쓰기**로 끝난다).

**정의가 "스펙의 1b 기준 경로 전체"로 정해질 경우에 한해**, 최소 범위는 (가) 위 5함수를 체인 단계(들)로 gui 에 연결, (나) 마지막에 DICOM 쓰기와 검증 호출, (다) 총 시간을 3000 ms 목표와 같이 보이게 하는 것이다. 다른 두 정의면 범위가 다르다("두 번 돌려 RMSE 0"은 전처리만으로 끝난다). 리더가 정의를 정하면 그 위에서 카드로 낸다.

## 3. 행 2 — Open DICOM (사실만)

사용자 후순위 지시에 따라 계획은 쓰지 않는다.

- **QA-B-184 의 관찰은 맞다. 단, 범위가 좁다.** `xpe_dicom_read` 를 요구하는 곳은 `gui/ImageProcTest/Services/PipelineOrchestrator.cs:31`(`xpe_dicom.dll` 의 필수 수출로 `xpe_dicom_read`, `xpe_dicom_write`)뿐이다. 모듈 헤더에는 그 이름이 없다(헤더 검색 0건, 대조군으로 `xpe_dicom_open` 은 1건). 이 파일의 클래스는 자기 파일 밖에서 만들어지지 않는다(`gui`·`clients` 에서 `PipelineOrchestrator` 검색: 자기 파일과 그 파일명을 건너뛰는 시험 한 줄). gui 어셈블리에는 컴파일되어 들어가지만 호출되지 않는다.
- **gui 운영 앱의 DICOM 호출은 없다.** `ImageProcTest.csproj` 가 연결하는 `clients` 파일에 DICOM 래퍼가 없고 `gui/ImageProcTest` 의 `xpe_dicom_*` 문자열은 위 한 줄뿐이다.
- 실제 수출(헤더 `dicom_api.h`, 이름·상태는 `exports_check.txt`)과 그 파일이 가정한 것의 차이:

| | `PipelineOrchestrator` 가 가정 | 실제 수출 |
|---|---|---|
| 읽기 | `xpe_dicom_read` 한 함수 | **없음.** `xpe_dicom_open(path, &handle)` → `xpe_dicom_read_image(handle, &img)` → `xpe_dicom_get_metadata(handle, &meta)` → `xpe_dicom_close(handle)` (핸들 방식, 닫기 필수) |
| 영상 소유권 | 알 수 없음 | `read_image` 가 `xpe_alloc_image` 로 모듈이 할당하고 호출자가 `xpe_free_image` 로 해제 |
| 메타데이터 | 읽기와 한 호출 | 별도 호출 |
| 쓰기 | `xpe_dicom_write` | 있음. `xpe_dicom_write(path, image, metadata)` 외에 `write_j2k`, `validate`, `cstore`, `cfind_mwl`, `cancel` 도 수출 |
| 같은 파일의 다른 DLL 요구 | `xpe_offset_correct`, `xpe_gain_correct`, 표시 LUT 3종, `enhance_basic` 5함수 | 전부 헤더에 있다(헤더 검색 각 1) |

- 옛 클라이언트 `clients/ImageProcTest` 의 `XpeDicomWrapper` 와 준비 상태 점검(`XpeDicomReadinessProbe`)은 **실제 이름** 10개(`open`, `read_image`, `get_metadata`, `close`, `write`, `write_j2k`, `validate`, `cstore`, `cfind_mwl`, `cancel`)를 쓴다. 그러므로 "GUI 가 존재하지 않는 이름을 요구한다"는 운영 앱의 죽은 파일에 한정된 사실이다.
- 읽기 쪽 현재 상태: 필수 속성 부재·비트 기술 위반·J2K/JPEG LL 코드스트림 불일치는 읽기에서 거부된다(`DicomReader.cpp`, QA-B-182 계열). MONOCHROME1 은 **저장된 그대로** 돌려준다(`DicomReader.cpp:369` 주석 "how to invert it is #235", QA-B-185 진행 중).

## 4. 행 18·19 — 사람 필요인지 확인

**행 18 QA Constancy (SWU-6.1): 여전히 사람 필요.**

| 필요한 것 | 지금 | 확인 방법 |
|---|---|---|
| 기준(평탄·균일) 영상 | **없음.** 추적 파일 중 `flat`·`uniform`·`constancy` 이름의 영상류 0건(걸린 5건은 `.moai/reports` 의 시험 출력 텍스트). 픽스처는 `synthetic_1024x1024.raw`, `wrist_lat_3072x3072.raw` 뿐. `xpe_calib_fixture_gen`(전처리 모듈 도구)은 합성 프레임에서 offset·gain·defect **보정 파일 세트**를 만들 뿐(소스 머리 주석), 실장비 기준 영상이 아니다 | `git ls-files` 이름 검색 |
| 판정 기준 | **일부만.** 균일도 σ/μ < 1 %(80 % FOV): `docs/calibration/xray-detector-calibration-prd.md:67·1924·2583`. 결함 SNR < 5 dB: `SRS-CALIB-FUNC-010`. 둘 다 **보정 품질** 기준이고, 시간에 따른 **항상성**(기준선 대비 허용 변화량)을 정한 문서는 없다 | `docs` 정규식 검색 |
| 재는 코드 | **없음.** SNR·균일도·결함 수를 내는 수출 함수가 어느 모듈에도 없다(`xpe_calc_exposure_index` 만 있음). 스텁 4개(`CalculateSnr`·`CalculateUniformity`·`CountDefects`·`LoadCalibrationImage`)는 죽은 파일에만 있다 | 헤더 검색 |
| 계획 근거 | 있음(`product.md:159` `SUP-05`→`SWU-6.1`) | — |

**행 19 GSDF Calibrate: 여전히 사람 필요, 다만 "입력 대화상자" 부분은 코드만으로 가능하다.**

- 문서가 정한 것: "`GSDF Calibrate...` → 휘도 입력 대화상자 → `xpe_gsdf_calibrate` 호출"(`XPE-GUI-DISP-INT-001` 349-350행). 툴팁은 "validated DICOM PS3.14 luminance calibration workflow"가 있을 때 활성이라고 한다.
- 지금의 gui 는 `GsdfEnabled` 일 때 휘도 **두 점**(0.05, 400 cd/m²)을 가정으로 넘긴다(`RealXpeBackend.cs:177`). 실측한 패널은 없다(`GsdfCalibrationInputTests` 의 설명도 같다).
- 사람이 필요한 것: ① 광도계로 잰 곡선(균등 간격 구동 레벨의 휘도), ② 검증된 절차(툴팁이 말하는 "validated").
- **대화상자를 지금 만들어도 "가능"으로 올리지 않는 이유**: 모듈은 "균등 간격에서 측정했다"는 절반을 검사하지 않고(`display_api.h` 주석: log 간격의 오름차순 사다리를 받아들여 틀린 LUT 를 만든다), 퇴화 입력은 조용히 고쳐 쓴다. 손으로 친 숫자가 "교정"이라는 이름으로 들어가면 검증되지 않은 값이 임상 표시 경로에 닿는다. 이 위험을 받아들일지는 리더 판단이다.

## 5. 152 의 정정

- 152 §4 의 "판정 기준(SNR·균일도·결함 수 임계)이 없다 — `docs/`·`.moai/specs/` 에서 `SNR ≥`·`uniformity ≤` 류 검색"은 **검색 패턴이 좁아서 틀렸다.** 균일도 1 %·결함 SNR 5 dB 기준은 있다(§4). 결론(사람 필요)은 바뀌지 않는다: 항상성 기준과 기준 영상과 재는 코드가 없다.
- 152 가 적은 `RealXpeBackend.cs:181` 의 위치는 현재 160-180행 근방이다(줄이 이동).

## 6. 미검증 (Gaps)

- 앱 실행은 개수 확인(Mock)뿐이다. 기능 확인이 아니라 선언·참조 검색에 근거한다.
- 수출 확인은 **헤더**(`XPE_API` 선언) 기준이다. 빌드된 DLL 의 실제 수출 표(`dumpbin`)는 보지 않았다.
- "정의 셋" 비교는 문서 읽기이고, 어느 문서가 현재 유효한지는 판단하지 않았다(`MENU-001` 은 v1.1 부록, `pipeline-spec` 은 정본으로 보이나 확인하지 않았다).
- 행 9 의 3000 ms 목표는 `product.md` 의 한 줄이고, 어떤 영상 크기·하드웨어 기준인지는 보지 않았다.
- 행 18 의 항상성 기준은 `docs` 전체를 정규식으로 검색한 결과이며, 이미지 파일 속 사양서나 외부 문서는 보지 않았다.
- `#130` AI 실구현의 진척은 이 4행에 영향이 없다고 판단했다(스펙상 AI 는 3단계이고 "AI 를 꺼도 결정론 출력을 보존"). 행 10(Full Pipeline)은 이미 활성이다. 이 판단은 문서에 근거하며 `#130` 의 현재 상태를 열어 확인하지 않았다.

## 7. 잔여 위험

- 행 9 의 정의가 정해지기 전에 구현에 들어가면 셋 중 하나를 골라 나머지 둘의 문서를 거짓으로 만든다.
