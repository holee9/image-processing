# GUI-C-13 — WPF 앱 enum 사본도 헤더와 1:1 + 드리프트 검출을 두 사본으로 확장

- 카드: GUI-C-13 · Refs #117 · 커밋 `73a2f0d` (미푸시)
- 워크트리: `D:/workspace-github/xpe-gui` / `dev/gui` / HEAD `eacd148`
- **결과: 실패 0 / 통과 106 / 건너뜀 1 / 전체 107**

---

## 1. 사본 전수 — 카드가 말한 2개가 아니라 3개였다

```
grep -rn "enum XpeErrorCode|enum.*ErrorCode" --include=*.cs clients gui
  clients/ImageProcTest/PInvokeWrapper.cs:33                 enum XpeErrorCode
  clients/ImageProcTest.IntegrationTests/PInvoke/XpeCommonNative.cs:25  enum XpeErrorCode
  gui/ImageProcTest/Services/Native/XpeDisplayInterop.cs:11   enum XpeErrorCodeNative
```

세 번째는 **미러가 아니다.** `Ok = 0` 하나뿐이고 `RealXpeBackend.cs:280` 에서
`if (code < (int)XpeErrorCodeNative.Ok)` — **부호 임계값**으로만 쓰인다. "동기화된 이름표"를
자처한 적이 없고 명명 규칙도 다르다(`Ok` vs `OK`). 1:1 을 강요하면 아무도 읽지 않는 멤버
16개가 늘 뿐이다.

그래서 **제외하되 침묵하지 않는다** — 테스트의 `NonMirroringCopies` 배열에 경로·이름·사유를
코드로 남겼다. 나중에 읽는 사람이 "빠뜨린 것"이 아니라 "판단한 것"으로 읽게.

## 2. 바꾼 것

### 2-1. 앱 enum 을 헤더와 1:1

`clients/ImageProcTest/PInvokeWrapper.cs` 의 `XpeErrorCode` 에 `-11`~`-16` 추가.
`@MX:NOTE "kept in sync intentionally"` 는 **의도의 선언이지 장치가 아니었다** — 실제로 6개가
빠져 있었다.

### 2-2. 드리프트 검출을 사본 목록 데이터로

```csharp
public static readonly MirroringCopy[] AllMirroringCopies =
{
    new("clients/ImageProcTest.IntegrationTests/PInvoke/XpeCommonNative.cs", "XpeErrorCode"),
    new("clients/ImageProcTest/PInvokeWrapper.cs", "XpeErrorCode"),
};
```

각 사본을 **소스 텍스트로 읽어** 헤더와 양방향 대조한다. 이 선택의 이유:

- **프로젝트 참조가 필요 없다** — §4 의 두 장벽을 우회한다
- **가시성 요구가 없다** — 앱 enum 은 `internal` 이다
- 새 사본이 생기면 **목록에 한 줄**. 카드가 요구한 "사본 목록을 데이터로" 그대로

케이스는 사본당 2개(헤더→사본 누락·값 불일치 / 사본→헤더 유령 멤버), 총 4개.

### 2-3. 파서 자신을 검증하는 케이스

`SourceParser_AgreesWithCompiledEnum_ForThisAssemblysCopy` — 이 어셈블리가 **실제로
컴파일한** enum(`Enum.GetValues`)과 같은 파일을 텍스트로 파싱한 결과가 같아야 한다.

없으면 파서 버그 하나로 **모든 사본이 준수처럼 보인다.** C-12 에서 잡은 "테스트가 자기 입력을
못 본다"와 같은 형태의 사각을 텍스트 파싱이 새로 만들 수 있어서 넣었다.

## 3. 반증 실험 — 새 사본에서도 발화하는가

앱 사본에서 `CALIB_NOT_LOADED = -16` 을 지우고 돌렸다.

```
실패! - 실패: 1, 통과: 4, 전체: 5

  clients/ImageProcTest/PInvokeWrapper.cs::XpeErrorCode is missing header code(s):
  CALIB_NOT_LOADED. Add them with the value from modules/common/include/xpe/common/xpe_error.h.
```

**어느 사본인지 파일 경로로 지목**한다. 로그: `step2-drift-negative.log`. 원복 후 재확인
(`step3-final.log`).

## 4. 사본 단일화 — 하지 않았다. 두 경로 다 막혔고 둘 다 실측했다

카드 3항("가능하면 사본을 하나로")에 대한 답이다. **추정이 아니라 확인한 결과다.**

### 경로 A — ProjectReference: TFM 불일치

```
clients/ImageProcTest/ImageProcTest.csproj:          OutputType=WinExe, net8.0-windows
clients/ImageProcTest.IntegrationTests/…csproj:      net8.0
```

`net8.0` 프로젝트는 `net8.0-windows` 어셈블리를 참조할 수 없다. 테스트 프로젝트를
`net8.0-windows` 로 올리면 WPF 툴체인을 테스트에 끌어들이게 되는데, 이 프로젝트가 애초에
`net8.0` 인 이유가 그 회피다(`XpeCommonNative.cs:1-2` 주석: "to avoid WPF/WinExe compilation
dependency").

### 경로 B — `<Compile Include>` 링크: 리졸버 중복 등록

이 csproj 는 이미 WPF 무의존 파일 4개를 링크한다. `PInvokeWrapper.cs` 도 문법상 가능하다
(그 파일에 WPF `using` 이 없음을 확인). 그러나 `XpeCommonApi` 의 **정적 생성자**가
`NativeLibrary.SetDllImportResolver(…)` 를 부르고, `NativeLibraryFixture` 도 **같은 어셈블리에**
등록한다.

프로브를 만들어 실제로 확인했다(임시 파일, 실행 후 삭제):

```
first=none  second=InvalidOperationException: A resolver is already set for the assembly.
```

즉 `XpeCommonApi` 의 정적 생성자가 한 번이라도 돌면 테스트 어셈블리가 예외로 죽는다.
링크는 **런타임에 깨지는 선택**이다.

### 결론

두 사본을 유지하되 **드리프트 테스트가 동기화 장치 역할**을 한다. 주석이 하던 일을 테스트가
한다 — 카드의 취지(사본이 어긋나지 않게)는 단일화 없이도 충족된다.

## 5. 앱의 오류 표시 경로 — 새 코드가 어떻게 보이나 (카드 1항)

`XpeErrorCode` 는 사용자에게 보이는 문자열로 **보간된다**:

| 위치 | 형태 |
|---|---|
| `MainWindow.xaml.cs:2281-2282` | 상태줄 `{stage.Stage}={stage.ErrorCode}` |
| `Services/GuiE2eReportService.cs:308,333,372` | E2E 리포트 마크다운 `` `{stage.ErrorCode}` `` |
| `Services/NativePresentationExportService.cs:359` | 예외 메시지 `returned {result.ErrorCode}` |
| `Diagnostics/NativeReadinessProbe.cs:193` | 진단 문자열 |

이름 없는 enum 값의 `ToString()` 은 숫자를 낸다. 따라서 **동기화 전에는** 캘리브레이션 미로드로
보정이 실패하면 상태줄·리포트·예외에 `CALIB_NOT_LOADED` 가 아니라 `-16` 이 찍혔다.
동기화 후에는 이름이 나온다.

앱은 `xpe_error_string` 을 `RealXpeCommonBackend.cs:26` 한 곳에서만 쓰고(진단 문자열 생성),
나머지 경로는 enum 이름에 의존한다 — 그래서 이 동기화가 표시 품질에 직접 걸린다.

## 6. 미검증 (Gaps)

- **WPF 앱을 실행해 눈으로 확인하지 않았다.** §5 는 보간 지점과 `Enum.ToString()` 동작에서
  도출한 것이고, 화면 캡처로 확인한 것이 아니다. 앱 실행은 이 레인 범위 밖이다
- **앱 프로젝트를 빌드하지 않았다.** 변경은 enum 멤버 추가뿐이고 테스트 스위트가 같은 헤더와
  대조하지만, `clients/ImageProcTest.slnx` 빌드는 돌리지 않았다 (GUI-C-07 에서 경고 0 확인한
  이후 앱 코드는 이 카드가 처음 건드린다)
- **CI 실행 결과는 아직 없다.** 로컬은 CI 아티팩트 바이너리로 돌렸다
- 드리프트 파서는 `#define` 과 `enum { NAME = value }` 형식만 읽는다. 형식이 바뀌면
  "파싱 0건" 으로 **실패**한다(조용히 통과하지 않는다)
- 건너뜀 1건은 C-12 와 같은 `-15` 문자열 미매핑이며 `#126` / QA-A-23 대기

## 7. 잔여 위험

- 이 테스트는 **드리프트를 잡을 뿐 고치지 않는다.** 사본이 2개인 사실은 그대로이고, 누군가
  세 번째 미러를 만들면 목록에 추가하기 전까지는 보이지 않는다. 목록 자체가 손으로 관리되는
  한 C-12 에서 잡은 "손 목록" 문제의 축소판이 남는다 — 다만 이번엔 **하나의 목록**이고
  대조 대상이 소스 전체가 아니라 사본이라, 새 사본 추가가 리뷰에서 눈에 띈다
- `gui/` 쪽 `XpeErrorCodeNative` 가 나중에 이름표로 쓰이기 시작하면 제외 판단이 뒤집힌다.
  그때는 `NonMirroringCopies` 에서 `AllMirroringCopies` 로 한 줄 옮기면 된다

## 부록 — 사용한 명령

```bash
grep -rn "enum XpeErrorCode\|enum.*ErrorCode" --include=*.cs --exclude-dir=obj --exclude-dir=bin clients gui
grep -n "TargetFramework\|OutputType" clients/ImageProcTest/ImageProcTest.csproj \
                                      clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
dotnet test … --filter "FullyQualifiedName~ErrorCodeHeaderParityTests"   # 반증 실험
dotnet test … --filter "FullyQualifiedName~TempResolverProbe"            # 리졸버 프로브(임시, 삭제함)
```
