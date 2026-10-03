# GUI-C-228c — 문자열·리플렉션 경로 감시를 "실제로 잡는 범위"로 (Codex #126)

## 무엇을 바꿨나

`Safety/MockBlockingStaticTests.cs` 만 바꿨다(앱 코드 변경 없음). 228b 의 `TypeFromString` 정규식 하나를, 형태를 쫓지 않는 소스 스캔으로 바꿨다.

- `ForbiddenApis`: 8개 패턴 묶음. AssemblyLoadContext · `Assembly.Load*` · `Type.GetType` · 인수가 있는 `.GetType(`(리터럴이든 변수든) · `Activator` · `CreateInstance*` · `ConstructorInfo`/`MethodInfo`/`MethodBase` 와 `GetConstructor(s)`·`GetMethod(s)`·`GetMember(s)`·`InvokeMember`·`DynamicInvoke` · `CreateDelegate`·`DynamicMethod`·`ILGenerator`·`GetUninitializedObject`·`MakeGeneric*`·`Expression.Lambda/New/Call/Invoke`. 인수 모양은 보지 않는다.
- 스캔에서 빠지는 파일은 없다. 이 파일도 스캔하고, 패턴표·허용 목록·통제 입력이 든 한 구간(`SCAN-EXEMPT-BEGIN` ~ `END`, 각 마커가 정확히 한 번)만 뺀다. 구간을 안 뺀 이 파일은 걸리고 뺀 이 파일은 깨끗함을 통제 시험이 보인다.
- 허용 목록은 3줄(파일 + 정확한 줄 + 이유, 와일드카드 없음). 허용 목록이 낡으면(줄이 바뀌거나 사용이 사라지면) 그것도 빨강이다.

| 파일 | 줄 | 이유 |
|------|----|------|
| `Functional/NativeCommonSingleInstanceTests.cs` | `var method = type.GetMethod(loader, …);` | `type` 은 이 어셈블리의 시험 클래스 3개 고정 배열에서 온다. 네이티브 모듈 적재용 비공개 메서드. 어셈블리를 올리지 않고 앱 타입을 만들지 않는다 |
| `Functional/RunnerProcessTests.cs` | `.GetMethod(nameof(ForEachCommand_…))!` | 자기 시험 메서드의 InlineData 개수를 읽을 뿐 호출하지 않는다 |
| `Functional/AlertDisplayFormatterTests.cs` | `.GetMembers(Public, Static)` | 소스로 링크된 AlertDisplayFormatter 의 공개 정적 멤버 이름을 읽어 심각도 재정의가 없음을 단언한다. 이름만 읽는다 |

시험 프로젝트 전체에서 패턴에 걸린 곳은 이 3줄뿐이었다(전체 스캔으로 확인, 시험이 매 실행 단언한다).

## 반증 (넣은 채로 커밋하지 않음, 원본 복구 확인)

| 팔 | 한 일 | 결과 |
|----|-------|------|
| F | Codex #126 변형: `AssemblyLoadContext.Default.LoadFromAssemblyPath(경로)` + 변수 이름 `asm.GetType(typeName)` + `ConstructorInfo.Invoke` 로 `MockXpeBackend` 생성. 이 시험에는 Mock 타입에 대한 메타데이터 참조가 0이다 | 소스 스캔 빨강. 메타데이터 단언과 기존 `MockBlockingTests` 는 초록(= 228b 의 맹점이 실제로 있었다는 증거) |
| C | `Activator.CreateInstance(Type.GetType(…))` | 소스 스캔 빨강 |
| G | 허용된 줄 하나의 변수명을 바꿈 | 소스 스캔 빨강(새 줄은 비허용, 옛 항목은 낡음) |
| H | `GetTypeWithArgument` 패턴을 문자열 리터럴만 잡도록 약화(228b 스캔 모양) + 변수 인수 `GetType` 사용 | 본 스캔은 통과하고, 통제 `EveryPattern_MatchesItsControlLines_…` 가 빨강(패턴 약화는 그 통제가 잡는다) |

파일: `falsification_{F,C,G,H}_*.txt`.

실수 기록: 반증 스크립트가 같은 파일을 여러 곳 고치는 팔 G 에서 복구용 원본을 첫 편집 뒤 내용으로 덮어써 `NativeCommonSingleInstanceTests.cs` 가 어긋난 채 남았다. `git status` 로 알아채고 `git checkout` 으로 복구했고(그 파일에는 다른 변경이 없었다), 스크립트를 고쳐 H 를 다시 돌렸다. 팔 H 는 첫 시도에서 C# 문자열 이스케이프 오류로 빌드가 깨졌고 같은 이유로 다시 돌렸다.

## 판정과 문서

007 은 구현됨(한계). 한계 문구를 실제로 감지하는 범위로 좁혔다. 메타데이터 참조 0 은 실행 순서와 무관하게 단언하고, 동적 로드·리플렉션 생성 API 사용은 소스 스캔으로 금지하며(허용 3줄), 스캔이 못 보는 것(다른 어셈블리를 거친 간접 호출, 코드 생성, 표에 없는 API, 경로로 올리는 네이티브 라이브러리)은 증명하지 않는다. 상태 합계는 불변이다. `doc_delta.txt` 는 228b 위의 007 행 차이만 담았고, `sufficiency_table.txt` 는 007 행을 갱신한 33행 표다.

## 증거

| 항목 | 값 |
|------|----|
| 통합 시험 전체 | 실패 0 · 통과 875 · 건너뜀 1 · 전체 876 (`integration_full_suite.txt`, 건너뜀 1 은 비관리자 셸의 파일 심볼릭 링크) |

## 미검증 / 잔여 위험

- 패턴표는 내가 떠올린 API 목록이다. 표에 없는 API(다른 방식의 코드 생성 등)는 못 잡고, 위 한계 문구에 그렇게 적었다.
- 허용 목록 3줄의 "이유"는 해당 줄 주변을 읽고 적은 것이다. NativeCommonSingleInstanceTests 의 `type` 이 항상 이 어셈블리의 타입임은 배열 정의(리터럴 `typeof` 3개)로 확인했다.
- 새 시험은 Native 구성(DLL 있음)에서 돌렸다. 이 시험들은 DLL 을 쓰지 않는다고 읽었으나 DLL 없는 구성으로는 이번에 다시 돌리지 않았다.
