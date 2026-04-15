#ifndef XPE_DICOM_API_H
#define XPE_DICOM_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Returns the xpe_dicom module version string. */
XPE_API const char* xpe_dicom_version(void);

#ifdef __cplusplus
}
#endif

#endif /* XPE_DICOM_API_H */
