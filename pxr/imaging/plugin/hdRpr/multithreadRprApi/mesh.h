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

#ifndef HDRPR_MULTITHREAD_RPR_API_MESH_H
#define HDRPR_MULTITHREAD_RPR_API_MESH_H

#include "context.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/matrix4d.h"

namespace rpr { class Context; class Shape; }

PXR_NAMESPACE_OPEN_SCOPE

class RprUsdMaterial;

namespace multithread_rpr_api {

class Mesh : public Resource {
public:
    static Mesh* Create(VtVec3fArray const& points,
                        VtIntArray const& pointIndices,
                        VtVec3fArray const& normals,
                        VtIntArray const& normalIndices,
                        VtVec2fArray const& uvs,
                        VtIntArray const& uvIndices,
                        VtIntArray const& vpf,
                        TfToken const& polygonWinding);
    static Mesh* Create(Mesh* prototype);

    ~Mesh();

    void SetRefineLevel(int level);
    void SetVertexInterpolationRule(TfToken const& boundaryInterpolation);
    void SetMaterial(RprUsdMaterial const* material, bool displacementEnabled);
    void SetVisibility(uint32_t visibilityMask);
    void SetTransform(GfMatrix4f const& transform);    
    void SetTransform(size_t numSamples, float const* timeSamples, GfMatrix4d const* transformSamples);
    void SetId(uint32_t id);

    bool Commit(rpr::Context* rprContext) override;

    rpr::Shape* GetRprShape() { return m_rprShape; }

protected:
    Mesh() = default;

protected:
    rpr::Shape* m_rprShape;

private:
    enum ChangeTracker {
        kAllClean = 0,
        kAllDirty = ~kAllClean,
        kDirtyRefineLevel = 1 << 0,
        kDirtyInterpolationRule = 1 << 1,
        kDirtyMaterial = 1 << 2,
        kDirtyVisibility = 1 << 3,
        kDirtyTransform = 1 << 4,
        kDirtyId = 1 << 5,
    };
    uint32_t m_dirtyBits = kAllClean;

    int m_refineLevel;
    TfToken m_boundaryInterpolation;
    RprUsdMaterial const* m_material;
    bool m_displacementEnabled;
    uint32_t m_visibilityMask;
    GfMatrix4f m_beginTransform;
    GfMatrix4f m_endTransform;
    bool m_endTransformValid;
    uint32_t m_id;
};

} // namespace multithread_rpr_api

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_MULTITHREAD_RPR_API_MESH_H
