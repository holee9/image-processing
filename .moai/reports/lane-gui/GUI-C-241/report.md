[2026-10-14 16:13 KST] [xpe-gui] GUI-C-241 보고 — PR #266 첫 CI 빨강 2건 (run 38523309095): NativeCallWrapperGuardTests, SelfCheck 시나리오 10 (Refs #251)

푸시하지 않았다. 기준 `c6d9028e`. 변경: `clients/ImageProcTest.IntegrationTests/Functional/NativeCallWrapperGuardTests.cs`(면제 목록 1줄), `gui/ImageProcTest.SelfCheck/LifetimeScenarios.cs`(시나리오 10 의 한 구간). 제품 코드 변경 없음.

## 1. NativeCallWrapperGuardTests — 면제 목록 (GUI-C-238 이 부른 빨강)

- **재현**: 로컬에서 그대로 빨강 (`guard_red.txt`): `RealXpeBackend.cs:609/618/628 in 'WhyUnusable'` 의 `NativeLibrary.TryLoad / TryGetExport / Free`. 내 쪽 행 번호가 리더가 본 통합 HEAD 와 같다. **왜 로컬에서 못 봤나**: 238b 때 통합 시험을 이름 필터(`NativeSearch|Oracle|…`)로 골라 돌렸고 이 시험은 그 필터에 없었다. 이번엔 전체를 돌렸다(아래).
- **원래는 어떻게 면제됐나**: 이 시험의 면제 목록에 `HasExports`("static export probe used before an instance exists (CanUseNative)")가 있었다. 238 에서 그 함수를 `WhyUnusable` 로 **개명하면서**(이유를 돌려주도록) 면제 항목의 이름이 낡았다 — 면제가 사라진 것이지 새로 생긴 호출이 아니다.
- **결정: 감싸지 않고 면제**. 근거는 원래 면제와 같다: 이 함수는 정적이고(백엔드 인스턴스가 만들어지기 전, 팩토리와 `CanUseNative` 가 부른다), DLL 을 불러 내보내기만 확인하고 같은 함수 안에서 `Free` 한다. 알림 대기열을 가진 인스턴스가 없으므로 비울 알림이 없고, `InvokeNative`(인스턴스의 `NativeAlertDrain`)로 감쌀 수도 없다. 면제 항목을 `WhyUnusable` 로 바꾸고(낡은 `HasExports` 항목 제거) 이유를 시험 파일에 적었다. 면제의 범위는 그 한 멤버이고, 같은 파일의 다른 멤버가 래퍼 밖에서 부르면 여전히 빨강이다.
- **빨강 → 초록**: `guard_red.txt`(실패 1, 통과 1) → `guard_green.txt`(통과 2).

## 2. SelfCheck 시나리오 10 — 시나리오의 경합 (제품 결함 아님)

- **가름**: 실패 문장 `a running wait holds 0 subscriptions, expected 1` 는 `WaitForCompositionTicksAsync(2, 120 ms, …)` 를 **호출한 직후** 구독 수가 1 이라고 단언한 줄이다. 그 대기는 제한 시간이 120 ms 라서, 호출과 확인 사이에 이 스레드가 120 ms 넘게 멈추면(CI 러너의 부하, 첫 호출의 JIT 와 WPF 형식 로드) 타이머가 먼저 대기를 끝내 구독이 이미 풀려 있다. 제품 코드는 그 경우에도 옳게 푼다(풀리는 것이 올바른 동작). 구독을 안 거는 경로가 아니라, **구독이 걸린 시점을 시간에 맡긴** 시나리오의 가정이었다.
- **재현**: 로컬 단독 실행은 통과였다. 실행기 6개를 **동시에** 돌리자(각자 출력 폴더 사본에서, 파일을 옆에 쓰므로; 모두 유한한 작업이고 `timeout 300` 이 바깥에서 묶는다 — 남는 부하 없음) 12개 중 **2개가 CI 와 똑같은 문장으로 실패**했다(`parallel_summary.txt`).
- **수정**: 확인하는 구간에서 시간 의존을 없앴다. ① 타임아웃 경로는 올 수 없는 틱 수(`int.MaxValue`)를 요청해 제한 시간만이 유일한 출구가 되게 하고(결과가 `TimedOut` 임을 단언), 그 대기가 **돌고 있는 동안**의 구독 수는 단언하지 않는다. ② "돌고 있을 때 구독 1" 은 이미 있던 30 초 제한 대기(시나리오만 끝낼 수 있음: 취소로 끝냄)에서 단언한다 — 이쪽은 어떤 지연으로도 먼저 끝나지 않는다. 취소·이미 취소된 토큰 경로는 그대로.
- **같은 부하에서 반복**: 수정 뒤 6개 동시 × 3회 = 18 + 6 × 5회 = 30, 모두 통과 (**48/48**, `parallel_summary.txt`). 수정 전 같은 부하에서는 2/12 실패.

## 로컬 전체 실행 (커밋 전)

- 통합 시험 전체: 통과 **964**, 실패 **1**, 건너뜀 2, 전체 967 (`integration_full.txt`). **실패 1건은 GUI-C-240 의 새 인용 시험**이다 — `tools\Get-CiNativeDlls.ps1` 이 `dev/gui` 에는 없다(240 보고서가 "#266 의 파일이 있어야 통과"라고 먼저 알린 그 의존이다; 통합 브랜치에서는 초록). 건너뜀 2건은 이 카드와 무관한 기존 건너뜀(`DllSearchPathSafetyTests…FileLink…`, `BaselineFingerprintProbe…`).
- SelfCheck 전체: 30 시나리오 통과, 종료 코드 0 (`selfcheck_full.txt`).
- 이 카드의 두 시험(`NativeCallWrapperGuardTests` 2/2, 시나리오 10 포함 SelfCheck)은 위처럼 초록이다.

## 한계
- 실행기를 동시에 돌린 부하는 CI 러너와 같지 않다. 같은 실패를 재현했다는 것까지가 관측이고, 수정 뒤 48/48 은 "이 부하에서 안 터진다"이지 "어떤 러너에서도"가 아니다. 시나리오가 더는 시간에 기대지 않는다는 것이 근거다.
- 가드 시험의 면제는 `WhyUnusable` 가 앞으로도 인스턴스 없이 로드·확인·해제만 한다는 것에 기댄다. 그 함수에 알림을 만드는 호출이 들어가면 이 면제는 거짓이 된다(시험은 그것을 잡지 못한다).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 1 NativeCallWrapperGuardTests 빨강 — 면제 근거 결정·기록, 로컬 빨강→초록 | GUI-C-241 |
| 2 SelfCheck 시나리오 10 — 결함인지 경합인지 가름, 결정적으로 수정, 부하에서 반복 | GUI-C-241 |
| 로컬 전체 실행 후 커밋(통과 수 보고) | GUI-C-241 |
