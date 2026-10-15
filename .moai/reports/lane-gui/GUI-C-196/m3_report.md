# GUI-C-196 M3 — DICOM 쓰기 · 검증 · 재읽기

## 만든 것

| 파일 | 역할 |
|---|---|
| `gui/ImageProcTest/Services/BaselineDicomExport.cs` | 판정 논리. 네이티브 없음. 쓰기 → 검증 → 재읽기 → 비교의 결론 한 개(`Passed`)와 단계별 필드 |
| `gui/ImageProcTest/Services/Native/XpeDicomInterop.cs` | `xpe_dicom_write` / `validate` / `open` / `read_image` / `get_metadata` / `close` 선언 |
| `gui/ImageProcTest/Services/Native/GuiDicomNative.cs` | 실제 모듈을 부르는 `IDicomSession` 구현 (고정 8192바이트 보고 버퍼, BUFFER_TOO_SMALL 이면 알려 준 크기로 한 번 더) |
| `GuiNativeLibraryResolver.cs` | `xpe_dicom.dll` 후보 경로 추가 |

## 판정 규칙

`Passed` 는 아래가 **전부** 참일 때만 참이다. 하나라도 거짓이면 어느 것이 거짓인지가 필드와 요약에 남는다.

1. `xpe_dicom_write` 가 0 을 돌려줌 (아니면 검증·재읽기는 시도조차 하지 않는다)
2. `xpe_dicom_validate` 가 0 을 돌려주고 JSON 보고서를 만듦
3. 보고서 최상위 `"valid"` 가 JSON 값 `true` (문자열 `"true"`, 보고서 없음, JSON 아님, 배열은 모두 거짓)
4. 파일을 다시 열어 읽은 크기가 기록한 크기와 같음
5. 읽은 픽셀이 기록한 픽셀과 **비트 단위로 같음** (허용오차 없음)
6. 몸통 부위·kVp·픽셀 피치가 읽혀 돌아온 값과 같음 (kVp·피치만 상대 1e-4 허용 — DICOM 십진 문자열 저장 때문. 근거는 코드 주석)

기록하는 메타데이터는 gui 가 아는 세 값(몸통 부위, kVp, 픽셀 피치)뿐이다. mAs·SID·촬영시각은 0(=미상)으로 쓴다. 타입에 그 필드가 없어서 실수로 값을 지어낼 수 없고, 소스 대조 시험이 `0f, 0f` 를 못박는다.

## 실제 모듈에서 나온 결과 (로컬)

`xpe_dicom.dll` + `xpe_common.dll` 을 `xpe-post` 레인의 빌드(`build/ci-dicom/bin`, RelWithDebInfo, 2026-10-02 14:42)에서 **복사**해(그 트리는 건드리지 않았다) `XPE_NATIVE_DIR` 로 지정하고, DCMTK/OpenJPEG 런타임은 그 빌드가 가리키는 `image-processing/build/release/vcpkg_installed/x64-windows/bin` 에서 가져왔다. 128×96, 16비트 전 범위(32767 초과 값 포함):

```
SUMMARY: write ok; validator: valid; read back 128x96: pixels identical; metadata agrees (body part, kVp, pixel pitch)
REPORT:  {"errors":[],"valid":true,"warnings":[]}
file bytes: 25584
```

같은 픽셀을 두 번 쓴 두 파일은 길이가 같고(9200바이트, 64×64) 5바이트가 다르다. **어디가 다른지는 찾지 않았다.** 이 값은 측정이며 게이트가 아니다.

## 검증

- 신규 27건: 가짜 세션 25건 + 실제 모듈 2건. Functional 전체: DLL 이 없는 기본 환경 473 통과·3 건너뜀(그중 2건이 이 단계의 실제 모듈 시험), DICOM DLL 을 지정한 환경 475 통과·1 건너뜀(나머지 1건은 M1 때부터 있던 것). 실패 0.
- 반증 12건 전부 빨강, 바이트 동일 복원, 복원 후 27/27 — `m3_falsification_arms.txt`. 실제 DLL 을 지정한 채 돌렸으므로 8·9·12번은 실제 모듈 시험이 빨강이 된 것이다.
- 12번(BitsStored 를 12 로 선언)이 왜 빨강인지 — 쓰기가 거절되는지, 재읽기가 거절되는지, 픽셀이 달라지는지 — 는 **보지 않았다**. 빨강이라는 것만 안다.

## 중요: CI 에서 이 시험은 돌지 않는다

`dotnet-tests` 잡(ci.yml 520–572행)은 `xpe-ci-common` · `xpe-ci-preprocess` · `xpe-ci-post-binaries` 만 `build/ci-common/bin` 에 받는다. `ci-post` 프리셋은 `BUILD_DICOM=OFF` 라 `xpe_dicom.dll` 이 들어 있지 않다(`CMakePresets.json:75` 읽음). 따라서 **이 푸시의 CI 에서 DICOM 네이티브 시험 2건은 건너뜀으로 끝난다.** 초록이어도 "CI 가 `valid:true` 를 확인했다"가 아니다.

`valid:true` 를 CI 에서 보려면 어느 잡이 `xpe_dicom.dll` 과 DCMTK 런타임을 `build/ci-common/bin` 에 놓아야 한다. 방법은 리더가 정할 일이다(워크플로는 리더 소유):

- (가) vcpkg 를 쓰는 `ci-dicom` 빌드를 하는 잡을 두고 그 산출물을 `dotnet-tests` 가 받음 — 현재 vcpkg 는 `coverage-dicom` 매트릭스에서만 쓰이고 바이너리 업로드는 하지 않는 것으로 읽었다(확인은 안 함).
- (나) 이 푸시 전에 리더가 자기 기계에서 위 로컬 결과를 재현 — 이 경우 필요한 것은 위에 적은 DLL 세트와 `XPE_NATIVE_DIR` 하나다.

## 미검증·한계

1. 위 로컬 결과는 `xpe-post` 의 `xpe_dicom.dll`(10/2 14:42 빌드)로 얻었다. 그 빌드가 `main` 의 어느 커밋인지, 리더가 푸시할 `main` 과 같은지는 확인하지 않았다.
2. 로컬 DLL 짝에서 처음 두 시도는 실패했다: 릴리스 vcpkg 런타임으로 **fmt.dll·spdlog.dll 까지 덮어쓴** 시도는 `0x8007007F`(프로시저 없음), 디버그 런타임 시도는 테스트 호스트가 죽었다(원인 미조사 — CRT 불일치로 추정할 뿐). 세 번째에서 `cp -n`(덮어쓰지 않음)으로 짝을 맞춰 통과했다.
3. 경로는 ANSI 로 넘긴다. ASCII 임시 경로에서만 시험했다. 한글이 든 경로(`evidence/<RunId>/` 아래)에서 DCMTK 가 파일을 여는지는 **미검증**이다.
4. 쓰기 전용 경로만 다뤘다. `xpe_dicom_write_j2k`, 네트워크 전송, MWL 은 이 단계의 범위가 아니다.
5. 재읽기가 MONOCHROME1/2 구분을 하지 않으므로(헤더: "returned as stored") 반전 영상 취급은 이 판정에 들어가지 않는다 — QA-B-185 진행 중이라고 알고 있다.
6. 시험 영상은 합성(`Random(777)` + 나선 값)이다. 실제 전처리·강조 출력의 DICOM 쓰기는 M4 의 전체 경로에서 보게 된다.

## M3 밖으로 남긴 것

명령·VM 연결·자동화 보고 필드·XAML 활성화(M4), E2E(M5), EI-0.
