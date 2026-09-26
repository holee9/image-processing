# GUI-C-15 — 저하 모드 판정을 R0 문자열까지 (A안 추출 + BP-08)

- 카드: GUI-C-15 · Refs #128 · 커밋 `6dc7ff2` (미푸시)
- 워크트리: `D:/workspace-github/xpe-gui` / `dev/gui` / HEAD `8c6b2cb`
- **결과: 실패 0 / 통과 113 / 건너뜀 1 / 전체 114** (C-14 의 113 + BP-08 1건)
- 앱 파일 **2개**(신설 1 + 수정 1) — 허용 범위 내

---

## 1. 왜 C-14 는 로케이터에서 멈췄나

R0 판정이 `ModuleReadinessService` 안의 `"R0"` **리터럴 7개**로 흩어져 있었고, 그 서비스를
부르려면 프로브 → `XpeCommonApi` → 리졸버 이중 등록 예외(GUI-C-13 실측)를 지나야 했다.
판정이 서비스에 갇혀 있었던 것이 원인이지, 판정 자체가 복잡해서가 아니었다.

## 2. 추출 (앱 파일 2개)

### 2-1. 신설 — `Services/ModuleReadinessGrading.cs`

```csharp
public const string NotReady = "R0";
public static string Grade(bool probeReportedReady, string readyLevel)
    => probeReportedReady ? readyLevel : NotReady;
public static string GradeDiscovery(string? resolvedPath, string readyLevel)
    => Grade(resolvedPath is not null, readyLevel);
```

상태 없음 · 네이티브 무의존 · `XpeCommonApi` 무참조 → **테스트 프로젝트가 직접 링크**한다.

**R1/R2/R3 는 옮기지 않았다.** 상위 등급은 모듈마다 근거가 다르고(버전 문자열, export
체크리스트, 스모크 결과) 단일한 형태가 없다. 공통인 것은 "미해결·미통과 → R0" 한 갈래뿐이라
그것만 뽑았다 — 추출을 넓히면 서비스의 모듈별 텍스트까지 끌려온다.

### 2-2. 수정 — `Services/ModuleReadinessService.cs`

| 위치 | 전 | 후 |
|---|---|---|
| `EvaluateDllPresence:337` 발견 분기 | `if (found != null)` | `if (GradeDiscovery(found, "R1") != NotReady)` |
| R0 스냅샷 7곳 | `"R0",` | `ModuleReadinessGrading.NotReady,` |
| `EvaluatePreprocess:152` | `sourceExists ? "R0" : "R0"` | `ModuleReadinessGrading.NotReady` |

마지막 줄은 삼항 양쪽이 같은 값이던 **죽은 분기**였다. 값이 바뀌지 않으므로 동작 불변이며,
상수 치환 과정에서 드러나 함께 정리했다.

`"R0"` 리터럴 잔여 **0건**, `ModuleReadinessGrading` 참조 **9곳**.

## 3. 동작 불변 근거 (카드 합격 조건)

| 근거 | 결과 |
|---|---|
| 치환의 성질 | 리터럴 → **같은 값**의 상수. 구성상 동일 |
| 기존 스위트 무회귀 | 113 → **114**(BP-08 1건 증가), 실패 0 |
| **WPF 앱 솔루션 재빌드** | `dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild` → **경고 0 / 오류 0** |

앱 빌드는 C-13 에서 Gap 으로 남겨 둔 항목이었다. 이번엔 앱 코드를 고쳤으므로 실제로 돌렸다
(`step2-app-build.log`).

## 4. 케이스 5건 — 등급 문자열까지

`DegradedModeReadinessTests`. 각 케이스가 이제 **탐색 결과 → 등급** 두 단계를 모두 단언한다.

| 케이스 | 단언 |
|---|---|
| BP-06 gsvg / 07 enhance_advanced / **08 enhance_basic** / 09 dicom / 10 display | 뺀 모듈: 미해결 **AND 등급 == `NotReady`** · 나머지 모듈: 해결 **AND 등급 != `NotReady`** · 예외 없음 |
| BP-09 보강 | 빈 디렉터리에서도 미해결 + `NotReady` |
| 대조군 | 아무것도 안 뺀 스테이징에서 5개 전부 해결 + `NotReady` 아님 |

즉 C-14 가 "입력" 에서 멈춘 지점을 **출력(R0 문자열)까지** 이었다.

## 5. 반증 실험 (카드 3항)

추출 함수가 실제로 결과를 좌우하는지 확인했다. `Grade` 를 항상 `NotReady` 로 만들고 실행:

```
Assert.NotEqual() Failure: Strings are equal   × 4
```

"나머지 모듈은 정상 등급" 단언 네 건이 무너진다 — 등급이 이 함수에서 나온다는 증거다.
로그: `step3-falsify-grading.log`. 원복 후 재확인(`step4-final.log`).

C-14 의 반증 2회(제거 없음 / 씸 끄기)도 그대로 유효하다 — 그 케이스들은 이 카드에서
단언만 강화됐고 구조는 같다.

## 6. 미검증 (Gaps)

- **BP-08 의 탐색 경로는 대역이다.** 프로덕션 `EvaluateEnhanceBasic` 은
  `XpeEnhanceBasicLibraryLocator` 를 쓰는데, 그 파일이 `XpeEnhanceBasicWrapper` →
  `XpeCommonApi` 를 끌어와 링크가 막힌다(리졸버 이중 등록). 그래서 테스트는 **공용
  로케이터로 부재를 만들고 같은 판정 함수로 등급을 확인**한다.
  **판정은 실제 경로, 탐색만 대역이다.**
  해소하려면 `XpeEnhanceBasicLibraryLocator.DllName` 이 래퍼 상수를 참조하는 한 줄을
  리터럴로 바꿔 그 파일을 무의존으로 만들면 된다(앱 파일 3번째) — 이 카드의 허용 범위를
  넘어 하지 않았다. **별도 판정 사안**
- **스냅샷 객체 자체는 여전히 만들지 않는다.** 등급 문자열은 실제 판정 함수에서 나오지만,
  `ModuleReadinessSnapshot` 의 나머지 필드(Status/Evidence/DegradedMode 텍스트)는 서비스에
  남아 있고 검증 대상이 아니다
- 프로브가 DLL 을 실제로 로드·버전확인하는 단계는 여전히 보지 않는다(자리표시자 사용)
- WPF 앱을 실행해 저하 모드 화면을 보지 않았다 — 빌드만 확인
- CI 실행 결과는 아직 없다
- 네이티브 자리표시자(BP-06~10) 삭제는 Lane A(A-24) 몫, 형제 저장소 탐색은 #129/C-16

## 7. 잔여 위험

- `Grade`/`GradeDiscovery` 는 지금 R0 갈래만 담는다. 나중에 상위 등급 판정까지 옮기고 싶어지면
  모듈별 텍스트가 따라오므로, 그때는 등급과 텍스트를 분리하는 설계가 먼저 필요하다
- `EvaluatePreprocess` 의 죽은 삼항을 정리했다. 원래 의도가 "sourceExists 에 따라 다른 등급"
  이었다면 그 의도는 코드에 없었다 — 값이 같았으므로 이 카드는 **관측된 동작을 보존**했고,
  의도 복원은 별도 사안이다
- 환경변수 기반 주입은 프로세스 전역이다(C-14 §7 와 동일). `parallelizeTestCollections: false`
  가 전제다

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild        # 동작 불변 근거
dotnet test … --filter "FullyQualifiedName~DegradedModeReadinessTests"   # 반증
grep -c '"R0"' clients/ImageProcTest/Services/ModuleReadinessService.cs   # → 0
```
