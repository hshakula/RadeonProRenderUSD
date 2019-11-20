#ifndef HDRPR_DISTANT_LIGHT_H
#define HDRPR_DISTANT_LIGHT_H

#include "lightBase.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRprDistantLight : public HdRprLightBase {

public:
    HdRprDistantLight(SdfPath const& id);
    ~HdRprDistantLight() override = default;

protected:
    bool SyncParams(HdSceneDelegate* sceneDelegate, SdfPath const& id) override;

    GfVec3f NormalizeLightColor(const GfMatrix4f& transform, const GfVec3f& emissionColor) override;

    void Update(HdRprRenderParam* renderParam, uint32_t dirtyFlags) override;

private:
    float m_angle = std::numeric_limits<float>::quiet_NaN();
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_DISTANT_LIGHT_H
