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
#include "instancer.h"
#include "renderParam.h"
#include "material.h"
#include "primvarUtil.h"
#include "motion.h"
#include "rprApi.h"

#include "pxr/imaging/rprUsd/material.h"
#include "pxr/imaging/rprUsd/error.h"

#include "pxr/imaging/pxOsd/tokens.h"
#include "pxr/imaging/pxOsd/subdivTags.h"

#include "pxr/imaging/hd/meshUtil.h"
#include "pxr/imaging/hd/sprim.h"
#include "pxr/imaging/hd/smoothNormals.h"
#include "pxr/imaging/hd/extComputationUtils.h"

#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/vec4f.h"

PXR_NAMESPACE_OPEN_SCOPE

HdRprMesh::HdRprMesh(SdfPath const& id, SdfPath const& instancerId)
    : HdMesh(id, instancerId)
    , m_visibilityMask(kVisibleAll) {

}

HdDirtyBits HdRprMesh::_PropagateDirtyBits(HdDirtyBits bits) const {
    return bits;
}

HdDirtyBits HdRprMesh::GetInitialDirtyBitsMask() const {
    // The initial dirty bits control what data is available on the first
    // run through _PopulateMesh(), so it should list every data item
    // that _PopluateMesh requests.
    int mask = HdChangeTracker::Clean
        | HdChangeTracker::DirtyPoints
        | HdChangeTracker::DirtyTopology
        | HdChangeTracker::DirtyTransform
        | HdChangeTracker::DirtyPrimvar
        | HdChangeTracker::DirtyNormals
        | HdChangeTracker::DirtyMaterialId
        | HdChangeTracker::DirtySubdivTags
        | HdChangeTracker::DirtyDisplayStyle
        | HdChangeTracker::DirtyVisibility
        | HdChangeTracker::DirtyInstancer
        | HdChangeTracker::DirtyInstanceIndex
        ;

    return (HdDirtyBits)mask;
}

void HdRprMesh::_InitRepr(TfToken const& reprName,
                          HdDirtyBits* dirtyBits) {
    TF_UNUSED(reprName);
    TF_UNUSED(dirtyBits);

    // No-op
}

template <typename T>
bool HdRprMesh::GetPrimvarData(TfToken const& name,
                               HdSceneDelegate* sceneDelegate,
                               std::map<HdInterpolation, HdPrimvarDescriptorVector> primvarDescsPerInterpolation,
                               VtArray<T>& out_data,
                               VtIntArray& out_indices) {
    out_data.clear();
    out_indices.clear();

    for (auto& primvarDescsEntry : primvarDescsPerInterpolation) {
        for (auto& pv : primvarDescsEntry.second) {
            if (pv.name == name) {
                auto value = GetPrimvar(sceneDelegate, name);
                if (value.IsHolding<VtArray<T>>()) {
                    out_data = value.UncheckedGet<VtArray<T>>();
                    if (primvarDescsEntry.first == HdInterpolationFaceVarying) {
                        if (m_sharedFaceVaryingIndices.empty()) {
                            m_sharedFaceVaryingIndices.reserve(m_topology.GetFaceVertexIndices().size());
                            for (int i = 0; i < m_topology.GetFaceVertexIndices().size(); ++i) {
                                m_sharedFaceVaryingIndices.push_back(i);
                            }
                        }
                        out_indices = m_sharedFaceVaryingIndices;
                    }
                    return true;
                }
                return false;
            }
        }
    }

    return false;
}
template bool HdRprMesh::GetPrimvarData<GfVec2f>(TfToken const&, HdSceneDelegate*, std::map<HdInterpolation, HdPrimvarDescriptorVector>, VtArray<GfVec2f>&, VtIntArray&);
template bool HdRprMesh::GetPrimvarData<GfVec3f>(TfToken const&, HdSceneDelegate*, std::map<HdInterpolation, HdPrimvarDescriptorVector>, VtArray<GfVec3f>&, VtIntArray&);

void HdRprMesh::InitFallbackMaterial(HdSceneDelegate* sceneDelegate, HdRprApi* rprApi) {
    if (!m_fallbackMaterial) {
        // XXX: Currently, displayColor is used as one color for whole mesh,
        // but it should be used as attribute per vertex/face.
        // RPR does not have such functionality, yet

        GfVec3f color(0.18f);

        HdPrimvarDescriptorVector primvars = sceneDelegate->GetPrimvarDescriptors(GetId(), HdInterpolationConstant);
        for (auto& pv : primvars) {
            if (pv.name == HdTokens->displayColor) {
                VtValue val = sceneDelegate->Get(GetId(), HdTokens->displayColor);
                if (val.IsHolding<VtVec3fArray>()) {
                    auto colors = val.UncheckedGet<VtVec3fArray>();
                    if (!colors.empty()) {
                        color = colors[0];
                    }
                    break;
                }
            }
        }

        m_fallbackMaterial = rprApi->CreateDiffuseMaterial(color);
    }
}

void FlipWinding(VtIntArray const& vpf, VtIntArray* indices) {
    size_t indicesOffset = 0;
    for (int iFace = 0; iFace < vpf.size(); ++iFace) {
        auto faceIndices = indices->data() + indicesOffset;
        std::swap(faceIndices[0], faceIndices[2]);
        indicesOffset += vpf[iFace];
    }
}

void SplitPolygons(VtIntArray const& indices, VtIntArray const& vpf, VtIntArray* out_indices, VtIntArray* out_vpf) {
    out_indices->clear();
    out_vpf->clear();

    out_indices->reserve(indices.size());
    out_vpf->reserve(vpf.size());

    VtIntArray::const_iterator idxIt = indices.begin();
    for (int vCount : vpf) {
        if (vCount == 3 || vCount == 4) {
            for (int i = 0; i < vCount; ++i) {
                out_indices->push_back(*idxIt);
                ++idxIt;
            }
            out_vpf->push_back(vCount);
        } else {
            const int commonVertex = *idxIt;
            constexpr int triangleVertexCount = 3;
            for (int i = 1; i < vCount - 1; ++i) {
                out_indices->push_back(commonVertex);
                out_indices->push_back(*(idxIt + i + 0));
                out_indices->push_back(*(idxIt + i + 1));
                out_vpf->push_back(triangleVertexCount);
            }
            idxIt += vCount;
        }
    }
}

void SplitPolygons(VtIntArray const& indices, VtIntArray const& vpf, VtIntArray* out_indices) {
    out_indices->clear();
    out_indices->reserve(indices.size());

    VtIntArray::const_iterator idxIt = indices.begin();
    for (int vCount : vpf) {
        if (vCount == 3 || vCount == 4) {
            for (int i = 0; i < vCount; ++i) {
                out_indices->push_back(*idxIt);
                ++idxIt;
            }
        } else {
            const int commonVertex = *idxIt;
            for (int i = 1; i < vCount - 1; ++i) {
                out_indices->push_back(commonVertex);
                out_indices->push_back(*(idxIt + i + 0));
                out_indices->push_back(*(idxIt + i + 1));
            }
            idxIt += vCount;
        }
    }
}

void PreprocessMeshData(
    RprUsdContextMetadata const& contextMetadata,
    TfToken const& windingOrder,
    VtVec3fArray const& points,
    VtIntArray const& vpf,
    VtIntArray const& pointIndices,
    VtIntArray* out_vpf,
    VtIntArray* out_pointIndices,
    VtVec3fArray* inout_normals,
    VtIntArray* inout_normalIndices,
    VtVec2fArray* inout_uvs,
    VtIntArray* inout_uvIndices) {

    bool splittingRequired = false;
    for (int numVertices : vpf) {
        if (numVertices > 4) {
            splittingRequired = true;
            break;
        }
    }

    out_vpf->clear();
    out_pointIndices->clear();

    if (splittingRequired) {
        SplitPolygons(pointIndices, vpf, out_pointIndices, out_vpf);
        if (windingOrder == HdTokens->leftHanded) {
            FlipWinding(*out_vpf, out_pointIndices);
        }
    } else {
        if (windingOrder == HdTokens->leftHanded) {
            *out_pointIndices = pointIndices;
            FlipWinding(vpf, out_pointIndices);
        }
    }

    auto& validVpf = out_vpf->empty() ? vpf : *out_vpf;
    auto& validPointIndices = out_pointIndices->empty() ? pointIndices : *out_pointIndices;

    auto preprocessPrimvarIndices = [splittingRequired, &windingOrder, &vpf, &out_vpf, &validPointIndices](VtIntArray* indices) {
        if (indices->empty()) {
            *indices = validPointIndices;
        } else {
            if (splittingRequired) {
                VtIntArray newIndices;
                SplitPolygons(*indices, vpf, &newIndices);
                if (windingOrder == HdTokens->leftHanded) {
                    FlipWinding(*out_vpf, &newIndices);
                }
                std::swap(*indices, newIndices);
            } else {
                if (windingOrder == HdTokens->leftHanded) {
                    FlipWinding(vpf, indices);
                }
            }
        }
    };

    if (inout_normals->empty()) {
        if (contextMetadata.pluginType == kPluginHybrid) {
            // XXX (Hybrid): crashing without normals, generate flat normals
            inout_normals->reserve(validVpf.size());
            inout_normalIndices->clear();
            inout_normalIndices->reserve(validPointIndices.size());

            size_t indicesOffset = 0u;
            for (auto numVerticesPerFace : validVpf) {
                for (int i = 0; i < numVerticesPerFace; ++i) {
                    inout_normalIndices->push_back(inout_normals->size());
                }

                auto indices = &validPointIndices[indicesOffset];
                indicesOffset += numVerticesPerFace;

                auto p0 = points[indices[0]];
                auto p1 = points[indices[1]];
                auto p2 = points[indices[2]];

                auto e0 = p0 - p1;
                auto e1 = p2 - p1;

                auto normal = GfCross(e1, e0);
                GfNormalize(&normal);
                inout_normals->push_back(normal);
            }
        }
    } else {
        preprocessPrimvarIndices(inout_normalIndices);
    }

    if (inout_uvs->empty()) {
        if (contextMetadata.pluginType == kPluginHybrid) {
            // XXX (Hybrid): crashing without uvs, generate zero uvs
            *inout_uvIndices = validPointIndices;
            *inout_uvs = VtVec2fArray(points.size(), GfVec2f(0.0f));
        }
    } else {
        preprocessPrimvarIndices(inout_uvIndices);
    }
}

void HdRprMesh::Sync(HdSceneDelegate* sceneDelegate,
                     HdRenderParam* renderParam,
                     HdDirtyBits* dirtyBits,
                     TfToken const& reprName) {
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    SdfPath const& id = GetId();

    auto rprRenderParam = static_cast<HdRprRenderParam*>(renderParam);
    auto rprApi = rprRenderParam->AcquireRprApiForEdit();


    ////////////////////////////////////////////////////////////////////////
    // 1. Pull scene data.

    bool newMesh = false;

    bool pointsIsComputed = false;
    auto extComputationDescs = sceneDelegate->GetExtComputationPrimvarDescriptors(id, HdInterpolationVertex);
    for (auto& desc : extComputationDescs) {
        if (desc.name != HdTokens->points) {
            continue;
        }

        if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, desc.name)) {
            auto valueStore = HdExtComputationUtils::GetComputedPrimvarValues({desc}, sceneDelegate);
            auto pointValueIt = valueStore.find(desc.name);
            if (pointValueIt != valueStore.end()) {
                m_points = pointValueIt->second.Get<VtVec3fArray>();
                m_normalsValid = false;
                pointsIsComputed = true;

                newMesh = true;
            }
        }

        break;
    }

    if (!pointsIsComputed &&
        HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points)) {
        VtValue pointsValue = sceneDelegate->Get(id, HdTokens->points);
        m_points = pointsValue.Get<VtVec3fArray>();

        m_normalsValid = false;

        newMesh = true;
    }

    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
        m_topology = GetMeshTopology(sceneDelegate);

        m_adjacencyValid = false;
        m_normalsValid = false;

        newMesh = true;
    }

    auto& geomSubsets = m_topology.GetGeomSubsets();

    std::map<HdInterpolation, HdPrimvarDescriptorVector> primvarDescsPerInterpolation = {
        {HdInterpolationFaceVarying, sceneDelegate->GetPrimvarDescriptors(id, HdInterpolationFaceVarying)},
        {HdInterpolationVertex, sceneDelegate->GetPrimvarDescriptors(id, HdInterpolationVertex)},
        {HdInterpolationConstant, sceneDelegate->GetPrimvarDescriptors(id, HdInterpolationConstant)},
    };

    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->normals)) {
        m_authoredNormals = GetPrimvarData(HdTokens->normals, sceneDelegate, primvarDescsPerInterpolation, m_normals, m_normalIndices);

        newMesh = true;
    }

    bool fallbackMaterialRequired = m_fallbackMaterial;
    if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
        m_materials.clear();

        auto& renderIndex = sceneDelegate->GetRenderIndex();
        auto getHdRprMaterial = [&renderIndex, &fallbackMaterialRequired](SdfPath const& materialId) {
            auto hdMaterial = renderIndex.GetSprim(HdPrimTypeTokens->material, materialId);
            auto hdRprMaterial = static_cast<const HdRprMaterial*>(hdMaterial);
            if (!hdRprMaterial && !hdRprMaterial->GetRprMaterialObject()) {
                fallbackMaterialRequired = true;
            }
            return hdRprMaterial;
        };

        if (geomSubsets.empty()) {
            m_materials.push_back(getHdRprMaterial(sceneDelegate->GetMaterialId(id)));
        } else {
            m_materials.reserve(geomSubsets.size());
            for (auto& subset : geomSubsets) {
                if (subset.type != HdGeomSubset::TypeFaceSet) continue;

                m_materials.push_back(getHdRprMaterial(subset.materialId));
            }
            m_materials.shrink_to_fit();
        }
    }

    if ((*dirtyBits & HdChangeTracker::DirtyMaterialId) ||
        (*dirtyBits & HdChangeTracker::DirtyPrimvar)) {
        rprApi->Release(m_fallbackMaterial);
        m_fallbackMaterial = nullptr;

        if (fallbackMaterialRequired) {
            InitFallbackMaterial(sceneDelegate, rprApi);
        }

        if (!m_materials.empty() && m_materials[0] &&
            m_materials[0]->GetRprMaterialObject()) {
            auto rprMaterial = m_materials[0]->GetRprMaterialObject();

            auto uvPrimvarName = &rprMaterial->GetUvPrimvarName();
            if (uvPrimvarName->IsEmpty()) {
                static TfToken st("st", TfToken::Immortal);
                uvPrimvarName = &st;
            }

            if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, *uvPrimvarName)) {
                GetPrimvarData(*uvPrimvarName, sceneDelegate, primvarDescsPerInterpolation, m_uvs, m_uvIndices);

                newMesh = true;
            }
        } else {
            if (!m_uvs.empty()) {
                m_uvs.clear();
                m_uvIndices.clear();

                newMesh = true;
            }
        }
    }

    if (*dirtyBits & HdChangeTracker::DirtyVisibility) {
        _sharedData.visible = sceneDelegate->GetVisible(id);
    }

    ////////////////////////////////////////////////////////////////////////
    // 2. Resolve drawstyles

    HdDirtyBits customDirtyBits = 0;

    int newRefineLevel = -1;
    if (*dirtyBits & HdChangeTracker::DirtyDisplayStyle) {
        m_displayStyle = sceneDelegate->GetDisplayStyle(id);
        newRefineLevel = m_displayStyle.refineLevel;
    }

    if (*dirtyBits & HdChangeTracker::DirtyPrimvar) {
        HdRprGeometrySettings geomSettings = {};
        geomSettings.visibilityMask = kVisibleAll;
        HdRprParseGeometrySettings(sceneDelegate, id, primvarDescsPerInterpolation.at(HdInterpolationConstant), &geomSettings);

        newRefineLevel = geomSettings.subdivisionLevel;

        if (m_visibilityMask != geomSettings.visibilityMask) {
            m_visibilityMask = geomSettings.visibilityMask;
            customDirtyBits |= kDirtyVisibilityMask;
        }
    }

    if (m_refineLevel != newRefineLevel) {
        m_refineLevel = newRefineLevel;
        customDirtyBits |= kDirtyRefineLevel;
    }

    bool enableSubdiv = m_topology.GetScheme() == PxOsdOpenSubdivTokens->catmullClark;

    bool smoothNormals = m_displayStyle.flatShadingEnabled;
    // Don't compute smooth normals on a refined mesh. They are implicitly smooth.
    smoothNormals = smoothNormals && !(enableSubdiv && m_refineLevel > 0);

    if (!m_authoredNormals && smoothNormals) {
        if (!m_adjacencyValid) {
            m_adjacency.BuildAdjacencyTable(&m_topology);
            m_adjacencyValid = true;
            m_normalsValid = false;
        }

        if (!m_normalsValid) {
            m_normals = Hd_SmoothNormals::ComputeSmoothNormals(&m_adjacency, m_points.size(), m_points.cdata());
            m_normalsValid = true;

            newMesh = true;
        }
    }

    if (*dirtyBits & HdChangeTracker::DirtyTransform) {
        sceneDelegate->SampleTransform(id, &m_transformSamples);
    }

    if (*dirtyBits & HdChangeTracker::DirtySubdivTags) {
        PxOsdSubdivTags subdivTags = sceneDelegate->GetSubdivTags(id);
        auto vertexInterpolationRule = subdivTags.GetVertexInterpolationRule();

        if (m_vertexInterpolationRule != vertexInterpolationRule) {
            m_vertexInterpolationRule = vertexInterpolationRule;
        } else {
            *dirtyBits &= ~HdChangeTracker::DirtySubdivTags;
        }
    }

    if (*dirtyBits & HdChangeTracker::DirtyInstancer) {
        m_instanceTransforms.clear();

        if (auto instancer = static_cast<HdRprInstancer*>(sceneDelegate->GetRenderIndex().GetInstancer(GetInstancerId()))) {
            auto instanceTransforms = instancer->SampleInstanceTransforms(id);
            auto newNumInstances = (instanceTransforms.count > 0) ? instanceTransforms.values[0].size() : 0;
            if (newNumInstances) {
                m_instanceTransforms.reserve(newNumInstances);

                for (size_t i = 0; i < newNumInstances; ++i) {
                    // Convert transform
                    // Apply prototype transform (m_transformSamples) to all the instances
                    m_instanceTransforms.emplace_back(instanceTransforms.count);
                    auto& instanceTransform = m_instanceTransforms.back();

                    if (m_transformSamples.count == 0 ||
                        (m_transformSamples.count == 1 && (m_transformSamples.values[0] == GfMatrix4d(1)))) {
                        for (size_t j = 0; j < instanceTransforms.count; ++j) {
                            instanceTransform[j] = instanceTransforms.values[j][i];
                        }
                    } else {
                        for (size_t j = 0; j < instanceTransforms.count; ++j) {
                            GfMatrix4d xf_j = m_transformSamples.Resample(instanceTransforms.times[j]);
                            instanceTransform[j] = xf_j * instanceTransforms.values[j][i];
                        }
                    }
                }

                std::swap(m_instanceTransformTimes, instanceTransforms.times);
            } else {
                m_instanceTransforms.clear();
                m_instanceTransformTimes.clear();
            }
        } else {
            for (auto instances : m_rprMeshInstances) {
                for (auto instance : instances) {
                    rprApi->Release(instance);
                }
            }
            m_rprMeshInstances.clear();
        }
    }

    ////////////////////////////////////////////////////////////////////////
    // 3. Reset RPR meshes if needed

    if (newMesh) {
        customDirtyBits |= kDirtyMesh;

        if (geomSubsets.empty()) {
            PreprocessMeshData(rprApi->GetContextMetadata(), m_topology.GetOrientation(), m_points, m_topology.GetFaceVertexCounts(), m_topology.GetFaceVertexIndices(),
                &m_convertedFaceCounts, &m_convertedFaceIndices, &m_normals, &m_normalIndices, &m_uvs, &m_uvIndices);
        } else {
            m_convertedFaceCounts.clear();
            m_convertedFaceIndices.clear();
        }
    }

    m_commitDirtyBits = *dirtyBits | customDirtyBits;
    if (m_commitDirtyBits != HdChangeTracker::Clean) {
        rprApi->EnqueuePrimForCommit(this);
    }

    *dirtyBits = HdChangeTracker::Clean;

}

void HdRprMesh::Finalize(HdRenderParam* renderParam) {
    auto rprApi = static_cast<HdRprRenderParam*>(renderParam)->AcquireRprApiForEdit();

    for (auto mesh : m_rprMeshes) {
        rprApi->Release(mesh);
    }
    for (auto instances : m_rprMeshInstances) {
        for (auto instance : instances) {
            rprApi->Release(instance);
        }
    }
    m_rprMeshInstances.clear();
    m_rprMeshes.clear();

    rprApi->Release(m_fallbackMaterial);
    m_fallbackMaterial = nullptr;

    HdMesh::Finalize(renderParam);
}

rpr::Shape* CreateRprMesh(
    rpr::Context* rprContext,
    rpr::Scene* rprScene,
    VtIntArray const& vpf,
    VtVec3fArray const& points,
    VtIntArray const& pointIndices,
    VtVec3fArray const& normals,
    VtIntArray const& normalIndices,
    VtVec2fArray const& uvs,
    VtIntArray const& uvIndices) {
    if (!rprContext || !rprScene ||
        points.empty() || pointIndices.empty()) {
        return nullptr;
    }

    auto normalIndicesData = normalIndices.data();
    if (normals.empty()) {
        normalIndicesData = nullptr;
    }

    auto uvIndicesData = uvIndices.data();
    if (uvs.empty()) {
        uvIndicesData = nullptr;
    }

    rpr::Status status;
    auto mesh = rprContext->CreateShape(
        (rpr_float const*)points.data(), points.size(), sizeof(GfVec3f),
        (rpr_float const*)(normals.data()), normals.size(), sizeof(GfVec3f),
        (rpr_float const*)(uvs.data()), uvs.size(), sizeof(GfVec2f),
        pointIndices.data(), sizeof(rpr_int),
        normalIndicesData, sizeof(rpr_int),
        uvIndicesData, sizeof(rpr_int),
        vpf.data(), vpf.size(), &status);
    if (!mesh) {
        RPR_ERROR_CHECK(status, "Failed to create mesh");
        return nullptr;
    }

    if (RPR_ERROR_CHECK(rprScene->Attach(mesh), "Failed to attach mesh to scene")) {
        delete mesh;
        return nullptr;
    }

    return mesh;
}

void HdRprSetVisibility(rpr::Shape* mesh, uint32_t visibilityMask, RprUsdContextMetadata const& contextMetadata, rpr::Scene* scene) {
    if (contextMetadata.pluginType == kPluginHybrid) {
        // XXX (Hybrid): rprShapeSetVisibility not supported, emulate visibility using attach/detach
        if (visibilityMask) {
            scene->Attach(mesh);
        } else {
            scene->Detach(mesh);
        }
    } else {
        RPR_ERROR_CHECK(mesh->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_PRIMARY_ONLY_FLAG, visibilityMask & kVisiblePrimary), "Failed to set mesh primary visibility");
        RPR_ERROR_CHECK(mesh->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_SHADOW, visibilityMask & kVisibleShadow), "Failed to set mesh shadow visibility");
        RPR_ERROR_CHECK(mesh->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_REFLECTION, visibilityMask & kVisibleReflection), "Failed to set mesh reflection visibility");
        RPR_ERROR_CHECK(mesh->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_REFRACTION, visibilityMask & kVisibleRefraction), "Failed to set mesh refraction visibility");
        RPR_ERROR_CHECK(mesh->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_TRANSPARENT, visibilityMask & kVisibleTransparent), "Failed to set mesh transparent visibility");
        RPR_ERROR_CHECK(mesh->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_DIFFUSE, visibilityMask & kVisibleDiffuse), "Failed to set mesh diffuse visibility");
        RPR_ERROR_CHECK(mesh->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_GLOSSY_REFLECTION, visibilityMask & kVisibleGlossyReflection), "Failed to set mesh glossyReflection visibility");
        RPR_ERROR_CHECK(mesh->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_GLOSSY_REFRACTION, visibilityMask & kVisibleGlossyRefraction), "Failed to set mesh glossyRefraction visibility");
        RPR_ERROR_CHECK(mesh->SetVisibilityFlag(RPR_SHAPE_VISIBILITY_LIGHT, visibilityMask & kVisibleLight), "Failed to set mesh light visibility");
    }
}

void HdRprRelease(rpr::Shape* shape, rpr::Scene* scene) {
    if (!shape) return;

    RPR_ERROR_CHECK(scene->Detach(shape), "Failed to detach mesh from scene");
    delete shape;
}

void HdRprSetTransform(rpr::SceneObject* object, GfMatrix4d const& transform) {
    auto rprTransform = GfMatrix4f(transform);
    RPR_ERROR_CHECK(object->SetTransform(rprTransform.GetArray(), false), "Fail set object transform");
}

void HdRprSetTransform(rpr::Shape* shape, size_t numSamples, float* timeSamples, GfMatrix4d* transformSamples) {
    if (numSamples == 1) {
        return HdRprSetTransform(shape, transformSamples[0]);
    }

    // XXX (RPR): there is no way to sample all transforms via current RPR API
    auto& startTransform = transformSamples[0];
    auto& endTransform = transformSamples[numSamples - 1];

    GfVec3f linearMotion, scaleMotion, rotateAxis;
    float rotateAngle;
    HdRprGetMotion(startTransform, endTransform, &linearMotion, &scaleMotion, &rotateAxis, &rotateAngle);

    auto rprStartTransform = GfMatrix4f(startTransform);

    RPR_ERROR_CHECK(shape->SetTransform(rprStartTransform.GetArray(), false), "Fail set shape transform");
    RPR_ERROR_CHECK(shape->SetLinearMotion(linearMotion[0], linearMotion[1], linearMotion[2]), "Fail to set shape linear motion");
    RPR_ERROR_CHECK(shape->SetScaleMotion(scaleMotion[0], scaleMotion[1], scaleMotion[2]), "Fail to set shape scale motion");
    RPR_ERROR_CHECK(shape->SetAngularMotion(rotateAxis[0], rotateAxis[1], rotateAxis[2], rotateAngle), "Fail to set shape angular motion");
}

void HdRprMesh::Commit(
    rpr::Context* rprContext, rpr::Scene* rprScene,
    RprUsdContextMetadata const& rprContextMetadata) {
    if (m_commitDirtyBits == HdChangeTracker::Clean) {
        TF_CODING_ERROR("False commit called");
        return;
    }

    if (m_commitDirtyBits & kDirtyMesh) {
        for (auto mesh : m_rprMeshes) {
            HdRprRelease(mesh, rprScene);
        }
        m_rprMeshes.clear();

        for (auto instances : m_rprMeshInstances) {
            for (auto instance : instances) {
                HdRprRelease(instance, rprScene);
            }
        }
        m_rprMeshInstances.clear();

        auto& geomSubsets = m_topology.GetGeomSubsets();
        if (geomSubsets.empty()) {
            auto& vpf = m_convertedFaceCounts.empty() ? m_topology.GetFaceVertexCounts() : m_convertedFaceCounts;
            auto& pointIndices = m_convertedFaceIndices.empty() ? m_topology.GetFaceVertexIndices() : m_convertedFaceIndices;

            auto rprMesh = CreateRprMesh(rprContext, rprScene, vpf, m_points, pointIndices, m_normals, m_normalIndices, m_uvs, m_uvIndices);
            if (rprMesh) {
                RPR_ERROR_CHECK(rprMesh->SetObjectID(GetPrimId()), "Failed to set mesh id");
                m_rprMeshes.push_back(rprMesh);
            }
        } else {
            auto& faceVertexIndices = m_topology.GetFaceVertexIndices();
            auto& faceVertexCounts = m_topology.GetFaceVertexCounts();

            auto numFaces = faceVertexCounts.size();
            std::vector<bool> faceIsUnused(numFaces, true);
            size_t numUnusedFaces = faceIsUnused.size();
            for (auto const& subset : geomSubsets) {
                for (int index : subset.indices) {
                    if (TF_VERIFY(index < numFaces) && faceIsUnused[index]) {
                        faceIsUnused[index] = false;
                        numUnusedFaces--;
                    }
                }
            }
            // If we found any unused faces, build a final subset with those faces.
            // Use the material bound to the parent mesh.
            VtIntArray unusedFaces;
            if (numUnusedFaces) {
                unusedFaces.reserve(numUnusedFaces);
                for (size_t i = 0; i < faceIsUnused.size() && unusedFaces.size() < numUnusedFaces; ++i) {
                    if (faceIsUnused[i]) {
                        unusedFaces.push_back(i);
                    }
                }
            }

            // GeomSubset may reference face subset in any given order so we need to be able to
            //   randomly lookup face indices but each face may be of an arbitrary number of vertices
            std::vector<int> indicesOffsetPrefixSum;
            indicesOffsetPrefixSum.reserve(faceVertexCounts.size());
            int offset = 0;
            for (auto numVerticesInFace : faceVertexCounts) {
                indicesOffsetPrefixSum.push_back(offset);
                offset += numVerticesInFace;
            }

            auto vertexIndexRemapping = std::make_unique<int[]>(m_points.size());
            std::unique_ptr<int[]> normalIndexRemapping;
            if (!m_normals.empty() && !m_normalIndices.empty()) {
                normalIndexRemapping = std::make_unique<int[]>(m_normals.size());
            }
            std::unique_ptr<int[]> uvIndexRemapping;
            if (!m_uvs.empty() && !m_uvIndices.empty()) {
                uvIndexRemapping = std::make_unique<int[]>(m_uvs.size());
            }

            auto processGeomSubset = [&](VtIntArray const& faceIndices) {
                VtVec3fArray subsetPoints;
                VtVec3fArray subsetNormals;
                VtVec2fArray subsetUvs;
                VtIntArray subsetNormalIndices;
                VtIntArray subsetUvIndices;
                VtIntArray subsetPointIndices;
                VtIntArray subsetVertexPerFace;
                subsetVertexPerFace.reserve(faceIndices.size());

                std::fill(vertexIndexRemapping.get(), vertexIndexRemapping.get() + m_points.size(), -1);
                if (normalIndexRemapping) {
                    std::fill(normalIndexRemapping.get(), normalIndexRemapping.get() + m_normals.size(), -1);
                }
                if (uvIndexRemapping) {
                    std::fill(uvIndexRemapping.get(), uvIndexRemapping.get() + m_uvs.size(), -1);
                }

                for (auto faceIndex : faceIndices) {
                    int numVerticesInFace = faceVertexCounts[faceIndex];
                    subsetVertexPerFace.push_back(numVerticesInFace);

                    int faceIndicesOffset = indicesOffsetPrefixSum[faceIndex];

                    for (int i = 0; i < numVerticesInFace; ++i) {
                        const int pointIndex = faceVertexIndices[faceIndicesOffset + i];
                        int subsetPointIndex = vertexIndexRemapping[pointIndex];
                        if (subsetPointIndex == -1) {
                            subsetPointIndex = static_cast<int>(subsetPoints.size());
                            vertexIndexRemapping[pointIndex] = subsetPointIndex;

                            subsetPoints.push_back(m_points[pointIndex]);
                        }
                        subsetPointIndices.push_back(subsetPointIndex);

                        if (!m_normals.empty()) {
                            if (m_normalIndices.empty()) {
                                subsetNormals.push_back(m_normals[pointIndex]);
                            } else {
                                const int normalIndex = m_normalIndices[faceIndicesOffset + i];
                                int subsetNormalIndex = normalIndexRemapping[normalIndex];
                                if (subsetNormalIndex == -1) {
                                    subsetNormalIndex = static_cast<int>(subsetNormals.size());
                                    normalIndexRemapping[normalIndex] = subsetNormalIndex;

                                    subsetNormals.push_back(m_normals[normalIndex]);
                                }
                                subsetNormalIndices.push_back(subsetNormalIndex);
                            }
                        }

                        if (!m_uvs.empty()) {
                            if (m_uvIndices.empty()) {
                                subsetUvs.push_back(m_uvs[pointIndex]);
                            } else {
                                const int uvIndex = m_uvIndices[faceIndicesOffset + i];
                                int subsetuvIndex = uvIndexRemapping[uvIndex];
                                if (subsetuvIndex == -1) {
                                    subsetuvIndex = static_cast<int>(subsetUvs.size());
                                    uvIndexRemapping[uvIndex] = subsetuvIndex;

                                    subsetUvs.push_back(m_uvs[uvIndex]);
                                }
                                subsetUvIndices.push_back(subsetuvIndex);
                            }
                        }
                    }
                }

                VtIntArray convertedSubsetVpf;
                VtIntArray convertedSubsetPointIndices;
                PreprocessMeshData(rprContextMetadata, m_topology.GetOrientation(), subsetPoints, subsetVertexPerFace, subsetPointIndices,
                    &convertedSubsetVpf, &convertedSubsetPointIndices, &subsetNormals, &subsetNormalIndices, &subsetUvs, &subsetUvIndices);

                auto& vpf = convertedSubsetVpf.empty() ? subsetVertexPerFace : convertedSubsetVpf;
                auto& pointIndices = convertedSubsetPointIndices.empty() ? subsetPointIndices : convertedSubsetPointIndices;

                auto rprMesh = CreateRprMesh(rprContext, rprScene, vpf, subsetPoints, pointIndices, subsetNormals, subsetNormalIndices, subsetUvs, subsetUvIndices);
                if (rprMesh) {
                    RPR_ERROR_CHECK(rprMesh->SetObjectID(GetPrimId()), "Failed to set mesh id");
                }
                m_rprMeshes.push_back(rprMesh);
            };

            for (auto& subset : geomSubsets) {
                if (subset.type != HdGeomSubset::TypeFaceSet) {
                    TF_RUNTIME_ERROR("Unknown HdGeomSubset Type");
                    continue;
                }

                processGeomSubset(subset.indices);
            }
        }
    }

    if (!m_rprMeshes.empty()) {
        bool newMesh = m_commitDirtyBits & kDirtyMesh;

        if (newMesh || (m_commitDirtyBits & HdChangeTracker::DirtySubdivTags)) {
            rpr_subdiv_boundary_interfop_type interfopType = m_vertexInterpolationRule == PxOsdOpenSubdivTokens->edgeAndCorner ?
                RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_AND_CORNER :
                RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_ONLY;
            for (auto& rprMesh : m_rprMeshes) {
                if (rprMesh) {
                    RPR_ERROR_CHECK(rprMesh->SetSubdivisionBoundaryInterop(interfopType), "Fail set mesh subdividion boundary");
                }
            }
        }

        if (newMesh || (m_commitDirtyBits & kDirtyRefineLevel)) {
            bool enableSubdiv = m_topology.GetScheme() == PxOsdOpenSubdivTokens->catmullClark;
            int refineLevel = enableSubdiv ? m_refineLevel : 0;
            for (auto& rprMesh : m_rprMeshes) {
                if (rprMesh) {
                    RPR_ERROR_CHECK(rprMesh->SetSubdivisionFactor(refineLevel), "Failed to set mesh subdividion level");
                }
            }
        }

        if (newMesh || ((m_commitDirtyBits & HdChangeTracker::DirtyVisibility) || (m_commitDirtyBits & kDirtyVisibilityMask))) {
            auto visibilityMask = GetVisibilityMask();
            for (auto& rprMesh : m_rprMeshes) {
                if (rprMesh) {
                    HdRprSetVisibility(rprMesh, visibilityMask, rprContextMetadata, rprScene);
                }
            }
        }

        if (newMesh ||
            (m_commitDirtyBits & HdChangeTracker::DirtyMaterialId) ||
            (m_commitDirtyBits & HdChangeTracker::DirtyDisplayStyle) || (m_commitDirtyBits & kDirtyRefineLevel)) { // update displacement material
            for (int i = 0; i < m_rprMeshes.size(); ++i) {
                auto rprMesh = m_rprMeshes[i];
                if (!rprMesh) continue;

                RprUsdMaterial const* material = m_fallbackMaterial;
                if (i < m_materials.size() && m_materials[i]) {
                    if (auto rprMaterial = m_materials[i]->GetRprMaterialObject()) {
                        material = rprMaterial;
                    }
                }

                if (material) {
                    material->AttachTo(rprMesh, m_displayStyle.displacementEnabled);
                } else {
                    RprUsdMaterial::DetachFrom(rprMesh);
                }
            }
        }

        if (newMesh || (m_commitDirtyBits & HdChangeTracker::DirtyInstancer)) {
            if (m_instanceTransforms.empty()) {
                // Reset to state without instances
                if (!m_rprMeshInstances.empty()) {
                    for (auto instances : m_rprMeshInstances) {
                        for (auto instance : instances) {
                            HdRprRelease(instance, rprScene);
                        }
                    }
                    m_rprMeshInstances.clear();

                    auto visibilityMask = GetVisibilityMask();
                    for (auto& rprMesh : m_rprMeshes) {
                        if (rprMesh) {
                            HdRprSetVisibility(rprMesh, visibilityMask, rprContextMetadata, rprScene);
                        }
                    }
                }
            } else {
                m_commitDirtyBits &= ~HdChangeTracker::DirtyTransform;

                // Release excessive mesh instances if any
                for (size_t i = m_rprMeshes.size(); i < m_rprMeshInstances.size(); ++i) {
                    for (auto instance : m_rprMeshInstances[i]) {
                        HdRprRelease(instance, rprScene);
                    }
                }
                m_rprMeshInstances.resize(m_rprMeshes.size());

                auto numInstances = m_instanceTransforms.size();

                for (int i = 0; i < m_rprMeshes.size(); ++i) {
                    auto prototype = m_rprMeshes[i];
                    if (!prototype) continue;

                    auto& meshInstances = m_rprMeshInstances[i];
                    if (meshInstances.size() != numInstances) {
                        if (meshInstances.size() > numInstances) {
                            for (size_t i = numInstances; i < meshInstances.size(); ++i) {
                                HdRprRelease(meshInstances[i], rprScene);
                            }
                            meshInstances.resize(numInstances);
                        } else {
                            for (int j = meshInstances.size(); j < numInstances; ++j) {
                                rpr::Status status;
                                auto instance = rprContext->CreateShapeInstance(prototype, &status);
                                if (instance) {
                                    if (!RPR_ERROR_CHECK(rprScene->Attach(instance), "Failed to attach mesh instance to scene")) {
                                        meshInstances.push_back(instance);
                                    } else {
                                        delete instance;
                                    }
                                } else {
                                    RPR_ERROR_CHECK(status, "Failed to create mesh instance");
                                }
                            }
                        }
                    }

                    for (int j = 0; j < meshInstances.size(); ++j) {
                        HdRprSetTransform(meshInstances[j], m_instanceTransformTimes.size(), m_instanceTransformTimes.data(), m_instanceTransforms[j].data());
                    }

                    // Hide prototype
                    HdRprSetVisibility(prototype, 0, rprContextMetadata, rprScene);
                }
            }
        }

        if (m_commitDirtyBits & HdChangeTracker::DirtyTransform) {
            for (auto rprMesh : m_rprMeshes) {
                if (!rprMesh) continue;

                HdRprSetTransform(rprMesh, m_transformSamples.count, m_transformSamples.times.data(), m_transformSamples.values.data());
            }
        }
    }

    m_commitDirtyBits = HdChangeTracker::Clean;

}

uint32_t HdRprMesh::GetVisibilityMask() const {
    auto visibilityMask = m_visibilityMask;
    if (!_sharedData.visible) {
        // Override m_visibilityMask
        visibilityMask = 0;
    }
    return visibilityMask;
}

PXR_NAMESPACE_CLOSE_SCOPE
