# GUI-C-202 — C09 를 서명과 무관한 워커 실패로 옮기기 (195 병합 전 조건)

증거(이 폴더): `measure_main_source_noworker.txt`, `measure_main.txt`, `measure_p195.txt`(측정 원문), `falsification_arms.txt`, `after_fix.txt`.
(`measure_main.txt` 의 C09b 는 **첫 버전**(-4 를 모든 모듈에 단정)이 main 소스 모듈의 -3 에 빨개진 실행이다 — 그래서 C09b 를 -3/-4 모두를 받는 형태로 고쳤다. `measure_p195.txt` 도 같은 첫 버전이며 195 에서는 통과.)

## 1. 측정 — "센다"

앱의 네이티브 디렉터리를 **`xpe_ai_worker.exe` 를 뺀 사본**으로 고정하고(L02 의 사설 복사와 같은 수법, 제품 DLL 그대로, 시험 훅·키 없음) 뼈 억제를 반복했다. 모듈 두 개를 이 기계에서 직접 빌드해 비교했다.

| 모듈 | 워커 뺀 사본에서 | "not a model" 파일(워커 있음) |
|---|---|---|
| **현재 main 소스**(스텁 AI 빌드, 서명 검사 없음) | 호출 1·2·3번째 모두 **code -9**, 상태 `worker=Active; failures=1/2` → **3번째에서 `worker=Disabled; failures=3; ceiling=3`**, 꺼짐 마크 표시("AI worker switched off for this session after 3 of 3 failures in a row …"). 재시작하면 `worker=Active; failures=0` | **code -3**, 실패로 센다(이전 C09 가 쓰던 방식) |
| **QA-B-195 M4**(xpe-post 77c7f868 을 `git archive` 로 임시 폴더에 풀어 빌드; post 작업 트리는 건드리지 않음) | 위와 **같은 결과**(-9 ×3 → 3번째에 Disabled, 마크, 재시작 정상) | **code -4**(모델 사용 불가), 4번 반복해도 `worker=Active; failures=0`, **마크 안 뜸** — 서명 거부는 세지 않는다 |

즉 **워커 부재는 서명과 무관하게 세어지고 꺼짐 마크로 이어진다**(post 의 보고와 일치, 앱에서 직접 확인). 그리고 이 카드가 풀어야 했던 문제도 실측으로 확인됐다: 195 모듈에서는 옛 C09 방식("not a model" 파일)이 마크를 만들지 못한다. 결과가 "센다"이므로 카드 2번을 수행했다.

## 2. 한 것

- **C09 이전**: 시나리오 본문을 `ProcessingChainScenarios.RunWorkerSwitchedOffScenario(Window, ITestOutputHelper)` 로 옮기고(내용 불변, 문서 주석만 갱신), 새 `AiWorkerAbsentScenarios.C09_AWorkerThatCannotBeStarted_…` 가 그것을 **워커 뺀 사본**에서 부른다. 새 고정 장치 `AiWorkerAbsentApplicationFixture`: `XPE_NATIVE_DIR` 의 파일을 임시 디렉터리로 복사하되 `xpe_ai_worker.exe` 만 뺀다(없으면 사유와 함께 건너뜀 — 같은 이유로 건너뛰는 L02 와 같은 형태). 모델 파일은 여전히 만들지만(앱의 모델 파일 사전 확인용) 내용은 무관하다.
- **"not a model" 시험은 의미를 바꿔 남김(`C09b`)**: 공유 앱에서 가짜 모델 파일로 4번 실행해, 호출이 -3 또는 -4 로 실패로 보고되고, -4 이면 GUI-C-201 의 문구("cannot be used by the module", "consecutive failures switch" 없음)이며, **4번 모두 -4 이면 마크가 뜨지 않아야 한다**고 단언한다. -3(서명 검사 없는 모듈)에서는 센다고 보고 마크 여부를 단언하지 않는다(출력에만 남김). 두 모듈 모두 통과.
  - 카드의 "실패로 세지 않음을 단언"을 -4 갈래에 걸었다. 195 가 아직 main 에 없어 CI 의 모듈은 -3(센다)이므로, 코드를 가리지 않고 "-4 가 안 센다"를 단정하면 지금 CI 가 빨개진다. -4 일 때만 단정하면 두 세계 모두에서 참이다.
- **`AiBoneSuppressionStageTests`**: 서명 의존이 없다 — 모듈을 부르지 않는 단위 시험이고, 가짜 파일은 `CheckModelFile` 의 **파일 존재** 확인에만 쓰인다(변경 불필요). C09 의 소스를 읽어 고정하던 시험 2건(`AiDiagnosticsTests`, `AiWorkerPanelLayoutTests`)은 옮겨진 메서드를 읽도록 고쳤고, 새 클래스가 건너뛰는 조건과 부르는 본문을 고정한다.
- C10(GUI-C-201)의 가짜 파일은 init 진단만 읽으므로 195 에서도 무관(두 모듈 모두 통과).

## 3. CI(`gui-e2e-native`)에서 되는가 — 잡 정의 읽기

- 잡이 `XPE_NATIVE_DIR=build/e2e-native-dlls`, `XPE_NATIVE_DIR_EXCLUSIVE=1` 을 주고, 단계 "Verify staged DLLs" 가 `xpe_ai.dll` 과 **`xpe_ai_worker.exe` 의 존재를 요구**한다. 즉 사본을 만들 원본에 워커가 들어 있다. 같은 수법인 L02(GUI-C-198)가 이미 이 잡에서 건너뛰지 않고 통과해 왔다(리더 보고: 잡 초록 + unexplained-skip 게이트 통과). 새 시험은 이 잡에서 건너뛰지 않는다(준비 조건 충족). Mock 잡에서는 건너뛰지만 그 잡에는 unexplained-skip 게이트가 없다(`ci.yml` 의 주석).
- **워크플로 변경 불필요 → 패치 초안 없음.**
- 한계: 사본 디렉터리(약 수십 MB)를 CI 러너의 임시 위치에 쓰는 데 권한·경로 문제가 없는지는 L02 의 선례로만 판단했고 이 시험의 CI 실행으로 확인하지 못했다.

## 4. 시험 결과 (`after_fix.txt`)

- 전체 IntegrationTests **640 통과 / 0 실패 / 1 건너뜀**(C09 소스 고정 2건이 처음 실행에서 빨강 — 옮긴 메서드 이름을 못 찾음 — 이어서 고침).
- E2E Mock(자동화 보고서·체인·AI 메뉴·AI 워커) **28 통과 / 0 / 12 건너뜀**.
- E2E Native — 현재 main 소스 모듈: C08·**C09(워커 뺀 사본)**·C09b·C10·AI 메뉴 **8/8 통과**. QA-B-195 M4 모듈: 같은 세트 **8/8 통과**. 쉘 E2E 통과.
- 반증(`falsification_arms.txt`): ① 사본에 워커를 도로 넣음(= 옛 방식과 같은 조건) → **195 모듈에서 C09 빨강**(마크가 안 뜸), main 소스 모듈에서는 초록(가짜 모델이 -3 으로 세어져서 — 옛 방식이 지금까지 통과한 이유) ② -4 를 일반 실패로 되돌림 → 195 모듈에서 C09b 빨강. 전부 바이트 동일 복원.

## 미검증·한계

1. 두 모듈은 이 기계에서 **스텁 AI 빌드**(ONNX 없음)로 빌드했다. 서명 거부는 빌드와 무관하게 먼저 돈다는 post 의 설명과 실측이 일치했지만, ONNX 빌드·운영 서명 키(#243)가 있는 빌드에서는 관측하지 못했다.
2. C09b 의 "모두 -4 이면 마크 없음" 단언은 -4 가 서명 거부일 때 참이다. 서명 검사가 없는 **ONNX** 빌드에서는 -4(읽을 수 없는 모델)를 세므로 이 단언이 거짓이 된다. gui 의 E2E 는 스텁 빌드(`ci-post`)를 쓰므로 해당 없음이지만, ONNX 모듈로 이 E2E 를 돌리게 되면 고쳐야 한다.
3. CI 에서 사본 디렉터리 방식의 실행은 확인하지 못했다(3 절).
4. 195 의 정확한 병합 상태(post 작업 트리는 이후 더 바뀔 수 있음)는 77c7f868 의 스냅샷으로만 확인했다.
