#ifndef HDRPR_LIGHT_POOL_H
#define HDRPR_LIGHT_POOL_H

#include "rprApi.h"

#include <mutex>
#include <memory>
#include <unordered_map>

PXR_NAMESPACE_OPEN_SCOPE

class HdRprRenderThread;

class HdRprLightPool {
public:
    enum LightMeshType {
        kCylinderLightMesh = 0,
        kDiskLightMesh,
        kRectLightMesh,
        kSphereLightMesh,
        kLightMeshTypesTotal
    };

    RprApiObject* CreateLightMeshInstance(HdRprApi* rprApi, LightMeshType lightMeshType);
    void ReleaseLightMeshInstance(LightMeshType lightMeshType, RprApiObject* lightMeshInstance);

    RprApiObject* CreateDistantLight(HdRprApi* rprApi);
    void ReleaseDistantLight(RprApiObject* distantLight);

    void GarbageCollectIfNeeded(HdRprRenderThread* renderThread);

private:
    //std::mutex m_lightMeshPrototypeMutex[kLightMeshTypesTotal];
    //std::unique_ptr<RprApiObject> m_lightMeshPrototypes[kLightMeshTypesTotal] = {};
    //std::unique_ptr<RprApiObject> m_lightMeshPrototypeMaterials[kLightMeshTypesTotal] = {};

    class ObjectPool {
    public:
        void Insert(std::unique_ptr<RprApiObject>&& object);

        void MarkAsGarbage(RprApiObject* object);
        RprApiObject* GetObjectFromGarbage();

        void GarbageCollectIfNeeded(HdRprRenderThread* renderThread);

    private:
        using Objects = std::unordered_map<RprApiObject*, std::unique_ptr<RprApiObject>>;
        Objects objects;
        Objects garbage;
        std::mutex mutex;
    };
    ObjectPool m_lightMeshInstancePools[kLightMeshTypesTotal];
    ObjectPool m_distantLightPool;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_LIGHT_POOL_H
