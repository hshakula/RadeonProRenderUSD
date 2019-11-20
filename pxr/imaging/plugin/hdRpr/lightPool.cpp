#include "lightPool.h"
#include "renderThread.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {

std::unique_ptr<RprApiObject> CreateLightMesh(HdRprApi* rprApi, HdRprLightPool::LightMeshType lightMeshType) {
    switch (lightMeshType) {
        case HdRprLightPool::kCylinderLightMesh:
            return rprApi->CreateCylinderLightMesh(1.0f, 1.0f);
        case HdRprLightPool::kDiskLightMesh:
            return rprApi->CreateDiskLightMesh(1.0f);
        case HdRprLightPool::kRectLightMesh:
            return rprApi->CreateRectLightMesh(1.0f, 1.0f);
        case HdRprLightPool::kSphereLightMesh:
            return rprApi->CreateSphereLightMesh(1.0f);
        default:
            TF_CODING_ERROR("Incorrect lightMeshType: %d", lightMeshType);
            return nullptr;
    }
}

} // namespace anonymous

RprApiObject* HdRprLightPool::CreateLightMeshInstance(HdRprApi* rprApi, LightMeshType lightMeshType) {
    if (lightMeshType < 0 || lightMeshType >= kLightMeshTypesTotal) {
        TF_CODING_ERROR("Incorrect lightMeshType: %d", lightMeshType);
        return nullptr;
    }

    // XXX (Tahoe): Ideally, we would like to use one base mesh for all lights of one type: disk, sphere, etc
    // And simply create an instance for each light to have different material and transform on it
    // But Tahoe's emissive material and instancing interaction do not allow us to do it currently
    // So for now, we will create the same mesh for each light to workaround it
    /*if (!m_lightMeshPrototypes[lightMeshType]) {
        std::lock_guard<std::mutex> lock(m_lightMeshPrototypeMutex[lightMeshType]);
        if (!m_lightMeshPrototypes[lightMeshType]) {
            m_lightMeshPrototypes[lightMeshType] = CreateLightMesh(rprApi, lightMeshType);
            rprApi->SetMeshVisibility(m_lightMeshPrototypes[lightMeshType].get(), false);

            // XXX (Tahoe): dummy material required to be set for Tahoe, FIR-1594
            MaterialAdapter materialAdapter(EMaterialType::EMISSIVE, MaterialParams{{HdRprMaterialTokens->color, VtValue(GfVec3f(1.0f, 0.0f, 0.0f))}});
            m_lightMeshPrototypeMaterials[lightMeshType] = rprApi->CreateMaterial(materialAdapter);
            rprApi->SetMeshMaterial(m_lightMeshPrototypes[lightMeshType].get(), m_lightMeshPrototypeMaterials[lightMeshType].get());
        }
    }*/

    auto instance = m_lightMeshInstancePools[lightMeshType].GetObjectFromGarbage();
    if (!instance) {
        auto instanceObject = CreateLightMesh(rprApi, lightMeshType);
        if (instanceObject) {
            instance = instanceObject.get();
            m_lightMeshInstancePools[lightMeshType].Insert(std::move(instanceObject));
        }
    }

    return instance;
}

void HdRprLightPool::ReleaseLightMeshInstance(LightMeshType lightMeshType, RprApiObject* lightMeshInstance) {
    if (lightMeshType < 0 || lightMeshType >= kLightMeshTypesTotal) {
        TF_CODING_ERROR("Incorrect lightMeshType: %d", lightMeshType);
        return;
    }

    m_lightMeshInstancePools[lightMeshType].MarkAsGarbage(lightMeshInstance);
}

RprApiObject* HdRprLightPool::CreateDistantLight(HdRprApi* rprApi) {
    auto light = m_distantLightPool.GetObjectFromGarbage();
    if (!light) {
        auto lightObject = rprApi->CreateDirectionalLight();
        if (lightObject) {
            light = lightObject.get();
            m_distantLightPool.Insert(std::move(lightObject));
        }
    }

    return light;
}

void HdRprLightPool::ReleaseDistantLight(RprApiObject* distantLight) {
    m_distantLightPool.MarkAsGarbage(distantLight);
}

void HdRprLightPool::GarbageCollectIfNeeded(HdRprRenderThread* renderThread) {
    for (auto& pool : m_lightMeshInstancePools) {
        pool.GarbageCollectIfNeeded(renderThread);
    }
    m_distantLightPool.GarbageCollectIfNeeded(renderThread);
}

void HdRprLightPool::ObjectPool::Insert(std::unique_ptr<RprApiObject>&& object) {
    if (!object) {
        return;
    }

    auto id = object.get();
    std::lock_guard<std::mutex> lock(mutex);
    auto emplaceStatus = objects.emplace(id, std::move(object));
    TF_VERIFY(emplaceStatus.second);
}

void HdRprLightPool::ObjectPool::MarkAsGarbage(RprApiObject* objectId) {
    if (!objectId) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex);

    auto objectIter = objects.find(objectId);
    if (objectIter == objects.end()) {
        TF_RUNTIME_ERROR("Failed to change object status: incorrect object");
        return;
    }

    garbage.emplace(objectId, std::move(objectIter->second));
    objects.erase(objectIter);
}

RprApiObject* HdRprLightPool::ObjectPool::GetObjectFromGarbage() {
    if (garbage.empty()) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex);
    auto objectIter = garbage.begin();
    auto object = objectIter->second.get();
    objects.emplace(objectIter->first, std::move(objectIter->second));
    garbage.erase(objectIter);
    return object;
}

void HdRprLightPool::ObjectPool::GarbageCollectIfNeeded(HdRprRenderThread* renderThread) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!garbage.empty()) {
        renderThread->StopRender();
        garbage.clear();
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
