# GUI-C-06 — 무검증 테스트를 Skip 으로 집계 전환

- 카드: GUI-C-06 · Refs #107
- 워크트리: `D:/workspace-github/xpe-gui` / 브랜치 `dev/gui`
- **상태: 완료.** 커밋 `c460da8`(패키지+1건 프로브), `fbd42ef`(나머지 59건). 미푸시
- 합격 조건 4개 전부 충족 (§5)

---

## 1. 결론

DLL 부재 상태에서 **실패 0 / 통과 30 / 건너뜀 60 / 전체 90**.
건너뜀 60은 GUI-C-05 분류값과 정확히 일치하고, 통과 30은 A 27 + C 3 이다.

전환 방식은 중간에 한 번 바뀌었다. 1차 판정의 `Xunit.Sdk.SkipException.ForSkip` 은
**실행 단계에서 실패**했고, 그 반증 위에서 leader 가 판정을 철회해 `Xunit.SkippableFact`
로 재확정했다. 아래 §2 가 그 반증, §3 이 최종 구현이다.

## 2. 1차 판정 반증 — `ForSkip` 은 2.9.3 에서 스킵으로 집계되지 않는다

판정 4("60건 다 바꾸기 전에 1건으로 확인하라")가 잡아낸 것이다.

| 단계 | 결과 | 로그 |
|---|---|---|
| 기준선 | 실패 0 / 통과 90 / 건너뜀 0 | `baseline-test.log` |
| `ForSkip` 로 1건 전환 | **실패 1** / 통과 89 / 건너뜀 0 | `probe4-onecase.log` |

오류 메시지에 `$XunitDynamicSkip$` 센티널이 그대로 노출됐다 — 아무도 이 토큰을 읽지 않았다는
직접 증거다. nuget 캐시 어셈블리 바이트 스캔(utf8/utf16):

| 어셈블리 | `$XunitDynamicSkip$` / `DynamicSkipToken` |
|---|---|
| `xunit.assert 2.9.3` | **있음** (생산자) |
| `xunit.extensibility.execution 2.9.3` | **없음** |
| `xunit.extensibility.core 2.9.3` | **없음** |
| `xunit.runner.visualstudio 3.0.0` | **없음** |

단정 어셈블리는 v2/v3 소스를 공유해 토큰을 **던지는** 코드를 갖지만, v2 실행 엔진도 VSTest
어댑터도 그것을 **읽지 않는다.** 그래서 평범한 실패가 된다.
`dotnet package search xunit` → 2.9.x 는 2.9.0~2.9.3, **2.9.3 이 v2 의 마지막**이라 마이너
판올림으로 빠져나갈 길도 없다.

앞 세션의 바이트 스캔이 찾았던 `CreateTestCaseForSkip` / `GetSkipReason` / `ITestSkipped` 는
정적 `[Fact(Skip=...)]` 경로용이었다. **존재를 동작으로 오귀속한 사례**로 `gate.md` #4 에 기록.

## 3. 최종 구현 (leader 2차 판정)

`Xunit.SkippableFact 1.5.85` (v2 전용, MS-PL, 전이 의존성 `Validation` 1개) 버전 고정 도입.

| 요소 | 내용 |
|---|---|
| csproj | `PackageReference` 1줄만 추가 |
| `SkipHelper.SkipIf(bool, string)` | 내부에서 `Skip.If` 호출. **`Skip.If` 는 이 파일에만 존재**(grep 검증) |
| 어트리뷰트 | `[Fact]`→`[SkippableFact]` 43건, `[Theory]`→`[SkippableTheory]` 2건 = 45 메서드 |
| 축1 `xpe_common.dll` | 36 메서드 / 51 케이스. 사유는 기존 `NativeLibraryFixture.SkipReason`(죽어 있던 API 를 배선) |
| 축2 `xpe_preprocess.dll` | 9 메서드 / 9 케이스. **2차 가드(`TryLoad` 실패)도 함께 전환** — 안 하면 경로는 있는데 로드 실패한 경우가 계속 Pass 로 남는다 |
| C 유형 3건 | **제외 유지**(판정 3). `gate.md` 에 사유 기록 |

`PreprocessCorrectionChainSmokeTests` 에는 사유 필드가 없어 `SkipReason` 을 신설했다
(다른 두 P1A 클래스는 기존 필드 사용). 로드 실패 사유는 경로를 포함한 별도 문자열이다.

**단언 내용은 한 줄도 바꾸지 않았다.** 가드 라인과 어트리뷰트만 교체했다.

`dotnet add package` 가 `<Copy>` 요소를 한 줄로 재포맷한 것은 판정("csproj 는 PackageReference
1줄만")을 벗어나므로 원래 서식으로 되돌렸다.

## 4. 실측 (Evidence)

```
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug

1건 프로브 후: 통과!  실패 0 / 통과 89 / 건너뜀  1 / 전체 90   (probe5-skippablefact-onecase.log)
60건 전환 후: 통과!  실패 0 / 통과 30 / 건너뜀 60 / 전체 90   (final-test.log)
```

스킵 사유가 실제로 기록되며 어느 DLL 인지 명시된다 (`final-test-detailed.log`,
집계는 `skip-reason-tally.txt`): `xpe_common.dll not found …` / `xpe_preprocess.dll not staged …`.

전환 완전성 검증(grep):
- 남은 인라인 조기 return 은 C 유형 3건 + `Dispose()` 1곳뿐 — 전부 의도된 잔존
- `Skip.If` 호출 지점은 `Fixtures/SkipHelper.cs` 단 한 곳
- 빌드 경고 0, 오류 0

## 5. 합격 조건 대조

| 조건 | 결과 |
|---|---|
| DLL 부재 시 건너뜀이 0이 아닌 실제 숫자 | **60** |
| 통과 + 건너뜀 = 90 | 30 + 60 = **90** |
| A 유형 27건은 계속 통과 | 통과 30 = A 27 + C 3 |
| 건너뜀이 60과 다르면 원인 규명 | **60 과 일치** — §6 참조 |

## 6. 함정 조건 — 예고했던 두 지점의 결말

앞 세션이 "숫자가 어긋날 후보"로 지목한 두 곳을, 숫자를 맞추려 조정하지 않고 관측했다.

1. **`[Theory]` 집계 단위 (17 vs 2)** — 러너는 **데이터 행 단위로 센다.** 두 Theory 는
   17 케이스로 집계됐다. 메서드 단위로 접혔다면 총합이 45가 됐을 텐데 60이 나왔으므로,
   행 단위 집계가 관측으로 확인된 것이다. GUI-C-05 의 환산이 맞았다.
2. **C 유형 3건** — 판정 3대로 제외했고, 그래서 통과 30에 남아 있다. 이 3건을 전환했다면
   건너뜀은 63이 되고 수행되던 검증이 보고에서 사라졌을 것이다.

즉 **60은 조정한 값이 아니라 두 축의 분류가 실행 결과와 독립적으로 일치한 값**이다.

## 7. 미검증 (Gaps)

- **축2 의 2차 가드(로드 실패) 경로는 실행되지 않았다.** `DllPath` 가 null 이라 1차 가드에서
  먼저 스킵된다. 로드 실패 사유 문자열이 실제로 보고에 나오는지는 **미관측**이다
  (`skip-reason-tally.txt` 의 `preprocess-load-failed = 0` 이 그 증거)
- **DLL 이 있을 때 이 60건이 통과하는지는 확인하지 않았다.** 카드가 네이티브 빌드를 금지했다.
  무검증에서 벗어나는 것과 통과하는 것은 다른 얘기다 — CI(#98) 몫
- Release 구성 미빌드, VS IDE 미확인, WPF 앱 미실행, E2E·SelfCheck 미실행
- `gui/ImageProcTest.slnx` 쪽 테스트는 이 카드 범위 밖
- 푸시하지 않았다. 커밋 2건은 이 워크트리의 미푸시 유일본이다

## 8. 잔여 위험

- `Xunit.SkippableFact` 는 v2 전용이다. 언젠가 v3 로 가면 패키지를 제거하고 어트리뷰트를
  기계적으로 역치환(`SkippableFact`→`Fact`)한 뒤 `SkipHelper` 본문만 `Assert.Skip` 으로
  바꾸면 된다. 결합면은 어트리뷰트 45곳 + 헬퍼 1곳이며, 전부 컴파일 에러로 드러난다
- 전이 의존성 `Validation` 이 함께 들어온다. 테스트 프로젝트 전용이라 제품 SOUP 는 아니지만
  테스트 도구 목록 기재가 필요하다 (판정 1' 지시)
- 건너뜀 60이 이제 대시보드에 보인다. 이는 문제 해결이 아니라 **문제의 가시화**다.
  실제 해소는 DLL 스테이징(#98)이며, `xpe_common.dll` 만 올리면 51건만 풀리고 9건은 남는다

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet add clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj \
  package Xunit.SkippableFact --version 1.5.85
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
dotnet test … --no-build --logger "console;verbosity=detailed"
dotnet package search xunit --exact-match --format json
# nuget 캐시 어셈블리 바이트 스캔 / 전환 스크립트 (python) — strings 는 이 환경에 없다
```

로그: `baseline-test.log`, `probe4-onecase.log`(ForSkip 실패), `probe5-skippablefact-onecase.log`,
`final-test.log`, `final-test-detailed.log`, `skip-reason-tally.txt`,
(조사 단계) `probe-build.log`, `probe2-build.log`, `probe3-build.log`
