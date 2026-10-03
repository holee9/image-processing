# GUI-C-228b — REQ-007 정적 단언과 008 표 (Codex #125 보류 2건)

## 1. REQ-GUI-IT-007: 실행 순서와 무관한 정적 단언

`clients/ImageProcTest.IntegrationTests/Safety/MockBlockingStaticTests.cs` (새 파일, 앱 코드 변경 없음).

- 단언: 컴파일된 시험 어셈블리의 메타데이터(`System.Reflection.Metadata`)에서 TypeDef·TypeRef·ExportedType 이름과 AssemblyRef 이름을 읽어, 앱의 비실제 백엔드 타입이 0이고 앱 어셈블리 참조가 0임을 단언한다. 로드 시점과 무관하다.
- Mock 백엔드의 정의(이름 목록이 아님): `gui/ImageProcTest` 와 `clients/ImageProcTest` 소스에서 `IXpeBackend` 를 구현하는 모든 클래스 중 `Real*` 을 뺀 집합. 지금은 MockXpeBackend(양쪽), CompositeXpeBackend, FaultInjectingBackend, 비공개 CompositeDisposableBackend 둘이다. 새 백엔드는 시험 수정 없이 집합에 들어온다.
- 제네릭 인수: 제네릭 인수도 자기 TypeRef/TypeDef 행을 가지므로 따로 디코딩하지 않는다. 통제 `TheMetadataReader_FindsDefinedReferencedAndGenericArgumentTypes` 가 제네릭 인수로만 쓰인 `SocketFlags` 를 읽어 보인다.
- 통제 3종: 집합이 비지 않고 기대한 백엔드를 담는다(`TheDerivedBackendSet_…`), 읽기가 정의·참조·제네릭 인수 타입을 찾는다, 비교가 실재 타입으로 적중을 낸다(`TheComparison_…`).
- 한계: 문자열로 만든 타입(`Type.GetType`·`Activator.CreateInstance`·`Assembly.Load`)은 메타데이터 행이 없다. 시험 프로젝트에서 그 패턴을 grep 한 결과 0건이고(아래), `TheTestProject_HasNoRuntimeTypeFromStringPath` 가 그것을 상시 단언한다. 새로 생기면 빨강이다.

### 반증 (넣은 채로 커밋하지 않음, 원본 복구 확인)

| 팔 | 한 일 | 결과 | 파일 |
|----|-------|------|------|
| E | 빌드된 gui 앱 어셈블리를 참조하고 실행되지 않는 분기에서 `typeof(MockXpeBackend)` 를 사용 | 새 정적 단언 빨강. 기존 `MockBlockingTests` 4건은 초록 그대로 (Codex 의 맹점 재현). 같은 실행에서 임시 시험 자체도 빨강(어셈블리를 복사하지 않아 JIT 때 로드 실패 — 시험이 아니라 팔의 부산물) | `falsification_E_…txt` |
| C | 기능 시험에서 `Activator.CreateInstance(Type.GetType(…))` | `TheTestProject_HasNoRuntimeTypeFromStringPath` 빨강 | `falsification_C_…txt` |
| B | 파생 집합을 빈 집합으로 교체 | 본 단언은 빈 집합이면 통과한다(그래서 통제가 있다). 통제 `TheDerivedBackendSet_…` 가 빨강 | `falsification_B_…txt` |

- 첫 시도의 두 가지 실수를 적어 둔다. 프로젝트 참조(`ProjectReference`)는 TFM 불일치로 빌드되지 않았고, 두 번째로 `_ = typeof(X);` 는 컴파일러가 지워 TypeRef 가 생기지 않아 정적 단언이 초록이었다(덤프 시험으로 `types has Mock=False` 확인). 필드에 대입하는 형태로 바꾸자 빨강이 됐다. 반증이 안 터졌을 때 "단언이 눈멀었다"가 아니라 "주입이 안 됐다"를 먼저 의심했다.
- 문자열 경로 grep: `Type.GetType(`, `Activator.CreateInstance`, `Assembly.Load*(`, `.GetType("` 가 시험 프로젝트의 `*.cs` 에서 0건(새 시험이 매 실행 단언).

### 판정

007 은 정적 단언이 들어갔으므로 **구현됨(한계: 문자열로 만든 타입)**. 상태 합계는 불변이다(구현됨 25 · 한계 8 · 부분 0 · 보류 3 = 36, `check_totals_output.txt`).

## 2. REQ-GUI-IT-008 표

`sufficiency_table.txt` 008 행에 `TheLoadedModule_IsReallyUnderAnApprovedFolder_WhenItsLinksAreFollowed`(실제 로드 모듈 경로)를 인용하고 한계를 좁혔다. 한계는 링크 권한이 없으면 파일 링크 시험이 건너뜀(CI 러너에서는 돌았다고 리더가 보고, 이번 실행의 로컬 비관리자 셸은 건너뜀 1건)과 실행하지 않은 변형이다. CI 에서 돌았다는 것은 리더의 보고를 옮긴 것이고 나는 CI 로그를 직접 보지 않았다.

## 3. 문서

`doc_delta.txt`: 007·008 의 RTM 행, SPEC §4.6 한계 행, AC-8 행의 BEFORE/AFTER 만. 나머지는 227 의 patch 와 리더의 v1.3.4 초안 그대로다.

## 증거

| 항목 | 값 |
|------|----|
| 통합 시험 전체 (환경 변수 지정) | 실패 0 · 통과 873 · 건너뜀 1 · 전체 874 (`integration_full_suite.txt`) |
| 새 시험 | 본 단언 1 + 통제 3 + 상시 grep 1 + 패턴 통제 4(이론) = 9 (874 − 865) |
| 줄 끝 공백 | 새 파일 전부 0건 |

## 미검증 / 잔여 위험

- 새 시험 9건은 Native 구성(DLL 있음)에서만 돌렸다. 이 시험들은 DLL 을 쓰지 않아 DLL 없는 구성에서도 같다고 읽었으나 이번에 그 구성으로 다시 돌리지는 않았다.
- Mock 집합의 정의는 소스 정규식(`class X … IXpeBackend`)이다. `IXpeBackend` 를 간접 상속(다른 인터페이스 경유)하는 클래스는 못 잡는다. 지금 앱 소스에는 그런 클래스가 없다고 읽었으나 전수 확인은 하지 않았다.
- 반증 E 는 로컬에 빌드된 gui 어셈블리(2026-10-03 18:26)를 썼다.
