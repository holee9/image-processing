# GUI-C-196 M3b — dotnet-tests 에 xpe_dicom.dll 을 놓는 DLL 짝 측정

증거: 이 폴더의 `step1_dll_inventory.txt`, `step2_copy_rules.txt`, `step3_assert_step_controls.txt`, `dotnet_tests_dicom.patch.txt`.

## 결론

**규칙: "이미 있는 이름은 덮어쓰지 않는다."** common·preprocess·post 세트(함께 빌드된 것)를 기준으로 두고, dicom 아티팩트의 DLL 중 그 세트에 **없는 이름만** 복사한다. 로컬 대조에서 이 규칙은 통과했고 반대 규칙(전부 덮어쓰기)은 0x8007007F 로 재현됐다.

## 1단계 — 같은 이름 DLL (`step1_dll_inventory.txt`)

비교한 세 묶음: ① `xpe-gui/build/ci-common/bin`(12개) ② `xpe-post/build/ci-dicom/bin`(8개) ③ `image-processing/build/release/vcpkg_installed/x64-windows/bin`(36개, ②가 가리키는 DCMTK/OpenJPEG).

| 이름 | 해시 | 내보내기 함수 수 | 판단 |
|---|---|---|---|
| fmt / spdlog / gtest / gmock / gmock_main / gtest_main | ①②③ 모두 다름 | ①과 ② **동일**(69 / 405 / 653 / 795 / 796 / 250), ③은 다름(64 / 397 / 660 / 802 / 803 / 252) | ①②는 같은 소스를 따로 빌드한 것(API 동일), ③은 다른 버전 |
| xpe_common | ①② 다름 | 둘 다 16개, 차이 0 | API 동일. 빌드 시각이 다름(로컬 두 빌드의 시점 차이) |

- ③에만 있는 이름: DCMTK 계열(dcmdata, dcmnet, ofstd, oflog …)과 openjp2 등 30개. ②에만 있는 것은 `xpe_dicom.dll`.
- 어느 쪽이 이겨야 하나: ①②의 같은 이름은 API 가 같아 어느 쪽이든 로드되지만, ③과 이름이 겹치는 것은 **버전이 달라** 덮어쓰면 짝이 깨진다. 그래서 먼저 놓인 ①(함께 빌드된 세트)이 이긴다.

## 2단계 — 규칙 대조 (`step2_copy_rules.txt`)

같은 입력으로 세 디렉터리를 만들고 각각 (가) 실제 모듈 DICOM 시험 2건 (나) Functional 전체를 돌렸다.

| 규칙 | DICOM 실제 시험 | Functional 전체 |
|---|---|---|
| A 덮어쓰지 않음 | 2 통과 (`write ok; validator: valid; … pixels identical`) | 475 통과, 1 건너뜀, 0 실패 |
| B 전부 덮어씀 | **2 건너뜀**, 사유 `0x8007007F`(프로시저를 찾을 수 없음) | 473 통과, **3 건너뜀**, 0 실패 |
| C dicom 쪽만 덮어씀(③은 안 덮음) | 2 통과 | 475 통과, 1 건너뜀, 0 실패 |

핵심 관찰: **B 에서도 Functional 전체는 0 실패다.** 시험이 건너뜀으로 빠지기 때문에 초록이 나온다. 그래서 3단계의 "건너뜀이면 실패" 단언이 이 측정의 실질적 결과물이다.

## 3단계 — 단계 초안 (`dotnet_tests_dicom.patch.txt`)

리더의 `main`(07db06bb)의 `ci.yml` 에 대한 diff. `git apply --check` 통과, 적용 결과를 의도한 파일과 대조(줄바꿈 제외 동일), YAML 파싱 통과. 내용:

1. `dotnet-tests` 의 `needs` 에 `dicom-build` 추가.
2. **`dicom-build` 의 아티팩트 업로드에 `build/ci-dicom/vcpkg_installed/x64-windows/bin/` 추가** — 지금은 `bin/` 만 올려서, 내려받아도 DCMTK/OpenJPEG DLL 이 없다. (로컬에서 `ci-dicom/bin` 8개 DLL 에 DCMTK 가 없는 것을 확인했고, 그 시험은 PATH 로 vcpkg bin 을 잡는다.)
3. 다운로드 → "없는 이름만 복사"(재귀 탐색이라 아티팩트 안 폴더 배치에 의존하지 않음) → 검증 목록에 `xpe_dicom.dll`, `dcmdata.dll`, `openjp2.dll` 추가.
4. 시험 실행 뒤 trx 를 읽어 `BaselineDicomNativeTests` ≥2건, `EnhanceBasicNativeTests` ≥4건이 **전부 Passed** 가 아니면 실패.

단언 단계 대조(`step3_assert_step_controls.txt`): 통과 trx → 종료 0, DICOM 건너뜀 trx → 종료 1, 빈 trx(0건) → 종료 1. 마지막 대조가 "0건이면 '실패 없음'으로 통과"하는 구멍을 막는다는 것을 보인다.

## 미검증·한계

1. 로컬 세트는 **대역**이다. ①은 9/26 빌드, ②는 10/2 빌드로 시점이 다르고, ③은 로컬 release vcpkg 트리(fmt/spdlog 를 포함)다. CI 의 `third_party/dicom/vcpkg.json` 은 dcmtk·openjpeg·gtest 만 요구하므로 CI 의 vcpkg bin 에는 fmt/spdlog 가 **없을** 수 있고, 그러면 B 의 실패 원인(fmt/spdlog 덮어쓰기)이 CI 에서 재현되지 않을 수 있다. CI 에서 겹치는 이름은 gtest/gmock 계열일 가능성이 있으나 **측정하지 않았다**. 규칙 A 는 어느 경우에도 안전하다는 점만 주장한다.
2. B 의 실패를 일으킨 DLL 이 fmt·spdlog·gtest 중 정확히 어느 것인지는 가르지 않았다(C 와 B 의 차이는 ③ 전체를 덮었는가이다).
3. 단언 단계는 Windows PowerShell 5.1 로 시험했다. CI 는 `pwsh`(7)이다. 쓴 구문은 두 쪽 공통이라고 보지만 pwsh 로 돌려 보지는 않았다. 빈 trx 대조에서 "trx holds 1 result(s)" 가 찍히는 것은 `@($null)` 의 길이이며 판정에는 영향이 없다.
4. `upload-artifact` 가 경로 두 개를 올릴 때의 아티팩트 안 배치(공통 조상이 루트)는 CI 에서 보지 않았다. 병합 단계가 재귀 탐색이라 영향이 없도록 썼다.
5. `needs: dicom-build` 는 dicom 시험이 실패하면 `dotnet-tests` 도 건너뛰게 만든다(gui 레인 CI 가 dicom 건강에 묶임). 분리하고 싶으면 `if: ${{ !cancelled() }}` 류가 필요하며 그 경우 아티팩트 부재를 다른 방식으로 처리해야 한다 — 리더 결정.
6. 검증 목록의 `dcmdata.dll`/`openjp2.dll` 이름은 로컬 vcpkg 트리에서 읽은 것이다. CI 트리에서 같은 이름인지는 실행으로 확인하지 못했다.
7. 이 패치는 적용하지 않았다(워크플로는 리더 소유).
