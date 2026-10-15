# QA-B-207 M1 (C3) — 서문·`DICM` 없는 파일은 `xpe_dicom_open` 이 `DICOM_INVALID` (#251, 사용자 결정)

카드: `.moai/lanes/post/inbox/QA-B-207.md` M1 항목 1. 이 보고서는 C3 만 다룬다. D1·D3·D8 은 같은 M1 의 뒤 커밋(`report_m1_d.md`).

## 0. 결과

| 항목 | 결과 |
|---|---|
| 수정 | `DicomReader::open` 이 DCMTK 를 부르기 전에 파일의 앞 132 바이트를 읽어 바이트 128..131 이 정확히 `DICM` 이 아니면(또는 132 바이트 미만이면) `XPE_ERR_DICOM_INVALID`, 핸들 NULL. 서문 128 바이트의 내용은 보지 않는다. 열 수 없는 파일(없는 파일·빈 경로)은 이 검사에서 판정하지 않고 기존 `loadFile` 로 넘겨 `XPE_ERR_IO_FAILED` 매핑을 유지한다 |
| 시험 | 새 시험 2개 + 바꾼 시험 4개(§2) |
| 반증 | 9팔 중 9팔 의미 있게 터지거나 관측으로 분류(§3), 소스 전부 바이트 단위 복원 |
| 검증(관측) | ci-dicom 전체 ctest(성능 시험 제외) 345 통과·0 실패, doxygen 경고 0, 헤더 점검 20개 0건 |

## 1. 영향 범위 — 먼저 찾은 것 (카드 지시)

**저장소 안, 읽거나 만드는 곳:**
- `modules/dicom/tests/test_dicom_reader.cpp`: `EWM_dataset` 로 메타 없이 저장하는 `OpenDatasetWithoutMetaHeader_TreatedAsExplicitLE` 1곳, #167 의 `WriteDatasetOnly` 도우미(맨 데이터셋 저장)를 쓰는 시험 7곳(그중 `xpe_dicom_open` 으로 여는 것은 `MetaLessNativeUnsupportedSyntaxesProduceNoPixels`, `KnownDivergence_MetaLessPathLeaksNativeUnsupportedSyntaxes`, `TsLessPathIsDecidedByChecksNotByStructure` 3개 — 나머지는 DCMTK 로 직접 읽는 측정용이라 영향 없음). 변경 전에 이 4개가 실제로 빨개지는 것을 관측했다(ctest 337 중 4 실패).
- `modules/dicom/tests/test_dicom_network_scu.cpp:262`: `EWM_dataset` 로 만든 파일을 **C-STORE** 에 준다. 이 경로는 `xpe_dicom_open` 이 아니라 SCU 가 자기 `loadFile` 로 읽으므로 영향 없음(시험 통과로 확인).
- 추적되는 `.dcm` 파일은 저장소에 **0개**(`git ls-files`).
- `modules/preprocess/tools/xpe_real_frames.cpp`: `xpe_dicom_open` 을 이름으로 연결해 외부 실데이터(저장소 밖)를 읽는 도구. 메타 없는 파일을 만들지 않는다.
- gui·clients: `gui/.../GuiDicomNative.cs`, `BaselineDicomExport.cs`(쓰기와 내보내기), `clients/.../XpeDicomReadinessProbe.cs`(내보내기 이름 점검) — `xpe_dicom_open` 으로 앱이 쓴 Part 10 파일이나 실제 Part 10 파일을 여는 코드로 보이며 메타 없는 파일을 만들거나 읽는 코드는 찾지 못했다(읽어서 확인, 실행하지 않음).

**깨지는 곳(영향 목록):** 위 시험 4개(모두 dicom 레인 소유). gui·clients 시험이 메타 없는 DICOM 을 열게 하는 곳은 찾지 못했다.

## 2. 시험

바꾼 것:
- `OpenDatasetWithoutMetaHeader_TreatedAsExplicitLE` → `OpenDatasetWithoutMetaHeader_IsDicomInvalid`: 맨 데이터셋은 `DICOM_INVALID`·핸들 NULL(센티널 0x1 을 덮어쓰는지 확인), 같은 데이터의 Part 10 원본은 열린다(통제). 이 이름을 인용하는 소스·문서는 없다(`git grep`; 옛 증거 보고서에만 있음).
- #167 의 세 시험은 **지우지 않고** 새 계약을 단언하게 바꿨다(리더 지시):
  - `MetaLessNativeUnsupportedSyntaxesProduceNoPixels`: 맨 데이터셋 통제(지원 구문 Explicit LE)가 이제 `DICOM_INVALID` 이어야 하고(예외 없음), 같은 파일의 Part 10 버전이 화소를 낸다는 것이 도우미가 멀쩡하다는 증거(통제). 지원되지 않는 네이티브 구문의 맨 데이터셋도 각각 `DICOM_INVALID`, 누수 0건 유지.
  - `KnownDivergence_MetaLessPathLeaksNativeUnsupportedSyntaxes`: 이 기록이 로그로 남기던 "화소가 나온 개수"를 단언(0)과 구문별 `DICOM_INVALID` 로 바꿨다. 이름은 유지.
  - `TsLessPathIsDecidedByChecksNotByStructure`: 표를 맨 데이터셋 / **Part 10 이지만 meta 에 TransferSyntaxUID 가 없는 파일** / 표지가 있는 파일의 세 종류로 나눴다. 맨 데이터셋은 구문과 무관하게 전부 `DICOM_INVALID`. TS 없는 Part 10 은 새 도우미 `WriteTsLessPart10`(서문+`DICM`+TransferSyntaxUID 만 뺀 meta)로 만들며, #167 의 두 검사가 여전히 닿는다: Explicit LE 네이티브는 열려 화소를 내고(통제), 캡슐화 `.70`·`.57` 은 `UNSUPPORTED_FORMAT`(모순 검사). 관측: DCMTK 는 meta 가 Explicit LE 로 읽혀야 해서 그 뒤의 Implicit LE·Big Endian 데이터셋을 파싱하지 못하므로 그 두 행은 `DICOM_INVALID`(`UNSUPPORTED_FORMAT` 이 아님). 이전에 이 구문들을 `UNSUPPORTED_FORMAT` 로 가르던 길은 맨 데이터셋 경로였고 지금은 문에서 막힌다.

추가한 것(`AFileThatIsNotPartTenIsDicomInvalidWhateverItContains`, `ThePreambleBytesThemselvesAreNotJudgedAndAMissingFileIsStillAnIoError`):
- 거부 12경우: 맨 데이터셋(서문·매직 없음), 서문 128 바이트 + 틀린 매직(`XXXX`), 마지막 바이트만 다른 매직(`DICX`), 소문자(`dicm`), 매직이 오프셋 0 에, 매직이 한 바이트 늦게(오프셋 129), 131 바이트(한 바이트 모자람), 빈 파일, 일반 텍스트, 그리고 **DCMTK 가 혼자 읽을 수 있는 맨 데이터셋 3개**(바이트 128..131 이 `DICX`·`dicm`·`DICN`). 마지막 세 개가 중요하다: 0 으로 채운 서문 + 틀린 매직은 DCMTK 자신이 파싱 실패로 거부해서, 문이 매직 비교에 느슨해도 같은 결과가 나와 시험이 그 느슨함을 못 본다(반증 첫 실행이 그것을 드러냈다 — §3). 그래서 DCMTK 가 읽는 데이터셋에 매직 자리만 틀린 파일을 구성했다.
- 통제: 서문이 문자 `A..Z` 로 채워진 Part 10 파일은 열린다(서문 내용은 판정하지 않음, PS3.10 7.5.1), 없는 파일은 여전히 `IO_FAILED`·핸들 NULL.

## 3. 반증 (`arms_c3.py.txt`, `arms_c3_out.txt`)

| 끈 방어 | 결과 |
|---|---|
| 문 전체 제거(매직을 보지 않음) | 거부 시험·#167 시험들·`OpenDatasetWithoutMetaHeader_IsDicomInvalid` 빨강(5개 기대 모두) |
| 매직을 오프셋 0 에서 비교 | 서문 통제·J2K 등 Part 10 파일을 여는 시험 다수 빨강 |
| 매직을 오프셋 129 에서 비교 | 같음 |
| 매직을 3 바이트만 비교 | 거부 시험 빨강(`DICX`·`DICN` 맨 데이터셋) |
| 매직을 대소문자 무시로 비교 | 거부 시험 빨강(`dicm` 맨 데이터셋) |
| 짧은 파일 검사(`gcount`) 제거 | 빨간 시험 없음 — 관측: 버퍼가 0 으로 채워져 있어 짧은 파일은 `DICM` 이 아니라서 어차피 거부된다(등가) |
| 서문이 모두 0 이어야 한다고 판정 | 서문 통제 시험 빨강 |
| 열 수 없는 파일도 문에서 `DICOM_INVALID` | 없는 파일·빈 경로 `IO_FAILED` 시험 3개 빨강 |
| 거부 코드를 `IO_FAILED` 로 | 거부 시험·#167 시험들 빨강 |

반증 첫 실행(`arms_c3_out.txt` 앞 블록)에서 세 가지가 판정 불가였다: 문 제거 팔은 미사용 변수 경고(오류 처리)로 빌드 실패, 매직 3바이트·대소문자 팔은 시험이 안 터졌다(위 §2 의 이유 — DCMTK 가 중복으로 막음), 마지막 팔은 패턴 중복으로 드라이버가 멈췄다. 시험에 DCMTK 가 혼자 읽는 맨 데이터셋을 더하고 드라이버를 고쳐 다시 돌린 결과가 뒤 블록이다.

## 4. 영향·비고
- 헤더 `xpe_dicom_open` 의 `@note`: "메타 없는 파일도 열린다" 를 "Part 10(서문 128 바이트 + `DICM`) 이 아니면 `DICOM_INVALID`, 서문 내용은 판정하지 않음, meta 에 TransferSyntaxUID 가 없는 Part 10 은 데이터셋에서 구문을 감지해 열린다(#167)" 로 바꿨다.
- 요구 문구(REQ-DICOM-003)는 이미 이 동작이다. 리더 소유 문서는 건드리지 않았다.

## 5. Gap / 잔여 위험
Gap
- 사용자가 가진 **실제 데이터 폴더의 파일**이 전부 Part 10 인지는 확인할 수 없다(저장소 밖). 서문·`DICM` 이 없는 DICOM 파일(드물지만 일부 도구가 만든다)은 이제 열리지 않는다.
- gui·clients 가 메타 없는 파일을 만드는지는 소스 읽기로만 확인했다(실행 없음).
- 서문 128 바이트보다 짧은 Part 10 아닌 파일은 열 수는 있어도 거부된다는 것만 시험했다. 읽기 오류로 132 바이트를 못 읽는 경우의 로그 문구는 시험하지 않았다.

잔여 위험
- 위의 외부 데이터가 가장 큰 위험이다: 이전에 열리던 파일이 열리지 않는다. 의도한 변경(사용자 결정 #251)이고 `INVALID` 로 분명히 알려 준다.
