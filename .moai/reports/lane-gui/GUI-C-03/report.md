# GUI-C-03 — 솔루션(.slnx) 정비

- 카드: GUI-C-03 (L5). **상태: 완료**
- 워크트리: `D:/workspace-github/xpe-gui` / 브랜치 `dev/gui`
- 선행: GUI-C-01 PASS(둘 다 유지), GUI-C-02 CLOSED(C안 — 개명 보류)
- 변경 파일 2건:
  - `clients/ImageProcTest.slnx` (수정)
  - `gui/ImageProcTest.slnx` (신규)

---

## 1. 결정 — 통합 솔루션이 아니라 앱별 솔루션 2개

lead 위임에 따라 Lane C 가 확정했다. **두 앱을 한 솔루션에 넣지 않는다.**

| 근거 | 내용 |
|------|------|
| GUI-C-01 결론과 정합 | 두 트리는 목적이 다른 독립 앱이다. 솔루션 경계가 그 사실을 그대로 반영해야 한다 |
| 타입 이름 중복 | GUI-C-02 에서 개명을 보류했으므로 두 어셈블리에 `ImageProcTest.MainWindow` 등 동일 수식 타입이 공존한다. 한 솔루션에 넣으면 IDE 탐색·자동완성에서 상시 모호해진다 |
| 플랫폼 설정 불일치 | `clients/ImageProcTest` 만 `Platforms` 미지정(AnyCPU)이고 나머지 4개는 x64 고정. 한 솔루션에 묶으면 솔루션 구성 매핑이 비대칭이 된다 |
| 공유 publish 함정 회피 | GUI-C-02 잔여 위험(같은 폴더 publish 시 산출물 충돌)을 솔루션 경계로 구조적으로 차단한다 |
| 빌드 표면 자립 | 각 레인이 자기 솔루션만으로 빌드·테스트를 완결할 수 있다 |

## 2. 정비 전 / 후

### 정비 전

```
clients/ImageProcTest.slnx   → IntegrationTests.csproj 한 줄뿐 (앱 본체 누락)
gui/                          → 솔루션 파일 자체가 없음
```

5개 csproj 중 **앱 본체 2개가 어느 솔루션에도 속하지 않은** 상태였다.

### 정비 후

`clients/ImageProcTest.slnx`

```xml
<Solution>
  <Project Path="ImageProcTest/ImageProcTest.csproj" />
  <Project Path="ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj" />
</Solution>
```

`gui/ImageProcTest.slnx` (신규)

```xml
<Solution>
  <Project Path="ImageProcTest/ImageProcTest.csproj" />
  <Project Path="ImageProcTest.E2E/ImageProcTest.E2E.csproj" />
  <Project Path="ImageProcTest.SelfCheck/ImageProcTest.SelfCheck.csproj" />
</Solution>
```

5개 csproj 전부가 정확히 하나의 솔루션에 소속된다.

### 범위 확장 고지

지시서의 scope 는 `clients/ImageProcTest.slnx` 였다. `gui/ImageProcTest.slnx` 신규 생성은 그 범위를 넘어선다.
근거: lead 지시 본문의 "**두 앱 본체가 어느 솔루션에도 없는 상태를 정비할 것**" 을 충족하려면 gui 측도 필요하다. 대상 디렉터리는 Lane C 소유(`/gui/`)이며 신규 파일 1건 추가로 되돌리기 쉽다.

---

## 3. 검증 (실행 증거)

SDK: `dotnet 9.0.315` (8.0.420 병존). `.slnx` 는 SDK 9.0.2xx+ 지원.

### 3-1. clients 솔루션 빌드

```
dotnet build clients/ImageProcTest.slnx -c Debug     → exit=0
```
```
ImageProcTest.IntegrationTests -> clients\ImageProcTest.IntegrationTests\bin\Debug\net8.0\ImageProcTest.IntegrationTests.dll
ImageProcTest                  -> clients\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.dll
빌드했습니다.  경고 0개  오류 0개
```
로그: `D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-03/build-clients.log`

### 3-2. gui 솔루션 빌드

```
dotnet build gui/ImageProcTest.slnx -c Debug         → exit=0
```
```
ImageProcTest           -> gui\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.dll
ImageProcTest.SelfCheck -> gui\ImageProcTest.SelfCheck\bin\Debug\net8.0-windows\ImageProcTest.SelfCheck.dll
ImageProcTest.E2E       -> gui\ImageProcTest.E2E\bin\Debug\net8.0-windows\ImageProcTest.E2E.dll
빌드했습니다.  경고 0개  오류 0개
```
로그: `D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-03/build-gui.log`

> AnyCPU 앱 + x64 테스트 혼재가 실제로 솔루션 구성 오류를 내는지 우려했으나, **경고 0개로 통과**했다. 우려는 해소되었다(단 §5 참조).

### 3-3. 통합 테스트 실행

```
dotnet test clients/ImageProcTest.slnx -c Debug --no-build   → exit=0
통과!  - 실패: 0, 통과: 90, 건너뜀: 0, 전체: 90, 기간: 247 ms
```
로그: `D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-03/test-clients.log`

---

## 4. GUI-C-02 미검증 항목 2건 해소 — M4 에 반영 필요

### 4-1. 테스트 수 확정: **90건** (75 도 78 도 아님)

GUI-C-02 에서 소스 어트리뷰트 기준 75건이라 보고했고 README 는 78 이라 기술한다. 실제 실행 결과는 **90건**이다.
차이는 `[Theory]` 의 `InlineData` 다중 확장이다. lead 의 문서 반영 시 **90** 을 쓸 것.

### 4-2. "네이티브 미검증 상태로 조용히 통과" — 가설이 아니라 확인된 사실

GUI-C-02 에서는 코드 구조만 보고 추론했으나, 이번에 실측했다.

```
build/ci-common/bin/**/xpe_common.dll                        → 존재하지 않음
build/default/bin/**/xpe_common.dll                          → 존재하지 않음
clients/ImageProcTest.IntegrationTests/bin/Debug/net8.0/
    xpe_common.dll                                           → 존재하지 않음
```

이 상태에서 **90건 전부 통과, 건너뜀 0건**. `SkipHelper.ShouldSkip()` 이 조기 return 으로 처리하므로 xUnit 은 이를 Skip 이 아니라 **Pass 로 집계**한다.

> 즉 네이티브 DLL 이 없어도 CI 는 "90/90 통과" 라는 **녹색 신호를 낸다.** 건너뜀 카운트조차 0이라 대시보드만 봐서는 무검증 상태를 식별할 방법이 없다.
> GUI-C-02 에서 권고한 `needs: [common-build]` + `build/ci-common` 아티팩트 인계는 선택이 아니라 **필수**다. 없으면 CI 는 검증하지 않으면서 검증했다고 보고한다.

---

## 5. 미검증 (Gaps)

- **Release 구성 미빌드.** Debug 만 검증했다. `-c Release` 및 x64 명시 구성(`-p:Platform=x64`)은 시도하지 않았다.
- **Visual Studio 로 열어보지 않았다.** `dotnet build` 는 통과했으나, VS IDE 가 AnyCPU/x64 혼재 솔루션 구성을 어떻게 매핑하는지는 확인하지 못했다.
- **앱 실행 미수행.** 두 WPF 앱을 기동하지 않았다. 빌드 성공이 실행 성공을 뜻하지는 않는다.
- **E2E/SelfCheck 미실행.** 빌드만 확인했고 `gui/ImageProcTest.SelfCheck` 실행은 하지 않았다.
- **90건의 내용 검증 안 함.** 통과 카운트만 확인했고 어떤 테스트가 네이티브 부재로 실질 무검증인지 개별 분류하지 않았다.

## 6. 잔여 위험

- `clients/ImageProcTest` 만 `Platforms` 미지정(AnyCPU)이다. net8.0 기본값상 x64 Windows 에서 64비트로 기동하므로 현재는 동작하지만, 같은 솔루션의 테스트 프로젝트는 x64 고정이라 설정이 비대칭이다. 별도 카드로 `<Platforms>x64</Platforms>` 정렬을 권고한다 — 이번 카드 범위 밖이라 손대지 않았다.
- 솔루션을 2개로 나눴으므로 "저장소 전체 .NET 빌드" 를 한 명령으로 할 수 없다. CI 는 솔루션 2개 또는 csproj 개별 지정이 필요하다. M4 작성 시 고려할 것.

---

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet --version ; dotnet --list-sdks
dotnet build clients/ImageProcTest.slnx -c Debug
dotnet build gui/ImageProcTest.slnx     -c Debug
dotnet test  clients/ImageProcTest.slnx -c Debug --no-build
ls build/ci-common/bin/*/xpe_common.dll build/default/bin/*/xpe_common.dll   # 부재 확인
git status --short   # M clients/ImageProcTest.slnx / ?? gui/ImageProcTest.slnx
```
