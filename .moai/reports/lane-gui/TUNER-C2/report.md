[2026-10-24 10:15 KST] [xpe-gui] 수신 — 리더 10:15 TUNER-C2 카드 및 Refs #279 범위를 확인했다.
착수 — S의 REQUIREMENTS §2/4.1/4.3/4.4와 52개 정의·GUI·워커 전달·저장 키를 정적 읽기/AST로 대조한다. S는 수정하지 않고, 필요한 수정안/순수 Python 시험은 이 보고서 폴더 patch/에만 둔다. 원본 DLL 출처 문제와 자체 GUI 계약 검증을 분리한다.

## 10:22 정적 대조 결과 — Refs #279

S = `D:/workspace/ProjectGit/PE_analysis/param_tuner` (아래 S 상대경로는 이 절대경로 기준). [#279](https://github.com/holee9/image-processing/issues/279)를 수정 전 읽었으며 TUNER-C2 GUI 범위, 검토사본→Reviewer→리더 반영 순서와 일치한다. 외부 S·설정·결과는 수정하지 않았다. 새 설치/환경/빌드/배포/엔진 실행 없음. `doc/`는 파일명만 확인했으며 별도 문서 승인이나 원본 보유 근거로 삼지 않았다.

### 52 슬롯 / 범위 / 초기값 / 정수 전달

- 상세 [parameter-matrix.md](parameter-matrix.md): Post 40 + LUT 12 저장 슬롯, GUI는 Post 36 + LUT XY 8 = 44행. Post 18/22/27/39 및 LUT W 4개는 고정/숨김. `widgets.py`에는 파라미터 입력 컨트롤이 없고 영상·그래프·카드·스테퍼만 있다. 실제 파라미터 입력은 `app.py:536-564`의 QTableWidgetItem 텍스트이며 슬라이더/명시적 step은 **코드에 없음**. 정수 `int()` 변환은 `app.py:959`, 실행 전달은 1036→1057→Worker:180-198→`tuner_core.py:101,125`.
- 범위/초기값 정의↔행 생성은 **일치**: `param_space.py:150-159`→`app.py:553-554`. Post 범위는 default까지 확장, LUT XY는 0..100; GUI 입력/JSON 재입력 시 추가 확장(`app.py:965,1001-1002`). 이는 명시된 사용자 탐색범위 정책으로 유지하며 엔진의 물리 허용범위로 인증하지 않는다.
- 초기값은 `param_space.py:111-122`의 run_pipeline 상수 참조. 예상 `D:/workspace/ProjectGit/PE_analysis/run_pipeline.py`는 **파일 없음**. Post 22=31/27=0/39=1 외 실제 숫자 default는 미확인; 다른 경로 전체 재검색이나 import로 추정하지 않았다. LUT default 범위 확장도 코드에 없다.
- Chest/비Chest/미사용 체크는 `param_space.py:131-143`, `app.py:640-653`; 체크 가능 플래그(546)로 사용자가 다시 켤 수 있다. 자동 체크 마스크와 강제 실행 금지는 다르다. REQUIREMENTS §4.3의 NR_SELECTION 고정 요구는 §8:132의 탐색대상 변경 기록으로 해소되어 범위 밖 원복을 제안하지 않는다.

### 발견 결함과 최소 검토사본

1. **불일치 / 수정안 있음**: `app.py:957-961`은 비정수·빈칸을 이전 값으로 조용히 대체해 보이는 입력과 실제 실행값이 달라진다. 최소>최대도 자동으로 덮어써 수락하며, `Params.lut_valid()`(:100-103)를 UI에서 검사하지 않는다. `_read_table`은 앞행부터 defs를 직접 변경한다.
   - [patch/app.py](patch/app.py), [patch/app.patch](patch/app.patch): 전체 테이블 정수·정방향 범위·기존 LUT X 계약을 먼저 검사하고 성공 시 defs 일괄 반영. `on_run`의 기존 ValueError 경고 경로 안으로 읽기를 이동. 정당한 initial 범위 확장, 엔진/JSON 정책은 유지.
   - [patch/test_table_contract.py](patch/test_table_contract.py): 실제 AST 추출 `_read_table`/`on_run`/`Params`만 실행. Qt는 셀 접근 최소 어댑터, 초기값은 명시적 fixture; S의 앱/패키지는 import하지 않는다. 원본 첫 6개 시험: 2 통과, 4 메서드 실패(12 failure/subcase). 수정사본 7개 시험: 7 통과. 추가 on_run 거부 경로 시험 포함. 숫자 fixture가 실제 엔진 기본값이라는 주장은 하지 않는다.
2. **불일치 / 정책 판정 필요**: JSON은 52개를 읽지만 `app.py:999-1002`는 44행만 갱신. `_read_table:955`에서 매번 default_params를 새로 만들므로 JSON의 숨긴 8슬롯은 재실행에 보존되지 않는다. 고정슬롯 정책을 임의로 바꾸지 않았으며 C1의 엄격 JSON 검토사본과 통합 판정 필요.
3. **불일치 / C1 분리**: `params_from_dict:170-174`는 길이만 검사하고 float/string/bool을 int 변환하며 범위/LUT/고정 슬롯은 미검사. C1 세션에 소유권 확인 전달; 이 파일 수정안은 C2에 중복하지 않았다.

### 입력 설정 / 비트심도 / dtype

- **일치**: W/H 정수 spin 16..20000 (`app.py:310-316`), bit 콤보14/16(:317-321), 프로토콜의 실제 ObjectType(:635-638), shutter 정수8개(:1006-1011)→JobConfig(:1015-1023)→`tuner_core.py:43-46`의 int32 4×2. Shutter 좌표 경계/점 순서/기하 검사는 **코드에 없음**, 요구사항에는 상세 허용규칙이 없어 이번 수정에서 임의 정책을 넣지 않았다.
- **일치**: rawsize는 14/16 모두2바이트 가정(:18-43). 자동 후보는256..8192·짝수·비율 제한, 수동은16..20000; 후보 없는 수동크기도 `app.py:760`의 정확한 바이트수면 수락하므로 서로 같은 허용범위일 필요는 없다.
- **일치(순서만)**: `app.py:15` bootstrap.init→`bootstrap.py:47-49` PE_BIT_DEPTH 할당 후 pe_post.common import→`app.py:21` run_pipeline import. 변경 시 `app.py:718-731` 설정저장/execv 재시작. CLI/기존 설정값의14/16 허용 검사는 bootstrap에 없음. CLI argv 잘못된 값까지 이번 GUI 입력 패치에 혼합하지 않았다.
- `tuner_core.py:18-23`은 RAW_DTYPE로 읽고 정확한 W×H 개수를 검사한 뒤 PIXEL_DTYPE 변환. 두 상수는 pe_post.common 외부 참조여서 실제14=int16/16=uint16 결론은 **미확인**. `results.py:56` 저장 dtype은14이하 int16/16 uint16 분기임을 정적으로 확인. 원본 DLL·common 상수·엔진 생성 계약 인증과 구분한다.

### 실행 / 중지 / 저장 / JSON 계약

|요구/동작|실제 호출과 키|판정|
|---|---|---|
|1회 처리|app.py:397→on_run(True):1025→Worker.run:192-196→TuneJob.run_single:91-99; 출력/손실만 반환|일치; 단일 처리 저장은 코드에 없음|
|중지|app.py:396→1067-1073→Worker.stop:185-186, TuneJob should_stop:190-191/optimizer:125; 현재 평가 종료 후 best 저장|일치; single 중 stop 버튼 비활성(:1079), 재개 기능 미구현은 REQUIREMENTS §8:138 기록|
|저장 버튼|app.py:386-399의 btn_open은 마지막 결과 폴더 열기, 수동 save 핸들러 없음|§4.4 수동 저장 버튼은 코드에 없음; 자동 저장과 구분|
|자동 저장|tuner_core.py:134-145→results.save_results:38-69|일치(자동 튜닝 완료/중지 best); 단일 실행 분기는 저장 안 함|
|best_params.json|post_param[40],lut_param[12],protocol,protocol_code,object_type,metrics,bit_depth (results.py:45-50)|일치; load는 배열만 사용하므로 metadata 복원 안 함|
|settings.json|tuner_core.py:48-53 src_path,target_path,width,height,bit_depth,protocol,protocol_code,object_type,shutter,eval_scale,weights,opt→results.py:53-54|일치; GUI last_settings와 다른 파일/계약|
|last_settings.json|app.py:699-709 width,height,bit_depth,protocol,body,shutter,known_sizes,max_evals,max_minutes,rounds,weights; load674-697|일치; 입력 경로·table bounds/initial은 저장 안 함|
|나머지 결과|snippet.py; output_post.raw/png; target_post.png; diff.png; history.csv(eval,loss,post0..39,lut0..11)|일치; REQUIREMENTS§4.5의 변경 파라미터 이름 열은 코드에 없음(전체52값 기록)|

### 공통 지시 / 범위 / 검토

- 사용자 제공 AGENTS.md의 Issue-before-edit 및 GUI `codex:` 보고 규칙을 적용했다. #279를 먼저 읽고, ACK→착수→정적대조/로컬순수시험을 이 보고서에 남겼다. 외부 이슈 코멘트·문서 동기화·제품 코드 변경은 하지 않는다. 기존 tracked dirty 5파일은 보존한다.
- REQUIREMENTS 머리말(:3)은 검토대기 초안이며 §8에는 구현 변경이 있다. 카드가 지정한 비교 목표로 사용하되, 이 문서나 시험만으로 의료규제 승인·배포 승인·원본 엔진 동일성을 선언하지 않는다.
- 검토 대상은 이 폴더만. 수정사본은 **미반영**이며 Reviewer 검토와 리더 반영 판정이 필요하다. 원본 DLL/EXE 빌드·원본 커밋/common 계약의 불명은 #279의 별도 미확인 상태로 유지한다.

## [2026-10-24 10:25 KST] 추가 카드 이행 — 실제 작업 사본 수정

- 10:20 추가 카드를 재확인하여 초안에 멈추지 않고 `workcopy/param_tuner/`에 S의 `*.py` 17개 + protocols.json + REQUIREMENTS.md를 **원본 바이트 그대로** 먼저 복사했다. 금지 폴더/last_settings.json 제외. `workcopy/orig-sha256.txt` 19개는 원본과 복사본 SHA256 일치 확인 후 작성했다.
- 원본 사본 기준 커밋: **84889dff** (`TUNER-C2-baseline-Refs-#279`, 사본19개+hash목록). 저장소 ignore에 Python/JSON이 걸려 명시 workcopy 경로만 force-add했다. 저장소 기존 줄끝 규칙에 따라 Git blob의 CRLF→LF 경고가 있으므로 orig-sha256은 **복사 직후 실제 바이트** 기준이며 Git blob SHA가 아니다. 설정은 변경하지 않았다.
- 이후 **실제 작업 사본 `workcopy/param_tuner/app.py`만 수정**했다. 위 patch/는 검토용 동일내용 사본/diff이다. S에는 반영하지 않았다. `patch/test_table_contract.py candidate`는 이제 patch/app.py가 아닌 이 실제 작업 사본을 시험한다.
- 최종 재현: `python -I .moai/reports/lane-gui/TUNER-C2/patch/test_table_contract.py original` → 7시험 중 2통과/5실패 메서드, 총13 failure/subcase. candidate → **7/7 통과**. 원문은 regression-red.txt / regression-green.txt. 프로젝트 전체 GUI/엔진 시험은 카드 금지에 따라 실행하지 않았다.
- `python -I .moai/reports/lane-gui/TUNER-C2/snapshot.py verify` → **원본19파일 불변, 작업사본 app.py만 변경, Python17개 AST 유효, patch와 실제수정 일치**. source-verification.txt 참조. `git diff --check` 통과.
- 공통 경고 `D:/workspace-github/image-processing/.moai/state/codex-msgs/20261022-1040-all-warning.md`의 카드 내 자율수행/범위외 확장금지/레인 push금지를 재확인했다. 이번 회수/수정/시험/보고 범위에서 반영했고 다른 변경은 섞지 않았다.
- Reviewer(`xpe-reviewer`, 01a15dc2-dcf3-7521-971c-e08dd7e74fe1)에 경로와 기준커밋, 실제수정/시험을 세션 통신했다. Reviewer는 다른 리더 작업 수행 중이며 **TUNER-C2를 아직 검토/승인하지 않았음**을 회신했다. 승인으로 오기하지 않으며 리더의 검토 배정/판정에 연결한다. 리더에게도 완료 자료와 미해결 JSON/엔진 범위를 전달한다.
- 최종 staged 검사에서 diff 아티팩트의 빈 context 행/시험 로그 끝 공백을 발견하여 정리했다. app.patch는 zero-context unified diff(`git apply --unidiff-zero` 용)이며 제품 코드 변경은 동일하다. 최종 staged `diff --check`로 다시 확인한다.
