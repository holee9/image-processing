// The old generator's output for the real crop (QA-A-210c): the pixels it fitted at degree 2 (max degree 2), as the bit
// patterns of the stored float32 c0, c1, c2. Captured with `--gtest_also_run_disabled_tests
// --gtest_filter=GainPolyPolicyTest.DISABLED_PrintGolden` against the generator of 63559d35/642503e8 (evidence/02_golden_capture_old_generator.txt).
struct GoldenPixel { size_t pix; uint32_t c0, c1, c2; };
static const GoldenPixel kGoldenMax2[] = {
    {10u, 0x3F88BF83u, 0x3308493Du, 0x2A522754u},
    {29u, 0x3F882804u, 0x348E2657u, 0xAB07CB31u},
    {84u, 0x3F881742u, 0x3207BEC1u, 0x2C4F85E2u},
    {130u, 0x3F876193u, 0x34453E1Au, 0x2AEBDCC1u},
    {131u, 0x3F87D255u, 0x31ABFC00u, 0x2B231EB9u},
    {135u, 0x3F86C1C6u, 0x34092A22u, 0x2C887D46u},
    {147u, 0x3F87BDC9u, 0x336F648Au, 0xAB821065u},
    {149u, 0x3F87E70Eu, 0x3304B6A1u, 0x2CCD6CE3u},
    {163u, 0x3F86F6C3u, 0x34FDD11Au, 0xAC298057u},
    {164u, 0x3F877DFBu, 0x3491569Du, 0xAB2BA709u},
    {167u, 0x3F87DF47u, 0x341F3CA6u, 0xAB80753Bu},
    {173u, 0x3F88991Cu, 0x343BC051u, 0xAC3A46F8u},
    {178u, 0x3F87CA14u, 0x34D0388Du, 0xACBB3F1Au},
    {180u, 0x3F86DDF4u, 0x358A6396u, 0xAD95DBB3u},
    {183u, 0x3F872FE0u, 0x350502FFu, 0xAD02AFE3u},
    {196u, 0x3F8820FBu, 0x2FB96BF5u, 0x2CDDA638u},
    {219u, 0x3F87A827u, 0x355C2EEEu, 0xAD707AF8u},
    {235u, 0x3F88FDF4u, 0x34712B96u, 0xAAE37A97u},
    {244u, 0x3F87DBEAu, 0x336D476Eu, 0x2C8C2C69u},
    {254u, 0x3F88834Cu, 0x32DC25B4u, 0x2C1B2075u},
};
