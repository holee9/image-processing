# GUI-C-213 — 시그니처 대조를 모든 네이티브 모듈로 (#249)

증거(이 폴더): `census.txt`, `falsification_arms.txt`(15팔), `legacy_call_shape_experiment.txt` + `…_program.cs.txt`. 앱 코드 변경 1건(레거시 앱의 전처리 미리보기) — Codex 대상.

## 한 줄 요약

모든 모듈 헤더(7개 모듈 + common)와 C# 바인딩(정적 `[DllImport]` **그리고 이름으로 묶이는 델리게이트**)을 대조하는 엔진과 시험을 만들었다. 처음 돌렸을 때 **실제 결함 1건**이 나왔다: 레거시 앱의 전처리 미리보기(`NativePreprocessPreviewService`)가 offset·gain·defect 보정 함수를 **#117 이전의 옛 호출 모양**으로 부르고 있었고, 현재 DLL 에서 offset 은 "OK 인데 이미지는 그대로", gain 은 "OK + 메모리 과읽기", defect 는 `BUFFER_TOO_SMALL` 이다. 고쳤고 시험으로 고정했다. 그 밖의 불일치는 헤더 쪽 오류가 아니라 **이름/문서**(아래 3)였다.

## 1. 목록: 모듈별 바인딩 수 (`census.txt`, 234개 파일 스캔)

| DLL | 정적 extern | 델리게이트로 묶인 export | 헤더 함수 중 C# 바인딩이 있는 수 |
|---|---|---|---|
| xpe_common | 36(앱 래퍼·gui·시험 거울 세 사본) | 5 | 16/16 |
| xpe_preprocess | 9 | 14 | **14/48** |
| xpe_enhance_basic | 6 | 7 | 7/10 |
| xpe_display | 6 | 6 | 6/6 |
| xpe_dicom | 6 | 5 | 7/10 |
| gsvg | 4 | 1 | 4/8 |
| xpe_ai | 4 | 0 | 4/11 |
| xpe_enhance_advanced | 0 | 0 | **0/9** |
| Win32(user32·kernel32 등) | 3 | — | 제외(허용 목록: 우리 DLL 아님) |

델리게이트 66형·바인딩 지점 89·구조체 20·열거 31. **한계(중요)**: 대조는 *C# 에 있는 선언*을 헤더에 맞춘다 — 바인딩이 아예 없는 함수(preprocess 34개, enhance_advanced 9개 등)는 대조 대상이 아니다. 그 함수를 어떤 C# 도 안 부르는지, 아니면 이 스캐너가 읽지 못하는 방식(리터럴이 아닌 이름 등)으로 부르는지는 구분하지 않았다(`census.txt` 에 함수별 미바인딩 목록).

## 2. 엔진과 시험

`ModuleSignatureParity`(엔진), `NativeModuleSignatureParityTests`(실제 소스 4개), `ModuleSignatureParityControlTests`(엔진 통제 23개). 208 M1 의 시험(xpe_common 세 사본)은 그대로 둔다.

- **헤더**: `XPE_API` 선언, `typedef struct/enum`, 불투명 핸들을 읽는다. 읽지 못하는 것·모르는 형식·`#pragma pack(8)` 이외는 **실패**(건너뛰지 않음). 함수 개수를 모듈별로 고정(48/10/9/6/10/8/11/16)해 파서가 아무것도 못 읽고 통과하는 길을 막는다.
- **C#**: 정적 `[DllImport]` extern + `delegate` 선언 + 바인딩 지점(`GetRequiredDelegate<T>(handle, "export")`, `TryGetDelegate(handle, "export", out T v)`, `TryGetExport`+`GetDelegateForFunctionPointer<T>`, 일반 `Export<T>("export")`). 주석은 문자열을 건드리지 않고 제거(주석 속 `[DllImport]` 가 선언으로 읽히던 것을 통제 시험이 막는다).
- **비교**: 함수 이름, 인자 수, 인자·반환 종류(정수 폭·부호, float/double, bool 폭 — `bool` 은 `[MarshalAs(U1)]` 요구), `ref`/`out`/`in` 과 const 정합, 문자열 마샬링(LPStr/LPUTF8Str, 와이드는 실패), 호출 규약(Cdecl 명시), **구조체 배치**(양쪽의 필드별 크기·오프셋·전체 크기를 계산해 대조 — `Pack` 과 `ByValArray`/`ByValTStr` 포함), **열거 값**. 같은 이름의 구조체·열거가 여러 파일에 있으면 **전부** 각각 대조한다.
- **한쪽에만 있는 선언**은 이유 달린 허용 목록: gui 의 `XpePixelFormatNative`(UInt16·Float32 만 넘김), 매이는 곳이 없는 델리게이트 12개(managed 콜백 2개 + 선언만 있고 어디서도 쓰이지 않는 10개), 낡은 항목은 실패.
- **통제**(엔진이 말한 것을 실제로 잡는지): 일치하는 작은 헤더/바인딩이 깨끗하고, 한 가지씩 깨면 해당 문구가 나온다 — 인자 형식·개수·반환, 값 전달 대 포인터, out/in 오용, 와이드 문자열, bool, 이름 없음, 구조체 필드 형식·이름·개수·`Pack=1`·배열 길이, 열거 값, 모르는 C#/네이티브 형식, 호출 규약(extern·delegate), 델리게이트 모양·없는 export. **통제 시험이 엔진 버그 2건을 찾았다**(bool 의 `!x?.Contains == true` 우선순위로 U1 없는 bool 이 통과, 모르는 네이티브 형식에서 NullReference) — 고쳤다.

## 3. 발견과 영향

| # | 발견 | 영향(측정) | 처리 |
|---|---|---|---|
| 1 | **레거시 앱 `NativePreprocessPreviewService` 의 보정 델리게이트가 `(image, map[, config])` — 헤더는 `(input, output, metadata)`** | 현재 DLL 에 대해 같은 입력으로 실행(`legacy_call_shape_experiment.txt`): **offset**: rc=OK, 이미지 **변경 없음**, 보정된 값(900…)이 *맵 배열*에 써짐(미리보기에서 offset 단계가 아무 효과 없음); **gain**: rc=OK, 결과는 *맵 배열*(float)에 써지고 코드는 이미지 버퍼에서 1024바이트를 복사하는데 그 버퍼는 512바이트 — **과읽기**; **defect**: 출력으로 UInt8 맵 버퍼를 넘겨 rc=-8 `BUFFER_TOO_SMALL`. 212 에서 "인자 하나를 빼도 시험이 안 터진" 것과 같은 계열이다(아래 4) | **고쳤다**: 델리게이트 3개를 3인자로, `CallOffset/CallGain/CallDefect` 가 단계마다 자기 출력 버퍼를 만들어 결과를 다음 단계로 넘기고(실패하면 입력 유지), 교정 맵은 `LoadCalibrationFiles` 가 이미 모듈 저장소에 적재하므로 인자에서 뺐다. 반증: 고치기 전 파일로 되돌리면 델리게이트 시험 빨강(2건) |
| 2 | 시험 쪽: 내 `NegativeInputPathTests` 의 `AllocFn`·`FreeFn` 델리게이트에 `[UnmanagedFunctionPointer(Cdecl)]` 이 없었다 | 영향 없음(x64 에서 기본이 cdecl 과 같다) — 선언이 호출 규약을 말해야 한다는 규칙에 걸림 | 고쳤다 |
| 3 | `XpePixelFormatNative`(gui)에 UInt8 없음 | 영향 없음(gui 는 UInt16·Float32 만 넘김) — 208 이 이미 허용한 부분 열거 | 허용 목록(이유) |
| 4 | 모듈 헤더 쪽 문제 | **헤더가 틀렸다고 볼 근거는 이번에 나오지 않았다.** 다만 아래 관찰 하나는 해당 레인(pre)이 판단할 일 | 목록(고치지 않음) |

**4번 관찰(헤더 문서 대 구현, pre 소유)**: `preprocess_api.h` 는 보정 함수의 `metadata` 를 "온도·촬영 시각"(offset), "kVp·SID"(gain)를 담는 것으로 문서화하는데, 구현(`offset_correct.cpp:153`, `gain_correct.cpp:247`, `defect_correct.cpp:223`)은 `!metadata` 로 **널 검사만** 하고 필드를 한 번도 읽지 않는다(`metadata->` 사용 0건, `preprocess` src·include 검색). 문서가 약속한 보간·kVp/SID 선택이 구현에 없거나 다른 곳에 있다 — 확인하지 않았다.

## 4. 212 의 "metadata 인자를 빼도 결과가 같았던" 이유 (항목 4, 한 줄)

세 보정 함수는 `metadata` 를 **널인지만 검사**하고(`offset_correct.cpp:153`·`gain_correct.cpp:247`·`defect_correct.cpp:223`: `if (!input || !output || !metadata) return XPE_ERR_INVALID_INPUT;`) **내용은 읽지 않는다** — 인자를 빼도 셋째 인자 자리에 남은 쓰레기 값이 널이 아니어서 검사를 통과하고 결과는 같았다. 우연히 통과한 것이라, 그 자리가 0 이었다면 `INVALID_INPUT` 이다(그래서 인자 개수 대조가 필요하다).

## 반증 (`falsification_arms.txt`) — 15팔 전부 빨강, 바이트 동일 복원

모듈마다 선언 하나: common(정적 `int→long`, 열거 `Float32=1→3`, 델리게이트 `uint→int`) / preprocess(정적 `StdCall`, 델리게이트 `int→long`, 바인딩 export 이름이 헤더에 없음, **#1 의 옛 모양 파일**) / enhance_basic(정적 `float→double`, 구조체 필드) / display(구조체 필드, 열거 값) / dicom(정적·델리게이트 `uint→ulong`) / gsvg(`string→int`) / ai(`out uint→out int`). 각 팔은 정적 extern 시험 또는 델리게이트 시험 중 해당하는 쪽이 빨강이다(widening 변경을 골라 호출부가 컴파일되게 했다 — 컴파일 안 되는 팔은 못 돈다).

## 한계 (정직하게)
- **바인딩이 없는 함수는 못 본다**(위 1). 헤더 함수 118개 중 C# 바인딩이 있는 것은 58개.
- **인자 타입이 맞아도 의미가 틀린 호출은 못 잡는다**: #1 의 `DefectCorrectionDelegate` 는 개수가 3개로 같아 이전 버전에서도 개수 검사는 통과했고, 의미(출력 자리에 맵을 넘김)는 **델리게이트 이름·인자 이름을 사람이 읽은 것**으로 찾았다. 엔진은 `IntPtr` 로 받는 포인터의 원소 형식과 인자의 *역할*을 검사하지 못한다.
- 수정한 미리보기 경로를 **픽스처로 끝까지 실행해 보지 않았다**(캘리브레이션 원본이 git 무시 로컬 자료라 이 환경에 없다). 확인한 것: 헤더 모양의 호출이 현재 DLL 에서 기대대로 동작함(`legacy_call_shape_experiment.txt` 의 B), 선언·호출 지점이 컴파일되고 대조 시험을 통과함, 레거시 앱의 UIA E2E 두 개 통과. 단계 사이의 값 전달(`currentUInt16`/`currentFloat` 교체)은 코드 읽기로만 확인.
- `NativePreviewMetrics`·성능 등 이 변경이 바꾸는 미리보기 수치를 비교하지 않았다.
- DLL 은 IntegrationTests 출력 폴더의 09-26 파일이다.
