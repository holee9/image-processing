# GUI-C-268 원본 단독 반복 실측

기준 HEAD: 60a88521de73f401f49e9158dc541197bda24799; dev/gui. 실행20회, 실제 시험 분모20.
결과: {'total': 20, 'passed': 20, 'failed': 0, 'skipped': 0}; 관측 실패0/20. 실패 확률0이라는 주장은 아니다.
시작 전/종료 후 소스 unique434파일 해시 불일치0건.
시험명: ImageProcTest.E2ETests.Scenarios.Workflows.StateDisplayScenarios.D258_MetricsFooterAndPipelineOrder_AreVisibleAndDistinct

각 회차 별도 testhost·TRX. 실패 재시도나 대기 증가 없음. 출력 로그 원본은 로컬 baseline/에 보존한다. 아래 SHA256은 원문 stdout의 바이트 해시다.

명령 형식: `dotnet test clients/ImageProcTest.E2ETests -c Debug --no-build --no-restore --filter FullyQualifiedName~D258_MetricsFooterAndPipelineOrder_AreVisibleAndDistinct --logger "console;verbosity=detailed" --logger "trx;LogFileName=red-NN.trx" --results-directory "D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-268\baseline"`

| 회차 | 시작 KST | 종료 KST | exit | Total/P/F/S | stdout SHA256 |
|---|---|---|---:|---|---|
| red-01 | 2026-11-17T02:44:40.0937289+09:00 | 2026-11-17T02:45:39.6833022+09:00 | 0 | 1/1/0/0 | 61bc9e6a0d8cc6c5230dbc938aa174ee152b3731aaa7cdeaec58aaa1d55e71a3 |
| red-02 | 2026-11-17T02:45:39.7540467+09:00 | 2026-11-17T02:46:36.6665933+09:00 | 0 | 1/1/0/0 | 8caac6e5c641259fbc9a9976cf1b64aac6448dd493739116186cf89434a1f83c |
| red-03 | 2026-11-17T02:46:36.7300410+09:00 | 2026-11-17T02:47:33.2595914+09:00 | 0 | 1/1/0/0 | b2cccfd805d133e60d11c3c24452b6a4c77bf0935f99d938aaa381b75e731f69 |
| red-04 | 2026-11-17T02:47:33.3226605+09:00 | 2026-11-17T02:48:28.4780536+09:00 | 0 | 1/1/0/0 | 62d182cd7fbb5f868ffc97eb2f81395e785fb8e3ffa57cbd44b411213538e47c |
| red-05 | 2026-11-17T02:48:28.5421061+09:00 | 2026-11-17T02:49:23.5018016+09:00 | 0 | 1/1/0/0 | 1607ab81331c40626f45e178eb17b8795f629530831f8f92c81282fbd7d2d5c1 |
| red-06 | 2026-11-17T02:49:23.5760999+09:00 | 2026-11-17T02:50:17.8672634+09:00 | 0 | 1/1/0/0 | 4fbde4dac9da841356897a3cff85bb9a5a060a8ccd086122d181f9a055d77c93 |
| red-07 | 2026-11-17T02:50:17.9979734+09:00 | 2026-11-17T02:51:15.4139018+09:00 | 0 | 1/1/0/0 | e6dcbef8edc82dcf8b9d3fd7f7653a40f9af0d3f72d4aae48be3906121ca6506 |
| red-08 | 2026-11-17T02:51:15.4914743+09:00 | 2026-11-17T02:52:13.6121915+09:00 | 0 | 1/1/0/0 | 9caa955d6b6d32d02b1295463869c38c0703cb67a4da5d440af96fc3c370c365 |
| red-09 | 2026-11-17T02:52:13.6719132+09:00 | 2026-11-17T02:53:09.5261145+09:00 | 0 | 1/1/0/0 | ec11db7ab16966f1931ebf89f233fe6b6cc3715044534a21d0f4b96dd4296cae |
| red-10 | 2026-11-17T02:53:09.6607751+09:00 | 2026-11-17T02:54:05.4389521+09:00 | 0 | 1/1/0/0 | 4cc95fe94babbeacb4ff384e67b4f75c136a949ba9b248b00195d3fddd55d195 |
| red-11 | 2026-11-17T02:54:05.5773559+09:00 | 2026-11-17T02:55:01.0926437+09:00 | 0 | 1/1/0/0 | 79f9d76bce6086f6c04aab9c7576eadd4a96f9149960ade18ef635cad9c79150 |
| red-12 | 2026-11-17T02:55:01.1573247+09:00 | 2026-11-17T02:55:55.6018471+09:00 | 0 | 1/1/0/0 | 14b4d22606e5bbd95a28b6a7105065f070f1ee754b02e6f037feb4a55398c4c4 |
| red-13 | 2026-11-17T02:55:55.6664768+09:00 | 2026-11-17T02:57:03.6852990+09:00 | 0 | 1/1/0/0 | 8b96e872aff2e82791a822e5e944d677067d9f9a1f3a407f9066ec3bc039339d |
| red-14 | 2026-11-17T02:57:04.7910718+09:00 | 2026-11-17T02:58:18.0402824+09:00 | 0 | 1/1/0/0 | faf23f793fe60c20395bc5831e3f6c47c9e7d5c535ed92fcba04742fbfff9d8d |
| red-15 | 2026-11-17T02:58:18.1051013+09:00 | 2026-11-17T02:59:10.3282868+09:00 | 0 | 1/1/0/0 | bf657deaf1df11b6fe2257a457e8ccacabdf94bd5a4925205a87a8dca831e947 |
| red-16 | 2026-11-17T02:59:10.3992873+09:00 | 2026-11-17T03:00:02.8932279+09:00 | 0 | 1/1/0/0 | eb66c36d54ed674bc07cdd445d32f397358329df5db18704d91e81dcf391fbac |
| red-17 | 2026-11-17T03:00:02.9595998+09:00 | 2026-11-17T03:00:54.1950471+09:00 | 0 | 1/1/0/0 | 939095c40f441a0fde387097c3df7089336178ef0d4568f5be5527be1a5b3efa |
| red-18 | 2026-11-17T03:00:54.2502867+09:00 | 2026-11-17T03:01:42.9118609+09:00 | 0 | 1/1/0/0 | ad8bc92b4c0f0ca61673d608989a1f3bfa81189272a7872bdb7d8ca46a0908c8 |
| red-19 | 2026-11-17T03:01:42.9808873+09:00 | 2026-11-17T03:02:32.8320537+09:00 | 0 | 1/1/0/0 | 0356e32b6f29064118e5b951d2372f5321c13e0f82ce3643f26099aa645dfaa1 |
| red-20 | 2026-11-17T03:02:32.9039288+09:00 | 2026-11-17T03:03:28.2317985+09:00 | 0 | 1/1/0/0 | fe0492c914bf2500cac086c95c01c2233ef7ca65a2f5fa7b1d442a925f24edc4 |

## CI 관측 근거

run43852576742, 원문 SHA256 `7d858e5c1d0a6085a062de4bc8ec2f6926dc91da8ccc121f093be1de47683291`.
- 원문3450행: 2026-11-16T12:21:58.0149799Z [xUnit.net 00:42:43.54]         D258 visible footer: Chain not run · window not measured | preprocess 3.63 ms · enhance_basic 109.036 ms · gsvg 0 ms · ai_bone_suppress 0 ms | WARN: Preprocessing is on but was skipped: no selected calibration: all stages Off or Auto without maps; executed=[]; input kept The image is shown WITHOUT offset/gain/defect correction.
run43905322843, 원문 SHA256 `8fec5042668b84f701029ac4dd7fdc16e08aade1ec1a72e3e425635357391edf`.
- 원문4064행: 2026-11-16T17:40:36.6488414Z [xUnit.net 00:44:36.14]         D258 visible footer: Chain not run · window not measured | preprocess 3.194 ms · enhance_basic 72.368 ms · gsvg 0 ms · ai_bone_suppress 0 ms | WARN: Preprocessing is on but was skipped: no selected calibration: all stages Off or Auto without maps; executed=[]; input kept The image is shown WITHOUT offset/gain/defect correction.

## 한계

로컬 실패 회차 없음. 개별 UIA 읽기 시각·SetRender 시각 직접 기록 없음. CI 출력 시각은 UI 읽기 시각이 아니다. VM 상태 전환/바인딩 반영/UIA 표현 중 어느 경계인지는 확정하지 못했다. 원인 수정·main CI 성공을 주장하지 않는다.
