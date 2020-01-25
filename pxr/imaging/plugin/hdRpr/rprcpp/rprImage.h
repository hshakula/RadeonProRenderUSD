#ifndef RPRCPP_IMAGE_H
#define RPRCPP_IMAGE_H

#include "rprObject.h"
#include "rprError.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace rpr {

class Image : public Object {
public:
    Image(rpr_context context, const char* path);
    Image(rpr_context context, rpr_uint width, rpr_uint height, rpr_image_format format, void const* data);
    Image(rpr_context context, rpr_image_desc const& imageDesc, rpr_image_format format, void const* data);
    Image(Image&& image) noexcept;
    ~Image() override = default;

    Image& operator=(Image&& image) noexcept;

    rpr_image_format GetFormat() const;
    rpr_image_desc GetDesc() const;
    rpr_image GetHandle();

private:
    rpr_image GetHandle() const;
};

} // namespace rpr

PXR_NAMESPACE_CLOSE_SCOPE

#endif // RPRCPP_IMAGE_H
