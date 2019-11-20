#ifndef HDRPR_RECT_LIGHT_H
#define HDRPR_RECT_LIGHT_H

#include "geometryLight.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRprRectLight : public HdRprGeometryLight {

public:
    HdRprRectLight(SdfPath const& id);
    ~HdRprRectLight() override = default;

protected:
    bool SyncParams(HdSceneDelegate* sceneDelegate, SdfPath const& id) override;

    // Normalize Light Color with surface area
    GfVec3f NormalizeLightColor(const GfMatrix4f& transform, const GfVec3f& emissionColor) override;

    HdRprLightPool::LightMeshType GetLightMeshType() const override;
    GfMatrix4f GetLightMeshTransform() const override;

private:
    float m_width = std::numeric_limits<float>::quiet_NaN();
    float m_height = std::numeric_limits<float>::quiet_NaN();
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_RECT_LIGHT_H
