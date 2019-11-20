#ifndef HDRPR_GEOMETRY_LIGHT_H
#define HDRPR_GEOMETRY_LIGHT_H

#include "lightBase.h"
#include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRprGeometryLight : public HdRprLightBase {
public:
    HdRprGeometryLight(SdfPath const& id);
    ~HdRprGeometryLight() override = default;

protected:
    void Update(HdRprRenderParam* renderParam, uint32_t dirtyFlags) override;

    virtual HdRprLightPool::LightMeshType GetLightMeshType() const = 0;
    virtual GfMatrix4f GetLightMeshTransform() const = 0;

protected:
    RprApiObjectPtr m_lightMaterial;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_GEOMETRY_LIGHT_H
