namespace ImageProcTest.Services.Comparison;

/// <summary>The Korean line shown next to a reason code of the comparison script (xpe-2g-pre-comparison/4). The code itself is always shown too: it is the contract's word. A code this table does not know is shown as it is.</summary>
public static class ComparisonReasonText
{
    public static string For(string code) => code switch
    {
        "SCHEMA_MISMATCH" => "Schema is not xpe-2g-pre-comparison/4.",
        "UNSUPPORTED_FIELD" => "An undefined field or invalid value type is rejected, not ignored.",
        "MISSING_FILE" => "A manifest file is missing or its path is empty.",
        "PATH_OUTSIDE_BASE" => "A path points outside the manifest directory.",
        "SHA_MISMATCH" => "File SHA-256 differs from the manifest.",
        "CORRECTION_STATE_UNVERIFIED" => "Input correction state needs verification, a known value and provenance. A declaration alone or unknown state blocks numeric comparison.",
        "MODE_MISSING" => "Input mode is empty or unknown.",
        "ENGINE_OUT_OF_SCOPE" => "Only A (official Samsung 2G X64W) and B (new PRE) are in scope; TAS and other engines are excluded.",
        "LIVE_EXECUTION_NOT_AUTHORIZED" => "Execution is not imported-file. Live execution is not authorized.",
        "ENGINE_IDENTITY_MISMATCH" => "Engine identity does not match: A requires the official DLL SHA-256; B requires a 40-character source HEAD and settings SHA-256.",
        "INPUT_LINK_MISMATCH" => "Engine output is not linked to this input SHA-256.",
        "MODE_MISMATCH" => "Engine mode differs from input mode.",
        "MAPS_UNDECLARED" => "Maps and the reason for not applying maps are both undeclared.",
        "UNSUPPORTED_STAGE" => "Output stage must be engine-direct or postprocessed.",
        "UNSUPPORTED_UNITS" => "Output units must be pixel value.",
        "UNSUPPORTED_DTYPE" => "Supported types are uint16, int16, uint32, int32 and float32.",
        "UNSUPPORTED_ENDIAN" => "Only little endian is supported.",
        "UNSUPPORTED_STRIDE" => "Stride differs from width × element size; padded rows require conversion.",
        "UNSUPPORTED_LAYOUT" => "Layout must be identity: top-left origin, rows downward, columns rightward, no value transform.",
        "LAYOUT_UNVERIFIED" => "Output layout needs verification and provenance.",
        "SETTINGS_UNVERIFIED" => "Applied offset/gain/defect settings need verification and provenance.",
        "SETTINGS_DIFFER" => "Applied offset/gain/defect settings differ between A and B.",
        "GEOMETRY_MISMATCH" => "A, B and input dimensions must match and be positive integers.",
        "FILE_SIZE_MISMATCH" => "File size differs from offset + width × height × element size.",
        "DTYPE_MISMATCH" => "A and B have different data types; signedness is not reinterpreted.",
        "STAGE_MISMATCH" => "A and B output stages differ.",
        "UNITS_MISMATCH" => "A and B output units differ.",
        "UNKNOWN_COMPARISON_KIND" => "Comparison kind must be common-condition, e2e-non-isolated or stored-array-difference.",
        "MAP_IDENTITY_DIFFERS" => "A and B map files differ byte for byte. Algorithm effects cannot be isolated.",
        "INVALID_ROI" => "ROI must use integer coordinates within the image.",
        "NONFINITE_VALUES" => "Nonfinite ROI values caused comparison rejection.",
        "SOURCE_CHANGED_DURING_RUN" => "Source files changed during comparison. Values are unstable and recorded as a failure.",
        "EXCEPTION" => "An exception occurred during comparison; it is not recorded as success.",
        "STAGE_NOT_ATTRIBUTABLE" => "Postprocessed output cannot isolate algorithm effects and is rejected for common-condition; the other two labels allow it only as non-attributable.",
        "VENDOR_LIVE_BLOCKED" => "Live 2G execution is blocked: G-2G-READY is not satisfied.",
        _ => "Unknown reason code: refer to the script's original code.",
    };
}
