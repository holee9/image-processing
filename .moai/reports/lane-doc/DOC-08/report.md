[2026-10-21 18:34 KST] [xpe-reviewer]
# DOC-08 — 2G·신규 PRE 평가 자료조사 (읽기 전용) — Refs #278

지시 PRE-EVAL-20261021-1823. 리더 우선순위 변경(18:31)에 따라 **2번(SDK 사용 조건 문서 후보)을 먼저** 적는다. 1번·3번은 이어서 같은 파일에 추가한다.

조사 방법: 파일 이름·디렉터리 목록, PDF 본문 텍스트 추출(`pdftotext -layout`), 헤더 파일 주석 읽기. DLL·실행 파일 로딩, 장비 접속, `param/*.par`·시리얼·키 열람, 원본 수정, push 는 하지 않았다.

---

## 2. SDK 사용·재배포·오프라인 사용 조건 문서 후보 — `Z:/공통자료/Software/2G/`

### 결론: **[근거 없음]** — 사용·재배포·오프라인 사용 조건을 적은 문서가 `Z:/공통자료/Software/2G/` 아래에 **없다.**

### 2.1 폴더 전체 목록 (파일 이름 수준, 전수)

`Z:/공통자료/Software/2G/` 에는 하위 폴더 하나뿐이다: `SDSDK2_abyz_ver_2.3.0.3/`.

| 경로 | 종류 | 사용 조건 문서인가 |
|---|---|---|
| `SDSDK2_programmer's_reference.pdf` | API 참조 문서(Doxygen 1.8.17 생성, 123쪽) | 아니다 — 2.2 참조 |
| `inc/SDSDK2_abyz.h` | C 헤더 | 아니다 — 2.3 참조 |
| `lib/X64A/`, `lib/X64W/`, `lib/X86A/`, `lib/X86W/` | 각각 `libxd2.dll`, `libxd2.lib` | 바이너리. 열지 않음 |
| `param/S4335-WA.par`, `param/S4335-WF.par`, `param/S4343-WA.par` | 매개변수 파일 3개 | 지시대로 내용을 열지 않음. 이름으로는 검출기 모델 매개변수다 |

`LICENSE`, `EULA`, `README`, `NOTICE`, `COPYING`, `*.txt`, `*.doc*`, `*.rtf`, `*.html` 에 해당하는 파일은 **하나도 없다** (`ls -R` 전수 목록, 위 표가 전부).

### 2.2 PDF 본문 검색 결과 — `SDSDK2_programmer's_reference.pdf`

- 목차(PDF i~ii쪽): `1 Overview / 2 Common Examples / 3 Release note / 4 Class Index / 5 File Index / 6 Class Documentation / 7 File Documentation / Index`. 라이선스·사용 조건 장이 **없다.**
- 전체 본문(추출 텍스트 4,930줄)에서 `license`, `licence`, `redistribut`, `copyright`, `confidential`, `proprietary`, `entitle`, `offline`, `terms of use`, `agreement`, `rights`, `warrant`, `disclaim`, `permission`, `prohibit`, `shall not`, `must not`, `NDA` 를 대소문자 무시로 검색 → **사용 조건 의미의 일치 0건.** (`rights`/`permission` 등은 구조체 필드 이름 `BrightSet`, `RequiredVariation` 같은 부분 일치만 나왔고, 조건 문구가 아니다.)
- Overview(PDF 13쪽/본문 1쪽) `Prerequisites` 절은 하드웨어·OS·컴파일러 사양(Intel i5, 2 GB RAM, Windows 7, Gigabit Ethernet, 802.11ac WiFi, Visual Studio 2019)만 적는다. 사용권·배포 조건이 아니다.
- Release note(PDF 25쪽/본문 13쪽): `version 2.3.0.3, 2022-09-15 — SDSDK initial version. Firmware version: 2.0.0.0.0`. 조건 문구 없음.
- 검색의 대조군: 같은 검색 방식으로 `Minimum`(본문 438줄), `Recommendation`(1355줄), `timeout`(1911줄)은 잡혔다. 검색이 파일을 읽고 있었다는 뜻이다.

### 2.3 헤더 파일 — `inc/SDSDK2_abyz.h`

- 머리말 주석은 `/// SDSDK2, Samsung detector system development kit for the second generation.` 한 줄뿐이다(1~3줄).
- 파일 전체에서 `licen`, `copyright`, `(c)`, `all rights`, `confidential`, `proprietary`, `redistribut` 검색 → **0건.**

### 2.4 오프라인(검출기 없이) 사용 조건

- PDF·헤더 어디에도 "검출기 없이 보정(calibration) API 를 쓸 수 있다/없다" 는 문장이 없다. 이미 `leader-2g-contract.md` 계약 표 `Product scope/init` 행이 같은 결론이다(Overview: "APIs to manipulate the connected 2G detector"). 이번 조사로 새로 추가된 근거는 없다.

### 2.5 범위 밖이지만 적어 두는 것 (제안이지 배정이 아님)

- 부모 폴더 `Z:/공통자료/Software/` 에는 형제 폴더 `AXD/`, `EngineeringTool/`, `FPGA/`, `Firmware/`, `SDK/`, `Setup/`, `iMX/` 가 있다. 카드가 지정한 범위는 `2G/` 아래뿐이라 열지 않았다. `SDK/` 는 `v_0_1_0_3/ (AbyzSdk.dll, AbyzSdk.xml, AbyzSdk_doc_v_0_1_0_3.zip)`, `v_1_0_0_1/ (AbyzSdk.dll)`, `v_1_0_0_2/ (AbyzSdk.dll)` 로 자사 SDK 로 보이며 2G(Samsung SDSDK2) 와 다른 제품이다. 2G 사용 조건 문서가 거기 있을 근거는 없다.
- 결론적으로 **2G DLL 의 사용·재배포·오프라인 사용 권한은 이 공유 폴더의 문서로는 판단할 수 없다.** 판단하려면 벤더(Samsung) 또는 구매·계약 담당자가 보관하는 계약·사용권 문서(비밀이 아닌 부분)가 필요하다. 이 조사는 "허용" 도 "금지" 도 뒷받침하지 않는다.

---

## 1. 108장의 근거 — 최초 검증용 장 수와 평소 운용 최소 장 수

[2026-10-21 18:38 KST 추가]

### 1.1 결론 요약

| 행 | 값 | 판정 | 근거 파일:줄 |
|---|---|---|---|
| (a) 최초 검증용 **108장** (정정 2026-10-21 18:43, 리더 지적) | 140장 묶음 중 **생성 목록에 들어가는** 부분: 다크 **33장 = 보관용 첫 장 D000 1 + 실제 사용 32** + 밝은 영상 75장(3단계 × 25) = 108. 140 − 독립 평가용 32 = 108 | **[확인됨]** 산술은 문서로 확인. 다만 "108" 이라는 숫자 자체는 10-20 저수량 보고서들에서 처음 쓰였고, 그 이전 문서에는 140 과 역할별 분해만 있다 | `xpe-pre/.moai/reports/lane-pre/PRE-GUI-20261019T063546Z-7f92a1/acquisition-guide-20261019-doc/acquisition-guide.ko.html:30` (원본 HTML 줄; 앞서 적은 44~45 는 태그 제거 텍스트 기준이라 틀림): "Dark 41 + Bright 99 = 140장 / 맵에 실제 쓰는 Dark 32 / Gain 25 / Defect Bright 75. 독립 평가용 총32장"; 같은 파일 `:130` "dark-gui 목록: D000 + D001–D032의33개 전체 경로", `:131` "D000이 한 번 제외되어32장만 사용됩니다"; 같은 폴더 `mail-draft.ko.txt:7`: "Dark41장(첫1보관·제외+생성32+별도평가8), Bright99장(3단계각생성25+별도평가8), 총140장"; `xpe-pre/.moai/reports/lane-pre/PRE-LOWCOUNT-20261020-leader/final-optimization-report.md:116`: "과거 생성108안 Dark33장"; 같은 파일 `:110`: "108생성 대41생성, 140전체 대73전체의 분모를 구별"; `:64`: "140/108/107/32는 과거 취득 계획이며 실제 확보된 stack이 아니다"; `PRE-LOWCOUNT-20261020-DOC-01a15359/research.md:9`: "140/108은 이전 운영안의 비교 숫자 … 107+32/108" |
| (a″) **108 과 107 의 차이** | **107 = 맵에 실제 쓰는 장 수** = 다크 32 + 밝은 75 (`acquisition-guide.ko.html:30` 의 "맵에 실제 쓰는 Dark 32 / Gain 25 / Defect Bright 75", Gain 25 는 Defect 75 안의 L2 와 공유라 더하지 않음). **108 = 107 + D000 1장** — D000 은 생성 목록(`dark-gui`)에 들어가되 도구가 "첫 다크 한 번 제외" 규칙으로 빼는 보관용 프레임(`:130~131`). 140 = 108 + 평가 32(다크 8 + 밝은 3×8) = 107 + 32 + 1 | **[확인됨]** | 위 인용 줄; `research.md:9` 의 "107+32/108" 표기가 같은 구분 |
| (a′) 108 의 상위 출처 — "3단계 × 25장" | 리더가 2026-10-16 보낸 결함 화소 맵 2판 촬영 요청서의 수량 | **[확인됨]** | `xpe-reviewer/.moai/project/bpm-v2-acquisition-request.md:16`: "다크 16장 이상 (가능하면 32장)"; `:17`: "5단계면 단계마다 17장 이상(3단계면 25장)" (초판 `58ea39c0`:17 은 "단계마다 8장 이상, 3~5단계", `e4ceb03b` 에서 17/25 로 올림); 취득 안내서 본문 47줄: "기존 BPM 요청에 있는 3단계×25 구성을 선택했습니다" |
| (b) 평소 운용 **최소** 취득량 (현행 1부 절차) | 다크 **최소 5장**(첫 장 포함 목록, 도구가 첫 장 1회 제외) + 게인용 평탄 영상 **한 선량 최소 5장** + 결함 맵용 밝은 영상 **최소 10장** (게인용과 공유 가능) → 역할 합계 최소 20장, 공유하면 15장 | **[확인됨]** | `image-processing/docs/calibration/IAP-CALIB-001_Image_Acquisition_Protocol.md:36~38`; `:67` "`--bpm-bright` 를 생략하면 게인용 평탄 영상을 결함 맵에도 쓴다(이때는 10장 이상 필요)"; `:69` "`--keep-first-dark` … 최소 장 수는 5장" |
| (b′) 같은 뜻의 다른 표면 | SAD 기본값 `min_frames_dark 5`, `min_frames_bright 10`; cyan_test `fixture.json` 검증 목표 `min_dark_frames 10`, `min_bright_frames 20`; SRS FUNC-024 단계: 최소 1 / 표준 5~10 / 권고 15~20 / 우수 25~30 | **[확인됨]** (서로 수치가 다르다 — 표면마다 "최소" 의 뜻이 다름: 도구 거부 하한 / 픽스처 검증 목표 / 요구 단계) | `docs/calibration/SAD-CALIB-001_Software_Architecture_Document.md:273~274`; `tests/test_data/cyan_test/fixture.json:49~50`; `docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md:166` |
| (c) 2G SDK 가 정한 최소 장 수 | 없음 | **[근거 없음]** — PDF 본문·헤더에 `minimum`/`at least`/`number of frames` 로 잡힌 것은 하드웨어 전제(Intel i5 등, 438줄)와 타임아웃 60초(1911줄)뿐. `TSDFrameSet` 의 32 는 경로 배열 용량이지 권장 장 수가 아니다 | `scratchpad sdk2g.txt`(PDF 텍스트) 438·1911줄; `leader-2g-contract.md` 계약 표 `FrameSet/BPM struct` 행 |

(a) 와 (b) 는 **다른 양**이다. (a) 는 결함 화소 맵 2판 검증을 위해 2026-10-19 한 번 계획한 **확대 취득 묶음(140장) 중 생성 입력 108장**이고, (b) 는 현행 절차서 1부가 운영자에게 요구하는 **도구 하한**이다. 취득 안내서 본문 299줄도 "140장은 이번 권장 패키지 구성이지 기존 GUI 실행 최소 장수나 모든 상황의 새 촬영 의무가 아니다" 라고 적고 있고, `coordination.md:79` 도 "Existing 108-frame campaign is not a recurring field requirement" 라고 같은 구분을 한다.

### 1.2 108 이 **없는** 곳 (검색 범위 명시)

다음 범위에서 `108` 을 장 수 의미로 검색했고 **0건**이다(숫자 `108` 이 섞인 다른 값, 이슈 번호, 시험 통과 수 `108/108`, 메모리 108 MB 는 제외):

- `image-processing/docs/calibration/*.md` (SRS·SAD·IAP·TDS·RTM·PRD 포함), `docs/quality-eval/*.md`, `docs/post-processing/xpe/*.md`
- `tests/test_data/cyan_test/README.md`, `tests/test_data/CalData_6/README.md`
- `#278` 본문과 코멘트 전체(`gh issue view 278 --comments`, 154줄) — `108` 0건, "최소 입력" 은 103줄의 G-2G-READY 최소 입력(계약·사용권 문서·생성 기록)이라 장 수가 아니다
- 2G SDK PDF 전체 텍스트 — `108` 은 색인(INDEX) 쪽 번호 1건(4654줄)뿐
- `image-processing/.moai/lanes/{pre,post,gui,doc}/inbox/*.md` — 장 수 의미의 108 은 DOC-08 카드 자신뿐

즉 108 은 SRS·절차서·QA 카드·이슈에서 나온 수가 아니라, 10-19 취득 안내서의 140 분해를 10-20 저수량 보고서들이 "생성 108 / 전체 140" 으로 다시 부른 수다.

### 1.3 TDS 의 "최소 수량" 은 별개의 표

`docs/calibration/TDS-CALIB-001_Test_Dataset_Specification.md:835~847` 의 §6.1 "최소 수량 요구사항" 은 알고리즘별 **시험 데이터셋** 요건(합계 1,030 프레임)이고, 운영 취득량이 아니다. 108 과 무관하다.

---

## 3. 기존 `_result.raw` 의 생산자 기록

[2026-10-21 18:38 KST 추가] 파일 이름·폴더 이름만으로 추정하지 않았다. 열어 본 것은 저장소 문서(README, fixture, 보고서)와 NAS 의 pptx·txt 다. DICOM 헤더·RAW 내용·실행 파일은 열지 않았다(QA inventory 와 같은 경계).

### 3.1 결론 요약

| 후보 | 생산 엔진·버전을 적은 문서 | 판정 | 근거 |
|---|---|---|---|
| cyan_test 루트 `*_result.raw` 5개 | **없음.** README 는 이 이름을 **우리 전처리 CLI(`xpe-preprocess-cli`)의 출력 자리**로 쓴다(float32 37.7 MB 라고 적음). 실제 파일은 18,874,368 바이트(uint16 크기)라 README 의 서술과 맞지 않는다. 장비 회사·2G 출력이라는 문장은 어디에도 없다 | **[근거 없음]** | `tests/test_data/cyan_test/README.md:282` "출력: … _result.raw (float32, 32-bit)", `:421` `--output …_result.raw`, `:4` "수집 기간: 2026-04-02"; `fixture.json:43` `"result_files": ["*_result.raw"]`(와일드카드만); `xpe-pre/.moai/reports/lane-pre/QA-A-250/report.md:37` "저장소 안의 서술에는 이 파일이 장비 회사 보정본이라는 말이 없다 … 출처는 확인되지 않았다" |
| cyan_test `바르넥스/` 4개(ankle result·sharp·soft·standard) | **없음.** README 와 fixture 는 이 하위 폴더를 언급하지 않는다. 파일이 `.gitignore:294`(`tests/test_data/**/*.raw`) 에 걸려 커밋 기록도 없다. `ankle_…_result.raw` 는 루트 발 팬텀 result 의 바이트 복사본(QA inventory) | **[근거 없음]** | `grep 바르넥스 tests/test_data/*/README.md` 0건; `git check-ignore -v` 결과 `.gitignore:294`; `qa-input-provenance.md` 중복 발견 3 |
| NAS `08. 바르넥스 엔진 비교/G1417CW/1417_Glass/*_result.raw`, `1417_Glass_Calibration map/Calibration_bright_*_result.raw` | **없음.** 그 폴더 트리에 문서는 두 개뿐: `G1417CW/1417_Glass_Crop/2500x3040.txt` (0 바이트, 2025-03-13) 와 상위 `영상 엔진 정리_20260512.pptx` (27.9 MB, 19쪽, DR 개발팀). pptx 는 `_result.raw` 를 언급하지 않고, 엔진 버전도 적지 않는다 | **[근거 없음]** | NAS `find -maxdepth 3` 문서 확장자 전수 결과 2건; pptx 슬라이드 텍스트 추출(`scratchpad/pptx.txt`) |
| NAS `20260624_영상 정리(전처리전 파일 포함)/1/*_Result.raw`, `*(Hnvue).dcm`, `*_Sharp/Soft/Standard.raw` | **없음.** 같은 폴더에 설명 파일 없음. DICOM 은 열지 않았다(리더 결정: DICOM 보류) | **[미확인]** | 디렉터리 목록 |

### 3.2 "08. 바르넥스 엔진 비교" 폴더가 2G 출력인가 — 근거 문서로만 판단

유일한 설명 문서인 `영상 엔진 정리_20260512.pptx` 의 본문:

- 1쪽: "영상 엔진 이미지 정리 <confidential> 일자 2026. 05. 12 부서 DR 개발팀"
- 2쪽: "영상 엔진 확인 테스트 — 영상 파일: G1417CW 제품의 이미지 사용, 영상 크기: 2500 X 3052, 조건: 다른 세팅을 건들지 않고 엔진에서 나온 결과값을 확인"
- 3~18쪽: 부위별(Chest PA, Ankle, Elbow, Hand, Knee, Pelvis, Shoulder, Skull …) 4분할 비교 — "바르넥스_Sharp / 바르넥스_Soft / 바르넥스_Standard / HnVue 영상엔진"
- 19쪽 결론: HnVue 영상 엔진은 heel effect 에 취약하고 바르넥스 엔진은 영향이 줄어든다; 바르넥스는 Soft 보다 Standard parameter 권장

→ 이 문서가 비교하는 것은 **"바르넥스 엔진" 과 "HnVue 영상엔진" 의 후처리 결과(Sharp/Soft/Standard 파라미터)** 다. `2G`, `SDSDK`, `Samsung`, `libxd2`, `caleng`, 버전 번호는 pptx 본문에 **한 번도 나오지 않는다**(`grep -i "2G|SDSDK|samsung|삼성|caleng|version|버전"` 0건).

판정: **[근거 없음]** — "바르넥스 엔진 비교" 폴더를 2G(SDSDK2) 출력이라고 볼 문서 근거는 없다. 반대로 "2G 가 아니다" 라고 단정할 문서도 없다 — 문서가 말하는 것은 바르넥스 대 HnVue 후처리 비교라는 것뿐이고, 그 전처리(`_result.raw` 단계)가 무엇으로 만들어졌는지는 적혀 있지 않다. 폴더 이름 "바르넥스" 와 `_result` 접미사는 이번 판정에 쓰지 않았다.

### 3.3 저장소 문서 안의 엔진 이름 검색(대조군)

`바르넥스|Varnex|휴이넘|HnVue|caleng|G1417CW|1417_Glass|엔진 비교` 를 `docs/`, `tests/test_data/*/README.md`, `.moai/specs`, `.moai/reports/lane-doc`, `.moai/lanes/doc` 에서 검색 → `HnVue` 만 잡힌다(IAP:285·413 의 "HnVue Calibration Tool v2.1+ — 캘리브레이션 영상 획득 및 맵 생성", PRD 여러 곳의 "HnVue Console SW"). 이는 **취득 도구/콘솔**을 가리키는 문장이고 `_result.raw` 를 만든 엔진이라는 기록은 아니다. `바르넥스`, `caleng`, `G1417CW` 는 저장소 문서에 0건(DOC-08 카드 자신 제외). 검색이 파일을 읽었다는 대조는 `HnVue` 적중이다.

### 3.4 남는 공백 (이 카드가 못 채운 것)

1. `_result.raw` 5개(cyan_test)·30여 개(NAS G1417CW) 의 **생산 엔진·버전·보정 체인**: 어느 문서에도 없다. 채우려면 파일을 만든 사람(DR 개발팀, 2025-03 / 2026-04 시점)의 기록이나 생성 로그가 필요하다.
2. NAS `20260624…/1/*.dcm` 의 DICOM 헤더(제조사·소프트웨어 버전 태그)는 열지 않았다. 리더 결정(2026-10-21 18:43): **DICOM 보류 — 열지 않는다.** 공백으로 남긴다.
3. "바르넥스" 가 어느 회사·제품(2G SDK 와의 관계)인지 설명하는 문서는 저장소·NAS 조사 범위 안에 없다.

---

## 조사 범위 밖 / 하지 않은 것

- `Z:/공통자료/Software/2G/` 바깥 폴더(`SDK/`, `AXD/` 등)의 내용은 이름만 봤다.
- DLL·exe 실행, 장비 접속, `param/*.par`·시리얼·키·DICOM 헤더 열람, RAW 내용 해석, 원본 수정, push 는 하지 않았다.
- 이 보고서 외에 쓴 파일은 없다(스크래치 추출 텍스트는 세션 임시 폴더).
