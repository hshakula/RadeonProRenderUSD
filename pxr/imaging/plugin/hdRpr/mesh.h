/************************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
************************************************************************/

#ifndef HDRPR_MESH_H
#define HDRPR_MESH_H

#include "rprApiSyncPrim.h"

#include "pxr/imaging/hd/mesh.h"
#include "pxr/imaging/hd/vertexAdjacency.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/matrix4f.h"

namespace rpr { class Shape; }

PXR_NAMESPACE_OPEN_SCOPE

class HdRprApi;
class HdRprMaterial;
class RprUsdMaterial;

class HdRprMesh final : public HdMesh, public HdRprApiSyncPrim {
public:
    HF_MALLOC_TAG_NEW("new HdRprMesh");

    HdRprMesh(SdfPath const& id, SdfPath const& instancerId = SdfPath());
    ~HdRprMesh() override = default;

    void Sync(HdSceneDelegate* sceneDelegate,
              HdRenderParam* renderParam,
              HdDirtyBits* dirtyBits,
              TfToken const& reprName) override;

    void Finalize(HdRenderParam* renderParam) override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Commit(rpr::Context* rprContext, rpr::Scene* rprScene,
                RprUsdContextMetadata const& rprContextMetadata) override;

protected:
    HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

    void _InitRepr(TfToken const& reprName, HdDirtyBits* dirtyBits) override;

private:
    template <typename T>
    bool GetPrimvarData(TfToken const& name,
                        HdSceneDelegate* sceneDelegate,
                        std::map<HdInterpolation, HdPrimvarDescriptorVector> primvarDescsPerInterpolation,
                        VtArray<T>& out_data,
                        VtIntArray& out_indices);

    void InitFallbackMaterial(HdSceneDelegate* sceneDelegate, HdRprApi* rprApi);

    uint32_t GetVisibilityMask() const;

private:
    // Pulled data
    std::vector<HdRprMaterial const*> m_materials;

    HdTimeSampleArray<GfMatrix4d, 2> m_transformSamples;

    TfSmallVector<float, 2> m_instanceTransformTimes;
    std::vector<TfSmallVector<GfMatrix4d, 2>> m_instanceTransforms;

    HdMeshTopology m_topology;
    VtVec3fArray m_points;

    Hd_VertexAdjacency m_adjacency;
    bool m_adjacencyValid = false;

    VtVec3fArray m_normals;
    VtIntArray m_normalIndices;
    bool m_normalsValid = false;
    bool m_authoredNormals = false;

    VtVec2fArray m_uvs;
    VtIntArray m_uvIndices;

    HdDisplayStyle m_displayStyle;
    int m_refineLevel = 0;

    TfToken m_vertexInterpolationRule;

    uint32_t m_visibilityMask;
    uint32_t m_commitDirtyBits;

    enum CustomDirtyBits {
        kDirtyVisibilityMask = HdChangeTracker::CustomBitsBegin << 0,
        kDirtyRefineLevel = HdChangeTracker::CustomBitsBegin << 1,
        kDirtyMesh = HdChangeTracker::CustomBitsBegin << 2,
    };

    // Custom data
    VtIntArray m_sharedFaceVaryingIndices;

    VtIntArray m_convertedFaceCounts;
    VtIntArray m_convertedFaceIndices;

    // Commit data
    std::vector<rpr::Shape*> m_rprMeshes;
    std::vector<std::vector<rpr::Shape*>> m_rprMeshInstances;

    RprUsdMaterial* m_fallbackMaterial = nullptr;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_MESH_H
