[2026-10-21 18:45 KST] [xpe-pre]
# QA-A-276b — QA-A-276 조건 기록 보완 (Reviewer #262 F262-1~6, Refs #278)

기록만 고쳤다. PRE 재실행·알고리즘 변경·제조사 DLL 실행·원본/맵/기존 출력 수정·push 없음. 이번 카드에서 새로 돈 프로세스는 파이썬 검증 스크립트뿐이다(동시 1개, 종료 후 `xpe_*`/`dump_pre` 프로세스 0개). 출력 파일 SHA 는 기존 값 그대로다(`c2210d3d…821a`).
이 파일 묶음은 **A(2G) 출력이 없는 미완성 provenance** 이며 비교 가능한 manifest 가 아니다.

## 파일 (이 폴더)

| 파일 | 내용 |
|---|---|
| `conditions.json` | 계약 4판 표준 필드만. 정의 안 된 키를 넣지 않았다(validate 가 `UNSUPPORTED_FIELD` 로 거부하는 키 없음) |
| `provenance.json` | 계약에 필드가 없는 증거: 입력 자료형·stride·길이, DLL SHA, 출처 HEAD 설명, 맵별 생성 프레임 목록·SHA·역할·제외 프레임, base_dir 방침 |
| `pre-config.json` | `pipeline_out` 에 넘긴 설정 JSON 의 정확한 바이트(106바이트, 줄바꿈 없음). SHA256 `945ced65cd3b5e809b0d120d5def42d16e294607c5179e73236559f638f08a6f` = `config_sha256` |
| `validate-output.txt` | validate 출력 원문 |
| `validate-control.txt` | 대조 3종 출력 원문 |

이전 `QA-A-276/` 의 파일은 고치지 않았다.

## F262-n 별로 고친 것

**F262-1 (보정 상태·mode).** `input.correction_state` 를 `{value:"unknown", status:"unverified", source:"<미확인 사유>"}` 로, `input.mode` 를 `"unknown"` 으로 명시했다. 확인하지 못한 값을 verified 로 바꾸지 않았다. 차단 사유로 `CORRECTION_STATE_UNVERIFIED`, `MODE_MISSING` 이 validate 출력에 나온다.

**F262-2 (PRE 식별·적용 설정·입력 연결).** B 에 `engine_id=new-pre`, `engine_identity.source_head`(40자리 `75c0043195e5afacf7d6b424bee32c063b9e99a0`), `config_sha256`(64자리, 위 파일의 SHA), `execution=imported-file`, `input_sha256`(최상위 input 과 같음), `mode`(input 과 같은 `unknown`)를 채웠다. `applied_settings` 는 offset·gain·defect 각각 `applied`, `status=verified`, `source` = `meta.flags=0x200c`(오프셋 0x2000 | 결함 0x4 | 이득 0x8, 우회 0x4000 없음), QA-A-276 실행 로그와 QA-276-INDEP 독립 관측을 인용. 계약에는 단계별 status/source 칸이 없어 한 개의 status/source 로 세 단계를 묶었다. 단계별 근거는 flags 비트 하나씩이다. flags 는 단계 완료 보고이며 화소별 감사가 아님을 source 에 적었다. 설정 JSON 은 요청값이라 적용의 증거로 쓰지 않았다. DLL SHA(`xpe_preprocess.dll` `d404d407…7643d`, `xpe_common.dll` `56bb73af…41fa4`)는 `provenance.json` 의 `engine_B_extra` 에 따로 뒀다. `source_head` 는 이 바이너리의 10/18 실행이 기록한 HEAD 이고, 바이너리는 그 HEAD 에 미커밋 변경을 더해 지었다(이후 `e404b70e` 로 커밋). **빌드와 커밋의 소스 일치는 확인하지 못했다**(provenance 에 명시).

**F262-3 (출력 필드).** `endian=little`, `width/height=3072`, `stride_bytes=12288`, `offset_bytes=0`, `stage=engine-direct`, `units=pixel value`, `layout`(top-left, top-to-bottom, left-to-right, identity)을 모두 채웠다. 길이 확인: 0 + 3072 × 3072 × 4 = 37,748,736 바이트(파일 크기와 같음, validate 가 `FILE_SIZE_MISMATCH` 를 내지 않음). `layout.status=verified` 의 출처는 세 가지: 버퍼를 변환 없이 씀, offset/gain/defect/pipeline 소스에 뒤집기·전치·회전 코드가 없음(grep), 입력과의 1/8 표본 상관(그대로 0.9935, 상하 0.9566, 좌우 0.4118, 전치 0.1765). 독립 증명은 아니라고 source 에 적었다. `stage=engine-direct` 는 "`pipeline_out` 의 detector-domain FLOAT32 출력을 후처리 없이 썼다"는 PRE 쪽 기록이며, 화면용 UINT16/VOI/클리핑/LNR/GLS 출력과 같은 단계라는 뜻이 아니다. 2G 쪽 단계는 모른다.

**F262-4 (맵·생성 프레임).** 맵 3개의 경로·SHA 를 `maps` 에 넣고(경로/SHA 외 키 없음), 나머지를 `provenance.json` 의 `maps_provenance` 에 연결했다.
- 프레임 수는 폴더 파일 개수가 아니라 생성기가 쓴 `manifest.json`(SHA256 `5f2a286b47f8c00f83a2b16a3a8c103e5efe4e4e5513c84acfb892b4b82de177`)의 `frames`(dark 7, bright 6, bpm_bright 18)와 `inputs` 26개 목록에서 가져왔다. 26개 파일을 지금 다시 해시해 매니페스트 SHA 와 대조: 불일치 0.
- 역할: `dark` 7, `bright_and_bpm_bright` 6, `bpm_bright` 12, `dark_excluded_first` 1(`Dark_07`, 첫 다크 제외 정책). 이외 calfiles 프레임(`Dark_15/16`, `Bright_29`, `CalSet_*`)은 이 맵들의 입력으로 나열되지 않았다.
- 맵별 소비 역할: 매니페스트의 `dark_policy.stages_on_the_used_list`(offset, 이득 암전 기준, 결함 맵)와 역할 이름을 따랐다. 맵↔프레임 개별 배정은 매니페스트가 명시한 단계까지만이고, 그 이상은 `provenance.json` 에 "역할 이름 기반"이라고 표시했다.
- 이번 입력과 같은 세션·같은 검출기 모드에서 취득됐는지는 미확인이다(`unverified`).

**F262-5 (최상위·base_dir·미완성 표시).**
- `schema` = `xpe-2g-pre-comparison/4`.
- `comparison_kind` 를 명시했다: `e2e-non-isolated`. 계약 기본값(common-condition)은 알고리즘 귀속으로 이어지므로 의도하지 않은 귀속을 피하려는 비귀속 자리표시이며, 짝이 E2E 라는 결정이 아니다(`note` 에 적음).
- `roi` = 저장 배열 전체 `{0,0,3072,3072}`. **근거 있는 ROI 가 아니라 자리표시(미정)** 이다. 계약이 정수 ROI 를 요구해서 넣었고 그 사실을 `note` 에 적었다.
- 미완성 표시: 최상위 `note` 첫머리에 "INCOMPLETE PROVENANCE, NOT A COMPARABLE MANIFEST. engines.A is absent on purpose".
- base_dir 방침(`provenance.json`): `D:/workspace-github` 를 base_dir 로 쓰고 모든 경로는 그 아래 상대 경로. 파일을 복사하지 않아 SHA 는 원본 바이트에 연결된다. 개인 출력은 `xpe-pre/build/qa276_out/`(git 무시).
- 계약에 없는 키(입력 dtype/stride/길이, DLL SHA, 출력 길이 등)는 표준 필드에 넣지 않고 `provenance.json` 으로 나눴다.

**F262-6 (후보 입력이 막히는 지점, 추정 없이 옮김).** 출처는 Reviewer #262 와 `qa-input-provenance.md`(PRE-2G-COMPARE-20261020)다.
1. `Bright_Chest phantom_75kv_320ma_25.6(SID_110)_00.raw`: 길이 18,874,368, SHA `62f29792…57f4be`(이번에 다시 해시해 같음). 가장 먼저 `input.correction_state` 가 막힌다. 파일 이름과 `.raw` 확장자는 Current/Previous/Current-minus-Previous 취득 옵션을 입증하지 않는다. mode·실제 취득 조건·검출기 대응도 미확인. 파일명의 75kV/320mA/25.6/SID_110 을 검증된 측정값으로 채우지 않았다. UINT16LE·3072×3072 는 카드 지정이며 길이만으로 독립 입증되지 않는다.
2. `calfiles/`: 취득 조건(다선량 그룹)·보정 옵션·맵 구성원이 미확인이고, 폴더 전체를 단일 선량 이득 입력이라고 선언하지 않았다. 구체 차단 지점은 `engines.B.maps`, `applied_settings.source`, 나중에 2G 맵과의 일치 여부다.
3. `MasterDark.raw`: `calfiles/Dark_16.raw` 와 SHA 가 같다(`f5e030c8…acac4`). "10장 평균"이라는 이름은 생성 이력의 증거가 아니며, 독립 다크 1장 추가로 세지 않았다. 이번 맵 생성에도 쓰이지 않았다.
4. `_00_result.raw`: 길이 18,874,368 이라 README 의 FLOAT32 3072² 기대(37,748,736)와 충돌한다. 입력과 별개 파일이며 생성기·버전·맵·설정·단계가 미확인이라 2G 정답 출력으로 채택하지 않았고, `engines.A` 의 어떤 필드도 이 파일로 채우지 않았다.
5. 보정 상태 미확인은 common-condition 뿐 아니라 stored-array 라벨에서도 허용된 완화가 아니다(계약 `tolerated` 표에서 `CORRECTION_STATE_UNVERIFIED` 는 어느 라벨에도 없음).
이 카드의 PRE 출력 1건은 탐색 산출물이다. 동일조건 적합 입력·정식 비교·품질 합격이 아니다.

## validate 다시 돌린 결과

명령: `comparison_manifest.validate_pair(conditions.json, base_dir=D:/workspace-github)` (`tools/calibration/comparison_manifest.py`, main `47d932e3`, schema 4판). 출력 원문(`validate-output.txt`):

```
{
  "comparable": false,
  "comparison_kind": "not-comparable",
  "algorithm_attribution": false,
  "reasons": [
    "CORRECTION_STATE_UNVERIFIED",
    "MODE_MISSING",
    "ENGINE_OUT_OF_SCOPE",
    "UNSUPPORTED_FIELD",
    "GEOMETRY_MISMATCH",
    "DTYPE_MISMATCH",
    "STAGE_MISMATCH",
    "UNITS_MISMATCH",
    "STAGE_NOT_ATTRIBUTABLE",
    "MAP_IDENTITY_DIFFERS",
    "INVALID_ROI"
  ],
  "blockers": [
    "VENDOR_LIVE_BLOCKED"
  ],
  "actual_2g_executed": false,
  "identities": {
    "input": "62f2979250d5e0a503bf59fff5775f907d750e81226815275a6c2f291e57f4be",
    "B.output": "c2210d3dc5ee329bab6203435b4395eb52e1a7dad61d489530625801c2e9821a",
    "B.map.defect.xcal": "4579e06bcf5be09828935eb48c71bd78d1f51afc9fc97f83e678158dfa2f105c",
    "B.map.gain.xcal": "a7ddadbd5b69a350c44c638fd8887096ca4cb448eda322e5d8976575c7520d22",
    "B.map.offset.xcal": "5569bacf66fb25f10b9137ebf3f466e9ce8d94857c90dc4a7340df362c290c39"
  }
}
```

사유 11개 중 `CORRECTION_STATE_UNVERIFIED`, `MODE_MISSING` 은 입력 쪽 미확인이고, 나머지 9개(`ENGINE_OUT_OF_SCOPE` 부터 `INVALID_ROI`)는 A 가 없어서 나온다. "A 가 원인"이라는 말은 코드 이름만으로 정하지 않고 대조로 확인했다(`validate-control.txt` 원문):

```
control-1 (A = in-memory copy of B, comparison_kind as written): reasons = ["CORRECTION_STATE_UNVERIFIED", "MODE_MISSING"]
control-2 (control 1 + test-only state/mode): reasons = [] comparable = True
control-3 (control 1 with B.output.sha256 zeroed): reasons = ["CORRECTION_STATE_UNVERIFIED", "MODE_MISSING", "SHA_MISMATCH"]
```

- 대조 1: A 를 B 의 메모리 복사본으로 두면(저장·사용하지 않음, 2G 출력이 아님) 남는 사유는 보정 상태·mode 둘뿐. 즉 B 블록과 입력 쪽 나머지 필드에서는 다른 차단이 없다.
- 대조 3: B 필드를 일부러 망가뜨리면(SHA 0) `SHA_MISMATCH` 가 나온다. 검증기가 B 를 실제로 읽는다는 양성 대조다.
- 대조 2 의 "comparable = True" 는 시험 전용 값으로 얻은 형식 통과이며 아무 비교 가능성도 주장하지 않는다.

## 남은 차단 (이 카드로 풀리지 않는 것)

1. 2G(A) 출력 없음 — 제조사 실행 권한 gate 는 이 카드 범위 밖.
2. 입력 보정 상태·mode 미확인.
3. PRE 출력이 FLOAT32 이며 화소의 0.48 %가 65535 초과(최대 77956.156). 2G 쪽 자료형을 모르므로 `DTYPE_MISMATCH` 가능성이 남는다.
4. 소스 HEAD ↔ DLL 대응 미확인.
5. 맵이 이번 입력과 같은 취득 조건인지, 2G 쪽 맵과 바이트 동일한지 미확인(다르면 common-condition 불가, 자동 완화 없음).
6. `roi`·`comparison_kind` 는 자리표시.
