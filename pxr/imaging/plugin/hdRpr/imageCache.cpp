#include "imageCache.h"
#include "imageLoader.h"
#include "pxr/base/arch/fileSystem.h"
#include "pxr/base/tf/diagnostic.h"

PXR_NAMESPACE_OPEN_SCOPE

ImageCache::ImageCache(rpr::Context* context)
    : m_context(context) {

}

std::shared_ptr<rpr::Image> ImageCache::GetImage(std::string const& path) {
    ImageMetadata md(path);

    auto it = m_cache.find(path);
    if (it != m_cache.end() && it->second.IsMetadataEqual(md)) {
        if (auto image = it->second.handle.lock()) {
            return image;
        }
    }

    auto cpuImage = HdRprLoadCpuImage(path);
    if (!cpuImage) {
        return nullptr;
    }

    auto image = std::make_shared<rpr::Image>(m_context, cpuImage->desc, cpuImage->format, cpuImage->data.get());
    if (image) {
        md.handle = image;
        m_cache[path] = md;
    }
    return image;
}

void ImageCache::InsertImage(std::string const& path, std::shared_ptr<rpr::Image> const& image) {
    if (TF_VERIFY(image)) {
        ImageMetadata md(path);
        md.handle = image;
        m_cache[path] = md;
    }
}

bool ImageCache::IsCached(std::string const& path) const {
    ImageMetadata md(path);

    auto it = m_cache.find(path);
    if (it != m_cache.end() && it->second.IsMetadataEqual(md)) {
        return it->second.handle.lock() != nullptr;
    }

    return false;
}

void ImageCache::RequireGarbageCollection() {
    m_garbageCollectionRequired = true;
}

void ImageCache::GarbageCollectIfNeeded() {
    if (!m_garbageCollectionRequired) {
        return;
    }

    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        if (it->second.handle.expired()) {
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }

    m_garbageCollectionRequired = false;
}

ImageCache::ImageMetadata::ImageMetadata(std::string const& path) {
    double time;
    if (!ArchGetModificationTime(path.c_str(), &time)) {
        return;
    }

    int64_t size = ArchGetFileLength(path.c_str());
    if (size == -1) {
        return;
    }

    m_modificationTime = time;
    m_size = static_cast<size_t>(size);
}

bool ImageCache::ImageMetadata::IsMetadataEqual(ImageMetadata const& md) const {
    return m_modificationTime == md.m_modificationTime &&
        m_size == md.m_size;
}

PXR_NAMESPACE_CLOSE_SCOPE
