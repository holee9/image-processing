/**
 * @file DicomWriter.cpp
 * @brief DICOM Part 10 file writer implementation (SWU-4.2).
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-013..022
 */
#include "DicomWriter.h"

#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcpxitem.h>
#include <openjpeg.h>

#include <spdlog/spdlog.h>
#include <vector>
#include <cstring>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

namespace xpe {
namespace dicom {

// DX IOD SOP Class UID for Digital X-Ray Image Storage — For Presentation
static constexpr const char* SOP_CLASS_DX_PRES = "1.2.840.10008.5.1.4.1.1.1.1";

// Transfer Syntax UIDs
static constexpr const char* TS_EXPLICIT_LE   = "1.2.840.10008.1.2.1";
static constexpr const char* TS_J2K_LOSSLESS  = "1.2.840.10008.1.2.4.90";

// @MX:ANCHOR: [AUTO] DicomWriter::write — exported API boundary for uncompressed DICOM write
// @MX:REASON: Called by xpe_dicom_write and used in round-trip tests; populateDataset is shared
// @MX:SPEC: SPEC-XPE-P1B-DICOM SWU-4.2
XpeErrorCode DicomWriter::write(const char* filePath,
                                 const XpeImageBuffer* img,
                                 const XpeImageMetadata* meta) {
    spdlog::debug("[DicomWriter] write: {}", filePath ? filePath : "(null)");
    if (!filePath || !img || !meta) return XPE_ERR_INVALID_INPUT;

    DcmFileFormat dcmff;
    DcmDataset* ds = dcmff.getDataset();

    XpeErrorCode rc = populateDataset(ds, img, meta);
    if (rc != XPE_OK) return rc;

    // Set uncompressed pixel data
    OFCondition status = ds->putAndInsertUint8Array(
        DCM_PixelData,
        static_cast<const Uint8*>(img->data),
        static_cast<unsigned long>(img->dataSize)
    );
    if (status.bad()) {
        spdlog::warn("[DicomWriter] putAndInsertUint8Array failed: {}", status.text());
        return XPE_ERR_PROCESSING_FAILED;
    }

    status = dcmff.saveFile(filePath, EXS_LittleEndianExplicit);
    if (status.bad()) {
        spdlog::warn("[DicomWriter] saveFile failed: {}", status.text());
        return XPE_ERR_IO_FAILED;
    }

    return XPE_OK;
}

// @MX:ANCHOR: [AUTO] DicomWriter::writeJ2K — exported API boundary for J2K lossless write
// @MX:REASON: Called by xpe_dicom_write_j2k; shares populateDataset with write()
// @MX:SPEC: SPEC-XPE-P1B-DICOM SWU-4.2 REQ-DICOM-019..022
XpeErrorCode DicomWriter::writeJ2K(const char* filePath,
                                    const XpeImageBuffer* img,
                                    const XpeImageMetadata* meta) {
    spdlog::debug("[DicomWriter] writeJ2K: {}", filePath ? filePath : "(null)");
    if (!filePath || !img || !meta) return XPE_ERR_INVALID_INPUT;

    // Compress pixel data with OpenJPEG J2K Lossless
    std::vector<uint8_t> j2kData = compressJ2K(img);
    if (j2kData.empty()) {
        spdlog::warn("[DicomWriter] J2K compression failed");
        return XPE_ERR_PROCESSING_FAILED;
    }

    DcmFileFormat dcmff;
    DcmDataset* ds = dcmff.getDataset();

    XpeErrorCode rc = populateDataset(ds, img, meta);
    if (rc != XPE_OK) return rc;

    // Insert J2K bitstream as encapsulated pixel data
    XpeErrorCode j2kRc = setJ2KPixelData(ds, j2kData);
    if (j2kRc != XPE_OK) return j2kRc;

    OFCondition status = dcmff.saveFile(filePath, EXS_JPEG2000LosslessOnly);
    if (status.bad()) {
        spdlog::warn("[DicomWriter] saveFile(J2K) failed: {}", status.text());
        return XPE_ERR_IO_FAILED;
    }

    return XPE_OK;
}

XpeErrorCode DicomWriter::populateDataset(void* dcmDataset,
                                           const XpeImageBuffer* img,
                                           const XpeImageMetadata* meta) {
    DcmDataset* ds = static_cast<DcmDataset*>(dcmDataset);
    if (!ds || !img || !meta) return XPE_ERR_INVALID_INPUT;

    // Generate SOPInstanceUID
    std::string sopUID = generateUID();

    // ---- Patient Module ----
    ds->putAndInsertString(DCM_PatientName, "ANONYMOUS");
    ds->putAndInsertString(DCM_PatientID, "ANONYMOUS");
    ds->putAndInsertString(DCM_PatientBirthDate, "");
    ds->putAndInsertString(DCM_PatientSex, "");

    // ---- General Study Module ----
    std::string studyUID = generateUID();
    ds->putAndInsertString(DCM_StudyInstanceUID, studyUID.c_str());
    ds->putAndInsertString(DCM_StudyDate, "");
    ds->putAndInsertString(DCM_StudyTime, "");
    ds->putAndInsertString(DCM_ReferringPhysicianName, "");
    ds->putAndInsertString(DCM_StudyID, "1");
    ds->putAndInsertString(DCM_AccessionNumber, "");

    // ---- General Series Module ----
    std::string seriesUID = generateUID();
    ds->putAndInsertString(DCM_Modality, "DX");
    ds->putAndInsertString(DCM_SeriesInstanceUID, seriesUID.c_str());
    ds->putAndInsertString(DCM_SeriesNumber, "1");
    ds->putAndInsertString(DCM_Laterality, "");

    // ---- General Equipment Module ----
    ds->putAndInsertString(DCM_Manufacturer, "XPE");
    ds->putAndInsertString(DCM_InstitutionName, "");
    ds->putAndInsertString(DCM_ManufacturerModelName, "XPE-DX");
    ds->putAndInsertString(DCM_SoftwareVersions, "1.0");

    // ---- General Image Module ----
    ds->putAndInsertString(DCM_InstanceNumber, "1");
    ds->putAndInsertString(DCM_PatientOrientation, "");
    ds->putAndInsertString(DCM_ImageType, "ORIGINAL\\PRIMARY\\");

    // ---- Image Pixel Module ----
    ds->putAndInsertUint16(DCM_SamplesPerPixel, 1);
    ds->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
    ds->putAndInsertUint16(DCM_Rows, static_cast<Uint16>(img->height));
    ds->putAndInsertUint16(DCM_Columns, static_cast<Uint16>(img->width));
    ds->putAndInsertUint16(DCM_BitsAllocated, static_cast<Uint16>(img->bitsAllocated > 0 ? img->bitsAllocated : 16));
    ds->putAndInsertUint16(DCM_BitsStored, static_cast<Uint16>(img->bitsStored > 0 ? img->bitsStored : 16));
    ds->putAndInsertUint16(DCM_HighBit, static_cast<Uint16>((img->bitsStored > 0 ? img->bitsStored : 16) - 1));
    ds->putAndInsertUint16(DCM_PixelRepresentation, 0); // unsigned

    // ---- DX Image Module ----
    ds->putAndInsertString(DCM_ImageLaterality, "");
    ds->putAndInsertString(DCM_BurnedInAnnotation, "NO");
    ds->putAndInsertString(DCM_PresentationLUTShape, "IDENTITY");

    // Rescale defaults: slope=1.0, intercept=0.0 (REQ-DICOM-022)
    ds->putAndInsertString(DCM_RescaleIntercept, "0");
    ds->putAndInsertString(DCM_RescaleSlope, "1");
    ds->putAndInsertString(DCM_RescaleType, "US");

    // ---- Acquisition Context Module ----
    if (meta->bodyPart[0] != '\0') {
        ds->putAndInsertString(DCM_BodyPartExamined, meta->bodyPart);
    } else {
        ds->putAndInsertString(DCM_BodyPartExamined, "");
    }

    if (meta->kVp > 0.0f) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.4f", static_cast<double>(meta->kVp));
        ds->putAndInsertString(DCM_KVP, buf);
    }

    if (meta->mAs > 0.0f) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.4f", static_cast<double>(meta->mAs));
        ds->putAndInsertString(DCM_ExposureInmAs, buf);  // DS VR, stores decimal mAs
    }

    if (meta->SID_mm > 0.0f) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.4f", static_cast<double>(meta->SID_mm));
        ds->putAndInsertString(DCM_DistanceSourceToDetector, buf);
    }

    if (meta->pixelPitch_mm > 0.0f) {
        // PixelSpacing: row_spacing\col_spacing in mm
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.6f\\%.6f",
                      static_cast<double>(meta->pixelPitch_mm),
                      static_cast<double>(meta->pixelPitch_mm));
        ds->putAndInsertString(DCM_PixelSpacing, buf);
    }

    // Acquisition time from epoch seconds (REQ-DICOM acquisitionTime)
    if (meta->acquisitionTime > 0) {
        time_t t = static_cast<time_t>(meta->acquisitionTime);
        struct tm tmUTC{};
#ifdef _WIN32
        gmtime_s(&tmUTC, &t);
#else
        gmtime_r(&t, &tmUTC);
#endif
        char dateBuf[16], timeBuf[16];
        std::snprintf(dateBuf, sizeof(dateBuf), "%04d%02d%02d",
                      tmUTC.tm_year + 1900, tmUTC.tm_mon + 1, tmUTC.tm_mday);
        std::snprintf(timeBuf, sizeof(timeBuf), "%02d%02d%02d.000000",
                      tmUTC.tm_hour, tmUTC.tm_min, tmUTC.tm_sec);
        ds->putAndInsertString(DCM_StudyDate, dateBuf);
        ds->putAndInsertString(DCM_AcquisitionTime, timeBuf);
    }

    // ---- SOP Common Module ----
    ds->putAndInsertString(DCM_SOPClassUID, SOP_CLASS_DX_PRES);
    ds->putAndInsertString(DCM_SOPInstanceUID, sopUID.c_str());

    return XPE_OK;
}

std::string DicomWriter::generateUID() {
    char uid[128] = {};
    dcmGenerateUniqueIdentifier(uid, SITE_INSTANCE_UID_ROOT);
    return std::string(uid);
}

std::vector<uint8_t> DicomWriter::compressJ2K(const XpeImageBuffer* img) {
    if (!img || !img->data || img->width == 0 || img->height == 0) {
        return {};
    }

    // Set up OpenJPEG J2K lossless compression parameters
    opj_cparameters_t params;
    opj_set_default_encoder_parameters(&params);
    params.irreversible   = 0;     // lossless (reversible wavelet)
    params.numresolution  = 1;
    params.tcp_numlayers  = 1;
    params.cp_disto_alloc = 1;
    params.tcp_rates[0]   = 0;     // lossless = rate 0

    // Create grayscale image with single 16-bit component
    opj_image_cmptparm_t cmptparm{};
    cmptparm.dx   = 1;
    cmptparm.dy   = 1;
    cmptparm.w    = img->width;
    cmptparm.h    = img->height;
    cmptparm.prec = 16;
    cmptparm.bpp  = 16;
    cmptparm.sgnd = 0;  // unsigned

    opj_image_t* opjImg = opj_image_create(1, &cmptparm, OPJ_CLRSPC_GRAY);
    if (!opjImg) {
        spdlog::warn("[DicomWriter] opj_image_create failed");
        return {};
    }

    opjImg->x0 = 0;
    opjImg->y0 = 0;
    opjImg->x1 = img->width;
    opjImg->y1 = img->height;

    // Copy uint16 pixel data into OPJ int32 array
    const uint16_t* src = static_cast<const uint16_t*>(img->data);
    OPJ_INT32* dst = opjImg->comps[0].data;
    size_t npix = static_cast<size_t>(img->width) * img->height;
    for (size_t i = 0; i < npix; ++i) {
        dst[i] = static_cast<OPJ_INT32>(src[i]);
    }

    // Create encoder
    opj_codec_t* codec = opj_create_compress(OPJ_CODEC_J2K);
    if (!codec) {
        opj_image_destroy(opjImg);
        spdlog::warn("[DicomWriter] opj_create_compress failed");
        return {};
    }

    // Silence codec messages
    opj_set_error_handler(codec, nullptr, nullptr);
    opj_set_warning_handler(codec, nullptr, nullptr);
    opj_set_info_handler(codec, nullptr, nullptr);

    if (!opj_setup_encoder(codec, &params, opjImg)) {
        opj_destroy_codec(codec);
        opj_image_destroy(opjImg);
        spdlog::warn("[DicomWriter] opj_setup_encoder failed");
        return {};
    }

    // Allocate output buffer (estimate: 2x input size is safe upper bound)
    size_t bufSize = static_cast<size_t>(img->width) * img->height * 4 + 4096;
    std::vector<uint8_t> compBuf(bufSize);
    OPJ_SIZE_T writtenBytes = 0;

    opj_stream_t* stream = opj_stream_create(bufSize, OPJ_FALSE);
    if (!stream) {
        opj_destroy_codec(codec);
        opj_image_destroy(opjImg);
        return {};
    }

    // Memory write callbacks
    struct WriteStreamData {
        uint8_t* buf;
        OPJ_SIZE_T pos;
        OPJ_SIZE_T size;
    } sd { compBuf.data(), 0, bufSize };

    opj_stream_set_user_data(stream, &sd, nullptr);
    opj_stream_set_user_data_length(stream, static_cast<OPJ_UINT64>(bufSize));

    opj_stream_set_write_function(stream, [](void* buf, OPJ_SIZE_T nb, void* udata) -> OPJ_SIZE_T {
        WriteStreamData* sd = static_cast<WriteStreamData*>(udata);
        if (sd->pos + nb > sd->size) return static_cast<OPJ_SIZE_T>(-1);
        std::memcpy(sd->buf + sd->pos, buf, nb);
        sd->pos += nb;
        return nb;
    });

    opj_stream_set_seek_function(stream, [](OPJ_OFF_T offset, void* udata) -> OPJ_BOOL {
        WriteStreamData* sd = static_cast<WriteStreamData*>(udata);
        if (offset < 0 || static_cast<OPJ_SIZE_T>(offset) > sd->size) return OPJ_FALSE;
        sd->pos = static_cast<OPJ_SIZE_T>(offset);
        return OPJ_TRUE;
    });

    OPJ_BOOL ok = opj_start_compress(codec, opjImg, stream);
    if (ok) ok = opj_encode(codec, stream);
    if (ok) ok = opj_end_compress(codec, stream);

    writtenBytes = sd.pos;

    opj_stream_destroy(stream);
    opj_destroy_codec(codec);
    opj_image_destroy(opjImg);

    if (!ok || writtenBytes == 0) {
        spdlog::warn("[DicomWriter] J2K encode failed");
        return {};
    }

    compBuf.resize(writtenBytes);
    return compBuf;
}

XpeErrorCode DicomWriter::setJ2KPixelData(DcmDataset* ds, const std::vector<uint8_t>& j2kData) {
    if (!ds || j2kData.empty()) return XPE_ERR_INVALID_INPUT;

    // Create a pixel sequence for encapsulated J2K data
    DcmPixelSequence* seq = new DcmPixelSequence(DcmTag(DCM_PixelData, EVR_OB));

    // Add empty basic offset table (required by DICOM standard)
    DcmPixelItem* offsetTable = new DcmPixelItem(DcmTag(DCM_Item, EVR_OB));
    seq->insert(offsetTable);

    // Add the J2K bitstream fragment
    DcmPixelItem* fragment = new DcmPixelItem(DcmTag(DCM_Item, EVR_OB));
    OFCondition rc = fragment->putUint8Array(
        j2kData.data(),
        static_cast<Uint32>(j2kData.size())
    );
    if (rc.bad()) {
        delete fragment;
        delete seq;
        return XPE_ERR_PROCESSING_FAILED;
    }
    seq->insert(fragment);

    // Create a DcmPixelData and set the compressed representation
    DcmPixelData* pd = new DcmPixelData(DCM_PixelData);
    pd->putOriginalRepresentation(EXS_JPEG2000LosslessOnly, nullptr, seq);
    // putOriginalRepresentation is void; seq ownership transferred to pd

    rc = ds->insert(pd, OFTrue);
    if (rc.bad()) {
        delete pd;
        return XPE_ERR_PROCESSING_FAILED;
    }

    return XPE_OK;
}

} // namespace dicom
} // namespace xpe
