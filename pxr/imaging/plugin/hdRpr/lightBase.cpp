#include "lightBase.h"
#include "renderParam.h"
#include "rprApi.h"

#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/usd/usdLux/blackbody.h"

PXR_NAMESPACE_OPEN_SCOPE

HdRprLightBase::HdRprLightBase(SdfPath const& id)
    : HdLight(id) {

}

void HdRprLightBase::Sync(HdSceneDelegate* sceneDelegate,
                          HdRenderParam* renderParam,
                          HdDirtyBits* dirtyBits) {
    SdfPath const& id = GetId();

    uint32_t dirtyFlags = ChangeTracker::Clean;

    if (*dirtyBits & DirtyBits::DirtyTransform) {
        m_transform = GfMatrix4f(sceneDelegate->GetLightParamValue(id, HdLightTokens->transform).Get<GfMatrix4d>());
        dirtyFlags |= ChangeTracker::DirtyTransform;
    }

    if (*dirtyBits & DirtyBits::DirtyParams) {
        if (SyncParams(sceneDelegate, id)) {
            dirtyFlags |= ChangeTracker::DirtyParams;
        }

        GfVec3f color = sceneDelegate->GetLightParamValue(id, HdPrimvarRoleTokens->color).Get<GfVec3f>();
        float intensity = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity).Get<float>();
        float exposure = sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure).Get<float>();

        if (sceneDelegate->GetLightParamValue(id, HdLightTokens->enableColorTemperature).Get<bool>()) {
            GfVec3f temperatureColor = UsdLuxBlackbodyTemperatureAsRgb(sceneDelegate->GetLightParamValue(id, HdLightTokens->colorTemperature).Get<float>());
            color = GfCompMult(color, temperatureColor);
        }

        float illuminationIntensity = intensity * exp2(exposure);

        const bool normalize = sceneDelegate->GetLightParamValue(id, HdLightTokens->normalize).Get<bool>();
        const GfVec3f illumColor = color * illuminationIntensity;
        const GfVec3f emissionColor = (normalize) ? NormalizeLightColor(m_transform, illumColor) : illumColor;

        if (m_emissionColor != emissionColor) {
            m_emissionColor = emissionColor;
            dirtyFlags |= ChangeTracker::DirtyEmissionColor;
        }
    }

    if (dirtyFlags & ChangeTracker::AllDirty) {
        auto rprRenderParam = static_cast<HdRprRenderParam*>(renderParam);
        Update(rprRenderParam, dirtyFlags);        
    }

    *dirtyBits = DirtyBits::Clean;
}

HdDirtyBits HdRprLightBase::GetInitialDirtyBitsMask() const {
    return DirtyBits::DirtyTransform
         | DirtyBits::DirtyParams;
}

void HdRprLightBase::Finalize(HdRenderParam* renderParam) {
    // Stop render thread to safely release resources
    static_cast<HdRprRenderParam*>(renderParam)->GetRenderThread()->StopRender();

    HdLight::Finalize(renderParam);
}

PXR_NAMESPACE_CLOSE_SCOPE
