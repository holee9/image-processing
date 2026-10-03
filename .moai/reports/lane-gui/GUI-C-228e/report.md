# GUI-C-228e — 스캔 대상은 실제 컴파일 목록, 줄 제외는 0 (Codex #128)

앱 코드는 고치지 않았다. 바뀐 것은 `Safety/MockBlockingStaticTests.cs`, `Resources/forbidden-reflection-apis.json` 뿐이다.

## 무엇을 바꿨나

1. **줄 제외 0**: 주석 판정(`CodeLines`)을 통째로 지웠다. 모든 줄(주석·문자열 포함)을 패턴에 건다. 따옴표 규칙 같은 예외도 없다. 문법을 추측하는 규칙이 남지 않는다. 오탐은 데이터 파일의 허용 목록으로 처리하고, 항목은 **파일 + 줄 전체(trim) 일치**다. 줄이 바뀌면 낡은 항목으로 빨강이다(반증 G, c3).
2. **스캔 대상 = 실제로 컴파일된 소스**: 폴더를 걷지 않는다. 시험 어셈블리의 포터블 PDB(옆 파일 또는 임베디드)에서 `MetadataReader.Documents` 를 읽어 `.cs` 경로를 모두 스캔한다. 링크 소스와 생성 소스가 들어온다.
   - PDB 가 없거나 Document 가 0이면 빨강. 문서 경로가 이 체크아웃에 없으면 저장소 루트 아래에서 경로의 가장 긴 꼬리로 찾고, 그래도 못 찾으면 빨강이다.
   - 파일 키는 저장소 상대 경로다. 저장소 밖 파일은 전체 경로 그대로다.
3. **통제**: `TheCompiledSourceList_HoldsThisFile_ALinkedAppSource_AGeneratedSource_AndEveryCompileItem`(이 검사 파일, 링크 파일 `XpePreprocessOracleProcess.cs`, `obj/` 의 생성 소스가 목록에 있고 csproj 의 모든 `Compile Include` 가 목록에 있다), `ThePdbReader_FailsWhenThereIsNoPdb`(PDB 없는 사본은 빈 목록이 아니라 오류), `ThisFile_IsScannedInFull_AndHoldsNoPattern`, `NoLineIsExcluded_…`(주석 줄·블록 주석 안·`*/` 뒤 코드·문자열 뒤 코드·보간 raw 문자열 안의 `//` 줄 모두 적중).

## 컴파일 목록과 csproj 대조 (`compile_list_comparison.txt`)

| 항목 | 수 |
|------|----|
| PDB 의 `.cs` Document | 167 |
| csproj 의 명시적 `Compile Include` | 60 (전부 PDB 목록에 있음 — 통제가 단언) |
| 프로젝트 밖에서 링크한 앱 소스 | 60 |
| 프로젝트 폴더의 직접 쓴 소스 | 103 |
| `obj/` 의 생성 소스 | 3 (`.NETCoreApp,Version=v8.0.AssemblyAttributes.cs`, `AssemblyInfo.cs`, `GlobalUsings.g.cs`) |
| 그 밖 | 1 (테스트 SDK 가 넣는 `Microsoft.NET.Test.Sdk.Program.cs`, NuGet 캐시) |

csproj 항목 수와 PDB 의 차이는 직접 쓴 103 + 생성 3 + SDK 1 = 107(= 167 − 60)이다. 모두 스캔 대상이다.

## 걸린 곳과 허용 목록 (4줄)

줄 제외를 없앴을 때 새로 걸린 줄은 **1줄**이었다(주석이나 문자열에 이름이 든 줄은 없었다).

| 파일 | 줄 | 이유 |
|------|----|------|
| `clients/ImageProcTest.IntegrationTests/Functional/NativeCommonSingleInstanceTests.cs` | `var method = type.GetMethod(loader, …);` | (228c 에서 유지) 이 어셈블리의 시험 클래스 3개 고정 배열의 비공개 로더 |
| `clients/ImageProcTest.IntegrationTests/Functional/RunnerProcessTests.cs` | `.GetMethod(nameof(ForEachCommand_…))!` | (유지) 자기 시험 메서드의 InlineData 개수를 읽음, 호출 안 함 |
| `clients/ImageProcTest.IntegrationTests/Functional/AlertDisplayFormatterTests.cs` | `.GetMembers(Public, Static)` | (유지) 링크된 formatter 의 공개 정적 멤버 이름 읽기 |
| `clients/ImageProcTest/Diagnostics/XpePreprocessOracleProcess.cs` | `record.GetConstructors()` | **새로**. 앱 소스 링크 파일. `RequiredNames` 가 record 타입의 생성자 매개변수 이름만 읽는다. 호출처 두 곳이 `typeof(PreprocessSyntheticOracleResult)`·`typeof(PreprocessSyntheticStageResult)` 를 넘기고, 호출·로드·생성은 없다 |

앞 3줄의 키가 `Functional/…` 에서 저장소 상대 `clients/ImageProcTest.IntegrationTests/Functional/…` 로 바뀌었다.

## 반증 (넣은 채로 커밋하지 않음, 원본 복구 확인)

| 팔 | 한 일 | 결과 |
|----|-------|------|
| (a) | 프로젝트 밖에서 링크되는 `clients/ImageProcTest/Diagnostics/XpePreprocessOracleProcess.cs` 에 금지 호출 추가 | 빨강 |
| (b) | Codex #128: 보간 raw 문자열 안의 `//` 로 시작하는 줄에 실행되는 식 | 빨강 |
| (c1) | 순수 주석 줄의 API 이름(허용 목록에 없음) | 빨강 (이제 그것이 의도) |
| (c2) | 같은 줄을 파일 + 줄 전체로 허용 목록에 추가 | 초록 14/14 |
| (c3) | 허용 항목이 줄의 앞부분뿐 | 빨강 (전체 일치) |
| (d) | PDB 를 지우고 `--no-build` 로 실행 | 빨강 2건 |
| (e1)~(e3) | 데이터 파일 패턴 비움 / 하나 삭제 / 깨진 JSON | 각 빨강 4건 |
| F, C, G, H | Codex #126 변형 / 동적 생성 호출 / 허용 줄 이름 변경 / 패턴 약화 | F·C·G 빨강, H 는 통제가 빨강 |

파일: `falsification_*.txt`. 반증 중 변경한 앱 파일(`XpePreprocessOracleProcess.cs`)과 시험 파일은 각 팔 뒤 복구했고, 최종 `git status` 는 의도한 두 파일(시험 `.cs`, 데이터 `.json`)만 수정으로 보인다.

## 증거

| 항목 | 값 |
|------|----|
| 통합 시험 전체 | 실패 0 · 통과 874 · 건너뜀 1 · 전체 875 (`integration_full_suite.txt`; 건너뜀 1 은 비관리자 셸의 파일 심볼릭 링크) |
| 출력 폴더의 데이터 파일 | 소스와 JSON 내용 동일(비교 실행) |

## 미검증 / 잔여 위험

- 테스트 SDK 소스(`Microsoft.NET.Test.Sdk.Program.cs`)는 PDB 가 NuGet 캐시의 절대 경로를 가리킨다. 이 시험은 그 파일이 있어야 하므로, 빌드한 기계와 다른 기계에서 PDB 만 가져와 돌리면 "찾을 수 없음"으로 빨강이 된다(CI 는 같은 잡에서 빌드하고 실행하므로 문제 없다고 읽었으나 CI 에서 확인하지 않았다).
- 패턴표는 내가 떠올린 API 목록이다. 표에 없는 API, 다른 어셈블리를 거친 호출, 실행 중 생성되는 코드, 경로로 올리는 네이티브 라이브러리는 못 본다(문서에 명시).
- 이 검사 파일의 시험 `.cs` 와 JSON 이 PDB 목록에 있다는 것은 통제가 단언한다. 다만 Debug 구성만 돌렸고 Release 구성(PDB 형식·경로)은 돌려 보지 않았다.
- DLL 없는 구성은 이번에도 다시 돌리지 않았다(이 시험들은 DLL 을 쓰지 않는다고 읽음).
