# 보정 취득 예산 조사 — 외부 출처 주장 대조 원장

- 대상 보고서: `.moai/state/research/dr-calibration-acquisition-research.md` (2026-10-16 14:25 KST 판)
- 방법: 보고서가 인용한 URL 을 모두 다시 내려받아(curl, 29건 전부 성공) `pdftotext`/HTML 텍스트 추출 후 정규식으로 해당 구절을 찾았다. 쪽 번호는 PDF 쪽(pdftotext 의 form-feed 기준)이다. Konstantinidis 2011 은 Surrey 저장소가 JS 셸만 돌려줘서 PubMed(PMID 22149850) 초록으로 대조했다.
- 판정: VERIFIED = 원문 구절이 주장을 뒷받침, PARTIAL = 원문이 더 좁거나 맥락이 다름, NOT SUPPORTED = 원문에 없거나 반대, UNREACHABLE = 받지 못함.
- 보고서가 스스로 [미검증]으로 표시한 항목(SPIE 5745, IUCr 2023, radiopaedia, Carestream QRG)은 대조 대상에서 뺐다.

## 1. 주장별 원장

| # | 주장(요약) | 인용 출처 | 판정 | 원문 인용 + 위치 / 설명 |
|---|---|---|---|---|
| 1 | HU = kVp·mA·s·상수, 단상 1, 고주파 1.4~1.45 | uomus 강의 PDF | VERIFIED | p5 "Heat Units (HU) = (kVp) (mA) (sec) . Constant"; p6 단상 예제 "98 kVp ×120mAs ×1", 고주파 예제 "× 1.4", "× 1.45". 주의: 강의 자료 자체가 HU 를 J 로 표기하는 오류가 있음("= 7000 J") |
| 2 | 350 kHU 양극, 최대 부하에서 완전 냉각 15분 | 같은 PDF | VERIFIED | p4 "maximum anode heat capacity of 350,000 HU … it would take 15 minutes for the anode to cool completely" |
| 3 | 냉각 속도는 처음 빠르고 점차 느려짐 | 같은 PDF | VERIFIED | p5 "The rate of cooling is rapid at first and slows as the anode cools" |
| 4 | 하우징 냉각 곡선 = 다음 노출 전 경과 시간 판단용 | 같은 PDF | VERIFIED | p7 "information regarding the amount of time that must elapse before initiating another exposure" |
| 5 | E7886X 양극 210 kJ(300 kHU) | Toshiba E7886X PDF | VERIFIED | p3 "Anode Heat Content … 210 kJ (300 kHU)" |
| 6 | E7886X 하우징 900 kJ(1250 kHU) | 같은 PDF | VERIFIED | p3 "X-ray Tube Assembly Heat Content … 900 kJ (1250 kHU)" |
| 7 | E7886X 최대 양극 방열 870 W(1226 HU/s) | 같은 PDF | VERIFIED | p3 "Maximum Anode Heat Dissipation … 870 W (1226 HU/s)" |
| 8 | E7886X 연속 양극 입력 60 W(85 HU/s) | 같은 PDF | VERIFIED (누락 있음) | p3 "Continuous Anode Input Power … 60 W (85HU/s)". 같은 쪽의 "Nominal Continuous Input Power: Without Air-circulator … 180 W (15 kHU/min)"(조립체 기준)를 보고서는 빠뜨렸다. 표의 Siemens 값은 조립체 기준이므로 같은 열에 양극 기준 60 W 와 조립체 기준 275~450 W 가 섞여 있다 |
| 9 | RAY-10 양극 141 kJ = 200 kHU, 조립체 0.9 MJ, 연속 275/450 W | Siemens RAY-10 PDF | VERIFIED | p2 "Anode heat storage capacity 141 kJ = 200 kHU … Heat storage capacity of assembly 0.9 MJ Nominal continuous input power of 275 W 450 W assembly (without/with fan)" |
| 10 | SV150 양극 450 kJ(600 kHU), 1.800 MJ(2.530 MHU), 연속 300/450 W | Siemens SV150 PDF | VERIFIED | p2 "Anode heat storage capacity 450 kJ (600 kHU)", "Heat storage capacity 1.800 MJ (2.530 MHU) … Nominal continuous input power 300 W/450 W (of X-ray tube assembly)" |
| 11 | Dunlee DU1750-E/DU33100-E 시간당 120회 과열 없음 | Dunlee 데이터시트 2종 | VERIFIED | 두 문서 p1 "Up to 120 exposures per hour without overheating" |
| 12 | 분당 2회 8시간에도 하우징 열용량 90% 미만 | 같은 데이터시트 | VERIFIED (조건 누락) | p2 "Even after eight hours of continual, consecutive examinations, fulfilling two exposures per minute, this tube assembly still reaches less than 90% of its heat storage capacity". 근거 곡선은 "typical radiography chest examinations" / "typical mix of radiographic hospital case examinations" 조건이다. Cu 1.5 mm 보정처럼 mAs 가 큰 노출에 그대로 옮길 수 없다 |
| 13 | 분당 기동·정지 최대 4회, 연속 적용 시 하우징 임계 온도 | 같은 데이터시트 | VERIFIED | p2 "Maximum Permissible Rate of Start-Stop Cycles per Minute 4 (Please note: this rate cannot be applied continuously without reaching critical housing temperature)" |
| 14 | Agfa DR 10s/14s, DR 10e C/14e C/17e C 는 관 과열 방지로 분당 2회 | Agfa 0134L EN 20260113 | VERIFIED | p19(DR 10s/14s 절), p21(DR 10e C 절) "To avoid overheating of the X-ray tube, limit the rate of exposures to two per minute." DR 10s 절은 DX-D 100 이동형 장비 사용을 함께 다룬다 |
| 15 | 유방용 DR 18M/24M 분당 1회 | 같은 매뉴얼 | VERIFIED | p22 "The rate of exposures is limited to one per minute." (과열 사유는 적혀 있지 않음) |
| 16 | DR 10e C 계열 노출 시간 200 ms 초과 시 서비스 연락 | 같은 매뉴얼 | VERIFIED | p21 "Check if the exposure time of the exposures is less or equal to 200 ms. If you notice longer exposure times, contact your local service organization." |
| 17 | RF 4343 FL: 지시 후 3분 넘게 미루지 말 것, 준비 전 노출은 보정 무효 | 같은 매뉴얼 | VERIFIED | p35·p37 "Initiating the exposure while the DR detector is not ready, invalidates the calibration. Do not wait longer than 3 minutes before initiating a next exposure" |
| 18 | GE: RC 충전 3T 95%, 4T 98%, 남은 몫이 다음 프레임 잔상 | GE 백서 | VERIFIED (맥락 주의) | p2 "VC(3T) = 95% Vs and VC(4T) = 98% Vs", "The remaining 5% in the t = 3T case and the remaining 2% in the t = 4T …". 30 fps 투시용 수동 화소 판독 설명이며 CMOS 홍보 문서다. 정지 영상 50 s 규모 잔상의 근거로는 약하다 |
| 19 | CsI:Tl 잔광 ms~분, 깊은 트랩 수십 분 이상 | arXiv 2608.26542 (ANU) | VERIFIED | p2 "afterglow in CsI:Tl is particularly pronounced on timescales ranging from milliseconds to minutes, memory effects associated with deep traps have also been reported on much longer timescales, extending to tens of minutes or longer" |
| 20 | 수 초 이상 잔상은 대개 수 %, 표준 영상에서 무시 | 같은 논문 | VERIFIED | p1 "lag effects (lasting longer than a few seconds) are relatively small, typically at the level of a few percent, and are therefore often neglected" |
| 21 | a-Si/CsI 단계 상승 뒤 평형값 초과 과도 응답이 수 시간 | 같은 논문 | VERIFIED (맥락 주의) | p5 "Overshoot on a timescale of several hours is observed in the CuTi (step-up) transitions, where the transient response exceeds the final value". 실험실 XCT 맥락이며 p2 에서 "much longer than those typically encountered in medical XCT acquisitions" 라고 스스로 적는다. 크기는 최대 약 50~60 ADU |
| 22 | TG-150: 고대비 물체 중간~높은 선량 → 수 분~수 시간 고스트, 그런 영상 최소화 | AAPM TG-150 (2024) | VERIFIED | p73 "Moderately high-dose exposures of high contrast objects such as solid-state dosimeter sensors can produce ghost artifacts that may take minutes or hours to decay … minimize the number of such images at the medium level." |
| 23 | DR 은 수동 고스트 시험, 근무일 늦게 또는 주말 전 | TG-150 | VERIFIED | p91 "the alternate procedure (passive test) should be used for DR systems … performed late in the workday, or else before a weekend" |
| 24 | IEC 62220-1-1:2015 목차에 4.6.3, 부속서 A, A.3.4 최소 간격, 잔상 결정이 보상 고려 | VDE 미리보기 | VERIFIED | p2 "4.6.3 Avoidance of LAG EFFECTS", "A.3.4 Determination of the minimum time between consecutive images"; p4 "the method for the determination of LAG EFFECTS now considers lag and ghosting compensation" |
| 25 | Schmidgunst: 잔상 합성 감쇠 곡선이 5% 안에서 화소 공통, 화소별 보정 불필요 | Schmidgunst 2007 (VCU 사본) | VERIFIED | p8 "be the same for each pixel, within the accuracy of the measurement 5%, so no pixel individual correction is necessary" |
| 26 | iRay Mars1417V 노출·맵 갱신 전 15분 예열 | iRay Mars1417V A5 | VERIFIED | p17 "The product should be warmed up for 15 minutes before exposure or updating the gain map and defect map." |
| 27 | Agfa 예열 1시간(기본), 유방 30분, RF FL 4시간·1시간, 예열 중 노출 금지 | Agfa 매뉴얼 | VERIFIED | p13 "Warm-up time is one hour, unless specified otherwise … During warm-up no exposures should be made"; p22 "30 minutes"; p34(model 3) "4 hours"; p36(model 4) "1 hour" |
| 28 | iRay Venu1717 "Exposure prohibited" → "Exposure enable" 후 조사 | Venu1717 V1.1 | VERIFIED | p27 "wait for message in status bar change from "Exposure prohibited state" to "Exposure enable state" and radiate" |
| 29 | Agfa 1수준 2+10회, 2수준 2+1회 = 15회 | Agfa 매뉴얼 | VERIFIED (해당 모델 한정) / 일반화는 PARTIAL | p17(XD/XF), p30(DX-D 40C/G), p32(DX-D 45/60) "Make two exposures … Make 10 exposures … Make two exposures … Make one exposure." 다른 모델은 다르다: DX-D 10C/20C 는 1수준(28 mAs, p23), DX-D 25C 는 3수준(12.5/8/4 mAs, p25), DX-D 30C/35C 는 균일도 점검 + 보정(50/20 mAs, p27), DR 10s·10e C 는 도구 지정. §3.1·§3.2·§5-4 의 "Agfa 15회 2수준"은 모델 한정이 빠졌다 |
| 30 | Agfa 75 kV, 25/32 mAs(1수준), 12.5/4 mAs(2수준), Cu 1.5 mm, 그리드 제거, 평소 SID | Agfa 매뉴얼 | VERIFIED | p16 "kV 75 kV … 25 mAs (XD 10…) 32 mAs (XD 14…) … 12.5 mAs … 4 mAs", "Remove the grid", "Mount the Agfa calibration filter (Cu 1.5 mm)", "Use the (average) SID for normal operation"; p12 Cu 1.5 mm 필요(DR 18M/24M·DX-D 25C 제외) |
| 31 | Agfa 재보정 3·6·12개월, 조건·HW/SW 변경 시 추가, 알림 | Agfa 매뉴얼 | VERIFIED | p9 "Table 1: Calibration interval … 3 months … 6 months … 12 months", "after a major change to the hardware or software configuration", "A timely reminder is displayed on the NX workstation." 보고서가 빠뜨린 사실: 같은 표에 "DR 14e G, DR 17e G no calibration required" |
| 32 | RF 4343 FL 모드마다 4개 선량, 첫 선량 gain, 나머지 defect | Agfa 매뉴얼 | VERIFIED | p34 "For each mode, exposures with 4 different doses settings are requested. The images made at the first dose setting will be used to calculate the gain calibration map. The images made at the other dose settings will be combined to calculate the defect calibration map." |
| 33 | RF FL 대개 10분(model 4), 최대 2시간(model 3) | Agfa 매뉴얼 | VERIFIED | p35(model 3) "The calibration may take up to two hours."; p37(model 4) "The calibration takes typically 10 minutes." |
| 34 | Mars gain: 목표 계조 10000±100, 4회 반복, 획득 지연 3000 ms | Mars1417V | VERIFIED (단위는 추정) | p68 "Set "Expected Gray Value" 10000 … Set "Acquire Delay" 3000 … in the range of 10000±100"; p69 "Repeat the same operation for 4 times". 원문에 ms 단위 표기 없음 |
| 35 | Mars defect: 4회째 이후 선량 고정, 19번째 영상까지 | Mars1417V | VERIFIED | p71 "After the fourth x ray shoot, you do not need to change the x ray dose, just repeat … until it comes to the 19th images." |
| 36 | Mars 선량 수준 "사용자 선택(다점 가능)" | Mars1417V | NOT SUPPORTED | Mars 절차는 단일 목표 계조(10000)만 적는다. 다점 gain 은 Venu1717 문서(p28)의 내용이다 |
| 37 | Mars SID 1.2 m, 구리 필터 없음 | Mars1417V | VERIFIED | p67·p69 "make sure SID1.2m, no copper is required" |
| 38 | Mars 설치·주요 변경 뒤, 6개월마다 권장 | Mars1417V | VERIFIED | p66 "after installation or any major change … it is also recommended to do the correction and calibration in each 6 months." |
| 39 | Venu 다점 gain, defect = dark 뒤 bright 5회 | Venu1717 | VERIFIED | p28 "Multiple gain point correction file can be generated by redo step (3) to step (6) several times with different radiation dose"; p29 "Press "Dark" … Follow message box instructions to repeat radiate 5 times." |
| 40 | Vieworks offset·gain shotCount 5, defect 는 불러오기만 | techsupport.vieworks.com ?p=200 | VERIFIED (해석 주의) | "ocp.shotCount = 5", "GET_GAIN_DATA; gcp.shotCount = 5", "//defect calibration cannot be performed by user, so just load supported defect cal file". SDK 예제 코드 주석이다. "공장 파일"이라는 표현은 원문에 없다(원문은 "supported defect cal file") |
| 41 | Canon CXDI NE: 보정+성능시험 대략 연 1회, 촬영 종류·매수 변경 불가, Exposure Memo 재사용 | Canon CXDI NE SG V1.20 | VERIFIED | p59 "is required regularly (approximately once a year)"; p60 "Selection of the calibration/performance test type and the number of the images to be captured cannot be modified. For details, consult your service engineer."; p62 "adjust the X-ray generator settings based on the Exposure Memo data recorded during previous calibration" |
| 42 | CXDI-40G 보정 촬영은 서비스 인력만, 그리드 제거, White.dcm, 1년 넘으면 보정 필요 | CXDI-40G 서비스 매뉴얼 | VERIFIED | p22 "Calibration imaging can be performed by service staff only. It cannot be performed by users. (4) Do not perform calibration imaging with the grid installed."; p181 "\White.dcm \defpix#.dat"; p182 "Calibration is required when the system has been used for over one year." (파일명은 `White.dcm` 단일, `defpix#.dat` 결함 파일도 있음) |
| 43 | Carestream: gain 절차 예시로 dark 보정 flat 16장 | US8894280 | VERIFIED (용어 정의 예시) | "Image calibration procedure: … for example, 16 dark corrected flat fields at a constant exposure level for gain calibration." 현장 절차 장수가 아니라 용어 정의 예시다 |
| 44 | 각 검출기 공장 고유 맵, 현장 보정은 그 갱신 | US8894280 | VERIFIED | "each detector manufactured has a unique set of calibration maps that are created at the factory. The set of initial or factory calibration maps can be updated each time a detector is subsequently calibrated by the user (e.g., field calibration)." |
| 45 | 보정 계기: 초기 등록·경과 시간·노출 수·낙하 | US8894280 | VERIFIED | "an event can occur (e.g., initial registration, time elapsed, number of exposures elapsed, detector dropped, etc.)" |
| 46 | 현장 보정은 촬영 업무를 끊음 | US8894280 | VERIFIED | "field calibrations can disrupt a workflow of a radiologic technician or radiographic imaging facility" |
| 47 | 현장 추가 결함 맵을 공장 맵 위에 덧붙이는 구조 | US8894280 | PARTIAL | 원문: "an additional defect identification procedure 722 can be run to find additional uncorrected defects. An additional defect map 724 containing these defects can be made." 이것은 다중 동작 모드(추가 offset 보정·온도 보정 gain ratio 맵) 실시예 안의 설명이며 "현장 맵을 공장 맵 위에 쌓는다"는 일반 구조로 서술되지 않는다. 보고서 §5-5 의 "Carestream 구조" 권고는 해석이다 |
| 48 | Schmidgunst dark 최대 100장, 선량 단계당 white 최대 100장 | Schmidgunst 2007 | VERIFIED | p7 "up to 100 dark field images … are acquired", "Up to 100 white field images … are acquired at a certain dose step" |
| 49 | 다점 gain 14개 선량 단계(Table VI) | Schmidgunst 2007 | VERIFIED | p7 "a complete dataset for a multipoint gain calibration consists of 14 gain maps or 56 matrices"; p8 "TABLE VI. Calibration step index/dose parameter" |
| 50 | 온도 보정 20~50 °C, 2 °C 간격, 15쌍 | Schmidgunst 2007 | VERIFIED | p7 "room temperature tB 20 °C and reference temperature tE 50 °C … a step of 2 °C is adequate … This results in 15 offset and gain maps." |
| 51 | "온도 보정 단계는 노화 때문에 정기 반복" | Schmidgunst 2007 | NOT SUPPORTED (원문과 반대) | p6 "Steps one to three are part of the basic calibration, which has to be done only once at initial operation. As a last step, a delta calibration is performed." p15 "Temperature calibration lasts until steady-state temperature is reached … up to 4 h. But these steps have to be done only once. Only the delta calibration has to be repeated in regular intervals approximately three months". 노화 보상·정기 반복은 4단계(delta calibration) 몫이다. 같은 p15 에 "Multipoint gain and lag calibration can be automated and performed in approximately 20 min" — 보고서가 놓친 소요 시간 근거 |
| 52 | 표준편차 맵으로 결함 검출, 포화 맵은 특수 결함용 | Schmidgunst 2007 | VERIFIED | p7 "The standard deviation map will be used for defect pixel detection", "Saturated high gain maps are not used for calibration but for the detection of a special class of defect pixel" |
| 53 | 섀시 온도 50 °C, 정상 상태 약 4시간, 1 °C 미만 변화는 유의하지 않음 | Schmidgunst 2007 | VERIFIED | p3 "temperatures of up to 50 °C", "The steady-state temperature approximately after 4 h"; p7 "for temperature variations of less than 1 °C no significant differences in the pixel individual sensitivity was observed" |
| 54 | Varex: offset 매일·세션 시작, gain 매주~매월, flood 평균, gain 맵에서 결함 검출 | Varex 블로그 1 | VERIFIED | "offset calibration is recommended daily or at the start of each imaging session, while full gain calibration is typically performed weekly or monthly"; "Run defective pixel detection on the resulting map" |
| 55 | Varex: 온도·누적 선량·노화가 전기적 기준선 이동 | Varex 블로그 | VERIFIED | 블로그 1(calibrate): "Temperature changes, accumulated radiation dose, and normal component aging all shift the electrical baseline of individual pixels." (보고서가 묶어 인용한 블로그 2 가 아니라 블로그 1 에 있다) |
| 56 | TG-150: 제조사 보정 프로토콜은 고유, 업계 표준 아님 | TG-150 | VERIFIED | p17 "manufacturers often have uniquely defined protocols for calibration of their systems using techniques that are not standardized across the industry" |
| 57 | 보정 구성에서 시험, 벗어나면 gain 보정 심각 영향 | TG-150 | VERIFIED | p70 "Deviation from the manufacturer's specified calibration configuration can seriously affect the gain calibration process" |
| 58 | 보정 중 결함 소자 맵 생성, 제조사 최대 수 규정, 독립 검출법 없음 | TG-150 | VERIFIED | p80 "During detector calibration, the flat panel digital radiographic system software generates a map of defective … detector elements", "Manufacturers generally specify the maximum number of allowable bad pixels", "No reliable methods of detecting bad pixels independent of manufacturer-supplied tests is known to the task group." |
| 59 | Agfa US10345454: 정기 QC 전·이동·변경·정비·수리 뒤 재보정, 여러 노출 수준 flat, 사용 시간에 따른 저하 | US10345454 | VERIFIED | "before each scheduled periodic quality control test. After moving the equipment, after system modifications and after preventive maintenance or repair interventions … recalibration is often necessary"; "multiple sets of evenly exposed (flatfield) images at various exposure levels"; "the signal-quality and the x-ray exposure response of the corrected panel image might degrade with usage time" |
| 60 | Siemens US7227154: 다수 선량 점 감도, 보간, 문턱 초과 수준만 재보정 | US7227154 | VERIFIED | 청구항 4 "a new detector calibration is carried out … for those dose levels at which a sensitivity exceeding a threshold value is found"; 청구항 5 "dose-dependently by the interpolation of previously acquired dose-specific calibration data" |
| 61 | RQA5 = 70 kV, Al 21.0 mm, HVL 6.8 mm, SNRin² 30174 | arXiv 2205.03362 | VERIFIED | p2 Table I "RQA5 70 21.0 6.8 30174" |
| 62 | "실제로" HVL 6.80 을 맞추려면 Al 21.8 mm 필요 | arXiv 2205.03362 | PARTIAL | p12 표 "RQA5 70 21.0 21.8 6.80 6.74 6.80". 그러나 이는 실측이 아니라 Tucker-Barnes 모델로 계산한 스펙트럼이다(p9 "We simulated poly-energetic x-ray spectra using the Tucker and Barnes algorithm"). "실제로 … 필요했다고 보고"는 계산값을 실측처럼 옮긴 것 |
| 63 | RQA5, 선원-신틸레이터 132 cm, 3.10 mAs → 2.5 µGy | arXiv 2011.05048 | VERIFIED (맥락 주의) | p9 "The distance source-to-scintillator was set at 132 cm", "A standard air kerma value of 2.5 µGy"; p17 "RQA5 beam (70kV and 3.10mAs, 2.5uGy)". 평판 검출기가 아니라 다중 카메라(렌즈 결합) 검출기 연구이며 mAs↔선량 관계는 그 장비 관 출력에 종속 |
| 64 | TG-150 flat: Al ~21 mm 또는 Cu 0.5+Al 1, SID 180 cm(또는 최대), 80 kV, 최소 3수준 | TG-150 | VERIFIED | p72 "Attach a ~21 mm aluminum block or 0.5 mm copper and 1 mm aluminum filters", "Set the SID to 180 cm, or the highest possible value, whichever is smaller", "Acquire flat field images at 80 kV for minimum of three different air kerma values (low, medium, and high as mentioned in Table 12)" |
| 65 | Table 12: 중간 ~3~5 µGy(IEC normal), 저 = 중간/3.2, 고 = 3.2×, 선택: 제조사 보정 선량·최대 | TG-150 | VERIFIED | p73 "described in IEC 62220-1-1 as the "normal" level … typically ~3–5 µGy", "~ 0.3125 * the medium level (i.e., medium level/3.2)", "~ 3.2 * the medium level", "The air kerma used for system calibration according to manufacturer specifications. The maximum air kerma within the image receptor's specified operating range." |
| 66 | 일부 제조사는 필터 없는 보정 → 에너지 의존성 문제 | TG-150 | VERIFIED | p73 "Some manufacturers follow calibration protocols that do not employ any level of beam filtration … Multiple authors have reported energy dependence" |
| 67 | TG-150 은 물리사 시험, 기사 QC 는 Report 151 | TG-150 | VERIFIED | p13 "cover only those tests performed by medical physicists, as tests performed by technologists are covered by AAPM Report 151" |
| 68 | 보정 직후 flat 취득이 이상적 | TG-150 | VERIFIED | p72 "Ideally, this test should be done immediately after calibration." |
| 69 | 관·수상체 상대 위치가 바뀌면 재보정 필요할 수 있음 | TG-150 | VERIFIED | p78 "If the relative position of the tube and image receptor have changed since calibration … This may require recalibration of the image receptor" |
| 70 | 비정상적으로 높거나 낮은 열 → 재보정 필요 시사 | TG-150 | VERIFIED | p82 "Anomalously high or low columns may indicate a need for recalibration." |
| 71 | 보정 주기는 제조사 권고를 평가, 고정 값 없음 | TG-150 | VERIFIED | p30 "the medical physicist performing acceptance testing should devote part of their effort to assessing the manufacturer's recommendations regarding device calibration and routine QC" (고정 값 부재는 전문 검색 "calibration frequency/interval" 0건으로 확인) |
| 72 | TG-151 초록: 거부 영상·노출 분석·인공물, 주기 값 없음 | Henry Ford 저장소 | VERIFIED | "The focus of this final report is the ongoing QC process, including rejected image analysis, exposure analysis, and artifact identification." 초록에 주기 값 없음 |
| 73 | Peck 발표: TG-10 기반 일·월·분기 기사 QC, 연간 물리사, 제조사 QC 에 더해 | AAPM 발표 PDF | VERIFIED | p2 "Daily (technologist) Quarterly (technologist)", "Monthly (technologist)", "*TG 10 Report: Acceptance Testing and QC of PSP Systems"; p3 "These should be done in addition to any routine QC activities recommended by the manufacturer" |
| 74 | Konstantinidis 2011: 보정 영상 잡음이 flat 장수에 의존, 10~16장 관행·48장 사례, 수정 시 1장까지, 최대 DQE | PubMed 22149850 초록 | VERIFIED (맥락 주의) | "the noise in the corrected image depends on the number of flat frames used for the average flat image", "Many publications suggest the use of 10-16 reference flat frames, while other studies use higher numbers (e.g., 48 frames)", "down to one frame", "leads to the maximum DQE value". 맥락은 NPS/DQE 성능 평가용 기준 flat 이며 임상 gain 보정 장수가 아니다. 수정 알고리즘은 NPS 계산에서 flat 잡음 기여를 보상하는 것이지 1장짜리 임상 gain 맵을 정당화하지 않는다 |
| 75 | Patat: σ² = σp² + I_sky²/(N_F I_F) | arXiv astro-ph/0301534 | VERIFIED | p10 "The expected global noise is in fact given by σ² = σp² + I²sky/(NF IF), where Isky is the background level in the input science frame." (CCD 천문 관측, 전자 단위) |
| 76 | TG-150 지표 검증: ROI 크기가 비균일도·SNR 에 영향, 기하가 MTF 외 지표에 영향 | PMC5874089 | VERIFIED | "ROI size affects nonuniformity and minimum SNR measurements, but not detection of anomalous pixels. Exposure geometry affects all tested image metrics except for the MTF." |

### 판정 집계

| 판정 | 건수 |
|---|---|
| VERIFIED | 71 (그중 맥락·누락 주의 표기 11: #8, #12, #18, #21, #31, #34, #40, #42, #43, #63, #74) |
| PARTIAL | 3 (#29 의 일반화, #47, #62) |
| NOT SUPPORTED | 2 (#36, #51) |
| UNREACHABLE | 0 (29개 URL 모두 수신, Surrey 페이지만 JS 셸이라 PubMed 로 대체) |

## 2. 산술 재계산

전제: 1회 열 ≈ kV × mAs (J), 고주파·파형 계수 1. HU 환산은 ×1.4.

| 조건 | 보고서 | 재계산 | 판정 |
|---|---|---|---|
| 75 kV × 25 mAs | 약 1.9 kJ, 31 s, 1.1% | 1.875 kJ(2.6 kHU), 60 W 로 31.2 s, 양극 210 kJ 대비 0.89% | 열·간격 맞음, "약 1.1%"는 32 mAs 값(1.14%)이고 25 mAs 는 0.9% |
| 75 kV × 32 mAs | 약 2.4 kJ, 40 s | 2.40 kJ(3.4 kHU), 40.0 s, 1.14% | 맞음 |
| 70 kV × 3.10 mAs | 0.22 kJ, 4 s, 0.1% | 0.217 kJ, 3.6 s, 0.10% | 맞음 |
| 180 cm 역제곱 | 5.8 mAs, 0.4 kJ, 7 s, 0.2% | 3.10 × (180/132)² = 5.76 mAs, 0.403 kJ, 6.7 s, 0.19% | 맞음 |
| 25 µGy, 58 mAs | 4 kJ, 68 s, 1.9% | 57.6 mAs → 4.03 kJ, 67.2 s, 1.92% | 맞음 |
| 같은 노출을 조립체 연속 입력 180 W 로 나누면 | (보고서에 없음) | 2.4 kJ → 13.3 s, 4.03 kJ → 22.4 s | 60 W 기준은 가장 보수적인 값임을 명시해야 함 |
| 단발 연속 15회(Agfa 조건 상한) 누적 | (보고서에 없음) | 15 × 2.4 = 36 kJ = 양극 용량의 17% | 짧은 연속 촬영에서는 열 저장 용량이 남으므로 "열 중립 간격"이 실제 간격을 정한다는 주장은 성립하지 않을 수 있음 |
| 1/N gain 맵 잡음(양자 한정, D_img = D_flat) | N=16: 분산 +6.25%, σ +3.1% / N=32: +3.1%, +1.6% | 분산 1+1/N → σ 증가 N=16 3.08%, N=32 1.55%(차이 1.53%p). gain 맵이 더하는 잡음 자체의 σ 는 영상 잡음의 1/√N = 25.0%(N=16), 17.7%(N=32) | 맞음. 다만 "더해지는 잡음 비율"과 "총 σ 증가"는 다른 양이니 표 머리에 구분해 둘 것 |
| N=4/8/64 | +11.8%/+6.1%/+0.8% | 11.80%/6.07%/0.78% | 맞음 |
| §5-4: 16장 × 2수준 × 50 s | flat 만 27분 이상 | 32 × 50 s = 26.7분(간격 31개로 세면 25.8분) | "27분 이상"은 약간 과장. "약 26~27분"이 정확 |
| 300 kHU / 210 kJ | — | 1.43 | HU 계수 1.4 와 일치(데이터시트 내부 일관성 확인) |

## 3. 사실처럼 쓰인 추론

1. §1.1 "Agfa 조건의 열 중립 간격 31~40 s 는 Agfa 가 적은 분당 2회와 같은 크기 → 관 열부하가 실제로 간격을 정한다": 두 숫자의 출처가 다르다. 25~32 mAs 는 XD/XF·DX-D 40/45/60 모델 값(p16·p29·p31)이고, "분당 2회"는 DR 10s/14s·DR 10e C 절(p19·p21, 이동형 DX-D 100 포함)에만 있다. 같은 모델의 조건이 아니며, 60 W 는 무기한 지속 가능한 양극 입력이라 단발 15회 연속(양극 용량 17%)에는 결정 요인이 아니다. Agfa 의 분당 2회는 Dunlee 의 "시간당 120회"(=분당 2회)와 같아서 관 종류를 모르는 상태의 일반 안전 규칙으로 읽는 편이 맞다.
2. §1.1 열 중립 표 전체: [추론]으로 표시는 되어 있으나 "E7886X 60 W" 하나만 쓰고 같은 문서의 조립체 연속 입력 180 W 를 빠뜨려 간격을 약 3배 길게 잡았다.
3. §1.1 표의 "냉각·연속 입력" 열: 양극 기준(E7886X 60 W)과 조립체 기준(Siemens 275~450 W)을 같은 열에 둬 비교 가능한 것처럼 보이게 했다.
4. §1.3 "첫 프레임 잔상이 수 % 수준이라는 서술의 근거"(GE): GE 문서는 30 fps 투시 판독 시간에서의 불완전 충전을 설명한다. 정지 영상 판독 뒤 50 s 안정 조건과 같은 현상이라는 연결은 해석이다.
5. §2.1 Vieworks "사용자는 공장 파일을 불러오기만 함": 원문은 SDK 예제 주석 "load supported defect cal file". 제품 소프트웨어 정책인지, 그 파일이 공장 생성인지는 원문에 없다.
6. §2.1 Mars1417V "선량 수준 수: 사용자 선택(다점 가능)": 원문 근거 없음(#36).
7. §2.1 Schmidgunst "온도 보정 단계는 노화 때문에 정기 반복": 원문과 반대(#51).
8. §2.2 "21.8 mm 가 필요했다고 보고 … HVL 기준으로 두께를 조정하는 것이 표준의 취지": 근거 논문은 시뮬레이션이다(#62). "표준의 취지"는 IEC 본문을 열지 않은 해석이다.
9. §3.2 [사실] 문단 끝 "동적 검출기는 연속 프레임으로 장수를 싸게 얻을 수 있어서다": 인용 문헌 어디에도 없는 이유 설명이다.
10. §3.2 "일반촬영 검출기의 현장 보정은 수십 장 이하 … Carestream 특허 예시 16장": Carestream 16장은 용어 정의 예시이지 현장 절차가 아니다(#43). Vieworks 5장도 SDK 예제의 기본값이다.
11. §3.3 "10~16장이 관행인 이유와 맞는다": Konstantinidis 의 10~16장은 DQE/NPS 성능 평가용 기준 flat 장수다(#74). 임상 gain 보정 관행의 근거로 옮긴 것은 해석이다.
12. §1.3 ANU 논문의 수 시간 과도 응답을 §5-7 에서 "고선량 뒤 잔상이 수 분~수 시간"의 근거로 묶었다: 논문은 실험실 XCT(수 시간 스캔)에 특유하다고 스스로 한정한다.
13. §5-5 "Carestream 구조처럼 공장 BPM 위에 현장 신규 결함만 추가": #47 의 PARTIAL 근거 위에 세운 권고다.

## 4. 출처 없는 수치

- §1.1 표 "RQA5 고선량 수준(예: 2.5 µGy 의 10배 = 25 µGy)": 10배는 임의 예시(TG-150 고수준은 중간의 3.2배). 출처 없음을 명시할 것.
- §5-4 "현장 보정은 1~2 수준, 수준당 10~16장 정도를 상한": 문헌 수치를 조합한 필자 제안. §5 전체가 [추론]으로 표시되어 있어 표기 자체는 맞다.
- §5-3 "수준당 16~32장을 기준선": 같은 성격의 제안.

## 5. 수정 필요 사항

1. **#51 (가장 중요)**: §2.1 Schmidgunst 행의 "온도 보정 단계는 노화 때문에 정기 반복"을 다음으로 바꾼다 — "온도·다점 gain·잔상 보정(1~3단계)은 초기 1회, 노화 등 동적 변화는 delta 보정(4단계)으로 약 3개월마다 수 분 안에 반복. 다점 gain + 잔상 보정은 자동화 시 약 20분, 온도 보정은 정상 상태까지 최대 4시간." 이 사실은 §5-3 공장/현장 분리 논리를 오히려 직접 뒷받침한다.
2. **#29**: §3.1·§3.2·§5-4·§5-6 의 "Agfa 15회 2수준"을 "Agfa XD/XF·DX-D 40/45/60 계열 15회 2수준"으로 한정하고, 다른 모델(1수준·3수준·균일도 점검형, DR 14e G/17e G 보정 불필요)을 덧붙인다.
3. **§1.1 열 중립 비교**: (a) 60 W 는 양극 지속 입력, 조립체 연속 입력은 180 W 임을 같이 적고, (b) "분당 2회" 출처 모델(DR 10s·10e C, 이동형 포함)과 25~32 mAs 출처 모델이 다르다는 점을 밝히고, (c) 단발 연속 15회는 양극 용량의 약 17% 라 열 저장 용량이 간격을 정하지 않는다는 계산을 추가한다. "관 열부하가 실제로 간격을 정한다"는 결론은 철회하거나 "지속 운전 시에만"으로 한정한다.
4. **#62**: "실제로 21.8 mm 가 필요했다" → "Tucker-Barnes 스펙트럼 계산상 21.8 mm 가 필요(실측 아님)".
5. **#36**: Mars1417V 행의 "사용자 선택(다점 가능)" → "단일 목표 계조(10000±100)".
6. **#47**: Carestream 행의 "현장 추가 결함 맵을 공장 맵 위에 덧붙이는 구조" → "다중 모드 실시예에서 추가 결함 식별 절차로 '추가 결함 맵'을 만들어 함께 적용하는 예시가 있음". §5-5 권고는 [추론]임을 유지.
7. **#74**: Konstantinidis 문단에 "DQE/NPS 성능 평가용 기준 flat" 맥락을 적고, §3.3 의 "관행" 연결을 [추론]으로 옮긴다.
8. **#12**: Dunlee "분당 2회 8시간" 뒤에 "흉부 촬영·병원 일반 촬영 조합 기준" 단서를 붙인다.
9. **#55**: Varex 기준선 이동 문장의 출처를 블로그 1(how-do-you-calibrate)로 고친다.
10. **#63**: arXiv 2011.05048 은 다중 카메라 검출기 연구임을 적는다(선량-mAs 값 사용에는 영향 없음).
11. §3.2 "동적 검출기는 연속 프레임으로 장수를 싸게 얻을 수 있어서다"를 [추론]으로 옮긴다.
12. §5-4 "27분 이상" → "약 26~27분".
13. §3.3 표에 "gain 맵이 더하는 잡음 σ(영상 잡음 대비) = 1/√N: N=16 25%, N=32 17.7%" 열을 추가해 총 σ 증가(3.1%, 1.6%)와 구분한다.
