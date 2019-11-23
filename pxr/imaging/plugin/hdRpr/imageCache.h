#ifndef HDRPR_IMAGE_CACHE_H
#define HDRPR_IMAGE_CACHE_H

#include "pxr/pxr.h"
#include "rprcpp/rprImage.h"

#include <memory>
#include <unordered_map>

PXR_NAMESPACE_OPEN_SCOPE

namespace rpr {

class Context;

} // namespace rpr

class ImageCache {
public:
    ImageCache(rpr::Context* context);

    void InsertImage(std::string const& path, std::shared_ptr<rpr::Image> const& image);
    std::shared_ptr<rpr::Image> GetImage(std::string const& path);

    bool IsCached(std::string const& path) const;

    void RequireGarbageCollection();
    void GarbageCollectIfNeeded();

private:
    class ImageMetadata {
    public:
        ImageMetadata() = default;
        ImageMetadata(std::string const& path);

        bool IsMetadataEqual(ImageMetadata const& md) const;

    public:
        std::weak_ptr<rpr::Image> handle;

    private:
        size_t m_size = 0u;
        double m_modificationTime = 0.0;
    };

private:
    rpr::Context* m_context;
    std::unordered_map<std::string, ImageMetadata> m_cache;
    bool m_garbageCollectionRequired = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_IMAGE_CACHE_H
