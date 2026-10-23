[2026-10-23 17:09 KST] [xpe-gui] GUI-C-255 검증 사실

- 최종 실행 폴더: run-20261023-165511.
- 네 조건의 실제 DPI 집합: s0_gf/s1_um300 각각 96 또는 144. 각 조건에 앱 종료 0/잔여 0/CLI 비교 equal=true.
- 각 GUI/CLI 파일 31,457,280바이트. 같은 세트의 두 DPI 결과 SHA-256도 같다.
- 음성 대조: 동일 크기의 다른 세트 GUI 결과와 비교하여 equal=false 및 종료 1을 확인했다.
- SHA-256 인덱스 125개를 재검산: 불일치 0. 원본 RAW·보정 자료 52개를 manifest와 최종 대조: 불일치 0.
- PNG 56개, 전경 앱 소유가 아니어서 최종 screen PNG 0개. PrintWindow PNG의 본문 흰색 비율 99% 초과 56개. 시각적 사용성 합격 판정 없음.
- 금지 입력 grep: 지정 C#/PowerShell 파일 5개에 Mouse./Keyboard./SendInput 검색. 종료 1, 출력 없음, 일치 0.
- verify.ps1 종료 0: OWNED_GUI_REMAINING=0 (2026-10-23T17:07:08.4286119+09:00).
- Windows Display UIA 복원 선택: 100%(권장), RESTORE_SCALE_EXIT=0.
- 내부 도구 코드 검토의 종료 코드/비동기 오류/캡처·DPI 확인 보완 후 최종 네 조건 재실행. 정식 QA/Reviewer 판정은 별도.
