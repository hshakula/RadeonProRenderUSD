#ifndef HDRPR_LIGHT_BASE_H
#define HDRPR_LIGHT_BASE_H

#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/imaging/hd/sprim.h"
#include "pxr/imaging/hd/light.h"
#include "pxr/usd/sdf/path.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRprRenderParam;
class RprApiObject;
using RprApiObjectPtr = std::unique_ptr<RprApiObject>;

class HdRprLightBase : public HdLight {
public:
    HdRprLightBase(SdfPath const& id);
    ~HdRprLightBase() override = default;

    void Sync(HdSceneDelegate* sceneDelegate,
              HdRenderParam* renderParam,
              HdDirtyBits* dirtyBits) override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Finalize(HdRenderParam* renderParam) override;

protected:
    virtual bool SyncParams(HdSceneDelegate* sceneDelegate, SdfPath const& id) = 0;

    // Normalize Light Color with surface area
    virtual GfVec3f NormalizeLightColor(const GfMatrix4f& transform, const GfVec3f& emissionColor) = 0;

    enum ChangeTracker : uint32_t {
        Clean = 0,
        AllDirty = ~0u,
        DirtyParams = 1 << 0,
        DirtyTransform = 1 << 1,
        DirtyEmissionColor = 1 << 2,
    };

    virtual void Update(HdRprRenderParam* renderParam, uint32_t dirtyFlags) = 0;

protected:
    std::unique_ptr<RprApiObject, std::function<void(RprApiObject*)>> m_light;
    GfVec3f m_emissionColor;
    GfMatrix4f m_transform;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_LIGHT_BASE_H
