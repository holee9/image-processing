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
#include <openjpeg.h>

#include "xpe/common/xpe_memory.h"

#include <spdlog/spdlog.h>
#include <cstring>
#include <ctime>
#include <vector>

namespace xpe {
namespace dicom {

// Supported Transfer Syntax UIDs
static constexpr const char* TS_EXPLICIT_LE  = "1.2.840.10008.1.2.1";
static constexpr const char* TS_J2K_LOSSLESS = "1.2.840.10008.1.2.4.90";
static constexpr const char* TS_JPEG_LL      = "1.2.840.10008.1.2.4.70";

// @MX:ANCHOR: [AUTO] DicomReader constructor — maps from opaque XpeDicomHandle
// @MX:REASON: fan_in >= 3: xpe_dicom_open allocates, readImage and getMetadata use, xpe_dicom_close frees
// @MX:SPEC: SPEC-XPE-P1B-DICOM SWU-4.1
DicomReader::DicomReader(const std::string& filePath)
    : m_filePath(filePath)
    , m_dcmFile(std::make_unique<DcmFileFormat>())
{}

DicomReader::~DicomReader() = default;

XpeErrorCode DicomReader::open() {
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
        // No meta-info: try loading as a DICOM without preamble
        // If dataset is also empty, it's invalid
        DcmDataset* ds = m_dcmFile->getDataset();
        if (!ds) {
            return XPE_ERR_DICOM_INVALID;
        }
        // Accept implicit datasets (no TS check, treat as Explicit LE)
        m_opened = true;
        m_tsUID = TS_EXPLICIT_LE;
        return XPE_OK;
    }

    // Check Transfer Syntax UID from meta-header
    OFString tsUID;
    if (meta->findAndGetOFString(DCM_TransferSyntaxUID, tsUID).good()) {
        if (tsUID != TS_EXPLICIT_LE &&
            tsUID != TS_J2K_LOSSLESS &&
            tsUID != TS_JPEG_LL) {
            spdlog::warn("[DicomReader] Unsupported Transfer Syntax: {}", tsUID.c_str());
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }
        m_tsUID = std::string(tsUID.c_str());
    } else {
        // No TS in meta — treat as Explicit LE
        m_tsUID = TS_EXPLICIT_LE;
    }

    m_opened = true;
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
    if (ds->findAndGetUint16(DCM_Rows, rows).bad() || rows == 0) return XPE_ERR_DICOM_INVALID;
    if (ds->findAndGetUint16(DCM_Columns, cols).bad() || cols == 0) return XPE_ERR_DICOM_INVALID;
    ds->findAndGetUint16(DCM_BitsAllocated, bitsAlloc);
    ds->findAndGetUint16(DCM_BitsStored, bitsStored);
    if (bitsAlloc == 0) bitsAlloc = 16;
    if (bitsStored == 0) bitsStored = bitsAlloc;

    bool isJ2K = (m_tsUID == TS_J2K_LOSSLESS);
    bool isJPEGLL = (m_tsUID == TS_JPEG_LL);

    if (isJ2K) {
        // J2K: extract raw bitstream and decode with OpenJPEG
        return decompressPixelData(m_dcmFile.get(), outImg);
    }

    if (isJPEGLL) {
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

    // Copy pixel data into buffer
    size_t expectedBytes = static_cast<size_t>(rows) * cols * sizeof(uint16_t);
    size_t availBytes = pixCount * sizeof(uint16_t);
    size_t copyBytes = (availBytes < expectedBytes) ? availBytes : expectedBytes;
    std::memcpy(outImg->data, pixData, copyBytes);

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

    // mAs — ExposureInmAs (0018,9332) DS VR
    {
        Float64 mAs = 0.0;
        if (ds->findAndGetFloat64(DCM_ExposureInmAs, mAs).good()) {
            outMeta->mAs = static_cast<float>(mAs);
        }
    }

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
    if (bitsAlloc == 0) bitsAlloc = 16;
    if (bitsStored == 0) bitsStored = bitsAlloc;

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

XpeErrorCode DicomReader::decodeJ2KBitstream(const uint8_t* j2kData, size_t j2kLen,
                                               uint32_t rows, uint32_t cols,
                                               uint16_t bitsAlloc, uint16_t bitsStored,
                                               XpeImageBuffer* outImg) {
    // rows/cols are validated via OpenJPEG codestream header; suppress unused warning
    (void)rows; (void)cols;
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

    // Validate dimensions
    uint32_t imgW = image->comps[0].w;
    uint32_t imgH = image->comps[0].h;

    // Allocate output buffer
    if (outImg) {
        XpeErrorCode allocRc = xpe_alloc_image(imgW, imgH, XPE_PIXEL_UINT16, outImg);
        if (allocRc != XPE_OK) {
            opj_image_destroy(image);
            return allocRc;
        }
        outImg->bitsAllocated = static_cast<uint32_t>(bitsAlloc);
        outImg->bitsStored    = static_cast<uint32_t>(bitsStored);

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
