# GUI-C-07 — C# 경고 정책 확인 (Lane C 분)

- 카드: GUI-C-07 · Refs #106
- 워크트리: `D:/workspace-github/xpe-gui` / 브랜치 `dev/gui` / HEAD `f92f4e6`
- **결론: 정책 공백 없음. 코드 무변경.** 단, 관찰 1건을 §4 에 남긴다
- 변경한 코드: **없음**

---

## 1단계 — 현재 정책 전수 기록

`Directory.Build.props` / `Directory.Build.targets` 는 **저장소 어디에도 없다**
(`find` 로 clients/ · gui/ · 저장소 루트 전수 확인). 상속되는 공통 정책이 없다는 뜻이다.

5개 csproj 전수:

| csproj | TreatWarningsAsErrors | WarningLevel | NoWarn | AnalysisMode / EnableNETAnalyzers |
|---|---|---|---|---|
| `clients/ImageProcTest/ImageProcTest.csproj` | 없음 | 없음 | 없음 | 없음 |
| `clients/ImageProcTest.IntegrationTests/…csproj` | 없음 | 없음 | 없음 | 없음 |
| `gui/ImageProcTest/ImageProcTest.csproj` | 없음 | 없음 | **`$(NoWarn);1591`** | 없음 |
| `gui/ImageProcTest.E2E/ImageProcTest.E2E.csproj` | 없음 | 없음 | 없음 | 없음 |
| `gui/ImageProcTest.SelfCheck/…csproj` | 없음 | 없음 | 없음 | 없음 |

즉 **전 프로젝트가 SDK 기본값**이고, 명시 설정은 `NoWarn 1591` 하나뿐이다.

## 2단계 — 경고 실측

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild   → exit 0, 경고 0개, 오류 0개
dotnet build gui/ImageProcTest.slnx     -c Debug -t:Rebuild   → exit 0, 경고 0개, 오류 0개
```

로그: `build-clients.log`, `build-gui.log`(증분), `rebuild-clients.log`, `rebuild-gui.log`(강제 재빌드)

### 이 "0" 이 실제 컴파일 결과인지 확인했다

증분 빌드의 "경고 0" 은 컴파일을 건너뛴 결과일 수 있다 — 이번 세션에서 네 번 나온
"없어 보이는 것"과 "실제로 없는 것"의 혼동 유형이다. 그래서 두 가지를 했다.

1. `-t:Rebuild` 로 강제 재빌드 → 여전히 0개
2. 같은 재빌드에서 억제만 끄면(`-p:NoWarn=`) **1620건이 쏟아진다**(§3)

2번이 결정적이다. 재빌드가 실제로 컴파일을 수행한다는 증거이면서, 그 상태에서 기본 설정의
경고가 0이라는 것도 같은 실행으로 확인된다.

## 3단계 — `NoWarn` 억제가 감추는 것

```
dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild -p:NoWarn=
   → warning CS1591 : 1620건 (다른 경고 코드는 0건)
```

로그: `rebuild-gui-nowarn-off.log`

- **CS1591** = "공개 형식/멤버에 XML 주석 없음"
- 짝이 되는 설정: 같은 csproj 11행 `<GenerateDocumentationFile>true</GenerateDocumentationFile>`
- 억제 대상이 **CS1591 한 종류뿐**이다. 실제 결함(널 참조·미사용 변수·형 변환 등)을 덮고 있지 않다

**은폐가 아니다.** 문서 파일을 생성하되 문서화 누락은 경고하지 않는, 널리 쓰이는 짝 설정이다.

## 4단계 — 판단: "이름값 못 하는 설정" 이 있는가

#106 의 본질은 "옵션이 있는데 무효였다" 이지 "경고가 많다" 가 아니었다. 그 기준으로 보면:

- **`/WX` 에 해당하는 공백은 없다.** C# 쪽에는 `TreatWarningsAsErrors` 자체가 어디에도 없고,
  "켜져 있는데 무효" 인 설정도 없다. **무설정이며, 무설정은 그 자체로 결함이 아니다**
- 다만 한 가지 관찰: `GenerateDocumentationFile=true` + `NoWarn 1591` 조합은 **XML 문서 파일을
  만들지만 그 완성도는 강제하지 않는다.** 실제로 공개 멤버 1620곳이 문서 없이 문서 파일에
  들어간다. "옵션이 켜져 있으나 의도한 효과는 없다" 는 #106 의 형태와 **모양이 닮았다**

후자는 보고만 한다. IEC 62304 문서화 관점에서 의미가 있을 수 있으나 **이 카드가 정할 사안이
아니고**, 켜는 순간 빌드가 1620건으로 깨지므로 별도 판정 대상이다.

## 하지 말라고 한 것 — 지켰다

- `TreatWarningsAsErrors` 신규 도입: **하지 않음**
- `NoWarn 1591` 제거: **하지 않음** (측정은 `-p:` 커맨드라인 오버라이드로만, 파일 무수정)
- 네이티브 모듈: **손대지 않음**

## 합격 조건 대조

| 조건 | 결과 |
|---|---|
| 5개 csproj + Directory.Build.props 설정 전수 기록 | 충족 (props 는 부재를 기록) |
| 두 솔루션의 실측 경고 건수 기록 | 충족 (0 / 0, 강제 재빌드) |
| `NoWarn` 억제 목록과 사유 기록 | 충족 (CS1591 1620건, `GenerateDocumentationFile` 짝) |
| 결론이 "정책 공백 없음" 이면 그대로 보고 | 충족 — 작업을 만들지 않았다 |

## 미검증 (Gaps)

- **Release 구성 미측정.** Debug 만 실측했다. 카드 지정이 Debug 였다
- 분석기(NetAnalyzers) 기본값이 SDK 버전별로 무엇인지 추적하지 않았다. 다만 결과 경고 0이므로
  현재 상태 판단에는 영향이 없다
- `clients/ImageProcTest.IntegrationTests` 는 `clients/ImageProcTest.slnx` 에 포함되어 함께
  빌드됐다고 보고 개별 빌드하지 않았다 — 솔루션 파일 내용을 직접 열어 확인하지는 않았다
- WPF XAML 컴파일 경고가 `-t:Rebuild` 경로에서 전부 재평가되는지는 별도로 확인하지 않았다

## 잔여 위험

- CS1591 1620건은 "지금 정책상 안 보이는" 상태다. 문서화 정책을 강화하는 별도 판정이 나오면
  한 번에 1620건이 표면화된다 — 점진 도입 계획이 필요할 수 있다

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
find clients gui -name "*.csproj" -o -name "Directory.Build.props" -o -name "Directory.Build.targets"
grep -rn "TreatWarningsAsErrors\|WarningLevel\|NoWarn\|AnalysisMode\|EnableNETAnalyzers" --include=*.csproj clients gui
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet build gui/ImageProcTest.slnx     -c Debug -t:Rebuild
dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug -t:Rebuild -p:NoWarn=
```
