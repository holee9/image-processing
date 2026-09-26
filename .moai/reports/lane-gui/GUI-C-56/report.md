# GUI-C-56 — 왜 가끔 `Win32` 공급자를 받는가 (#136)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-56 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **C-51·C-52 의 재획득 동작과 `Skip` 처리는 손대지 않았다**(카드 금지 사항).
- **결과: Native 3회 0 실패 · Mock 0/47/1/48 · 통합 0/180/1/181 · slnx 0경고 0오류**

---

## 1. 주장 (Claim)

**두 요소는 같은 HWND 다.** 창이 둘이 아니다 — 카드의 결정적 질문에 답이 나왔다.

**원인은 요소가 만들어진 시점이다.** 기동 시 받은 요소 **인스턴스 하나**가 망가져 있고, 같은
HWND 를 **어떤 경로로든 잠시 뒤에 다시 만들면 언제나 정상**이다. 경로 차이도, FlaUI 의 캐시도
아니다 — 둘 다 대조로 배제했다.

**빈도(표본 포함): 기동 76회 중 24회**(≈ 3회 중 1회). Native 기준. C-50 의 Mock 프로브 60회 중
3회(5 %)보다 뚜렷이 높다.

**배제 목록**(다음 사람의 출발점):

| 가설 | 판정 | 근거 |
|---|---|---|
| 창이 둘이다 | **배제** | 관측 전건에서 `launched == reacquired` HWND, 프로세스 최상위 창 1개 |
| 시간이 지나면 낫는다 | 배제(C-50) | 같은 요소에 12만 회 재시도 실패 |
| **획득 경로가 다르다** | **배제** | 같은 순간 `FromHandle`·트리 검색 **둘 다 성공** |
| **FlaUI 가 망가진 요소를 기억한다** | **배제** | **같은 호출** `GetMainWindow` 재실행이 9/9 성공 |
| 앱 폴더의 네이티브 DLL 그림자(#129) | **배제** | 있을 때 7/24, 없을 때 9/24 — 빈도 안 바뀜 |
| C-36 형 동일 exe 두 인스턴스 경합 | **미검증** | §5 |

**메커니즘(가설, 측정 아님)**: UIA 요소는 생성 시점에 응답한 공급자에 묶이고, WPF 자동화 피어가
아직 등록되기 전에 만들어진 요소는 HWND(Win32) 공급자에 영구히 묶인다. **측정된 것은 "늙은
요소는 낫지 않고 새 요소는 항상 정상" 까지**이고, 공급자 바인딩 자체는 보지 못했다.

## 2. 증거 (Evidence)

### (a) 결정적 질문 — 같은 HWND 인가

노트에 두 핸들과 프로세스의 최상위 창 수를 실었다. 배치 A·B 의 재획득 전건:

```
did not support AutomationId (framework=Win32); re-located it by process id 8136.
  hwnd launched=0x1EB80396 reacquired=0x1EB80396 topLevelWindowsForProcess=1
… (관측 전건에서 launched == reacquired, topLevelWindowsForProcess=1)
```

### (b) 세 경로 동시 프로브 — 기동 32회

```
14  XPE-C56-PROBE launched=PropertyNotSupportedException fromHandle=ok fromTree=ok framework=Win32
18  XPE-C56-PROBE launched=ok                           fromHandle=ok fromTree=ok framework=Wpf
```

**같은 HWND·같은 순간에** `FromHandle` 로 새로 만든 요소도, 데스크톱 트리에서 찾은 요소도
**14/14 전부 정상**이다.

### (c) 같은 호출 재실행 — 기동 32회

```
 9  … launched=PropertyNotSupportedException fromHandle=ok fromTree=ok secondCall=ok framework=Win32
23  … launched=ok                            fromHandle=ok fromTree=ok secondCall=ok framework=Wpf
```

`secondCall` 은 **망가진 요소를 만든 바로 그 호출**(`GetMainWindow`)을 잠시 뒤 다시 한 것이다.
**9/9 성공** — FlaUI 는 기억하지 않는다. (비용을 남기지 않으려고 이 열은 측정 후 제거했고,
무엇을 봤는지는 프로브의 주석에 적었다.)

### (d) 조건 대조 — 앱 폴더 네이티브 DLL

| 배치 | 조건 | S05 | 재획득이 난 실행 | 결함이 난 기동 |
|---|---|---|---|---|
| A | 앱 폴더에 DLL 있음 | **8/8 실패** | 5/8 | 7 / 24 |
| B | DLL 치움 | 8/8 통과 | 7/8 | 9 / 24 |

### (e) 최종 검증 (verbatim)

```
Repeat-E2E.ps1 -Times 3 -Backend Native → runs with a failure: 0 / 3
run 1 74.4s · run 2 75s · run 3 73.6s
  (이 3회의 프로브: 기동 12회 중 1회 launched=PropertyNotSupportedException)

dotnet test …E2ETests… --no-build                       (Mock)
통과!  - 실패: 0, 통과: 47, 건너뜀: 1, 전체: 48 (15 s)

dotnet test …IntegrationTests… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug
    경고 0개 / 오류 0개
```

## 3. Baseline 귀속

- 프로브 표본 **76 기동** = 8회×4 (배치 C) + 8회×4 (배치 D) + 3회×4 (최종). 기동 1회 = 픽스처
  1개 생성이고, 한 실행에 픽스처 4개(Application · Workflow · WindowReacquire 2개)가 뜬다.
- 배치 A·B 의 "결함이 난 기동 n/24" 는 **노트 기반**이라 분모가 실제 결함이 보고될 수 있는
  3 기동/실행(모사 방아쇠 픽스처 제외)이다. 프로브 기반 수치와 분모가 다르다 — 섞어 읽지 말 것.
- E2E 건수는 C-55 시점 48건 그대로(Mock). 통합 181 불변.

## 4. 이전 진술 정정 1건

조사 중간에 **"`framework=Win32` 는 결함의 판별 기준이 아니다"** 라고 판단했다. 노트에
`framework=Wpf` 인 재획득이 5건 있었기 때문이다. **틀렸다.** 그 5건은 전부
`UnreadableWindow_IsReplaced_AndTheRunSaysSo` — **방아쇠를 흉내 낸 테스트**이고, 거기서는
읽기가 실제로 실패한 적이 없다. 실제 결함에서는 프로브 76건 전부 `Win32` ↔ 실패가 일치한다.
**C-50 의 관찰은 유효하다.**

(같은 이유로 그 테스트의 클래스 주석에 적힌 "여기서는 `framework=Wpf` 로 보고된다" 는 서술도
절반만 맞다 — 모사 실행에서도 `Win32` 로 찍히는 경우가 있다. 카드가 그 테스트를 건드리지 말라고
했으므로 고치지 않고 여기 적어 둔다.)

## 5. 미검증 (Gaps)

- **공급자 바인딩 메커니즘을 직접 보지 못했다.** §1 의 메커니즘은 가설이고, 측정된 것은 요소의
  나이와 결과의 대응뿐이다. 앱 안쪽(자동화 피어 등록 시점)에는 계측을 넣지 않았다.
- **FlaUI 의 `GetMainWindow` 가 내부적으로 어떤 API 로 요소를 만드는지 소스로 확인하지 않았다.**
  동작으로만 갈랐다.
- **C-36 형 경합(동일 exe 두 인스턴스)은 재지 않았다.** 스위트는 컬렉션 단위로 직렬화되어 있어
  기회가 드물지만, 없다고 보이지 않았다.
- **Mock 백엔드에서 다시 재지 않았다.** C-50 의 5 % 는 다른 프로브·다른 조건의 수치이고, 이번
  76건은 전부 Native 다. **"Native 에서 더 자주 난다" 는 비교는 하지 않는다** — 프로브가 다르다.
- **기계 1대·세션 1개**의 값이다. CI 에서의 빈도는 모른다.
- **고치지 않았다.** 요소를 늦게 만드는 것이 해법으로 타당한지, 기존 재획득으로 충분한지는
  리더 판단이다.

## 6. 잔여 위험 (Residual risk)

- **프로브는 기동마다 UIA 조회 2건을 더 한다.** 실측 벽시계는 배치 간 71~75 s 범위로 변화가
  보이지 않았지만, 공짜는 아니다.
- **프로브 출력은 실행 단위 StdOut 으로만 남는다**(테스트에 붙지 않는다). trx 를 보존하지 않는
  환경에서는 사라진다.
- **앱 폴더의 네이티브 DLL 5개를 세션 스크래치로 옮겨 두었다**(§2d). 그 상태라야 S05 가 통과한다
  (#129 의 알려진 성질). 앱을 `XPE_NATIVE_DIR` 없이 직접 띄우면 이제 네이티브를 못 찾는다 —
  되돌리려면 스크래치의 `appfolder-native/` 를 도로 복사하면 된다.
- **결함 자체는 그대로 있다.** 이 카드는 원인을 좁혔을 뿐이고, 증상 대응(C-51 의 재획득)이
  여전히 유일한 방어선이다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 8 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c56{,b,c,d}
grep -ho 'XPE-C56-PROBE[^<&]*' build/e2e-c56c/*.trx | sort | uniq -c
dotnet test clients/ImageProcTest.E2ETests/… --no-build           # Mock
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
dotnet build clients/ImageProcTest.slnx -c Debug
```
