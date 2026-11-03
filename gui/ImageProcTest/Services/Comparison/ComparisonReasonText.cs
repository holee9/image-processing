namespace ImageProcTest.Services.Comparison;

/// <summary>The Korean line shown next to a reason code of the comparison script (xpe-2g-pre-comparison/4). The code itself is always shown too: it is the contract's word. A code this table does not know is shown as it is.</summary>
public static class ComparisonReasonText
{
    public static string For(string code) => code switch
    {
        "SCHEMA_MISMATCH" => "스키마가 xpe-2g-pre-comparison/4 이 아닙니다.",
        "UNSUPPORTED_FIELD" => "이 스키마가 정의하지 않은 필드가 있거나 값 형식이 맞지 않습니다(무시하지 않고 거부).",
        "MISSING_FILE" => "매니페스트가 가리키는 파일이 없거나 경로가 비어 있습니다.",
        "PATH_OUTSIDE_BASE" => "경로가 매니페스트 폴더 밖을 가리킵니다.",
        "SHA_MISMATCH" => "파일의 SHA-256 이 매니페스트의 값과 다릅니다.",
        "CORRECTION_STATE_UNVERIFIED" => "입력의 보정 상태가 '확인됨 + 알려진 값 + 출처'가 아닙니다. 선언만 한 값이나 unknown 은 숫자를 막습니다.",
        "MODE_MISSING" => "입력의 모드가 비었거나 unknown 입니다.",
        "ENGINE_OUT_OF_SCOPE" => "엔진이 A(공식 Samsung 2G X64W)와 B(신규 PRE) 둘이 아닙니다(TAS 등은 범위 밖).",
        "LIVE_EXECUTION_NOT_AUTHORIZED" => "가져온 파일(imported-file)이 아닌 실행 방식입니다. 실제 실행은 허가되지 않았습니다.",
        "ENGINE_IDENTITY_MISMATCH" => "엔진 식별 정보가 맞지 않습니다(A: 공식 DLL SHA-256, B: 소스 HEAD 40자리와 설정 SHA-256).",
        "INPUT_LINK_MISMATCH" => "엔진 출력이 이 입력(SHA-256)에서 만들어졌다는 연결이 맞지 않습니다.",
        "MODE_MISMATCH" => "엔진의 모드가 입력의 모드와 다릅니다.",
        "MAPS_UNDECLARED" => "맵이 선언되지 않았고 '맵을 적용하지 않은 사유'도 없습니다.",
        "UNSUPPORTED_STAGE" => "출력 단계가 engine-direct / postprocessed 가 아닙니다.",
        "UNSUPPORTED_UNITS" => "출력 단위가 'pixel value' 가 아닙니다.",
        "UNSUPPORTED_DTYPE" => "지원하지 않는 자료형입니다(uint16, int16, uint32, int32, float32 만).",
        "UNSUPPORTED_ENDIAN" => "little endian 이 아닙니다.",
        "UNSUPPORTED_STRIDE" => "행 간격(stride)이 가로×자료형 크기와 다릅니다(채워진 행은 변환 없이 받지 않음).",
        "UNSUPPORTED_LAYOUT" => "배치가 항등 배치(좌상단 원점, 위→아래 행, 좌→우 열, 값 변환 없음)가 아닙니다.",
        "LAYOUT_UNVERIFIED" => "출력의 배치(layout)가 '확인됨 + 출처'가 아닙니다.",
        "SETTINGS_UNVERIFIED" => "엔진에 적용한 offset·gain·defect 설정이 '확인됨 + 출처'가 아닙니다.",
        "SETTINGS_DIFFER" => "A 와 B 에 적용한 offset·gain·defect 설정이 다릅니다.",
        "GEOMETRY_MISMATCH" => "A·B·입력의 가로·세로가 같지 않거나 양의 정수가 아닙니다.",
        "FILE_SIZE_MISMATCH" => "파일 크기가 offset + 가로×세로×자료형 크기와 다릅니다.",
        "DTYPE_MISMATCH" => "A 와 B 의 자료형이 다릅니다(부호 재해석 없음).",
        "STAGE_MISMATCH" => "A 와 B 의 출력 단계가 다릅니다.",
        "UNITS_MISMATCH" => "A 와 B 의 단위가 다릅니다.",
        "UNKNOWN_COMPARISON_KIND" => "비교 종류가 common-condition / e2e-non-isolated / stored-array-difference 가 아닙니다.",
        "MAP_IDENTITY_DIFFERS" => "A 와 B 의 맵 파일이 바이트 단위로 같지 않습니다. 알고리즘 효과를 분리하지 못합니다.",
        "INVALID_ROI" => "ROI 가 정수 좌표로 영상 안에 들어 있지 않습니다.",
        "NONFINITE_VALUES" => "ROI 안에 유한하지 않은 값이 있어 비교를 거부했습니다.",
        "SOURCE_CHANGED_DURING_RUN" => "비교하는 동안 원본 파일이 바뀌었습니다. 이 숫자는 안정된 값이 아니므로 실패로 기록합니다.",
        "EXCEPTION" => "비교 중 예외가 났습니다. 성공으로 기록하지 않습니다.",
        "STAGE_NOT_ATTRIBUTABLE" => "후처리(postprocessed) 출력은 알고리즘 효과로 해석할 수 없어 common-condition 에서 거부됩니다. 다른 두 라벨에서는 비귀속으로만 허용됩니다.",
        "VENDOR_LIVE_BLOCKED" => "실제 2G 실행은 막혀 있습니다(G-2G-READY 미충족).",
        _ => "(이 창이 모르는 사유 코드입니다 — 스크립트의 원문 코드를 그대로 보십시오.)",
    };
}
