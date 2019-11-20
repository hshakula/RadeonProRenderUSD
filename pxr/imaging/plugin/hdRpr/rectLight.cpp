#include "rectLight.h"
#include "rprApi.h"
#include "pxr/imaging/hd/sceneDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

HdRprRectLight::HdRprRectLight(SdfPath const& id)
    : HdRprGeometryLight(id) {
}

bool HdRprRectLight::SyncParams(HdSceneDelegate* sceneDelegate, SdfPath const& id) {
    float width = std::abs(sceneDelegate->GetLightParamValue(id, HdLightTokens->width).Get<float>());
    float height = std::abs(sceneDelegate->GetLightParamValue(id, HdLightTokens->height).Get<float>());

    bool isDirty = (width != m_width || m_height != height);

    m_width = width;
    m_height = height;

    return isDirty;
}

GfVec3f HdRprRectLight::NormalizeLightColor(const GfMatrix4f& transform, const GfVec3f& inColor) {
    const GfVec4f ox(m_width, 0., 0., 0.);
    const GfVec4f oy(0., m_height, 0., 0.);

    const GfVec4f oxTrans = ox * transform;
    const GfVec4f oyTrans = oy * transform;

    return static_cast<float>(1. / (oxTrans.GetLength() * oyTrans.GetLength())) * inColor;
}

HdRprLightPool::LightMeshType HdRprRectLight::GetLightMeshType() const {
    return HdRprLightPool::kRectLightMesh;
}

GfMatrix4f HdRprRectLight::GetLightMeshTransform() const {
    return GfMatrix4f(1.0f).SetScale({m_width, m_height, 1.0f});
}

PXR_NAMESPACE_CLOSE_SCOPE
