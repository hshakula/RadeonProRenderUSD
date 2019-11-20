#include "distantLight.h"
#include "renderParam.h"

#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/usd/usdLux/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

HdRprDistantLight::HdRprDistantLight(SdfPath const& id)
    : HdRprLightBase(id) {

}

bool HdRprDistantLight::SyncParams(HdSceneDelegate* sceneDelegate, SdfPath const& id) {
    float angle = std::abs(sceneDelegate->GetLightParamValue(id, UsdLuxTokens->angle).Get<float>());

    bool isDirty = angle != m_angle;

    m_angle = angle;

    return isDirty;
}

GfVec3f HdRprDistantLight::NormalizeLightColor(const GfMatrix4f& transform, const GfVec3f& emissionColor) {
    return emissionColor;
}

void HdRprDistantLight::Update(HdRprRenderParam* renderParam, uint32_t dirtyFlags) {
    auto rprApi = renderParam->AcquireRprApiForEdit();

    bool newLight = false;
    if (!m_light) {
        auto lightPool = renderParam->GetLightPool();
        auto light = lightPool->CreateDistantLight(rprApi);
        if (light) {
            m_light = decltype(m_light)(light, [lightPool](RprApiObject* light) {
                lightPool->ReleaseDistantLight(light);
            });
            newLight = true;
        }
    }

    if (m_light) {
        if (newLight || (dirtyFlags & ChangeTracker::DirtyTransform)) {
            rprApi->SetLightTransform(m_light.get(), m_transform);
        }

        if (newLight ||
            (dirtyFlags & ChangeTracker::DirtyEmissionColor) ||
            (dirtyFlags & ChangeTracker::DirtyParams)) {
            // TODO: implement physically correct conversion
            float shadowSoftness = std::min(m_angle * (M_PI / 180.0) * M_PI, 1.0);

            rprApi->SetDirectionalLightAttributes(m_light.get(), m_emissionColor, shadowSoftness);
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
