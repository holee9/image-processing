#include "xpe/common/xpe_memory.h"
#include <cstdlib>
#include <cstring>

namespace {

bool bytes_per_pixel(XpePixelFormat format, size_t* bytesOut) noexcept {
    if (!bytesOut) {
        return false;
    }

    switch (format) {
    case XPE_PIXEL_UINT8:
        *bytesOut = 1;
        return true;
    case XPE_PIXEL_UINT16:
        *bytesOut = 2;
        return true;
    case XPE_PIXEL_FLOAT32:
        *bytesOut = 4;
        return true;
    default:
        return false;
    }
}

uint32_t bits_per_pixel(XpePixelFormat format) noexcept {
    return (format == XPE_PIXEL_UINT8) ? 8u :
           (format == XPE_PIXEL_FLOAT32) ? 32u : 16u;
}

} // namespace

XPE_API XpeErrorCode xpe_alloc_image(uint32_t width, uint32_t height,
                                     XpePixelFormat format, XpeImageBuffer* out) {
    if (!out || width == 0 || height == 0)
        return XPE_ERR_INVALID_INPUT;
    if (width > 4096 || height > 4096)
        return XPE_ERR_INVALID_INPUT;

    size_t bytesPerPixel = 0;
    if (!bytes_per_pixel(format, &bytesPerPixel))
        return XPE_ERR_UNSUPPORTED_FORMAT;

    size_t dataSize = (size_t)width * height * bytesPerPixel;

    void* data = std::malloc(dataSize);
    if (!data)
        return XPE_ERR_OUT_OF_MEMORY;

    std::memset(data, 0, dataSize);

    out->width = width;
    out->height = height;
    out->bitsAllocated = bits_per_pixel(format);
    out->bitsStored = out->bitsAllocated;
    out->format = format;
    out->data = data;
    out->dataSize = dataSize;

    return XPE_OK;
}

XPE_API XpeErrorCode xpe_free_image(XpeImageBuffer* buf) {
    if (!buf)
        return XPE_ERR_INVALID_INPUT;
    if (buf->data) {
        std::free(buf->data);
        buf->data = nullptr;
    }
    buf->dataSize = 0;
    return XPE_OK;
}

XPE_API XpeErrorCode xpe_copy_image(const XpeImageBuffer* src, XpeImageBuffer* dst) {
    if (!src || !dst || !src->data || !dst->data)
        return XPE_ERR_INVALID_INPUT;
    if (dst->dataSize < src->dataSize)
        return XPE_ERR_BUFFER_TOO_SMALL;

    std::memcpy(dst->data, src->data, src->dataSize);
    dst->width = src->width;
    dst->height = src->height;
    dst->bitsAllocated = src->bitsAllocated;
    dst->bitsStored = src->bitsStored;
    dst->format = src->format;
    dst->dataSize = src->dataSize;

    return XPE_OK;
}
