/**
 * @file DicomReader.cpp
 * @brief DICOM Part 10 file reader implementation (SWU-4.1).
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-001..012
 */
#include "DicomReader.h"

#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcpxitem.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#include <openjpeg.h>

#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_memory.h"

#include <spdlog/spdlog.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <mutex>
#include <vector>

namespace xpe {
namespace dicom {

namespace {

// #146: DCMTK decodes JPEG only through codecs that must be registered first.
// REQ-DICOM-004 lists JPEG Lossless (1.2.840.10008.1.2.4.70) among the transfer
// syntaxes this module SHALL read, and REQ-DICOM-008 requires the pixel data to
// be decompressed -- but no registration existed, so open() accepted the syntax
// and readImage then failed on a dataset it could not convert (QA-B-44 found
// this; QA-B-45 fixes it).
//
// Registered once per process, on the first open(). DCMTK's registration
// mutates global codec tables, so it is guarded by call_once rather than left
// to whichever thread arrives first.
//
// There is no matching cleanup() call: this module exports no shutdown entry
// point, and the codec tables live as long as the process. That is a deliberate
// asymmetry, recorded in the QA-B-45 report rather than hidden.
//
// J2K is unaffected -- that path calls OpenJPEG directly and never consults
// DCMTK's codec tables.
void ensure_jpeg_codecs_registered() {
    static std::once_flag once;
    std::call_once(once, []() { DJDecoderRegistration::registerCodecs(); });
}

// #150 (QA-B-50): read the frame size out of a JPEG bitstream's SOF marker.
//
// Why this is needed at all: DCMTK decompresses a JPEG frame INTO a buffer sized
// from the dataset's Rows/Columns, so a frame carrying fewer rows than the header
// declares comes back padded to the declared size. By the time the native
// short-PixelData guard (#150) looks, there is no shortfall left to see -- the
// image is full-size with a black lower half, which is exactly the HAZ-DCM-002
// hazard that guard exists to stop. The only place the real height still exists
// is the JPEG frame header itself, so that is where this looks.
//
// Returns false when no SOF marker is found. A false is NOT a verdict: the
// caller must not reject on it, because "we could not read the frame header" is
// a different statement from "the frame is too small".
bool jpeg_frame_dimensions(const Uint8* data, size_t len, uint32_t& outW, uint32_t& outH, uint32_t* outPrecision = nullptr, uint32_t* outComponents = nullptr) {
    if (data == nullptr || len < 4) return false;
    size_t i = 0;
    if (!(data[0] == 0xFF && data[1] == 0xD8)) return false;   // SOI
    i = 2;
    while (i + 3 < len) {
        if (data[i] != 0xFF) { ++i; continue; }                // resync on fill bytes
        const uint8_t marker = data[i + 1];
        if (marker == 0xFF) { ++i; continue; }
        if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            i += 2;                                            // standalone markers
            continue;
        }
        if (i + 3 >= len) return false;
        const size_t segLen = (static_cast<size_t>(data[i + 2]) << 8) | data[i + 3];
        // SOF0..SOF3, SOF5..SOF7, SOF9..SOF11, SOF13..SOF15 all carry the frame
        // header in the same layout; JPEG Lossless (Process 14) is SOF3 (0xC3).
        const bool isSOF = (marker >= 0xC0 && marker <= 0xCF) &&
                           marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (isSOF) {
            // length(2) precision(1) height(2) width(2)
            if (i + 8 >= len) return false;
            if (outPrecision != nullptr) *outPrecision = data[i + 4];   // QA-B-182e: sample precision P
            // Nf, the component count. QA-B-181h: it exists only if the SEGMENT is long enough to hold it (length(2) +
            // precision(1) + height(2) + width(2) + Nf(1) = 8) and the data reaches it; otherwise it is reported as 0, and
            // the caller refuses a header whose Nf is anything but exactly 1. 0 is therefore a real answer ("no
            // component, or no Nf byte"), never a "not read" marker -- it used to be both.
            if (outComponents != nullptr) *outComponents = (segLen >= 8 && i + 9 < len) ? data[i + 9] : 0;
            outH = (static_cast<uint32_t>(data[i + 5]) << 8) | data[i + 6];
            outW = (static_cast<uint32_t>(data[i + 7]) << 8) | data[i + 8];
            return outW != 0 && outH != 0;
        }
        if (marker == 0xDA) return false;                      // reached scan data
        if (segLen < 2) return false;
        i += 2 + segLen;
    }
    return false;
}

}  // namespace

// Supported Transfer Syntax UIDs
static constexpr const char* TS_EXPLICIT_LE  = "1.2.840.10008.1.2.1";
static constexpr const char* TS_J2K_LOSSLESS = "1.2.840.10008.1.2.4.90";
static constexpr const char* TS_JPEG_LL      = "1.2.840.10008.1.2.4.70";
// #147 (QA-B-68): Process 14 without first-order prediction. A different
// bitstream from .70 despite both being called "JPEG Lossless" -- that shared
// name is what let the two requirements drift apart (REQ-IOP-003 names .57,
// REQ-DICOM-004 names .70).
static constexpr const char* TS_JPEG_LL_P14  = "1.2.840.10008.1.2.4.57";

// @MX:ANCHOR: [AUTO] DicomReader constructor — maps from opaque XpeDicomHandle
// @MX:REASON: fan_in >= 3: xpe_dicom_open allocates, readImage and getMetadata use, xpe_dicom_close frees
// @MX:SPEC: SPEC-XPE-P1B-DICOM SWU-4.1
DicomReader::DicomReader(const std::string& filePath)
    : m_filePath(filePath)
    , m_dcmFile(std::make_unique<DcmFileFormat>())
{}

DicomReader::~DicomReader() = default;

XpeErrorCode DicomReader::open() {
    ensure_jpeg_codecs_registered();
    spdlog::debug("[DicomReader] open: {}", m_filePath);

    // Load the DICOM file with unknown transfer syntax (auto-detect)
    OFCondition status = m_dcmFile->loadFile(
        m_filePath.c_str(),
        EXS_Unknown,
        EGL_noChange,
        DCM_MaxReadLength
    );

    if (status.bad()) {
        OFString errText = status.text();
        spdlog::warn("[DicomReader] loadFile failed: {}", errText.c_str());

        // If load fails with "no element found" or similar DICOM parse error
        // it might be a non-DICOM file. Check by trying minimal read.
        // DCMTK returns EC_InvalidTag, EC_StreamNotifyClient, etc. for bad DICOM
        // and platform-specific I/O errors for missing files.

        // Attempt to probe for file existence via a minimal load
        if (status == EC_InvalidFilename || status == EC_IllegalParameter) {
            return XPE_ERR_IO_FAILED;
        }

        // Check if the error is an I/O error (file not found) vs parse error
        std::string errStr(errText.c_str());
        if (errStr.find("No such file") != std::string::npos ||
            errStr.find("cannot open") != std::string::npos ||
            errStr.find("can't open") != std::string::npos ||
            errStr.find("not found") != std::string::npos ||
            errStr.find("cannot be found") != std::string::npos ||
            status == OFCondition(OFM_dcmdata, 18, OF_error, "")) {
            return XPE_ERR_IO_FAILED;
        }

        // For DICOM parse errors (bad preamble, missing required elements, etc.)
        return XPE_ERR_DICOM_INVALID;
    }

    // Validate DICOM Part 10 meta-information header
    DcmMetaInfo* meta = m_dcmFile->getMetaInfo();
    if (!meta) {
        // #167 (QA-B-72): this branch is CLOSED, not handled.
        //
        // It used to accept the file and record Explicit VR Little Endian with no
        // syntax check. QA-B-71/72 could not produce any input that reaches it --
        // five attempts (empty file, preamble only, random bytes, a lone tag, and
        // a DcmFileFormat that was never loaded) all returned a non-NULL
        // getMetaInfo(). With no reachable input there is no way to learn whether
        // a detected syntax would be available or correct here, so the branch
        // fails closed rather than guessing. It has NO execution test; the
        // decision rests on the reachability measurement alone.
        spdlog::warn("[DicomReader] no meta-information object; transfer syntax "
                     "cannot be established -- refusing");
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    // Check Transfer Syntax UID from meta-header
    OFString tsUID;
    if (meta->findAndGetOFString(DCM_TransferSyntaxUID, tsUID).good()) {
        bool accepted = false;
        for (size_t i = 0; i < kSupportedTransferSyntaxCount; ++i) {
            if (tsUID == kSupportedTransferSyntaxes[i].uid) {
                accepted = true;
                break;
            }
        }
        if (!accepted) {
            spdlog::warn("[DicomReader] Unsupported Transfer Syntax: {}", tsUID.c_str());
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }
        m_tsUID = std::string(tsUID.c_str());
    } else {
        // #167 (QA-B-72): a meta-header with no TransferSyntaxUID. This branch
        // used to ASSUME Explicit VR Little Endian and skip the accepted-list
        // check -- so a file refused when labelled was read when the label was
        // missing (QA-B-69: Implicit VR LE and Explicit VR BE both came back as
        // full frames). Every meta-less fixture QA-B-71 measured took this
        // branch, not the one above.
        //
        // Two checks now, each closing a case the other cannot:
        DcmDataset* ds = m_dcmFile->getDataset();
        if (!ds) {
            return XPE_ERR_DICOM_INVALID;
        }
        const E_TransferSyntax detected = ds->getOriginalXfer();
        const DcmXfer detectedXfer(detected);
        const std::string detectedUid =
            (detected == EXS_Unknown) ? std::string() : std::string(detectedXfer.getXferID());

        // (1) The syntax DCMTK detected must be on the accepted list -- the same
        //     list a labelled file is held to. This is what closes the native
        //     leaks: QA-B-71 measured the detection as correct for Explicit LE,
        //     Implicit LE and Explicit BE.
        bool accepted = false;
        for (size_t i = 0; i < kSupportedTransferSyntaxCount; ++i) {
            if (detectedUid == kSupportedTransferSyntaxes[i].uid) {
                accepted = true;
                break;
            }
        }
        if (!accepted) {
            spdlog::warn("[DicomReader] no TransferSyntaxUID in meta; detected "
                         "syntax '{}' is not supported -- refusing",
                         detectedUid.empty() ? "(unknown)" : detectedUid);
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }

        // (2) The detection cannot be trusted when it contradicts the pixel data.
        //     QA-B-71 measured encapsulated .70 and .57 files DETECTED as Explicit
        //     VR Little Endian -- a value that is on the list, so (1) passes them.
        //     They were still stopped, but only by the native read failing on an
        //     encapsulated stream (DICOM_INVALID from readImage): structure, not a
        //     check. An encapsulated PixelData is written with an undefined length
        //     field, and QA-B-72 measured that field alone as separating the two
        //     groups (native 0/3, encapsulated 2/2) -- unlike the syntax-keyed
        //     getEncapsulatedRepresentation(), which returned false for BOTH
        //     encapsulated files because DCMTK had filed them as uncompressed.
        //     So the syntax-free signal is the one used here.
        DcmElement* pix = nullptr;
        const bool pixelDataEncapsulated =
            ds->findAndGetElement(DCM_PixelData, pix).good() && pix != nullptr &&
            pix->getLengthField() == DCM_UndefinedLength;
        if (pixelDataEncapsulated && !detectedXfer.usesEncapsulatedFormat()) {
            spdlog::warn("[DicomReader] no TransferSyntaxUID in meta; PixelData is "
                         "encapsulated but the detected syntax '{}' is native -- the "
                         "detection cannot be trusted, refusing", detectedUid);
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }

        // For every file measured, detectedUid here is Explicit VR Little Endian,
        // which is what this branch used to hard-code. Recording the detected
        // value rather than the assumption is what keeps readImage's dispatch
        // honest if DCMTK ever detects an accepted encapsulated syntax.
        m_tsUID = detectedUid;
    }

    m_opened = true;
    return XPE_OK;
}

// QA-B-182 / QA-B-182b (#235, QA-B-180): what the reader can hand back is ONE plane of UNSIGNED 16-bit words.
// A dataset that says otherwise used to be copied anyway and come back as OK with wrong pixels: a signed pixel
// became a large unsigned one, a 3-frame image became its first frame, an RGB image became byte pairs read as 16-bit
// words, a 1- or 32-bit image became 16-bit words, an 8-bit image was refused as "short" (a corrupt file) when it
// is merely unsupported. They are refused here, before anything is allocated or written, so outImg is untouched and
// the handle still answers the same way (the QA-B-48 contract; the handle's internal parse state is not part of it,
// DCMTK loads elements lazily).
//
// Two kinds of refusal, kept apart on purpose:
//   XPE_ERR_DICOM_INVALID       the file is malformed: a Type 1 attribute of the Image Pixel module (PS3.3
//                               C.7.6.3.1.1: Samples per Pixel, Pixel Representation, Bits Allocated, Bits Stored,
//                               High Bit) is absent,
//                               empty or not a number, or Number of Frames is present and not a positive number.
//                               There is no default for a Type 1 attribute; reading its absence as a value was how
//                               a damaged file used to be accepted.
//   XPE_ERR_UNSUPPORTED_FORMAT  the file is well formed and this reader cannot return it faithfully.
// Number of Frames alone may be absent (it belongs to the Multi-frame Module): absent means one frame.
//
// Native and JPEG Lossless pixel data are copied as 16-bit words, so they are accepted only as
// Bits Allocated == 16, Bits Stored <= 16, High Bit == Bits Stored - 1 (8 bits included: the general rule absorbs
// the earlier special case). A Bits Stored / High Bit that breaks the rule is unsupported. Both are Type 1 as
// well (QA-B-182c): absent, empty or unreadable is malformed, with no default -- a default would make the rule
// and the J2K precision check test a value the reader invented.
//
// JPEG 2000 is judged against its codestream (decodeJ2KBitstream, before any output is allocated): PS3.5 8.2.4
// requires these attributes to be consistent with the codestream, and the codestream's own characteristics are the
// ones used for decoding. Here only what the dataset alone can say is checked: Bits Allocated is 8 or 16.
//
// Rescale is judged in checkRescale and the bits above BitsStored are masked in maskToBitsStored (QA-B-187).
// MONOCHROME1 is accepted here and INVERTED after decoding (QA-B-185, normaliseMonochrome1).
// A refusal is reported twice: to the log, and as an ALERT the operator can read. The return code alone says
// "malformed" or "unsupported" but not which attribute or which two values disagree (QA-B-182c). The alert wording
// is a contract with the clients that display alerts: change it only together with them.
static XpeErrorCode refuse(XpeErrorCode code, const char* fmt, ...) {
    char why[320];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(why, sizeof(why), fmt, args);
    va_end(args);
    char msg[400];
    std::snprintf(msg, sizeof(msg), "dicom read refused: %s", why);
    spdlog::warn("[DicomReader] {}", msg);
    xpe_alert_push(msg, XPE_ALERT_ERROR);
    return code;
}

// A Type 1 attribute: present, with a value, readable as one unsigned short.
static bool readType1Uint16(DcmDataset* ds, const DcmTagKey& key, Uint16& out) {
    return ds->tagExists(key) && ds->findAndGetUint16(key, out).good();
}

// QA-B-185 (#235; leader decision after the QA-B-184 report, requirement FR-DCM-109 of SRS-DICOM-001): the image is
// returned in MONOCHROME2 sense whatever the file says. PS3.3 C.7.6.3.1.2 defines MONOCHROME1 as "the minimum sample
// value is intended to be displayed as white" and MONOCHROME2 as "... black", so a MONOCHROME1 sample v becomes the
// value MONOCHROME2 would have shown the same brightness for. The sample is the low BitsStored bits of the word, so
// the bits above it are masked off first (they are not part of the sample) and the maximum is 2^BitsStored - 1:
//     v' = (2^B - 1) - (v & (2^B - 1)).
// It runs on the CALLER'S buffer after the decode, never on the dataset the handle keeps, so a second read on the same
// handle gives the same words. Every path ends here: the uncompressed and JPEG Lossless copy and the JPEG 2000 decode
// all produce one unsigned uint16 plane (signed pixels, PixelRepresentation 1, were refused before any decode -- for
// them the inversion would be -1 - v, a different formula, and no signed image is returned today). outImg->bitsStored
// is the sample width the returned words really have (the codestream's precision for JPEG 2000).
// The Window Center/Width and Rescale values in the FILE refer to the STORED samples (PS3.3 C.11.2: the polarity is
// applied after the VOI transformation). This API returns neither (the metadata struct has no such field), so no
// returned value needs adjusting; a caller reading them from the file itself must mirror them -- see dicom_api.h.
static bool isMonochrome1(DcmDataset* ds) {
    OFString v;
    return ds->findAndGetOFString(DCM_PhotometricInterpretation, v).good() && std::string(v.c_str()) == "MONOCHROME1";
}

static void normaliseMonochrome1(XpeImageBuffer* img) {
    const uint32_t bits = img->bitsStored;   // 1..16 here: BitsStored <= BitsAllocated 16, JPEG 2000 precision <= 16
    const uint32_t mask = bits >= 16 ? 0xFFFFu : ((1u << bits) - 1u);
    uint16_t* px = static_cast<uint16_t*>(img->data);
    const size_t n = static_cast<size_t>(img->width) * img->height;
    for (size_t i = 0; i < n; ++i) px[i] = static_cast<uint16_t>(mask - (px[i] & mask));
    // CROSS-LANE CONTRACT (QA-B-185b): the whole text, including the formula, which is exactly what is computed above
    // (the stored word is masked to BitsStored first). Change it only together with the clients.
    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "MONOCHROME1 pixel values were inverted to MONOCHROME2 sense: "
                  "value = (2^BitsStored - 1) - (stored & (2^BitsStored - 1)), BitsStored %u",
                  static_cast<unsigned>(bits));
    spdlog::info("[DicomReader] {}", msg);
    xpe_alert_push(msg, XPE_ALERT_INFO);
}

// QA-B-187 (#235; leader decisions after the QA-B-186 matrix): the Modality LUT's Rescale Slope / Intercept are NOT
// applied -- the returned pixels are the stored values, and applying or reporting the rescale is a separate decision --
// but they are no longer ignored in silence:
//   * a Rescale attribute that is present and is not ONE finite number (empty, text, two values, inf/nan), or a Rescale
//     Slope of 0 (the rescale would send every pixel to the intercept), is a malformed dataset: XPE_ERR_DICOM_INVALID,
//     before any decode, with an alert that names the attribute and the value;
//   * a rescale that is not the identity (slope 1 and intercept 0; an absent attribute counts as its identity value)
//     posts ONE Warning after a successful read: the pixels are stored values and the rescale was not applied.
// The check runs for every path because it runs before the path-specific decode.
struct RescaleNote {
    bool nonIdentity = false;
    bool lutSequence = false;   // a Modality LUT Sequence (0028,3000) with at least one item
    std::string slope = "(absent)";
    std::string intercept = "(absent)";
};

static std::string trimSpaces(const char* raw) {
    std::string s(raw);
    const size_t b = s.find_first_not_of(' ');
    if (b == std::string::npos) return std::string();
    return s.substr(b, s.find_last_not_of(' ') - b + 1);
}

// One decimal string (PS3.5 VR DS: digits, sign, '.', 'E'/'e') that is a finite number. A backslash would be a second value.
static bool parseDecimalString(const std::string& s, double* out) {
    if (s.empty() || s.find_first_not_of("0123456789+-.eE") != std::string::npos) return false;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || !std::isfinite(v)) return false;
    *out = v;
    return true;
}

static XpeErrorCode readRescaleAttribute(DcmDataset* ds, const DcmTagKey& key, const char* label, const char* tag,
                                         double identityValue, double* out, std::string* text, bool* present) {
    *out = identityValue;
    *present = ds->tagExists(key);
    if (!*present) return XPE_OK;
    OFString raw;
    const OFCondition c = ds->findAndGetOFStringArray(key, raw);
    const std::string s = trimSpaces(raw.c_str());
    if (c.bad() || !parseDecimalString(s, out)) {
        return refuse(XPE_ERR_DICOM_INVALID, "%s (%s) \"%s\" is not a single finite number", label, tag, s.c_str());
    }
    *text = s;
    return XPE_OK;
}

static XpeErrorCode checkRescale(DcmDataset* ds, RescaleNote* note) {
    double slope = 1.0, intercept = 0.0;
    bool hasSlope = false, hasIntercept = false;
    XpeErrorCode rc = readRescaleAttribute(ds, DCM_RescaleSlope, "RescaleSlope", "0028,1053", 1.0, &slope, &note->slope, &hasSlope);
    if (rc != XPE_OK) return rc;
    rc = readRescaleAttribute(ds, DCM_RescaleIntercept, "RescaleIntercept", "0028,1052", 0.0, &intercept, &note->intercept, &hasIntercept);
    if (rc != XPE_OK) return rc;
    // PS3.3 C.11.1: the Slope / Intercept pair is required together; one without the other is not a Modality LUT.
    if (hasSlope != hasIntercept) {
        return refuse(XPE_ERR_DICOM_INVALID, "%s is absent while %s is present: the Modality LUT needs both RescaleSlope and RescaleIntercept (PS3.3 C.11.1)",
                      hasSlope ? "RescaleIntercept (0028,1052)" : "RescaleSlope (0028,1053)",
                      hasSlope ? "RescaleSlope (0028,1053)" : "RescaleIntercept (0028,1052)");
    }
    DcmSequenceOfItems* lut = nullptr;
    note->lutSequence = ds->findAndGetSequence(DCM_ModalityLUTSequence, lut).good() && lut != nullptr && lut->card() > 0;
    if (slope == 0.0) {
        return refuse(XPE_ERR_DICOM_INVALID, "RescaleSlope (0028,1053) is zero: the rescale would map every pixel to the intercept");
    }
    note->nonIdentity = slope != 1.0 || intercept != 0.0;
    return XPE_OK;
}

// QA-B-187: the bits above BitsStored are not part of the sample (PS3.5 8.1.1), so every returned word is
// stored & (2^BitsStored - 1) -- MONOCHROME2 as well as MONOCHROME1 (which was already masked before its inversion).
// Returns how many words changed. BitsStored 16 masks nothing, so the pass is skipped.
static size_t maskToBitsStored(XpeImageBuffer* img) {
    const uint32_t bits = img->bitsStored;   // 1..16 here: BitsStored <= BitsAllocated 16, JPEG 2000 precision <= 16
    if (bits >= 16) return 0;
    const uint16_t mask = static_cast<uint16_t>((1u << bits) - 1u);
    uint16_t* px = static_cast<uint16_t*>(img->data);
    const size_t n = static_cast<size_t>(img->width) * img->height;
    size_t changed = 0;
    for (size_t i = 0; i < n; ++i) {
        const uint16_t v = static_cast<uint16_t>(px[i] & mask);
        if (v != px[i]) { px[i] = v; ++changed; }
    }
    return changed;
}

// What every successful read does to the decoded words, in this order, whichever path produced them (QA-B-185/187).
static void finishRead(XpeImageBuffer* img, bool mono1, const RescaleNote& rescale) {
    const size_t masked = maskToBitsStored(img);
    if (masked > 0) {
        // CROSS-LANE CONTRACT (QA-B-187): the whole text. Posted only when at least one word changed.
        char msg[200];
        std::snprintf(msg, sizeof(msg),
                      "%llu pixel(s) had bits above BitsStored %u set; those bits were masked off: value = stored & (2^BitsStored - 1)",
                      static_cast<unsigned long long>(masked), static_cast<unsigned>(img->bitsStored));
        spdlog::info("[DicomReader] {}", msg);
        xpe_alert_push(msg, XPE_ALERT_INFO);
    }
    if (mono1) normaliseMonochrome1(img);
    if (rescale.lutSequence) {
        // CROSS-LANE CONTRACT (QA-B-187c): the whole text.
        const char* msg = "ModalityLUTSequence (0028,3000) is present: returned pixels are stored values; the Modality LUT was not applied";
        spdlog::warn("[DicomReader] {}", msg);
        xpe_alert_push(msg, XPE_ALERT_WARNING);
    }
    if (rescale.nonIdentity) {
        // CROSS-LANE CONTRACT (QA-B-187): the whole text, with the values as the file spells them.
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "RescaleSlope %s, RescaleIntercept %s (the identity is 1 and 0): returned pixels are stored values; rescale not applied",
                      rescale.slope.c_str(), rescale.intercept.c_str());
        spdlog::warn("[DicomReader] {}", msg);
        xpe_alert_push(msg, XPE_ALERT_WARNING);
    }
}

// IS (Integer String), PS3.5 Table 6.2-1: "a string of characters representing an Integer in base-10 (decimal), shall contain
// only the characters 0 - 9, with an optional leading "+" or "-". It may be padded with leading and/or trailing spaces.
// Embedded spaces are not allowed. The integer, n, represented shall be in the range: -2^31 <= n <= (2^31 - 1)"; 12 bytes
// maximum. The WHOLE value must match -- a prefix that parses ("2500junk", "7.5", "0x10") is not an IS (QA-B-200 M2a3).
static bool parseIntegerString(const OFString& raw, int64_t* out) {
    if (raw.size() > 12) return false;
    size_t b = 0, e = raw.size();
    while (b < e && raw[b] == ' ') ++b;
    while (e > b && raw[e - 1] == ' ') --e;
    if (b == e) return false;
    bool negative = false;
    if (raw[b] == '+' || raw[b] == '-') {
        negative = raw[b] == '-';
        ++b;
    }
    if (b == e) return false;
    int64_t v = 0;   // at most 12 digits: cannot overflow
    for (; b < e; ++b) {
        if (raw[b] < '0' || raw[b] > '9') return false;
        v = v * 10 + (raw[b] - '0');
    }
    if (negative) v = -v;
    if (v < -2147483648LL || v > 2147483647LL) return false;
    *out = v;
    return true;
}

// mAs (QA-B-200 M2a/M2a2/M2a3, C1). A file can carry the exposure in three attributes; the order of trust, most precise
// first, and the tolerance within which two of them still agree (a rounded IS cannot be exact):
//   (0018,1153) Exposure in uAs   IS, microampere-seconds, /1000 -> mAs   rounded to 1 uAs: +-0.0005 mAs
//   (0018,9332) Exposure in mAs   FD, the exact value                     written with 4 decimals: +-0.0001 mAs
//   (0018,1152) Exposure          IS, whole mAs                           rounded to 1 mAs: +-0.5 mAs
// An attribute that is absent, not a valid IS (or an FD that is not finite), or negative is skipped. The first valid one
// is the mAs. When any two valid ones differ by more than their two tolerances together, the file contradicts itself:
// the choice is still the first, and ONE WARNING alert names every value as the file spells it and the one used.
static void readExposure(DcmDataset* ds, XpeImageMetadata* outMeta) {
    struct Candidate {
        const char* tag;
        const char* unit;
        double mAs;
        double tolerance;
        std::string spelled;
        bool valid;
    };
    Candidate c[3] = {{"(0018,1153)", "uAs", 0.0, 0.0005, "", false},
                      {"(0018,9332)", "mAs", 0.0, 0.0001, "", false},
                      {"(0018,1152)", "mAs", 0.0, 0.5, "", false}};
    // All three attributes have VM 1 (PS3.6): DCMTK would hand back the first value of "25\00" as 25, so a value count
    // other than one is refused here, as an invalid value is. The element's own length (the bytes in the file, padding
    // included) is checked too, because OFString values come back with their padding already stripped: an IS of 12
    // spaces and a "1" is 13 bytes (14 padded) and not an IS, but reads as "1" (QA-B-200 M2a4, Codex #105).
    auto single = [ds](const DcmTagKey& key, Uint32 maxBytes) {
        DcmElement* e = nullptr;
        return ds->findAndGetElement(key, e).good() && e && e->getVM() == 1 && e->getLength() <= maxBytes;
    };
    OFString text;
    int64_t whole = 0;
    if (single(DCM_ExposureInuAs, 12) && ds->findAndGetOFString(DCM_ExposureInuAs, text).good() &&
        parseIntegerString(text, &whole) && whole >= 0) {
        c[0].valid = true;
        c[0].mAs = static_cast<double>(whole) / 1000.0;
        c[0].spelled = std::to_string(whole);
    }
    Float64 exact = 0.0;
    if (single(DCM_ExposureInmAs, 8) && ds->findAndGetFloat64(DCM_ExposureInmAs, exact).good() && std::isfinite(exact) &&
        exact >= 0.0) {
        c[1].valid = true;
        c[1].mAs = exact;
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%.6g", exact);
        c[1].spelled = buf;
    }
    if (single(DCM_Exposure, 12) && ds->findAndGetOFString(DCM_Exposure, text).good() && parseIntegerString(text, &whole) &&
        whole >= 0) {
        c[2].valid = true;
        c[2].mAs = static_cast<double>(whole);
        c[2].spelled = std::to_string(whole);
    }

    const Candidate* chosen = nullptr;
    for (const Candidate& k : c) {
        if (k.valid) {
            chosen = &k;
            break;
        }
    }
    if (!chosen) return;
    outMeta->mAs = static_cast<float>(chosen->mAs);

    // EVERY pair of valid values is compared, not only each against the chosen one: two values can each be within their
    // tolerance of the chosen one and still contradict each other (1153 = 2500 uAs, 9332 = 2.4994, 1152 = 3: the chosen 2.5
    // agrees with both, yet 9332 and 1152 are 0.5006 apart and the two tolerances allow 0.5001; QA-B-200 M2a4).
    bool disagree = false;
    for (int i = 0; i < 3; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            if (c[i].valid && c[j].valid && std::fabs(c[i].mAs - c[j].mAs) > c[i].tolerance + c[j].tolerance + 1e-9) disagree = true;
        }
    }
    if (!disagree) return;
    // CROSS-LANE CONTRACT (QA-B-200 M2a3): the whole text.
    std::string msg = "DICOM exposure attributes disagree:";
    for (const Candidate& k : c) {
        if (k.valid) msg += std::string(" ") + k.tag + " = " + k.spelled + " " + k.unit + ";";
    }
    char tail[80];
    std::snprintf(tail, sizeof(tail), " using %s = %.4f mAs", chosen->tag, chosen->mAs);
    msg += tail;
    spdlog::warn("[DicomReader] {}", msg);
    xpe_alert_push(msg.c_str(), XPE_ALERT_WARNING);
}

static XpeErrorCode checkSupportedImageModule(DcmDataset* ds, bool isJ2K, bool isJpegLL) {
    Uint16 samples = 0, pixelRepresentation = 0, bitsAlloc = 0, bitsStored = 0, highBit = 0;
    struct Required { const char* name; DcmTagKey key; Uint16* value; };
    const Required required[] = {
        {"SamplesPerPixel (0028,0002)", DCM_SamplesPerPixel, &samples},
        {"PixelRepresentation (0028,0103)", DCM_PixelRepresentation, &pixelRepresentation},
        {"BitsAllocated (0028,0100)", DCM_BitsAllocated, &bitsAlloc},
        {"BitsStored (0028,0101)", DCM_BitsStored, &bitsStored},
        {"HighBit (0028,0102)", DCM_HighBit, &highBit},
    };
    for (const Required& r : required) {
        if (!readType1Uint16(ds, r.key, *r.value)) {
            return refuse(XPE_ERR_DICOM_INVALID, "%s is absent, empty or not a number (it is a Type 1 attribute and has no default)", r.name);
        }
    }

    // QA-B-182f (Codex #54): PhotometricInterpretation (0028,0004) is Type 1 in the Image Pixel Description Macro
    // (PS3.3 Table C.7-11c) and was never read, so a file without it, or with PALETTE COLOR, came back as a gray
    // uint16 image. A CS value of odd length is padded with a space and leading and trailing spaces are not
    // significant (PS3.5 6.2); DCMTK normalises both when it hands the string over (measured: the values " MONOCHROME2"
    // and "MONOCHROME2 " read as MONOCHROME2 with the reader's own trimming removed), so none is done here.
    std::string pi;
    {
        OFString v;
        if (ds->findAndGetOFString(DCM_PhotometricInterpretation, v).good()) pi.assign(v.c_str());
    }
    if (pi.empty()) {
        return refuse(XPE_ERR_DICOM_INVALID, "PhotometricInterpretation (0028,0004) is absent, empty or not a string (it is a Type 1 attribute and has no default)");
    }

    long frames = 1;
    if (ds->tagExists(DCM_NumberOfFrames)) {
        Sint32 v = 0;
        if (ds->findAndGetSint32(DCM_NumberOfFrames, v).bad() || v < 1) {
            return refuse(XPE_ERR_DICOM_INVALID, "NumberOfFrames (0028,0008) is present but is not a number >= 1");
        }
        frames = v;
    }

    if (frames > 1) return refuse(XPE_ERR_UNSUPPORTED_FORMAT, "NumberOfFrames %ld (only single-frame images are supported)", frames);
    if (samples != 1) return refuse(XPE_ERR_UNSUPPORTED_FORMAT, "SamplesPerPixel %u (only one sample per pixel is supported)", static_cast<unsigned>(samples));
    if (pixelRepresentation != 0) return refuse(XPE_ERR_UNSUPPORTED_FORMAT, "PixelRepresentation %u (only unsigned pixels are supported)", static_cast<unsigned>(pixelRepresentation));

    // QA-B-182f: SamplesPerPixel is 1 here. The buffer this API returns is one gray uint16 plane, so only the two
    // monochrome values describe it (PS3.3 C.7.6.3.1.2). A value the standard defines for three samples only is a
    // malformed dataset when SamplesPerPixel is 1; PALETTE COLOR (the value is an index into palette tables), the
    // retired values and values whose meaning the standard does not define are well formed, or at least not
    // malformed, and cannot be returned faithfully. MONOCHROME1 is accepted: readImage inverts it (QA-B-185).
    if (pi != "MONOCHROME1" && pi != "MONOCHROME2") {
        const bool threeSamplesOnly = pi == "RGB" || pi == "YBR_FULL" || pi == "YBR_FULL_422" || pi == "YBR_PARTIAL_420" ||
                                      pi == "YBR_ICT" || pi == "YBR_RCT";
        if (threeSamplesOnly) {
            return refuse(XPE_ERR_DICOM_INVALID, "PhotometricInterpretation %s with SamplesPerPixel 1 (PS3.3 C.7.6.3.1.2: it may be used only when SamplesPerPixel is 3)", pi.c_str());
        }
        return refuse(XPE_ERR_UNSUPPORTED_FORMAT, "PhotometricInterpretation %s (only MONOCHROME1 and MONOCHROME2 are supported)", pi.c_str());
    }

    // QA-B-182e: the bits attributes are classified against the standard (text quoted in the QA-B-182e report).
    //   VIOLATION (XPE_ERR_DICOM_INVALID): the dataset breaks a "shall" of PS3.5 8.1.1, which holds for every Pixel
    //   Data whatever the transfer syntax -- Bits Allocated is 1 or a multiple of 8, Bits Stored is never larger
    //   than Bits Allocated, High Bit is one less than Bits Stored -- or a value the transfer syntax's own table
    //   (PS3.5 Table 8.2.1-2 for JPEG Lossless, Table 8.2.4-1 for JPEG 2000) does not list.
    //   VALID BUT UNSUPPORTED (XPE_ERR_UNSUPPORTED_FORMAT): the dataset is well formed and this reader cannot return
    //   it (for example 8 or 32 bits allocated in native data).
    // High Bit = Bits Stored - 1 has been a "shall" since PS3.5 2014c; before that, files with another High Bit were
    // legal, so an old file can be refused here that its own edition allowed.
    if (bitsAlloc != 1 && bitsAlloc % 8 != 0) {
        return refuse(XPE_ERR_DICOM_INVALID, "BitsAllocated %u (PS3.5 8.1.1: it shall be 1 or a multiple of 8)", static_cast<unsigned>(bitsAlloc));
    }
    if (bitsStored < 1 || bitsStored > bitsAlloc || highBit != bitsStored - 1) {
        return refuse(XPE_ERR_DICOM_INVALID, "BitsStored %u with HighBit %u and BitsAllocated %u (PS3.5 8.1.1: HighBit shall be BitsStored - 1, and BitsStored 1..BitsAllocated)",
                      static_cast<unsigned>(bitsStored), static_cast<unsigned>(highBit), static_cast<unsigned>(bitsAlloc));
    }

    if (isJ2K) {
        const bool inTable = bitsAlloc == 1 || bitsAlloc == 8 || bitsAlloc == 16 || bitsAlloc == 24 || bitsAlloc == 32 || bitsAlloc == 40;
        if (!inTable) {
            return refuse(XPE_ERR_DICOM_INVALID, "BitsAllocated %u for JPEG 2000 (PS3.5 Table 8.2.4-1 lists 1, 8, 16, 24, 32 and 40)", static_cast<unsigned>(bitsAlloc));
        }
        // QA-B-181g (Codex #57): Table 8.2.4-1 limits BitsStored to 1-38 and HighBit to 0-37 as well, separately from
        // the allocation. BitsAllocated 40 is listed, so a BitsStored of 39 or 40 under it is a value the table does
        // not list (a violation), not a well-formed file this reader merely does not return. With HighBit = BitsStored - 1
        // (checked above) the two limits are one, and this is judged before the "unsupported" test below.
        if (bitsStored > 38) {
            return refuse(XPE_ERR_DICOM_INVALID, "BitsStored %u with HighBit %u for JPEG 2000 (PS3.5 Table 8.2.4-1 lists BitsStored 1-38 and HighBit 0-37)",
                          static_cast<unsigned>(bitsStored), static_cast<unsigned>(highBit));
        }
        if (bitsAlloc != 8 && bitsAlloc != 16) {
            return refuse(XPE_ERR_UNSUPPORTED_FORMAT, "BitsAllocated %u for JPEG 2000 (8 or 16 are supported)", static_cast<unsigned>(bitsAlloc));
        }
        return XPE_OK;
    }
    if (isJpegLL) {
        if (bitsAlloc != 8 && bitsAlloc != 16) {
            return refuse(XPE_ERR_DICOM_INVALID, "BitsAllocated %u for JPEG Lossless (PS3.5 Table 8.2.1-2 lists 8 and 16)", static_cast<unsigned>(bitsAlloc));
        }
    }
    if (bitsAlloc != 16) {
        return refuse(XPE_ERR_UNSUPPORTED_FORMAT, "BitsAllocated %u (only 16 is supported for uncompressed and JPEG Lossless data)", static_cast<unsigned>(bitsAlloc));
    }
    return XPE_OK;
}

XpeErrorCode DicomReader::readImage(XpeImageBuffer* outImg) {
    spdlog::debug("[DicomReader] readImage");
    if (!outImg) return XPE_ERR_INVALID_INPUT;
    if (!m_opened) return XPE_ERR_NOT_INITIALIZED;

    DcmDataset* ds = m_dcmFile->getDataset();
    if (!ds) return XPE_ERR_DICOM_INVALID;

    // Read image dimensions
    Uint16 rows = 0, cols = 0, bitsAlloc = 0, bitsStored = 0;
    // QA-B-182f: Rows and Columns are Type 1 (PS3.3 Table C.7-11c). The code was already DICOM_INVALID; the alert that
    // names the cause (as for the other Type 1 attributes, QA-B-182c) was missing.
    if (ds->findAndGetUint16(DCM_Rows, rows).bad() || rows == 0) {
        return refuse(XPE_ERR_DICOM_INVALID, "Rows (0028,0010) is absent, empty, not a number or zero (it is a Type 1 attribute and has no default)");
    }
    if (ds->findAndGetUint16(DCM_Columns, cols).bad() || cols == 0) {
        return refuse(XPE_ERR_DICOM_INVALID, "Columns (0028,0011) is absent, empty, not a number or zero (it is a Type 1 attribute and has no default)");
    }
    ds->findAndGetUint16(DCM_BitsAllocated, bitsAlloc);
    ds->findAndGetUint16(DCM_BitsStored, bitsStored);
    // Bits Allocated and Bits Stored are Type 1: checkSupportedImageModule below refuses their absence, so no
    // default is taken here.

    bool isJ2K = (m_tsUID == TS_J2K_LOSSLESS);
    // #147 (QA-B-68): both JPEG-Lossless syntaxes take this branch. The dispatch
    // is per-SYNTAX, not per-compressed-path, so .57 did not inherit anything
    // from .70 by being compressed -- it had to be named here. The encapsulated
    // representation is then requested with ITS OWN key; asking for the .70 key
    // on a .57 dataset finds nothing and the size guard below would silently do
    // no work.
    bool isJPEGLL = (m_tsUID == TS_JPEG_LL) || (m_tsUID == TS_JPEG_LL_P14);

    // QA-B-182 (#235): refuse what this reader cannot return faithfully, before any decode or allocation.
    {
        const XpeErrorCode scope = checkSupportedImageModule(ds, isJ2K, isJPEGLL);
        if (scope != XPE_OK) return scope;
    }

    // The scope check has pinned PhotometricInterpretation to MONOCHROME1 or MONOCHROME2.
    const bool mono1 = isMonochrome1(ds);
    RescaleNote rescale;
    {
        const XpeErrorCode rrc = checkRescale(ds, &rescale);   // QA-B-187: malformed Rescale is refused before any decode
        if (rrc != XPE_OK) return rrc;
    }

    if (isJ2K) {
        // J2K: extract raw bitstream and decode with OpenJPEG
        const XpeErrorCode drc = decompressPixelData(m_dcmFile.get(), outImg);
        if (drc == XPE_OK) finishRead(outImg, mono1, rescale);
        return drc;
    }

    if (isJPEGLL) {
        // #150: check the encoded frame against the declared size BEFORE letting
        // DCMTK decompress. Afterwards the padding hides a shortfall (see
        // jpeg_frame_dimensions), and an oversized frame surfaces only as a
        // decoder fault (PROCESSING_FAILED), which misnames what is wrong with
        // the file. Judged here, nothing has been allocated yet, so the QA-B-48
        // contract holds on this path: outImg is not touched at all.
        DcmElement* encElem = nullptr;
        if (ds->findAndGetElement(DCM_PixelData, encElem).good() && encElem != nullptr) {
            DcmPixelData* encPd = OFstatic_cast(DcmPixelData*, encElem);
            DcmPixelSequence* encSeq = nullptr;
            E_TransferSyntax encKey = (m_tsUID == TS_JPEG_LL_P14)
                                          ? EXS_JPEGProcess14      // .57
                                          : EXS_JPEGProcess14SV1;  // .70
            const DcmRepresentationParameter* encParam = nullptr;
            if (encPd != nullptr &&
                encPd->getEncapsulatedRepresentation(encKey, encParam, encSeq).good() &&
                encSeq != nullptr) {
                DcmPixelItem* frag = nullptr;
                if (encSeq->getItem(frag, 1).good() && frag != nullptr) {
                    Uint8* fragData = nullptr;
                    uint32_t frameW = 0, frameH = 0, framePrecision = 0, frameComponents = 0;
                    if (frag->getUint8Array(fragData).good() && fragData != nullptr &&
                        jpeg_frame_dimensions(fragData, static_cast<size_t>(frag->getLength()),
                                              frameW, frameH, &framePrecision, &frameComponents)) {
                        if (frameW != cols || frameH != rows) {
                            spdlog::error("[DicomReader] JPEG frame size does not match the "
                                          "declared size: dataset says {}x{}, frame carries {}x{}",
                                          cols, rows, frameW, frameH);
                            return XPE_ERR_DICOM_INVALID;
                        }
                        // QA-B-182e: PS3.5 8.2 requires the pixel attributes to be "consistent with the
                        // characteristics of the compressed data stream", and the sample precision P in the frame
                        // header is the one bit depth the stream carries. P below Bits Stored cannot hold the
                        // declared values and P above Bits Allocated does not fit the container: both are refused
                        // before DCMTK decodes. P above Bits Stored is accepted -- the standard does not say what
                        // "consistent" means there and refusing would turn away files with a wider P.
                        // The frame header was read (jpeg_frame_dimensions returned true), so its component count is
                        // judged: exactly 1, the SamplesPerPixel that checkSupportedImageModule has already pinned.
                        // QA-B-181h: 0, an absent Nf byte and 2 or more are all refused. A header that could not be read
                        // at all gives no verdict here; DCMTK then decodes or fails on it. DCMTK does not make this
                        // comparison itself: a frame header declaring three components under SamplesPerPixel 1 was read
                        // with rc=0 (QA-B-182e, measured). A JPEG Lossless frame header has no sign flag, so there is
                        // nothing to compare PixelRepresentation with; it is judged from the dataset alone.
                        if (frameComponents != 1) {
                            return refuse(XPE_ERR_DICOM_INVALID, "JPEG Lossless stream carries %u components, the dataset says 1 (SamplesPerPixel)",
                                          static_cast<unsigned>(frameComponents));
                        }
                        if (framePrecision != 0 && (framePrecision < bitsStored || framePrecision > bitsAlloc)) {
                            return refuse(XPE_ERR_DICOM_INVALID, "JPEG Lossless stream sample precision %u does not fit the dataset (BitsStored %u, BitsAllocated %u)",
                                          static_cast<unsigned>(framePrecision), static_cast<unsigned>(bitsStored), static_cast<unsigned>(bitsAlloc));
                        }
                    }
                }
            }
        }

        // JPEG Lossless: use DCMTK's built-in JPEG decoder
        OFCondition repStatus = ds->chooseRepresentation(EXS_LittleEndianExplicit, nullptr);
        if (repStatus.bad()) {
            spdlog::warn("[DicomReader] JPEG-LL chooseRepresentation failed: {}", repStatus.text());
            return XPE_ERR_PROCESSING_FAILED;
        }
        ds->loadAllDataIntoMemory();
    }

    // Allocate output buffer
    XpeErrorCode allocRc = xpe_alloc_image(
        static_cast<uint32_t>(cols),
        static_cast<uint32_t>(rows),
        XPE_PIXEL_UINT16,
        outImg
    );
    if (allocRc != XPE_OK) return allocRc;

    outImg->bitsAllocated = static_cast<uint32_t>(bitsAlloc);
    outImg->bitsStored    = static_cast<uint32_t>(bitsStored);

    // Extract pixel data from dataset
    const Uint16* pixData = nullptr;
    unsigned long pixCount = 0;
    OFCondition rc = ds->findAndGetUint16Array(DCM_PixelData, pixData, &pixCount);
    if (rc.bad() || !pixData || pixCount == 0) {
        xpe_free_image(outImg);
        return XPE_ERR_DICOM_INVALID;
    }

    // Copy pixel data into buffer.
    //
    // #150: a PixelData shorter than Rows x Columns declares is a corrupt file,
    // not a readable one. Copying what exists and returning XPE_OK would hand
    // the caller a full-size image whose lower part is black padding, with no
    // signal that anything is missing -- HAZ-DCM-002 names exactly that failure
    // ("픽셀 데이터 불완전 ... 호출자에게 '성공' 반환") and SR-DCM-003 requires
    // rejection instead. The buffer is released so no partial image escapes.
    //
    // A LONGER PixelData stays a success: trailing padding is legal in DICOM,
    // so the surplus is simply not copied.
    size_t expectedBytes = static_cast<size_t>(rows) * cols * sizeof(uint16_t);
    size_t availBytes = pixCount * sizeof(uint16_t);
    if (availBytes < expectedBytes) {
        spdlog::error("[DicomReader] PixelData is short: {}x{} declares {} bytes, "
                      "file carries {} bytes",
                      cols, rows, expectedBytes, availBytes);
        xpe_free_image(outImg);
        return XPE_ERR_DICOM_INVALID;
    }
    std::memcpy(outImg->data, pixData, expectedBytes);
    finishRead(outImg, mono1, rescale);

    return XPE_OK;
}

XpeErrorCode DicomReader::getMetadata(XpeImageMetadata* outMeta) {
    spdlog::debug("[DicomReader] getMetadata");
    if (!outMeta) return XPE_ERR_INVALID_INPUT;
    if (!m_opened) return XPE_ERR_NOT_INITIALIZED;

    DcmDataset* ds = m_dcmFile->getDataset();
    if (!ds) return XPE_ERR_DICOM_INVALID;

    std::memset(outMeta, 0, sizeof(XpeImageMetadata));

    // Body part examined (0008,2015) / BodyPartExamined
    {
        OFString val;
        if (ds->findAndGetOFString(DCM_BodyPartExamined, val).good()) {
            std::strncpy(outMeta->bodyPart, val.c_str(), sizeof(outMeta->bodyPart) - 1);
        }
    }

    // kVp (0018,0060)
    {
        Float64 kvp = 0.0;
        if (ds->findAndGetFloat64(DCM_KVP, kvp).good()) {
            outMeta->kVp = static_cast<float>(kvp);
        }
    }

    readExposure(ds, outMeta);

    // SID — DistanceSourceToDetector (0018,1110) in mm
    {
        Float64 sid = 0.0;
        if (ds->findAndGetFloat64(DCM_DistanceSourceToDetector, sid).good()) {
            outMeta->SID_mm = static_cast<float>(sid);
        }
    }

    // PixelSpacing (0028,0030) — row_spacing\col_spacing in mm
    {
        OFString pixSpacing;
        if (ds->findAndGetOFString(DCM_PixelSpacing, pixSpacing).good()) {
            float pitch = static_cast<float>(std::atof(pixSpacing.c_str()));
            if (pitch > 0.0f) outMeta->pixelPitch_mm = pitch;
        }
    }

    // Acquisition time: combine StudyDate (0008,0020) + AcquisitionTime (0008,0032)
    {
        OFString studyDate, acqTime;
        bool hasDate = ds->findAndGetOFString(DCM_StudyDate, studyDate).good();
        bool hasTime = ds->findAndGetOFString(DCM_AcquisitionTime, acqTime).good();

        if (hasDate && hasTime && studyDate.length() == 8) {
            std::string dateStr(studyDate.c_str());
            std::string timeStr(acqTime.c_str());

            int year   = std::atoi(dateStr.substr(0, 4).c_str());
            int month  = std::atoi(dateStr.substr(4, 2).c_str());
            int day    = std::atoi(dateStr.substr(6, 2).c_str());
            int hour   = 0, minute = 0, second = 0;

            if (timeStr.length() >= 6) {
                hour   = std::atoi(timeStr.substr(0, 2).c_str());
                minute = std::atoi(timeStr.substr(2, 2).c_str());
                second = std::atoi(timeStr.substr(4, 2).c_str());
            }

            struct tm t{};
            t.tm_year  = year - 1900;
            t.tm_mon   = month - 1;
            t.tm_mday  = day;
            t.tm_hour  = hour;
            t.tm_min   = minute;
            t.tm_sec   = second;
            t.tm_isdst = 0;

#ifdef _WIN32
            time_t epoch = _mkgmtime(&t);
#else
            time_t epoch = timegm(&t);
#endif
            if (epoch >= 0) {
                outMeta->acquisitionTime = static_cast<uint64_t>(epoch);
            }
        }
    }

    return XPE_OK;
}

/**
 * Decompress J2K Lossless pixel data using OpenJPEG.
 * Extracts the raw J2K bitstream from the DICOM pixel sequence,
 * then decodes to uint16 using libopenjp2.
 */
XpeErrorCode DicomReader::decompressPixelData(DcmFileFormat* dcm, XpeImageBuffer* outImg) {
    DcmDataset* ds = dcm->getDataset();
    if (!ds) return XPE_ERR_DICOM_INVALID;

    // Get image dimensions
    Uint16 rows = 0, cols = 0, bitsAlloc = 0, bitsStored = 0;
    ds->findAndGetUint16(DCM_Rows, rows);
    ds->findAndGetUint16(DCM_Columns, cols);
    ds->findAndGetUint16(DCM_BitsAllocated, bitsAlloc);
    ds->findAndGetUint16(DCM_BitsStored, bitsStored);
    // Both are Type 1 and were read by checkSupportedImageModule before this point: no defaults.

    // Get pixel data element (J2K is encapsulated)
    DcmElement* pixElem = nullptr;
    OFCondition findRc = ds->findAndGetElement(DCM_PixelData, pixElem);
    if (findRc.bad() || !pixElem) {
        return XPE_ERR_DICOM_INVALID;
    }

    // For J2K, pixel data is a DcmPixelData (contains encapsulated sequence)
    DcmPixelData* pd = OFstatic_cast(DcmPixelData*, pixElem);
    if (!pd) {
        return XPE_ERR_DICOM_INVALID;
    }

    // Get the encapsulated pixel sequence
    DcmPixelSequence* seq = nullptr;
    E_TransferSyntax repKey = EXS_JPEG2000LosslessOnly;
    const DcmRepresentationParameter* repParam = nullptr;

    OFCondition seqRc = pd->getEncapsulatedRepresentation(repKey, repParam, seq);
    if (seqRc.bad() || !seq) {
        // Try with unknown TS
        repKey = EXS_Unknown;
        seqRc = pd->getEncapsulatedRepresentation(repKey, repParam, seq);
    }

    if (seqRc.bad() || !seq) {
        return XPE_ERR_DICOM_INVALID;
    }

    // Get the J2K fragment (item 1, skip offset table at item 0)
    DcmPixelItem* item = nullptr;
    Uint8* j2kData = nullptr;
    Uint32 j2kLen = 0;

    // Try item index 1 first (after the offset table)
    if (seq->getItem(item, 1).bad() || !item) {
        if (seq->getItem(item, 0).bad() || !item) {
            return XPE_ERR_DICOM_INVALID;
        }
    }

    j2kLen = static_cast<Uint32>(item->getLength());
    if (item->getUint8Array(j2kData).bad() || !j2kData || j2kLen == 0) {
        return XPE_ERR_DICOM_INVALID;
    }

    return decodeJ2KBitstream(
        static_cast<const uint8_t*>(j2kData),
        static_cast<size_t>(j2kLen),
        rows, cols, bitsAlloc, bitsStored, outImg
    );
}

// The codestream header against the dataset (QA-B-182b). Codes: a file whose own parts contradict each other is
// malformed (DICOM_INVALID, as for a codestream of the wrong size); a codestream this reader cannot return in a
// 16-bit unsigned buffer is unsupported.
static XpeErrorCode checkJ2kCodestreamShape(const opj_image_t* image, uint32_t rows, uint32_t cols,
                                            uint16_t bitsAlloc, uint16_t bitsStored) {
    if (image->numcomps != 1 || image->comps == nullptr) {
        return refuse(XPE_ERR_DICOM_INVALID, "JPEG 2000 codestream carries %u components, the dataset says 1 (SamplesPerPixel)",
                      static_cast<unsigned>(image->numcomps));
    }
    const opj_image_comp_t& c = image->comps[0];
    if (c.sgnd != 0) {
        return refuse(XPE_ERR_DICOM_INVALID, "JPEG 2000 codestream is signed, the dataset says unsigned pixels (PixelRepresentation 0)");
    }
    if (c.prec > 16) {
        return refuse(XPE_ERR_UNSUPPORTED_FORMAT, "JPEG 2000 codestream precision %u exceeds the 16 bits this reader returns",
                      static_cast<unsigned>(c.prec));
    }
    if (c.prec != bitsStored || bitsAlloc < bitsStored) {
        return refuse(XPE_ERR_DICOM_INVALID, "JPEG 2000 codestream precision %u does not match the dataset (BitsStored %u, BitsAllocated %u)",
                      static_cast<unsigned>(c.prec), static_cast<unsigned>(bitsStored), static_cast<unsigned>(bitsAlloc));
    }
    if (c.w != cols || c.h != rows) {
        return refuse(XPE_ERR_DICOM_INVALID, "JPEG 2000 codestream size %ux%u does not match the declared Columns x Rows %ux%u",
                      static_cast<unsigned>(c.w), static_cast<unsigned>(c.h), static_cast<unsigned>(cols), static_cast<unsigned>(rows));
    }
    return XPE_OK;
}

XpeErrorCode DicomReader::decodeJ2KBitstream(const uint8_t* j2kData, size_t j2kLen,
                                               uint32_t rows, uint32_t cols,
                                               uint16_t bitsAlloc, uint16_t bitsStored,
                                               XpeImageBuffer* outImg) {
    // #150: rows/cols ARE used below -- the comment that once stood here claimed
    // they were "validated via OpenJPEG codestream header", but nothing compared
    // them, so a codestream smaller than the dataset declared was decoded and
    // returned as a success at the codestream's own size (QA-B-50 observed
    // declared 256x256 -> returned 256x128, rc=0). A caller that trusts the
    // metadata then indexes past the buffer it was handed.
    if (!j2kData || j2kLen == 0) return XPE_ERR_DICOM_INVALID;

    // Setup OpenJPEG decoder
    opj_codec_t* codec = opj_create_decompress(OPJ_CODEC_J2K);
    if (!codec) {
        spdlog::warn("[DicomReader] opj_create_decompress failed");
        return XPE_ERR_PROCESSING_FAILED;
    }

    // Silence decoder messages
    opj_set_error_handler(codec, nullptr, nullptr);
    opj_set_warning_handler(codec, nullptr, nullptr);
    opj_set_info_handler(codec, nullptr, nullptr);

    opj_dparameters_t params;
    opj_set_default_decoder_parameters(&params);
    opj_setup_decoder(codec, &params);

    // Create memory read stream using custom callbacks
    struct MemStream {
        const uint8_t* data;
        OPJ_SIZE_T      pos;
        OPJ_SIZE_T      len;
    };

    MemStream ms{ j2kData, 0, static_cast<OPJ_SIZE_T>(j2kLen) };

    opj_stream_t* stream = opj_stream_create(j2kLen, OPJ_TRUE);
    if (!stream) {
        opj_destroy_codec(codec);
        spdlog::warn("[DicomReader] opj_stream_create failed");
        return XPE_ERR_PROCESSING_FAILED;
    }

    opj_stream_set_user_data(stream, &ms, nullptr);
    opj_stream_set_user_data_length(stream, static_cast<OPJ_UINT64>(j2kLen));

    opj_stream_set_read_function(stream,
        [](void* buf, OPJ_SIZE_T nb, void* udata) -> OPJ_SIZE_T {
            MemStream* ms = static_cast<MemStream*>(udata);
            OPJ_SIZE_T avail = ms->len - ms->pos;
            if (avail == 0) return static_cast<OPJ_SIZE_T>(-1);
            OPJ_SIZE_T toRead = (nb < avail) ? nb : avail;
            std::memcpy(buf, ms->data + ms->pos, toRead);
            ms->pos += toRead;
            return toRead;
        });

    opj_stream_set_seek_function(stream,
        [](OPJ_OFF_T offset, void* udata) -> OPJ_BOOL {
            MemStream* ms = static_cast<MemStream*>(udata);
            if (offset < 0 || static_cast<OPJ_SIZE_T>(offset) > ms->len) return OPJ_FALSE;
            ms->pos = static_cast<OPJ_SIZE_T>(offset);
            return OPJ_TRUE;
        });

    opj_stream_set_skip_function(stream,
        [](OPJ_OFF_T nb, void* udata) -> OPJ_OFF_T {
            MemStream* ms = static_cast<MemStream*>(udata);
            OPJ_SIZE_T avail = ms->len - ms->pos;
            OPJ_SIZE_T toSkip = (static_cast<OPJ_SIZE_T>(nb) < avail) ? static_cast<OPJ_SIZE_T>(nb) : avail;
            ms->pos += toSkip;
            return static_cast<OPJ_OFF_T>(toSkip);
        });

    opj_image_t* image = nullptr;
    OPJ_BOOL ok = opj_read_header(stream, codec, &image);
    if (!ok || !image) {
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        spdlog::warn("[DicomReader] opj_read_header failed");
        return XPE_ERR_PROCESSING_FAILED;
    }

    // QA-B-182b (#235, Codex #39): the codestream is judged BEFORE it is decoded and before anything is allocated.
    // PS3.5 8.2.4: the Image Pixel attributes shall be consistent with the compressed data stream, and the
    // stream's own characteristics are the ones used for decoding. readImage has already refused every dataset
    // that is not one unsigned sample per pixel (checkSupportedImageModule), so the codestream is compared with
    // exactly that: one component, unsigned, at most 16 bits of precision, Bits Stored equal to the precision,
    // and the declared size. The first component is read below as 16-bit words, so any other shape would be
    // decoded into wrong pixels (a second component dropped, a signed one reinterpreted, bits above 16 cut).
    {
        const XpeErrorCode shape = checkJ2kCodestreamShape(image, rows, cols, bitsAlloc, bitsStored);
        if (shape != XPE_OK) {
            opj_image_destroy(image);
            opj_stream_destroy(stream);
            opj_destroy_codec(codec);
            return shape;
        }
    }

    ok = opj_decode(codec, stream, image);
    if (!ok) {
        opj_image_destroy(image);
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        spdlog::warn("[DicomReader] opj_decode failed");
        return XPE_ERR_PROCESSING_FAILED;
    }

    opj_end_decompress(codec, stream);
    opj_stream_destroy(stream);
    opj_destroy_codec(codec);

    // Dimensions (already compared with the declared ones from the header; the decoder does not change them)
    uint32_t imgW = image->comps[0].w;
    uint32_t imgH = image->comps[0].h;

    // #150: a codestream whose size does not match the header is a corrupt file,
    // not a readable one -- the HAZ-DCM-002 hazard when it is smaller, and a file
    // contradicting its own description when it is larger (QA-B-51). Rows and
    // Columns are the dataset's claim ABOUT these pixels; decoding something else
    // and passing it on hands the next consumer a wrong claim.
    //
    // Distinct from the decode failures above: those mean OpenJPEG could not
    // decode and return XPE_ERR_PROCESSING_FAILED; this means it decoded fine and
    // the result does not match what the dataset promised, which is a DICOM
    // consistency fault. The two are not merged into one code on purpose.
    if (imgW != cols || imgH != rows) {
        spdlog::error("[DicomReader] J2K codestream size does not match the declared "
                      "size: dataset says {}x{}, codestream carries {}x{}",
                      cols, rows, imgW, imgH);
        opj_image_destroy(image);
        return XPE_ERR_DICOM_INVALID;
    }

    // Allocate output buffer
    if (outImg) {
        XpeErrorCode allocRc = xpe_alloc_image(imgW, imgH, XPE_PIXEL_UINT16, outImg);
        if (allocRc != XPE_OK) {
            opj_image_destroy(image);
            return allocRc;
        }
        // The buffer is UINT16, two bytes per sample: it holds 16 allocated bits whatever the file allocated (an
        // 8-bit file used to be described as bitsAllocated 8 over 2-byte samples). Bits Stored is the codestream's
        // precision, which the check above made equal to the dataset's.
        outImg->bitsAllocated = 16;
        outImg->bitsStored    = static_cast<uint32_t>(image->comps[0].prec);

        // Convert OPJ int32 array to uint16
        uint16_t* dst = static_cast<uint16_t*>(outImg->data);
        OPJ_INT32* src = image->comps[0].data;
        size_t npix = static_cast<size_t>(imgW) * imgH;
        for (size_t i = 0; i < npix; ++i) {
            dst[i] = static_cast<uint16_t>(src[i] & 0xFFFF);
        }
    }

    opj_image_destroy(image);
    return XPE_OK;
}

} // namespace dicom
} // namespace xpe
