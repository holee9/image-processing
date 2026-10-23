QA-A-252 work in progress, parked on 2026-10-16 when the lead stopped the card (QA-A-251b first, Codex #204 on the evaluation tool).
Nothing here is built or measured. The method definition (30_method_fixed_before_measurement.md, commit 4ba2e8bd) is unchanged.
 wip_xpe_bpm_v2.cpp.txt        src/xpe_bpm_v2.cpp (the detector; the linearity feature is already the amended one-sided robust z, see below)
 wip_test_bpm_v2.cpp.txt       tests/test_bpm_v2.cpp (8 tests; 7 passed, the stripes test missed its bound by one pixel: 9 flagged against 8 allowed)
 wip_tracked_changes.patch.txt diff of CMakeLists.txt (two sources), include/xpe/preprocess_api.h (xpe_bpm_generate_v2 and its types), tools/xpe_bpm_eval.cpp (map-v2); trailing blanks stripped
 ../02_patch_header.py.txt, ../03_patch_eval_map_v2.py.txt, ../04_patch_linearity_amendment.py.txt   the scripts that made those edits
AMENDMENT to the method (to be added to 30_method_fixed_before_measurement.md as section 6 before any measurement of the detector):
 The straight-line residual feature F3b is not judged by a chi-square variance ratio (2.4) but as a one-sided robust z of ln(SSE / (G - 2)), with the threshold from
 P(z > lambda) = T / N. Reason, seen on the synthetic frames of the unit test and NOT on the evaluation data: with unequal noise in the dose-group means the unweighted residual
 is not chi-square distributed; the chi-square form flagged 17-18 pixels against a target of 1, the z form 0.
