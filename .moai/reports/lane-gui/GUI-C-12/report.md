# GUI-C-12 — C# 오류 코드 enum 을 `xpe_error.h` 와 1:1 동기화 + 전수 순회 테스트

- 카드: GUI-C-12 · Refs #117 · 커밋 `eacd148` (미푸시)
- 워크트리: `D:/workspace-github/xpe-gui` / `dev/gui` / HEAD `acf727e` (main 병합, `.claude` 변경 0건)
- **결과: 실패 0 / 통과 103 / 건너뜀 1 / 전체 104**

---

## 1. 무엇이 사각이었나

`ErrorString_ForAllDefinedCodes` 는 이름과 달리 전수가 아니었다. **순회 대상이 손으로 적은
`InlineData` 목록이었고, 그 목록은 enum 을 따라간다.** 그래서 enum 자체에 `-11`~`-15` 가
없는 상태로 "ForAllDefinedCodes" 가 계속 통과했다 — 테스트가 자기 입력의 완전성을 볼 수
없는 구조였다.

같은 성격의 실패가 이 레인에 이미 두 번 있었다(`gate.md` #2 정의≠호출, #8 대입≠정확).
이번 것은 **"목록을 다 돌았다" ≠ "다 확인했다"** 다.

## 2. 바꾼 것

### 2-1. enum 을 헤더와 1:1 로

`xpe_error.h` 를 정본으로 `SAFETY_VIOLATION(-11)` `INTERNAL(-12)` `DICOM_INVALID(-13)`
`DICOM_CONFORMANCE(-14)` `NOT_IMPLEMENTED(-15)` 추가. `-16` 은 C-11 에서 이미 넣었다.
이제 헤더의 `XPE_OK` + `XPE_ERR_*` 16개와 enum 17멤버가 정확히 대응한다.

### 2-2. 순회를 enum 자신에서 뽑는다

```csharp
public static IEnumerable<object[]> AllErrorCodes() =>
    Enum.GetValues<XpeCommonNative.XpeErrorCode>().Select(code => new object[] { code });
```

`[MemberData(nameof(AllErrorCodes))]` 로 바꿔, enum 에 코드가 추가되면 **자동으로** 순회
대상이 된다. 손 목록과 enum 이 어긋날 여지를 없앴다.

단언도 강화했다: 널 아님·비어있지 않음에 더해 **`"Unknown error"` 도 실패로 본다.**
그것이 미매핑 코드의 반환값이라, 허용하면 매핑 누락이 조용히 통과한다.

### 2-3. 드리프트 검출 — 헤더를 직접 읽는다

`Functional/ErrorCodeHeaderParityTests.cs` 신설. `modules/common/include/xpe/common/xpe_error.h`
를 저장소 루트 기준으로 찾아 `#define XPE_OK|XPE_ERR_*` 를 파싱하고 enum 집합과 **양방향**
대조한다.

| 케이스 | 잡는 것 |
|---|---|
| `Enum_CoversEveryHeaderCode_WithMatchingValue` | 헤더에 있는데 enum 에 없음 / 값 불일치 |
| `Enum_DeclaresNoCodeAbsentFromHeader` | enum 에만 있는 유령 코드 |

**네이티브 DLL 이 필요 없다** — 헤더는 소스 파일이므로 CI 에서도 항상 존재하고, 이 두 건은
어디서도 스킵되지 않는다. 헤더를 못 찾으면 **스킵이 아니라 실패**시킨다: 조용히 스킵하면
이 클래스가 없애려는 사각이 그대로 돌아온다.

## 3. 반증 실험 — 드리프트 테스트가 실제로 잡는가

통과를 보고 넘기지 않았다. `DICOM_INVALID = -13` 을 일부러 지우고 돌렸다.

```
dotnet test --filter "FullyQualifiedName~ErrorCodeHeaderParityTests"
실패! - 실패: 1, 통과: 1, 전체: 2

  XpeErrorCode is missing header code(s): DICOM_INVALID.
  Add them to PInvoke/XpeCommonNative.cs with the value from
  modules/common/include/xpe/common/xpe_error.h.
```

누락 코드의 **이름을 지목**하고 고칠 파일까지 알려준다. 로그: `step2-drift-negative.log`.
원복 후 재확인했다(`step3-final.log`).

## 4. 재실측

CI 아티팩트 바이너리(run `34446484770`, sha `7275916`)를 그대로 썼다.
`git diff --stat 7275916..acf727e -- modules/common/` 이 **빈 결과**라 `modules/common` 은
무변경이고, 이 카드가 보는 것은 그 모듈의 `xpe_error_string` 뿐이므로 재다운로드 없이 유효하다.
(`acf727e` 용 CI 실행 `34447327400` 은 당시 빌드 잡이 진행 중이었다.)

```
실패 0 / 통과 103 / 건너뜀 1 / 전체 104        step3-final.log
```

케이스 수 검산: 97(C-11) + 5(enum 5멤버가 순회에 추가) + 2(드리프트 2건) = **104**. 일치.

### 건너뜀 1건에 대하여

카드의 목표는 "스킵 0" 이었으나, **카드가 스스로 허용한 유일한 예외**다
(`-15` 는 #126/A-23 전엔 실패할 수 있으니 그 1건만 `Skip.If` 로 사유 명시).

`XPE_ERR_NOT_IMPLEMENTED (-15)` 는 네이티브 `xpe_error_string` 의 `switch` 에 **case 가
없어** `default: "Unknown error"` 로 떨어진다(`xpe_common.cpp:256-272` 전수 확인). 강화한
단언이 이것을 정확히 잡아냈다 — 스킵은 회피가 아니라 **관측된 사실의 기록**이다.
#126 / QA-A-23 이 문자열을 추가하면 그 `Skip.If` 한 줄을 지우면 된다.

## 5. 미검증 (Gaps)

- **CI 실행 결과는 아직 없다.** 위는 CI 아티팩트를 로컬에서 돌린 것이다
- **`clients/ImageProcTest/PInvokeWrapper.cs` 의 `XpeErrorCode` 는 손대지 않았다.**
  그 파일은 같은 enum 의 또 다른 사본이고(`@MX:NOTE` 에 "kept in sync intentionally"),
  거기도 `-11`~`-16` 이 없을 가능성이 높다. **카드 범위가 테스트 프로젝트라 확인만 하고
  변경하지 않았다** — 별도 판정 사안 (§6)
- 드리프트 테스트는 `#define` 형식만 파싱한다. 헤더가 `enum` 선언으로 바뀌면 파싱이 0건이
  되는데, 그때는 "형식이 바뀌었다" 는 메시지로 **실패**한다(조용히 통과하지 않는다)
- Release 구성 미빌드, WPF 앱 미실행

## 6. 잔여 위험 / 보고 사항

- **같은 enum 사본이 최소 2개다.** `PInvoke/XpeCommonNative.cs`(테스트)와
  `clients/ImageProcTest/PInvokeWrapper.cs`(WPF 앱). 이번 드리프트 테스트는 **전자만** 헤더와
  대조한다. 후자가 어긋나면 앱 쪽에서 같은 사각이 재발한다 — 드리프트 테스트를 두 사본 모두에
  적용할지는 leader 판정 사안이다
- `gui/ImageProcTest/Services/Native/` 쪽에도 별도 interop 선언이 있다(GUI-C-09 §6 에서 관찰).
  오류 코드 enum 을 쓰는지는 이 카드에서 확인하지 않았다

## 부록 — 사용한 명령

```bash
git merge origin/main                                   # → acf727e
git diff --stat 7275916..acf727e -- modules/common/     # 빈 결과 = 스테이징 바이너리 유효
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
dotnet test … --filter "FullyQualifiedName~ErrorCodeHeaderParityTests"   # 반증 실험
grep -n -A 30 "xpe_error_string" modules/common/src/xpe_common.cpp
```
