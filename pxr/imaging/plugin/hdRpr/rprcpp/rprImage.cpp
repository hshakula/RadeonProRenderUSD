#include "rprImage.h"
#include "rprContext.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace rpr {

namespace {

rpr_image_desc GetRprImageDesc(rpr_image_format format, rpr_uint width, rpr_uint height, rpr_uint depth = 1) {
    int bytesPerComponent = 1;
    if (format.type == RPR_COMPONENT_TYPE_FLOAT16) {
        bytesPerComponent = 2;
    } else if (format.type == RPR_COMPONENT_TYPE_FLOAT32) {
        bytesPerComponent = 4;
    }

    rpr_image_desc desc = {};
    desc.image_width = width;
    desc.image_height = height;
    desc.image_depth = depth;
    desc.image_row_pitch = width * format.num_components * bytesPerComponent;
    desc.image_slice_pitch = desc.image_row_pitch * height;

    return desc;
}

} // namespace anonymous

Image::Image(rpr_context context, const char* path) {
    rpr_image image = nullptr;
    RPR_ERROR_CHECK_THROW(rprContextCreateImageFromFile(context, path, &image), "Failed to create image from file");
    m_rprObjectHandle = image;
}

Image::Image(rpr_context context, void const* encodedData, size_t dataSize) {
    throw Error("Image::Image(rpr_context, void const*, size_t) not implemented. This functionality can be added only with RPR 1.34.3");
}

Image::Image(rpr_context context, rpr_uint width, rpr_uint height, rpr_image_format format, void const* data)
    : Image(context, GetRprImageDesc(format, width, height), format, data) {

}

Image::Image(rpr_context context, rpr_image_desc const& desc, rpr_image_format format, void const* data) {
    rpr_image image = nullptr;
    RPR_ERROR_CHECK_THROW(rprContextCreateImage(context, format, &desc, data, &image), "Failed to create image");
    m_rprObjectHandle = image;
}

Image::Image(Image&& image) noexcept {
    *this = std::move(image);
}

Image& Image::operator=(Image&& image) noexcept {
    Object::operator=(std::move(image));
    return *this;
}

rpr_image_format Image::GetFormat() const {
    rpr_image_format format = {};
    size_t ret;
    RPR_ERROR_CHECK_THROW(rprImageGetInfo(GetHandle(), RPR_IMAGE_FORMAT, sizeof(format), &format, &ret), "Failed to get image format");
    return format;
}

rpr_image_desc Image::GetDesc() const {
    rpr_image_desc desc = {};
    size_t ret;
    RPR_ERROR_CHECK_THROW(rprImageGetInfo(GetHandle(), RPR_IMAGE_DESC, sizeof(desc), &desc, &ret), "Failed to get image desc");
    return desc;
}

rpr_image Image::GetHandle() {
    return static_cast<rpr_image>(m_rprObjectHandle);
}

rpr_image Image::GetHandle() const {
    return static_cast<rpr_image>(m_rprObjectHandle);
}

} // namespace rpr

PXR_NAMESPACE_CLOSE_SCOPE
