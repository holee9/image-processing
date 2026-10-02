# GUI-C-199 — main 빨강: GainPolyClampAlertTests 2건 "Alerts seen: 0"

증거: `reproduction.txt`(CI 로그 사실·재현·원인 문답), `falsification_arms.txt`, `after_fix.txt`(이 폴더).

## 원인 (확정 범위는 아래 한계 1 참조)

196 시리즈가 추가한 네이티브 시험 세 클래스(`BaselineDicomNativeTests`, `BaselineDisplayNativeTests`, `EnhanceBasicNativeTests`)가 **`xpe_common.dll` 을 자기 모듈이 있는 디렉터리의 전체 경로로 직접 로드**했다. 알림 큐는 `xpe_common.dll` 안에 있고, 윈도우는 같은 파일 이름이 다른 경로에 있으면 서로 다른 모듈로 본다. 먼저 로드된 사본 A 에 `xpe_preprocess` 가 이름으로 묶이고, 큐를 읽는 쪽이 사본 B 를 보면 알림이 0건이다(시험 파일의 주석이 이미 이 메커니즘을 적어 두었다).

- `BaselineDicomNativeTests` 는 `build/ci-dicom/bin` 을 **첫 후보**로 찾는다. CI 의 dicom 아티팩트는 자기 `xpe_common.dll` 을 들고 있다(CI 로그: 병합 단계가 "kept the common set's copy" 20개 중에 `xpe_common.dll` 포함). 나머지 둘은 `build/ci-common/bin` 을 찾는다. 다른 시험들이 쓰는 사본은 테스트 출력 디렉터리 것이다.
- 196 이전에는 경로로 사본을 먼저 올리는 시험이 없었다. 그래서 직전 세 커밋은 초록이었다.
- dicom 병합 단계(워크플로)는 원인이 아니다: 병합은 `xpe_common.dll` 을 정확히 덮어쓰지 않고 남겨 둔다. 그 사본이 `build/ci-dicom/` 에 **있다는 것**이 시험 쪽 후보 목록과 만나 문제가 됐다.

**원인인가 증상인가**(카드가 요구한 한 번의 물음): `reproduction.txt` — 게인 시험만 돌리면 3건 통과(대조). 같은 시험 앞에 `build/ci-dicom/bin/xpe_common.dll` 을 전체 경로로 먼저 올리는 임시 줄 하나만 넣으면 **CI 와 같은 2건 빨강, 같은 메시지**("Alerts seen: 0"). 임시 줄은 되돌렸고 해시로 확인했다.

## 고친 것 (gui 코드가 아니라 시험 쪽, 워크플로 변경 없음)

- `Fixtures/SharedCommonModule.cs`(새): `xpe_common.dll` 이 프로세스에 이미 있으면 **아무것도 로드하지 않고**, 없으면 모듈 탐색기(`NativeModuleLibraryLocator`, 테스트 출력 디렉터리 우선 — `XpePreprocessNative.TryFindDll()` 과 게인 시험이 쓰는 같은 순서)가 찾는 사본을 로드한다. 세 네이티브 시험 클래스는 `NativeLibrary.Load(Path.Combine(NativeDirectory!, "xpe_common.dll"))` 대신 이 로더를 부르고, 자기 모듈(`xpe_dicom`/`xpe_display`/`xpe_enhance_basic`)만 전체 경로로 로드한다(그 모듈의 `xpe_common` 의존성은 이름으로 이미 올라온 사본에 묶인다).
- `Functional/NativeCommonSingleInstanceTests.cs`(새, 2건): ① 세 클래스의 소스가 공유 로더를 쓰고 자기 경로 로드를 되살리지 않았는지. ② 세 클래스의 로드 메서드를 실제로 부른 뒤(게인 시험이 하는 대로 이름으로도 한 번 불러) **프로세스에 `xpe_common.dll` 이 한 경로뿐**이고, 어떤 `xpe_*` 모듈도 두 경로에서 올라오지 않았는지.

## 시험 결과

- 전체 IntegrationTests 어셈블리, 기본 환경(CI 와 같은 배치: `build/ci-dicom/bin` 에 자기 `xpe_common.dll` 이 있음): **637 통과 / 0 실패 / 1 건너뜀**. `XPE_NATIVE_DIR`(xpe_dicom 이 있는 조립 폴더) 지정: **637 / 0 / 1**.
- 반증(`falsification_arms.txt`): 세 클래스 각각이 자기 경로 로드로 되돌아가면 새 시험 2건이 빨강, 로더가 모듈 탐색기 대신 fallback 디렉터리 사본을 로드하면 새 시험 + `GainPolyClampAlertTests` 2건이 빨강. 전부 바이트 동일 복원. (수정 전 시험 순서에서 게인 시험 2건이 같은 메시지로 빨개지는 것은 `reproduction.txt` 의 2·3 항.)

## 미검증·한계

1. **CI 에서 세 클래스 중 어느 것이 먼저 올렸는지는 읽지 못했다.** CI 로그는 실패한 시험만 보여 주고, 통과 시험의 순서는 없다. 원인은 로컬 재현(같은 서명, 한 줄 차이로 뒤집힘)으로 확정했을 뿐, CI 실행 순서로 확인한 것은 아니다. 고친 뒤 CI 가 초록인지는 푸시 후에만 안다.
2. 카드가 요구한 "dicom 병합 단계를 뺀 구성" 대조는 하지 않았다. 대신 로컬에서 `build/ci-dicom/bin` 을 만들어(조립 폴더 복사 — 자기 `xpe_common.dll`, `xpe_dicom.dll`, vcpkg 런타임 포함) CI 의 dicom 아티팩트 배치를 흉내 냈다. 그 `xpe_common.dll` 은 CI 아티팩트의 실제 파일이 아니다(경로가 둘이라는 구조만 같다). 이 `build/ci-dicom` 은 `.gitignore` 대상이며 이 워크트리에 남아 있다.
3. 고치기 전 단계의 반증 출력 중 "게인 시험 2건 빨강"(항 3)은 로더를 지금 버전으로 바꾸기 전에 돌린 것이라 그 출력 파일은 남지 않았다(지금의 `falsification_arms.txt` 는 새 로더 기준). 같은 서명의 증거는 `reproduction.txt` 2항이 직접 남긴다.
4. 시험 클래스 순서는 이 저장소가 통제하지 않는다. 이번 고침은 순서와 무관하게(이미 올라와 있으면 올리지 않음) 두 번째 사본을 만들지 않게 한 것이지, 다른 시험이 경로로 새 사본을 올리는 것까지 막지는 않는다. 새 시험 ② 는 그때 어느 `xpe_*` 모듈이 두 경로인지 이름을 말하며 빨개진다.
5. `gui-e2e-native` 는 이 실패 때문에 취소돼 있었다. 그 잡의 결과는 이 카드에서 보지 못했다.
