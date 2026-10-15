# GUI-C-225c — 225b 가 지운 시험 하나를 되살림

`86cd0972`(GUI-C-225b)에서 `DllLoadSmokeTests.cs` 의 구간을 통째로 바꾸다가 `TheLocator_WithNothingToFind_ReportsNotFound_AndNamesTheFoldersItSearched`(REQ-GUI-IT-041 의 로케이터 반쪽)를 함께 지웠다. 225b 보고서와 커밋 메시지는 "로케이터 반쪽은 그대로"라고 적었는데 사실이 아니었다. GUI-C-227(문서 상태 수치) 준비 중, 요구별로 인용한 시험 이름이 실재하는지 스크립트로 전수 대조하다가 발견했다(71개 이름 중 1개 부재). `d918c619` 의 같은 시험을 그대로 되살렸고, 반증(메시지에서 탐색 폴더를 빼면 빨강)을 `falsification_restored_locator_test.txt` 에 남겼다.

이 되살림으로 REQ-041 의 시험은 셋(고정물 부트스트랩, 로케이터 메시지, 직접 로드 예외)이다. 225b 의 반증 표는 고정물·로드 쪽만 다뤘으므로 로케이터 쪽 반증은 이 카드가 처음이다.
