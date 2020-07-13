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

#include "mesh.h"
#include "context.h"
#include "../rprApi.h"

#include "pxr/imaging/pxOsd/tokens.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/rprUsd/error.h"
#include "pxr/imaging/rprUsd/motion.h"
#include "pxr/imaging/rprUsd/material.h"

#include <RadeonProRender.hpp>

PXR_NAMESPACE_OPEN_SCOPE

namespace multithread_rpr_api {

namespace {

void FlipWinding(VtIntArray* indices, VtIntArray const& vpf, TfToken const& windingOrder) {
    if (windingOrder == HdTokens->rightHanded) {
        return;
    }

    // XXX: RPR does not allow to select which winding order we want to use and it's by default right handed
    size_t indicesOffset = 0;
    for (int iFace = 0; iFace < vpf.size(); ++iFace) {
        auto faceIndices = indices->data() + indicesOffset;
        std::swap(faceIndices[0], faceIndices[2]);
        indicesOffset += vpf[iFace];
    }
}

void SplitPolygons(VtIntArray const& indices, VtIntArray const& vpf, VtIntArray& out_indices, VtIntArray& out_vpf) {
    out_indices.clear();
    out_vpf.clear();

    out_indices.reserve(indices.size());
    out_vpf.reserve(vpf.size());

    VtIntArray::const_iterator idxIt = indices.begin();
    for (int vCount : vpf) {
        if (vCount == 3 || vCount == 4) {
            for (int i = 0; i < vCount; ++i) {
                out_indices.push_back(*idxIt);
                ++idxIt;
            }
            out_vpf.push_back(vCount);
        } else {
            const int commonVertex = *idxIt;
            constexpr int triangleVertexCount = 3;
            for (int i = 1; i < vCount - 1; ++i) {
                out_indices.push_back(commonVertex);
                out_indices.push_back(*(idxIt + i + 0));
                out_indices.push_back(*(idxIt + i + 1));
                out_vpf.push_back(triangleVertexCount);
            }
            idxIt += vCount;
        }
    }
}

void SplitPolygons(VtIntArray const& indices, VtIntArray const& vpf, VtIntArray& out_indices) {
    out_indices.clear();
    out_indices.reserve(indices.size());

    VtIntArray::const_iterator idxIt = indices.begin();
    for (int vCount : vpf) {
        if (vCount == 3 || vCount == 4) {
            for (int i = 0; i < vCount; ++i) {
                out_indices.push_back(*idxIt);
                ++idxIt;
            }
        } else {
            const int commonVertex = *idxIt;
            for (int i = 1; i < vCount - 1; ++i) {
                out_indices.push_back(commonVertex);
                out_indices.push_back(*(idxIt + i + 0));
                out_indices.push_back(*(idxIt + i + 1));
            }
            idxIt += vCount;
        }
    }
}

class MeshPrototype : public Mesh {
public:
    MeshPrototype(VtVec3fArray const& points,
                  VtIntArray const& pointIndices,
                  VtVec3fArray const& normals,
                  VtIntArray const& normalIndices,
                  VtVec2fArray const& uvs,
                  VtIntArray const& uvIndices,
                  VtIntArray const& vpf,
                  TfToken const& polygonWinding);

    bool Commit(rpr::Context* rprContext) override;

private:
    struct CommitData {
        VtVec3fArray points;
        VtVec3fArray normals;
        VtVec2fArray uvs;
        VtIntArray pointIndices;
        VtIntArray normalIndices;
        VtIntArray uvIndices;
        VtIntArray vpf;
    };
    std::unique_ptr<CommitData> m_commitData;
};

MeshPrototype::MeshPrototype(
    VtVec3fArray const& points,
    VtIntArray const& pointIndices,
    VtVec3fArray const& normals,
    VtIntArray const& normalIndices,
    VtVec2fArray const& uvs,
    VtIntArray const& uvIndices,
    VtIntArray const& vpf,
    TfToken const& polygonWinding)
    : m_commitData(std::make_unique<CommitData>()) {

    m_commitData->points = points;

    bool splittingRequired = false;
    for (int vCount : vpf) {
        if (vCount > 4) {
            splittingRequired = true;
            break;
        }
    }

    if (splittingRequired) {
        SplitPolygons(pointIndices, vpf, m_commitData->pointIndices, m_commitData->vpf);
    } else {
        // No actual data copy made due to nature of VtArray (copy-on-write)
        m_commitData->pointIndices = pointIndices;
        m_commitData->vpf = vpf;
    }
    FlipWinding(&m_commitData->pointIndices, m_commitData->vpf, polygonWinding);

    if (normals.empty()) {
        // if (m_rprContextMetadata.pluginType == kPluginHybrid) {
        //     // XXX (Hybrid): we need to generate geometry normals by ourself
        //     normals.reserve(m_commitData->vpf.size());
        //     m_commitData->normalIndices.clear();
        //     m_commitData->normalIndices.reserve(newIndices.size());

        //     size_t indicesOffset = 0u;
        //     for (auto numVerticesPerFace : m_commitData->vpf) {
        //         for (int i = 0; i < numVerticesPerFace; ++i) {
        //             m_commitData->normalIndices.push_back(normals.size());
        //         }

        //         auto indices = &newIndices[indicesOffset];
        //         indicesOffset += numVerticesPerFace;

        //         auto p0 = points[indices[0]];
        //         auto p1 = points[indices[1]];
        //         auto p2 = points[indices[2]];

        //         auto e0 = p0 - p1;
        //         auto e1 = p2 - p1;

        //         auto normal = GfCross(e1, e0);
        //         GfNormalize(&normal);
        //         normals.push_back(normal);
        //     }
        // }
    } else {
        m_commitData->normals = normals;
        if (!normalIndices.empty()) {
            if (splittingRequired) {
                SplitPolygons(normalIndices, vpf, m_commitData->normalIndices);
            } else {
                m_commitData->normalIndices = normalIndices;
            }
            FlipWinding(&m_commitData->normalIndices, m_commitData->vpf, polygonWinding);
        } else {
            // m_commitData->normalIndices = newIndices;
        }
    }

    if (uvs.empty()) {
        // if (m_rprContextMetadata.pluginType == kPluginHybrid) {
        //     m_commitData->uvIndices = newIndices;
        //     uvs = VtVec2fArray(points.size(), GfVec2f(0.0f));
        // }
    } else {
        m_commitData->uvs = uvs;
        if (!uvIndices.empty()) {
            if (splittingRequired) {
                SplitPolygons(uvIndices, vpf, m_commitData->uvIndices);
            } else {
                m_commitData->uvIndices = uvIndices;
            }
            FlipWinding(&m_commitData->uvIndices, m_commitData->vpf, polygonWinding);
        } else {
            // m_commitData->uvIndices = newIndices;
        }
    }
}

template <typename T>
int const* GetIndicesPtr(VtArray<T> const& data, VtIntArray const& dataIndices, VtIntArray const& fallbackIndices) {
    if (data.empty()) {
        return nullptr;
    }

    if (dataIndices.empty()) {
        return fallbackIndices.data();
    } else {
        return dataIndices.data();
    }
}

bool MeshPrototype::Commit(rpr::Context* rprContext) {
    if (!m_commitData) {
        return Mesh::Commit(rprContext);
    }

    auto normalIndices = GetIndicesPtr(m_commitData->normals, m_commitData->normalIndices, m_commitData->pointIndices);
    auto uvIndices = GetIndicesPtr(m_commitData->uvs, m_commitData->uvIndices, m_commitData->pointIndices);

    rpr::Status status;
    auto rprShape = rprContext->CreateShape(
        (rpr_float const*)m_commitData->points.data(), m_commitData->points.size(), sizeof(GfVec3f),
        (rpr_float const*)m_commitData->normals.data(), m_commitData->normals.size(), sizeof(GfVec3f),
        (rpr_float const*)m_commitData->uvs.data(), m_commitData->uvs.size(), sizeof(GfVec2f),
        m_commitData->pointIndices.data(), sizeof(rpr_int),
        normalIndices, sizeof(rpr_int),
        uvIndices, sizeof(rpr_int),
        m_commitData->vpf.data(), m_commitData->vpf.size(), &status);
    if (!rprShape) {
        RPR_ERROR_CHECK(status, "Failed to create mesh");
        return false;
    }

    rpr::Scene* scene;
    if (RPR_ERROR_CHECK(rprContext->GetScene(&scene), "Failed to get rpr::Scene") ||
        RPR_ERROR_CHECK(scene->Attach(rprShape), "Failed to attach mesh to scene")) {
        delete rprShape;
        return false;
    }

    m_rprShape = rprShape;
    m_commitData = nullptr;
    Mesh::Commit(rprContext);
    return true;
}

class MeshInstance : public Mesh {
public:
    MeshInstance(Mesh* prototypeMesh) : m_prototypeMesh(prototypeMesh) {}

    bool Commit(rpr::Context* rprContext) override;

private:
    Mesh* m_prototypeMesh;
};

bool MeshInstance::Commit(rpr::Context* rprContext) {
    if (!m_prototypeMesh) {
        return Mesh::Commit(rprContext);
    }

    auto prototypeShape = m_prototypeMesh->GetRprShape();
    if (!prototypeShape) {
        TF_CODING_ERROR("Missing prototype shape");
        return false;
    }

    rpr::Status status;
    if (auto rprShape = rprContext->CreateShapeInstance(prototypeShape, &status)) {
        rpr::Scene* scene;
        if (RPR_ERROR_CHECK(rprContext->GetScene(&scene), "Failed to get rpr::Scene") ||
            RPR_ERROR_CHECK(scene->Attach(rprShape), "Failed to attach mesh to scene")) {
            delete rprShape;
        } else {
            m_rprShape = rprShape;
            m_prototypeMesh = nullptr;
            Mesh::Commit(rprContext);
            return true;
        }
    } else {
        RPR_ERROR_CHECK(status, "Failed to create mesh instance");
    }

    return false;
}

} // namespace anonymous

Mesh* Mesh::Create(
    VtVec3fArray const& points,
    VtIntArray const& pointIndices,
    VtVec3fArray const& normals,
    VtIntArray const& normalIndices,
    VtVec2fArray const& uvs,
    VtIntArray const& uvIndices,
    VtIntArray const& vpf,
    TfToken const& polygonWinding) {
    return new MeshPrototype(points, pointIndices, normals, normalIndices, uvs, uvIndices, vpf, polygonWinding);
}

Mesh* Mesh::Create(
    Mesh* prototype) {
    return new MeshInstance(prototype);
}

Mesh::~Mesh() {
    if (m_rprShape) {
        auto& rprContext = m_rprShape->GetContext();

        std::lock_guard<std::mutex> rprLock(rprContext.GetMutex());

        rpr::Scene* scene;
        if (!RPR_ERROR_CHECK(rprContext.GetScene(&scene), "Failed to get rpr::Scene")) {
            RPR_ERROR_CHECK(scene->Detach(m_rprShape), "Failed to detach mesh from scene");
        }
        delete m_rprShape;
    }
}

void Mesh::SetRefineLevel(int level) {
    if (m_refineLevel != level) {
        m_refineLevel = level;
        m_dirtyBits |= kDirtyRefineLevel;
    }
}

void Mesh::SetVertexInterpolationRule(TfToken const& boundaryInterpolation) {
    if (m_boundaryInterpolation != boundaryInterpolation) {
        m_boundaryInterpolation = boundaryInterpolation;
        m_dirtyBits |= kDirtyInterpolationRule;
    }
}

void Mesh::SetMaterial(RprUsdMaterial const* material, bool displacementEnabled) {
    if (m_material != material ||
        m_displacementEnabled != displacementEnabled) {
        m_material = material;
        m_displacementEnabled = displacementEnabled;
        m_dirtyBits |= kDirtyMaterial;
    }
}

void Mesh::SetVisibility(uint32_t visibilityMask) {
    if (m_visibilityMask != visibilityMask) {
        m_visibilityMask = visibilityMask;
        m_dirtyBits |= kDirtyVisibility;
    }
}

void Mesh::SetTransform(GfMatrix4f const& transform) {
    m_beginTransform = transform;
    m_endTransformValid = false;
    m_dirtyBits |= kDirtyTransform;
}

void Mesh::SetTransform(size_t numSamples, float const* timeSamples, GfMatrix4d const* transformSamples) {
    if (numSamples == 1) {
        SetTransform(GfMatrix4f(*transformSamples));
    } else {
        // XXX (RPR): there is no way to sample all transforms via current RPR API
        m_beginTransform = GfMatrix4f(transformSamples[0]);
        m_endTransform = GfMatrix4f(transformSamples[numSamples - 1]);
        m_endTransformValid = true;
        m_dirtyBits |= kDirtyTransform;
    }
}

void Mesh::SetId(uint32_t id) {
    if (m_id != id) {
        m_id = id;
        m_dirtyBits |= kDirtyId;
    }
}

bool Mesh::Commit(rpr::Context* rprContext) {
    if (!m_rprShape) {
        return false;
    }

    if (m_dirtyBits & kDirtyRefineLevel) {
        if (RPR_ERROR_CHECK(m_rprShape->SetSubdivisionFactor(m_refineLevel), "Failed to set mesh subdividion level")) {
            m_dirtyBits &= ~kDirtyRefineLevel;
        }
    }

    if (m_dirtyBits & kDirtyInterpolationRule) {
        rpr_subdiv_boundary_interfop_type boundaryInterop = m_boundaryInterpolation == PxOsdOpenSubdivTokens->edgeAndCorner ?
            RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_AND_CORNER :
            RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_ONLY;
        if (RPR_ERROR_CHECK(m_rprShape->SetSubdivisionBoundaryInterop(boundaryInterop), "Fail set mesh subdividion boundary")) {
            m_dirtyBits &= ~kDirtyInterpolationRule;
        }
    }

    if (m_dirtyBits & kDirtyMaterial) {
        if (m_material) {
            m_material->AttachTo(m_rprShape, m_displacementEnabled);
        } else {
            RprUsdMaterial::DetachFrom(m_rprShape);
        }
    }

    if (m_dirtyBits & kDirtyVisibility) {
        // if (m_rprContextMetadata.pluginType == kPluginHybrid) {
        //     // XXX (Hybrid): rprCurveSetVisibility not supported, emulate visibility using attach/detach
        //     if (m_visibilityMask) {
        //         m_scene->Attach(curve);
        //     } else {
        //         m_scene->Detach(curve);
        //     }
        //     m_dirtyFlags |= ChangeTracker::DirtyScene;
        // } else {
            if (RPR_ERROR_CHECK(m_rprShape->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_PRIMARY_ONLY_FLAG, m_visibilityMask & kVisiblePrimary), "Failed to set mesh primary visibility") ||
                RPR_ERROR_CHECK(m_rprShape->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_SHADOW, m_visibilityMask & kVisibleShadow), "Failed to set mesh shadow visibility") ||
                RPR_ERROR_CHECK(m_rprShape->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_REFLECTION, m_visibilityMask & kVisibleReflection), "Failed to set mesh reflection visibility") ||
                RPR_ERROR_CHECK(m_rprShape->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_REFRACTION, m_visibilityMask & kVisibleRefraction), "Failed to set mesh refraction visibility") ||
                RPR_ERROR_CHECK(m_rprShape->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_TRANSPARENT, m_visibilityMask & kVisibleTransparent), "Failed to set mesh transparent visibility") ||
                RPR_ERROR_CHECK(m_rprShape->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_DIFFUSE, m_visibilityMask & kVisibleDiffuse), "Failed to set mesh diffuse visibility") ||
                RPR_ERROR_CHECK(m_rprShape->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_GLOSSY_REFLECTION, m_visibilityMask & kVisibleGlossyReflection), "Failed to set mesh glossyReflection visibility") ||
                RPR_ERROR_CHECK(m_rprShape->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_GLOSSY_REFRACTION, m_visibilityMask & kVisibleGlossyRefraction), "Failed to set mesh glossyRefraction visibility") ||
                RPR_ERROR_CHECK(m_rprShape->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_LIGHT, m_visibilityMask & kVisibleLight), "Failed to set mesh light visibility")) {
                m_dirtyBits &= ~kDirtyVisibility;
            }
        // }
    }

    if (m_dirtyBits & kDirtyId) {
        if (RPR_ERROR_CHECK(m_rprShape->SetObjectID(m_id), "Failed to set mesh id")) {
            m_dirtyBits &= ~kDirtyId;
        }
    }

    if (m_dirtyBits & kDirtyTransform) {
        if (RPR_ERROR_CHECK(m_rprShape->SetTransform(m_beginTransform.GetArray(), false), "Failed to set mesh transform")) {
            m_dirtyBits &= ~kDirtyTransform;
        } else if (m_endTransformValid) {
            GfVec3f linearMotion, scaleMotion, rotateAxis;
            float rotateAngle;
            RprUsdGetMotion(m_beginTransform, m_endTransform, &linearMotion, &scaleMotion, &rotateAxis, &rotateAngle);

            if (RPR_ERROR_CHECK(m_rprShape->SetLinearMotion(linearMotion[0], linearMotion[1], linearMotion[2]), "Fail to set shape linear motion") ||
                RPR_ERROR_CHECK(m_rprShape->SetScaleMotion(scaleMotion[0], scaleMotion[1], scaleMotion[2]), "Fail to set shape scale motion") ||
                RPR_ERROR_CHECK(m_rprShape->SetAngularMotion(rotateAxis[0], rotateAxis[1], rotateAxis[2], rotateAngle), "Fail to set shape angular motion")) {
                m_dirtyBits &= ~kDirtyTransform;
            }
        }
    }

    bool isDirty = m_dirtyBits != kAllClean;
    m_dirtyBits = kAllClean;
    return isDirty;
}

} // namespace multithread_rpr_api

PXR_NAMESPACE_CLOSE_SCOPE
