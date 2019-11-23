#include "imageLoader.h"
#include "pxr/imaging/glf/glew.h"
#include "pxr/imaging/glf/uvTextureData.h"
#include "pxr/imaging/glf/image.h"
#include "pxr/base/tf/pathUtils.h"

#ifdef ENABLE_RAT
#include <IMG/IMG_File.h>
#include <PXL/PXL_Raster.h>
#endif

PXR_NAMESPACE_OPEN_SCOPE

namespace {

std::unique_ptr<HdRprCpuImage> LoadRatImage(std::string const& path) {
    auto ratImage = std::unique_ptr<IMG_File>(IMG_File::open(path.c_str()));
    if (!ratImage) {
        TF_RUNTIME_ERROR("Failed to load image: %s", path.c_str());
        return nullptr;
    }

    UT_Array<PXL_Raster*> images;
    std::shared_ptr<void> deferImagesRelease(nullptr, [&images](...) {
        for (auto image : images) {
            delete image;
        }
    });
    if (!ratImage->readImages(images) ||
        images.isEmpty()) {
        TF_RUNTIME_ERROR("Failed to load image: %s", path.c_str());
        return nullptr;
    }

    // XXX: use the only first image, find out what to do with other images
    auto image = images[0];

    rpr_image_format format = {};
    if (image->getPacking() == PACK_SINGLE) {
        format.num_components = 1;
    } else if (image->getPacking() == PACK_DUAL) {
        format.num_components = 2;
    } else if (image->getPacking() == PACK_RGB) {
        format.num_components = 3;
    } else if (image->getPacking() == PACK_RGBA) {
        format.num_components = 4;
    } else {
        TF_RUNTIME_ERROR("Failed to load image \"%s\": unsupported RAT packing - %d", path.c_str(), image->getPacking());
        return nullptr;
    }

    if (image->getFormat() == PXL_INT8) {
        format.type = RPR_COMPONENT_TYPE_UINT8;
    } else if (image->getFormat() == PXL_FLOAT16) {
        format.type = RPR_COMPONENT_TYPE_FLOAT16;
    } else if (image->getFormat() == PXL_FLOAT32) {
        format.type = RPR_COMPONENT_TYPE_FLOAT32;
    } else {
        TF_RUNTIME_ERROR("Failed to load image \"%s\": unsupported RAT format - %d", path.c_str(), image->getFormat());
        return nullptr;
    }

    rpr_image_desc desc = {};
    desc.image_width = image->getXres();
    desc.image_height = image->getYres();
    desc.image_depth = 1;
    if (desc.image_height < 1 || desc.image_width < 1) {
        TF_RUNTIME_ERROR("Failed to load image \"%s\": incorrect dimensions", path.c_str());
        return nullptr;
    }

    // RAT image is flipped in Y axis
    auto flippedImage = std::unique_ptr<unsigned char[]>(new unsigned char[image->getStride() * desc.image_height]);
    for (int y = 0; y < desc.image_height; ++y) {
        auto srcData = reinterpret_cast<uint8_t*>(image->getPixels()) + image->getStride() * y;
        auto dstData = &flippedImage[image->getStride() * (desc.image_height - 1 - y)];
        std::memcpy(dstData, srcData, image->getStride());
    }

    auto cpuImage = std::unique_ptr<HdRprCpuImage>(new HdRprCpuImage);
    cpuImage->data = std::move(flippedImage);
    cpuImage->format = format;
    cpuImage->desc = desc;

    if (image->getColorSpace() == PXL_CS_LINEAR ||
        image->getColorSpace() == PXL_CS_GAMMA2_2 || 
        image->getColorSpace() == PXL_CS_CUSTOM_GAMMA) {
        cpuImage->customGamma = image->getColorSpaceGamma();
    }

    return cpuImage;
}

} // namespace anonymous

std::unique_ptr<HdRprCpuImage> HdRprLoadCpuImage(std::string const& path) {
    if (!GlfImage::IsSupportedImageFile(path)) {
#ifdef ENABLE_RAT
        if (TfGetExtension(path) == "rat") {
            return LoadRatImage(path);
        }
#endif
        return nullptr;
    }

    auto textureData = GlfUVTextureData::New(path, INT_MAX, 0, 0, 0, 0);
    if (textureData && textureData->Read(0, false) && textureData->GetRawBuffer()) {
        rpr_image_format format = {};
        switch (textureData->GLType()) {
            case GL_UNSIGNED_BYTE:
                format.type = RPR_COMPONENT_TYPE_UINT8;
                break;
            case GL_HALF_FLOAT:
                format.type = RPR_COMPONENT_TYPE_FLOAT16;
                break;
            case GL_FLOAT:
                format.type = RPR_COMPONENT_TYPE_FLOAT32;
                break;
            default:
                TF_RUNTIME_ERROR("Failed to load image %s. Unsupported pixel data GLtype: %#x", path.c_str(), textureData->GLType());
                return nullptr;
        }

        switch (textureData->GLFormat()) {
            case GL_RED:
                format.num_components = 1;
                break;
            case GL_RGB:
                format.num_components = 3;
                break;
            case GL_RGBA:
                format.num_components = 4;
                break;
            default:
                TF_RUNTIME_ERROR("Failed to load image %s. Unsupported pixel data GLformat: %#x", path.c_str(), textureData->GLFormat());
                return nullptr;
        }

        int bytesPerComponent = 1;
        if (format.type == RPR_COMPONENT_TYPE_FLOAT16) {
            bytesPerComponent = 2;
        } else if (format.type == RPR_COMPONENT_TYPE_FLOAT32) {
            bytesPerComponent = 4;
        }

        rpr_image_desc desc = {};
        desc.image_width = textureData->ResizedWidth();
        desc.image_height = textureData->ResizedHeight();
        desc.image_depth = 1;
        desc.image_row_pitch = bytesPerComponent * format.num_components * desc.image_width;
        desc.image_slice_pitch = desc.image_row_pitch * desc.image_height;

        if (desc.image_width < 1 || desc.image_height < 1) {
            TF_RUNTIME_ERROR("Failed to load image %s. Incorrect dimensions: %u x %u", path.c_str(), desc.image_width, desc.image_height);
            return nullptr;
        }

        auto cpuImage = std::unique_ptr<HdRprCpuImage>(new HdRprCpuImage);

        cpuImage->format = format;
        cpuImage->desc = desc;
        cpuImage->data = std::unique_ptr<unsigned char[]>(new unsigned char[desc.image_slice_pitch]);
        std::memcpy(cpuImage->data.get(), textureData->GetRawBuffer(), desc.image_slice_pitch);

        return cpuImage;
    }

    TF_RUNTIME_ERROR("Failed to load image %s", path.c_str());
    return nullptr;
}

PXR_NAMESPACE_CLOSE_SCOPE
