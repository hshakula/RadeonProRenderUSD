#ifndef HDRPR_DISK_LIGHT_H
#define HDRPR_DISK_LIGHT_H

#include "geometryLight.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRprDiskLight : public HdRprGeometryLight {
    
public:
    HdRprDiskLight(SdfPath const & id);
    ~HdRprDiskLight() override = default;

protected:
    bool SyncParams(HdSceneDelegate* sceneDelegate, SdfPath const& id) override;

    // Normalize Light Color with surface area
    GfVec3f NormalizeLightColor(const GfMatrix4f& transform, const GfVec3f& emissionColor) override;

    HdRprLightPool::LightMeshType GetLightMeshType() const override;
    GfMatrix4f GetLightMeshTransform() const override;

private:
    float m_radius = std::numeric_limits<float>::quiet_NaN();
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_DISK_LIGHT_H
