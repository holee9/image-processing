m1-baseline-v2  --  stage-1 reference image, the INPUT of stage 2 (QA-A-243, Refs #245)

WHAT THIS IS
  The real frame gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw processed by the stage-1 basic path of the
  preprocess module: offset -> gain -> defect correction, ghost off, temperature / nonlinearity / binning bypassed. Stage 1 fixed
  the procedure for the gain: it is made from the flats of ONE acquisition condition. Here that is flats 4, 5 and 6 of CalData_6
  (all three: this image is an input, not a pass/fail judgement). The older reference m1-baseline/ used a gain made from all six
  flats, which mix conditions (QA-A-242); it stays where it is as a regression pin.

FILES
  wrist_lat_3072x3072_corrected_v2_f32le.raw
      float32, little endian, 3072 x 3072, 37,748,736 bytes, row-major, top row first. The unmodified output of the shipping entry
      point xpe_preprocess_pipeline_out. Corrected values in ADU units (offset subtracted, gain applied). Use THIS file for any
      computation.
      sha256 d152e8ebba2cba142a95ba7ba981354a393fa18551b46572d8b1c3f36e8fdfd4
  wrist_lat_3072x3072_corrected_v2_DISPLAY_ONLY_p1-p99_linear_16bit.png
      16 bit grey PNG for LOOKING at the image. DISPLAY ONLY: a linear window from the image's 1st percentile (139.760 ADU) to its
      99th percentile (2833.780 ADU) mapped to 0..65535, values outside clipped (1.00 % below, 1.00 % above). It is not
      a diagnostic rendering and loses the values outside the window. Never feed it back into processing.
      sha256 b97741bbb933a0caeb2644a9999fd45ccd8526ea6fd2f6a038c54c66a29a3fa9

HOW IT WAS MADE (reproducible, from scratch, byte-identical)
  product + harness commit : 16cdbfc3  (dev/preprocess; product code of QA-A-241b c8d31bb2, harness test A241Measure.DISABLED_B10v2_Baseline)
  build                    : cmake --preset ci-preprocess ; cmake --build --preset ci-preprocess --parallel 2 --target xpe_preprocess_tests
  inputs                   : tests/test_data/CalData_6/{dark.raw, bright04.raw, bright05.raw, bright06.raw, BPMap.map}
                             gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw  (sha256 C823233F...9A3D70, from fixture-manifest.json)
  command (bash, one line) : XPE_A240_CAL=<repo>/tests/test_data/CalData_6 XPE_A240_WRIST=<repo>/gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw XPE_A243_OUT=<scratch dir> build/ci-preprocess/bin/xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter="A241Measure.DISABLED_B10v2_Baseline"
  what the harness does   : 1. xpe_calib_generate_offset(dark.raw, 1 frame, 100 ms, 25 C)
                            2. xpe_calib_generate_gain(bright04, bright05, bright06; no dark reference)   [dose-weighted ADU mean, see preprocess_api.h]
                            3. defect map = BPMap.map wrapped as an XCal defect file (23,505 pixels)
                            4. the three maps are given one session id, loaded, and the wrist frame goes through
                               xpe_preprocess_pipeline_out with {"bypassTemp":true,"bypassNonlinearity":true,"bypassBinning":true}, no ghost handle
  display file             : evidence/baseline_v2.py (numpy + Pillow) from the .raw above; the window is the 1st/99th percentile
  reproducibility          : the whole chain was run twice from scratch (maps regenerated each time, separate processes):
                             both outputs are byte-identical, sha256 d152e8ebba2cba142a95ba7ba981354a393fa18551b46572d8b1c3f36e8fdfd4

LIMITS (read before using it)
  - "flats 4, 5, 6 are one acquisition condition" is an ESTIMATE from their identical large-scale shape (QA-A-242); there is no dose / kVp /
    distance record. Flats 1 and 2 are estimated to be other conditions.
  - The acquisition condition of wrist_lat itself is unknown, so which gain suits it is unknown. This image is the stage-1 PROCEDURE
    applied to the wrist frame, not a claim that the wrist is perfectly flat-fielded.
  - The maps are generated from the repository's own dark and flat images with the product's generators; they are not production maps.
  - 42,026 gain pixels (the gain map classifies pixels outside 0.1..10 as defective) and the 23,505 BPMap pixels are filled from their neighbours
    by the defect stage.
  - The wrist frame's own field is NOT flattened by this correction. In air, away from the limb, the corrected background still rises 47 % from
    the top-left to the bottom-right block (with the old six-flat gain: 34 %; raw minus offset: 52 %), although the flats themselves vary 27 %
    across the frame and the gain removes that. The wrist was therefore exposed with a field (distance, collimation, tube geometry) the flats
    do not share, or the gradient is physical. This image is the procedure applied, not a flat-fielded wrist. (Measured in evidence/20_baseline_v2_stats.txt.)

OLD REFERENCE (kept as a regression pin)
  ../m1-baseline/wrist_lat_3072x3072_corrected_f32le.raw  sha256 42e78facd6cd75edbb59d26b9be1cca313d431d07900e3064cd472c068d1ef02   (gain from all six flats)
  Pixel statistics of v2 minus old: see evidence/20_baseline_v2_stats.txt (mean difference +7.179 ADU, std 55.332, max |diff| 650.4).
