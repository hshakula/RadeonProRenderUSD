#ifndef HDRPR_IMAGE_LOADER_H
#define HDRPR_IMAGE_LOADER_H

#include "pxr/pxr.h"

#include <RadeonProRender.h>
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

struct HdRprCpuImage {
    rpr_image_desc desc;
    rpr_image_format format;
    std::unique_ptr<unsigned char[]> data;

    static constexpr float kInvalidCustomGamma = -1.0f;
    float customGamma = kInvalidCustomGamma;
};

std::unique_ptr<HdRprCpuImage> LoadImage(std::string const& path);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_IMAGE_LOADER_H
