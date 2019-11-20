#include "geometryLight.h"
#include "materialFactory.h"
#include "renderParam.h"
#include "rprApi.h"

PXR_NAMESPACE_OPEN_SCOPE

HdRprGeometryLight::HdRprGeometryLight(SdfPath const& id)
    : HdRprLightBase(id) {

}

void HdRprGeometryLight::Update(HdRprRenderParam* renderParam, uint32_t dirtyFlags) {
    auto rprApi = renderParam->AcquireRprApiForEdit();

    if ((dirtyFlags & ChangeTracker::DirtyEmissionColor) || !m_lightMaterial) {
        MaterialAdapter matAdapter(EMaterialType::EMISSIVE, MaterialParams{{HdLightTokens->color, VtValue(m_emissionColor)}});
        m_lightMaterial = rprApi->CreateMaterial(matAdapter);
    }

    bool newLight = false;
    if (!m_light) {
        auto lightPool = renderParam->GetLightPool();
        auto lightMeshType = GetLightMeshType();
        auto instance = lightPool->CreateLightMeshInstance(rprApi, lightMeshType);
        if (instance) {
            m_light = decltype(m_light)(instance, [lightPool, lightMeshType](RprApiObject* instance) {
                lightPool->ReleaseLightMeshInstance(lightMeshType, instance);
            });
            newLight = true;
        }
    }

    if (m_light) {
        if (newLight ||
            (dirtyFlags & ChangeTracker::DirtyParams) ||
            (dirtyFlags & ChangeTracker::DirtyTransform)) {
            auto transform = GetLightMeshTransform() * m_transform;
            rprApi->SetMeshTransform(m_light.get(), transform);
        }

        if (newLight || (dirtyFlags & ChangeTracker::DirtyEmissionColor)) {
            rprApi->SetMeshMaterial(m_light.get(), m_lightMaterial.get());
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
