# GUI-C-89 보고 — Initialize Backend 성공 = 새 실행 묶음 (#178)

커밋(dev/gui, 미푸시): 50cfe06. 멈춤 보고(1차)는 판정 ①로 해소됐다.

## 1. 변경

- **`InitializeBackend()`**: `_backend.Initialize(Settings)` 와 텔레메트리 수거를 마친 직후 `RunSet = new RunSetState();` 를 실행한다. 새 `RunSetState` 가 새 `RunId` 와 `StartedAt` 을 갖는다.
- **Initialize 가 실패하면**: 백엔드 생성·초기화 중 예외가 나면 이 줄 **앞에서** catch 로 빠진다. 기존 실행 묶음이 그대로 남고 상태줄에 `Backend initialization failed: …` 가 표시된다.
  - 이는 판독이다. 실패를 일으키는 수단(결함 주입은 파이프라인 전용)이 없어 실행하지 않았다.
- **시작 시점**: 생성자도 `InitializeBackend()` 를 부른다. 그래서 필드 초기값으로 만든 묶음은 시작 직후 한 번 교체된다. 교체 전 묶음으로 기록된 증거는 없다.
- **TopBar "Run #"**: 새 묶음을 따라간다. IB-01 에서 `'20260917-140153-f23923' -> '20260917-140156-5a0502'` 로 바뀌었고, 내보낸 zip 의 RunId 와 같았다.

## 2. 시험

| 시험 | 수정 전 트리 | 수정 후 | 반증: 대입 줄 끔 (`falsify-native`) |
|---|---|---|---|
| IB-01 판정·내보내기 → Initialize → 판정·내보내기 | **실패** — `share run id 20260917-140028-c3aaf8` (Mock) / `…140126-48ef41` (Native) | 통과 — Mock `f23923 → 5a0502`, Native 도 통과 | **실패** |
| IB-02 빈 DLL 폴더(실제 Mock) → DLL 복사 → Initialize(실제 Native) | **실패** — `Assert.NotEqual … Strings are equal` (Native) | 통과 (Native) — 아래 기록 | **실패** |
| EB-01~03 (C-86) | — | 통과 | 통과 |

IB-02 기록 (Native, `green-native`):
- 첫 번째 폴더 `20260917-140243-46cd4b`: `actual=Mock requested=Native`
- 두 번째 폴더 `20260917-140246-e4c8d7`: `actual=Native requested=Native`
- 두 번째 Initialize 뒤 상태줄: `mode=Native | common=xpe_display 1.0.0 | display=1.0.0 | src=xpe-e2e-empty-native-eb-…`. 요청 표기 `(requested Native)` 가 사라졌다.

IB-02 의 방향과 조건:
- **순서를 Mock → Native 로 한 이유**: 카드는 Native → Mock(DLL 삭제)을 제안했다. 하지만 실행 중인 앱이 로드한 DLL 은 Windows 에서 잠겨 지울 수 없어서 순서를 바꿨다.
- **Mock 스위트에서는 건너뛴다**: Native 스위트이면서 `XPE_NATIVE_DIR` 이 있을 때만 돈다(복사해 넣을 DLL 이 필요하다).

## 3. 회귀 — `BUILD_EXIT=0` (수정 후·반증·복원 빌드 모두, 경고 0 / 오류 0)

- **Mock E2E** (`full-mock.txt`): 실패 0 / 통과 88 / 건너뜀 2 (NativeProvenance, IB-02 — 둘 다 Mock 실행이라 건너뜀).
- **Native E2E, 새 DLL** (`full-native`, provenance `776292b`): **통과 90 / 실패 0**.
- **통합** (`full-int.txt`): 실패 0 / 통과 209 / 건너뜀 1.

## 4. 미검증 / 잔여 위험

- **Initialize 실패 경로**: 실행하지 않았다(1절).
- **Shutdown Backend 뒤 Initialize**: 이 경우는 따로 시험하지 않았다. 같은 `InitializeBackend()` 경로를 타므로 새 묶음이 된다(판독).
- **증거 폴더 누적 속도**: Initialize 를 누를 때마다 판정 증거 폴더가 새로 생길 수 있어, 폴더가 전보다 빨리 쌓인다. 정리 정책은 여전히 없다(범위 밖).
- **CI 에서 IB-02**: CI 의 `gui-e2e-native` 에서 IB-02 가 도는지(`XPE_NATIVE_DIR` 설정 여부)는 CI 결과로 확인해야 한다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| Initialize = 새 실행 묶음 | GUI-C-89 (이 카드) |
| 증거 폴더 정리 정책 | 범위 밖 (기록만) |
