# 52-slot static matrix

S = `D:/workspace/ProjectGit/PE_analysis/param_tuner`. Expected `D:/workspace/ProjectGit/PE_analysis/run_pipeline.py` is missing; external run_pipeline defaults remain UNRESOLVED, no alternative module search/import. GUI initial-value expressions and fallback behavior are verified, actual numeric startup values are not.

Common GUI trace: app.py:223-225 -> 536-564 -> 953-970 -> 1036/1057 -> Worker:180-198. Integer text parsing, no slider or configured step; min/max expanded to include initial. All stored slots: param_space.py:166-174. Fixed rows have no table cell and are re-created by default_params, including after JSON load.

|Slot|Name / constant|Declared range|Default @ run_pipeline line|GUI range/init/search|Definition line|Judgment|
|---|---|---|---|---|---|---|
|post.0|Contrast / CONTRAST|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:24|일치 (table mapping)|
|post.1|Brightness / BRIGHTNESS|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:25|일치 (table mapping)|
|post.2|Latitude / LATITUDE|0..10|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:27|일치 (table mapping)|
|post.3|Latitude Over / LATITUDE_OVER|0..25|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:28|일치 (table mapping)|
|post.4|Latitude ROI / LATITUDE_ROI|0..20|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:30|일치 (table mapping)|
|post.5|Large Black Halo / LARGE_BLACK_HALO|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:43|일치 (table mapping)|
|post.6|IBDC_W_01 / IBDC_W_01|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:50|일치 (table mapping)|
|post.7|ROI Type / ROI_TYPE|0..4|UNRESOLVED @ ?|min/max(default); UNRESOLVED; Chest off / other on|param_space.py:31|일치 (table mapping)|
|post.8|Feature Pt / FEATURE_PT|0..50|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:32|일치 (table mapping)|
|post.9|Key Feature L / KEY_FEATURE_PERCENT_L|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; Chest on / other off|param_space.py:33|일치 (table mapping)|
|post.10|Key Feature S / KEY_FEATURE_PERCENT_S|-20..20|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:34|일치 (table mapping)|
|post.11|Target Bright L / TARGET_BRIGHTNESS_L|0..50|UNRESOLVED @ ?|min/max(default); UNRESOLVED; off (user may check)|param_space.py:35|일치 (table mapping)|
|post.12|Target Bright S / TARGET_BRIGHTNESS_S|0..50|UNRESOLVED @ ?|min/max(default); UNRESOLVED; off (user may check)|param_space.py:36|일치 (table mapping)|
|post.13|Latitude Under / LATITUDE_UNDER|-5..25|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:29|일치 (table mapping)|
|post.14|Fine Sharpness / FINE_SHARPNESS|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:37|일치 (table mapping)|
|post.15|Sharpness / SHARPNESS|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:38|일치 (table mapping)|
|post.16|Struct Strength2 / STRUCT_STRENGTH2|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:39|일치 (table mapping)|
|post.17|Struct Strength3 / STRUCT_STRENGTH3|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:40|일치 (table mapping)|
|post.18|fixed STRUCT_CONTRAST0|fixed|UNRESOLVED @ ?|no row; default_params restores|param_space.py:13,115-116|코드에 없음 (editable row)|
|post.19|Struct Contrast1 / STRUCT_CONTRAST1|0..20|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:41|일치 (table mapping)|
|post.20|Struct Contrast2 / STRUCT_CONTRAST2|0..20|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:42|일치 (table mapping)|
|post.21|Struct Density / STRUCT_DENSITY|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:44|일치 (table mapping)|
|post.22|fixed 31|fixed|31 @ literal|no row; default_params restores|param_space.py:13,115-116|코드에 없음 (editable row)|
|post.23|Noise Reduction / NOISE_REDUCTION|0..5|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:58|일치 (table mapping)|
|post.24|NR Selection / NR_SELECTION|0..1|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:57|일치 (table mapping)|
|post.25|NR Deblurring / NR_DEBLURRING|0..8|UNRESOLVED @ ?|min/max(default); UNRESOLVED; off (user may check)|param_space.py:59|일치 (table mapping)|
|post.26|NR Whitening / NR_WHITENING|0..5|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:60|일치 (table mapping)|
|post.27|fixed 0|fixed|0 @ literal|no row; default_params restores|param_space.py:13,115-116|코드에 없음 (editable row)|
|post.28|NECR_W_01 / NECR_W_01|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:46|일치 (table mapping)|
|post.29|NECR_W_23 / NECR_W_23|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:47|일치 (table mapping)|
|post.30|NECR_B_01 / NECR_B_01|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:48|일치 (table mapping)|
|post.31|NECR_B_23 / NECR_B_23|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:49|일치 (table mapping)|
|post.32|Chest Balance / CHEST_BALANCE|0..50|UNRESOLVED @ ?|min/max(default); UNRESOLVED; Chest on / other off|param_space.py:26|일치 (table mapping)|
|post.33|Detail ROI / DETAIL_ROI|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:45|일치 (table mapping)|
|post.34|Small White Halo / SMALL_WHITE_HALO|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:54|일치 (table mapping)|
|post.35|Small Black Halo / SMALL_BLACK_HALO|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:55|일치 (table mapping)|
|post.36|IBDC_W_23 / IBDC_W_23|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:51|일치 (table mapping)|
|post.37|IBDC_B_01 / IBDC_B_01|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:52|일치 (table mapping)|
|post.38|IBDC_B_23 / IBDC_B_23|0..30|UNRESOLVED @ ?|min/max(default); UNRESOLVED; on|param_space.py:53|일치 (table mapping)|
|post.39|fixed 1|fixed|1 @ literal|no row; default_params restores|param_space.py:13,115-116|코드에 없음 (editable row)|
|lut.0|LUT_CP1_X|0..100|UNRESOLVED @ ?|0..100; UNRESOLVED; on|param_space.py:119-121,154-159|일치 (table mapping)|
|lut.1|LUT_CP1_Y|0..100|UNRESOLVED @ ?|0..100; UNRESOLVED; on|param_space.py:119-121,154-159|일치 (table mapping)|
|lut.2|LUT_CP1_W|fixed|UNRESOLVED @ ?|no row; default_params restores|param_space.py:119-121,154-159|코드에 없음 (W editable row)|
|lut.3|LUT_CP2_X|0..100|UNRESOLVED @ ?|0..100; UNRESOLVED; on|param_space.py:119-121,154-159|일치 (table mapping)|
|lut.4|LUT_CP2_Y|0..100|UNRESOLVED @ ?|0..100; UNRESOLVED; on|param_space.py:119-121,154-159|일치 (table mapping)|
|lut.5|LUT_CP2_W|fixed|UNRESOLVED @ ?|no row; default_params restores|param_space.py:119-121,154-159|코드에 없음 (W editable row)|
|lut.6|LUT_CP3_X|0..100|UNRESOLVED @ ?|0..100; UNRESOLVED; on|param_space.py:119-121,154-159|일치 (table mapping)|
|lut.7|LUT_CP3_Y|0..100|UNRESOLVED @ ?|0..100; UNRESOLVED; on|param_space.py:119-121,154-159|일치 (table mapping)|
|lut.8|LUT_CP3_W|fixed|UNRESOLVED @ ?|no row; default_params restores|param_space.py:119-121,154-159|코드에 없음 (W editable row)|
|lut.9|LUT_CP4_X|0..100|UNRESOLVED @ ?|0..100; UNRESOLVED; on|param_space.py:119-121,154-159|일치 (table mapping)|
|lut.10|LUT_CP4_Y|0..100|UNRESOLVED @ ?|0..100; UNRESOLVED; on|param_space.py:119-121,154-159|일치 (table mapping)|
|lut.11|LUT_CP4_W|fixed|UNRESOLVED @ ?|no row; default_params restores|param_space.py:119-121,154-159|코드에 없음 (W editable row)|

Counts: Post 40 + LUT 12 = 52 serialized; 36 Post + 8 LUT XY = 44 GUI rows; 4 Post + 4 LUT W = 8 hidden/fixed slots. Unused/conditional checkboxes remain user-checkable (app.py:546); automatic masking is not a hard lock.
