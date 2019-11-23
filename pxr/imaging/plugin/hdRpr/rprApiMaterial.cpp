#include "rprApiMaterial.h"
#include "materialAdapter.h"
#include "imageCache.h"
#include "rprApi.h"
#include "rprcpp/rprImage.h"
#include "pxr/usd/ar/resolver.h"

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

bool GetWrapType(const EWrapMode& wrapMode, rpr_image_wrap_type& rprWrapType) {
    switch (wrapMode) {
        case EWrapMode::BLACK:
            rprWrapType = RPR_IMAGE_WRAP_TYPE_CLAMP_ZERO;
            return true;
        case EWrapMode::CLAMP:
            rprWrapType = RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE;
            return true;
        case EWrapMode::MIRROR:
            rprWrapType = RPR_IMAGE_WRAP_TYPE_MIRRORED_REPEAT;
            return true;
        case EWrapMode::REPEAT:
            rprWrapType = RPR_IMAGE_WRAP_TYPE_REPEAT;
            return true;
        default:
            break;
    }
    return false;
}

bool GetSelectedChannel(const EColorChannel& colorChannel, rpr_int& out_selectedChannel) {
    switch (colorChannel) {
        case EColorChannel::R:
            out_selectedChannel = RPR_MATERIAL_NODE_OP_SELECT_X;
            return true;
        case EColorChannel::G:
            out_selectedChannel = RPR_MATERIAL_NODE_OP_SELECT_Y;
            return true;
        case EColorChannel::B:
            out_selectedChannel = RPR_MATERIAL_NODE_OP_SELECT_Z;
            return true;
        case EColorChannel::A:
            out_selectedChannel = RPR_MATERIAL_NODE_OP_SELECT_W;
            return true;
        default:
            break;
    }
    return false;
}

} // namespace anonymous

HdRprApiMaterial::InitialState::InitialState(std::unique_ptr<MaterialAdapter>&& adapter)
    : m_adapter(std::move(adapter)) {

}

HdRprApiMaterial::InitialState::~InitialState() {
    for (auto shape : m_attachedShapes) {
        shape->DetachOnReleaseAction(HdRprApiObjectActionTokens->detachMaterial, false);
    }
}

void HdRprApiMaterial::InitialState::AttachToShape(RprApiObject* shape) {
    m_attachedShapes.push_back(shape);
    shape->AttachOnReleaseAction(HdRprApiObjectActionTokens->detachMaterial, [this, shape](void*) {
        auto shapeIter = std::find(m_attachedShapes.begin(), m_attachedShapes.end(), shape);
        if (TF_VERIFY(shapeIter != m_attachedShapes.end())) {
            m_attachedShapes.erase(shapeIter);
        }
    });
}

void HdRprApiMaterial::InitialState::AttachToCurve(RprApiObject* shape) {
    // no - op
}

std::unique_ptr<HdRprApiMaterial::CompiledState>
HdRprApiMaterial::CompiledState::Create(
    InitialState const* initialState,
    rpr_material_system matSys,
    ImageCache* imageCache) {
    auto& materialAdapter = initialState->GetMaterialAdapter();

    rpr_material_node_type materialType = 0;

    switch (materialAdapter.GetType()) {
        case EMaterialType::EMISSIVE:
            materialType = RPR_MATERIAL_NODE_EMISSIVE;
            break;
        case EMaterialType::TRANSPERENT:
            materialType = RPR_MATERIAL_NODE_TRANSPARENT;
            break;
        case EMaterialType::COLOR:
        case EMaterialType::USD_PREVIEW_SURFACE:
            materialType = RPR_MATERIAL_NODE_UBERV2;
            break;
        default:
            return nullptr;
    }

    rpr_material_node rootMaterialNode = nullptr;
    auto status = rprMaterialSystemCreateNode(matSys, materialType, &rootMaterialNode);
    if (!rootMaterialNode) {
        if (status != RPR_ERROR_UNSUPPORTED) {
            RPR_ERROR_CHECK(status, "Failed to create material node");
        }
        return nullptr;
    }

    auto material = std::unique_ptr<CompiledState>(new CompiledState());
    material->m_rootMaterial = rootMaterialNode;

    for (auto const& param : materialAdapter.GetVec4fRprParams()) {
        const uint32_t& paramId = param.first;
        const GfVec4f& paramValue = param.second;

        if (materialAdapter.GetTexRprParams().count(paramId)) {
            continue;
        }
        rprMaterialNodeSetInputFByKey(rootMaterialNode, paramId, paramValue[0], paramValue[1], paramValue[2], paramValue[3]);
    }

    for (auto param : materialAdapter.GetURprParams()) {
        const uint32_t& paramId = param.first;
        const uint32_t& paramValue = param.second;

        rprMaterialNodeSetInputUByKey(rootMaterialNode, paramId, paramValue);
    }

    auto getTextureMaterialNode = [&material](ImageCache* imageCache, rpr_material_system matSys, MaterialTexture const& matTex) -> rpr_material_node {
        if (matTex.path.empty()) {
            return nullptr;
        }

        rpr_int status = RPR_SUCCESS;

        auto image = imageCache->GetImage(matTex.path);
        if (!image) {
            return nullptr;
        }
        auto rprImage = image->GetHandle();
        material->m_materialImages.push_back(std::move(image));

        rpr_image_wrap_type rprWrapSType;
        rpr_image_wrap_type rprWrapTType;
        if (GetWrapType(matTex.wrapS, rprWrapSType) && GetWrapType(matTex.wrapT, rprWrapTType)) {
            if (rprWrapSType != rprWrapTType) {
                TF_CODING_WARNING("RPR renderer does not support different WrapS and WrapT modes");
            }
            rprImageSetWrap(rprImage, rprWrapSType);
        }

        rpr_material_node materialNode = nullptr;
        rprMaterialSystemCreateNode(matSys, RPR_MATERIAL_NODE_IMAGE_TEXTURE, &materialNode);
        if (!materialNode) {
            return nullptr;
        }

        rprMaterialNodeSetInputImageDataByKey(materialNode, RPR_MATERIAL_INPUT_DATA, rprImage);
        material->m_materialNodes.push_back(materialNode);

        if (matTex.isScaleEnabled || matTex.isBiasEnabled) {
            rpr_material_node uv_node = nullptr;
            status = rprMaterialSystemCreateNode(matSys, RPR_MATERIAL_NODE_INPUT_LOOKUP, &uv_node);
            if (uv_node) {
                status = rprMaterialNodeSetInputUByKey(uv_node, RPR_MATERIAL_INPUT_VALUE, RPR_MATERIAL_NODE_LOOKUP_UV);

                rpr_material_node uv_scaled_node = nullptr;
                rpr_material_node uv_bias_node = nullptr;

                if (matTex.isScaleEnabled) {
                    const GfVec4f& scale = matTex.scale;
                    status = rprMaterialSystemCreateNode(matSys, RPR_MATERIAL_NODE_ARITHMETIC, &uv_scaled_node);
                    if (uv_scaled_node) {
                        material->m_materialNodes.push_back(uv_scaled_node);

                        status = rprMaterialNodeSetInputUByKey(uv_scaled_node, RPR_MATERIAL_INPUT_OP, RPR_MATERIAL_NODE_OP_MUL);
                        status = rprMaterialNodeSetInputNByKey(uv_scaled_node, RPR_MATERIAL_INPUT_COLOR0, uv_node);
                        status = rprMaterialNodeSetInputFByKey(uv_scaled_node, RPR_MATERIAL_INPUT_COLOR1, scale[0], scale[1], scale[2], 0);
                    }
                }

                if (matTex.isBiasEnabled) {
                    const GfVec4f& bias = matTex.bias;
                    rpr_material_node& color0Input = uv_scaled_node ? uv_scaled_node : uv_node;

                    status = rprMaterialSystemCreateNode(matSys, RPR_MATERIAL_NODE_ARITHMETIC, &uv_bias_node);
                    if (uv_bias_node) {
                        material->m_materialNodes.push_back(uv_bias_node);

                        status = rprMaterialNodeSetInputUByKey(uv_bias_node, RPR_MATERIAL_INPUT_OP, RPR_MATERIAL_NODE_OP_ADD);
                        status = rprMaterialNodeSetInputNByKey(uv_bias_node, RPR_MATERIAL_INPUT_COLOR0, color0Input);
                        status = rprMaterialNodeSetInputFByKey(uv_bias_node, RPR_MATERIAL_INPUT_COLOR1, bias[0], bias[1], bias[2], 0);
                    }
                }

                if (!uv_bias_node && !uv_scaled_node) {
                    rprObjectDelete(uv_node);
                } else {
                    rpr_material_node uvIn = uv_bias_node ? uv_bias_node : uv_scaled_node;
                    rprMaterialNodeSetInputNByKey(materialNode, RPR_MATERIAL_INPUT_UV, uvIn);
                    material->m_materialNodes.push_back(uv_node);
                }
            }
        }

        if (matTex.isOneMinusSrcColor) {
            rpr_material_node arithmetic = nullptr;
            status = rprMaterialSystemCreateNode(matSys, RPR_MATERIAL_NODE_ARITHMETIC, &arithmetic);
            if (arithmetic) {
                status = rprMaterialNodeSetInputFByKey(arithmetic, RPR_MATERIAL_INPUT_COLOR0, 1.0, 1.0, 1.0, 1.0);
                status = rprMaterialNodeSetInputNByKey(arithmetic, RPR_MATERIAL_INPUT_COLOR1, materialNode);
                status = rprMaterialNodeSetInputUByKey(arithmetic, RPR_MATERIAL_INPUT_OP, RPR_MATERIAL_NODE_OP_SUB);
                material->m_materialNodes.push_back(arithmetic);

                materialNode = arithmetic;
            }
        }

        rpr_material_node outTexture = nullptr;
        if (matTex.channel != EColorChannel::NONE) {
            rpr_int selectedChannel = 0;

            if (GetSelectedChannel(matTex.channel, selectedChannel)) {
                rpr_material_node arithmetic = nullptr;
                status = rprMaterialSystemCreateNode(matSys, RPR_MATERIAL_NODE_ARITHMETIC, &arithmetic);
                if (arithmetic) {
                    status = rprMaterialNodeSetInputNByKey(arithmetic, RPR_MATERIAL_INPUT_COLOR0, materialNode);
                    status = rprMaterialNodeSetInputFByKey(arithmetic, RPR_MATERIAL_INPUT_COLOR1, 0.0, 0.0, 0.0, 0.0);
                    status = rprMaterialNodeSetInputUByKey(arithmetic, RPR_MATERIAL_INPUT_OP, selectedChannel);
                    material->m_materialNodes.push_back(arithmetic);

                    outTexture = arithmetic;
                }
            } else {
                outTexture = materialNode;
            }
        } else {
            outTexture = materialNode;
        }

        return outTexture;
    };

    rpr_material_node emissionColorNode = nullptr;

    for (auto const& texParam : materialAdapter.GetTexRprParams()) {
        const uint32_t& paramId = texParam.first;
        const MaterialTexture& matTex = texParam.second;

        rpr_material_node outNode = getTextureMaterialNode(imageCache, matSys, matTex);
        if (!outNode) {
            continue;
        }

        if (paramId == RPR_UBER_MATERIAL_INPUT_EMISSION_COLOR) {
            emissionColorNode = outNode;
        }

        // SIGGRAPH HACK: Fix for models from Apple AR quick look gallery.
        //   Of all the available ways to load images, none of them gives the opportunity to
        //   get the gamma of the image and does not convert the image to linear space
        if ((paramId == RPR_UBER_MATERIAL_INPUT_DIFFUSE_COLOR ||
             paramId == RPR_UBER_MATERIAL_INPUT_REFLECTION_COLOR) &&
            ArGetResolver().GetExtension(matTex.path) == "png") {
            rprImageSetGamma(material->m_materialImages.back()->GetHandle(), 2.2f);
        }

        // normal map textures need to be passed through the normal map node
        if (paramId == RPR_UBER_MATERIAL_INPUT_DIFFUSE_NORMAL ||
            paramId == RPR_UBER_MATERIAL_INPUT_REFLECTION_NORMAL) {
            rpr_material_node textureNode = outNode;
            int status = rprMaterialSystemCreateNode(matSys, RPR_MATERIAL_NODE_NORMAL_MAP, &outNode);
            if (status == RPR_SUCCESS) {
                material->m_materialNodes.push_back(outNode);
                status = rprMaterialNodeSetInputNByKey(outNode, RPR_MATERIAL_INPUT_COLOR, textureNode);
            }
        }

        rprMaterialNodeSetInputNByKey(rootMaterialNode, paramId, outNode);
    }

    if (emissionColorNode) {
        rpr_material_node averageNode = nullptr;
        rprMaterialSystemCreateNode(matSys, RPR_MATERIAL_NODE_ARITHMETIC, &averageNode);
        if (averageNode) {
            rprMaterialNodeSetInputUByKey(averageNode, RPR_MATERIAL_INPUT_OP, RPR_MATERIAL_NODE_OP_AVERAGE_XYZ);
            rprMaterialNodeSetInputNByKey(averageNode, RPR_MATERIAL_INPUT_COLOR0, emissionColorNode);

            rpr_material_node isBlackColorNode = nullptr;
            rprMaterialSystemCreateNode(matSys, RPR_MATERIAL_NODE_ARITHMETIC, &isBlackColorNode);
            if (isBlackColorNode) {
                material->m_materialNodes.push_back(averageNode);
                material->m_materialNodes.push_back(isBlackColorNode);
                rprMaterialNodeSetInputUByKey(isBlackColorNode, RPR_MATERIAL_INPUT_OP, RPR_MATERIAL_NODE_OP_GREATER);
                rprMaterialNodeSetInputNByKey(isBlackColorNode, RPR_MATERIAL_INPUT_COLOR0, averageNode);
                rprMaterialNodeSetInputFByKey(isBlackColorNode, RPR_MATERIAL_INPUT_COLOR1, 0.0f, 0.0f, 0.0f, 0.0f);

                rprMaterialNodeSetInputNByKey(rootMaterialNode, RPR_UBER_MATERIAL_INPUT_EMISSION_WEIGHT, isBlackColorNode);
            } else {
                rprObjectDelete(averageNode);
            }
        }
    }

    material->m_displacementMaterial = getTextureMaterialNode(imageCache, matSys, materialAdapter.GetDisplacementTexture());

    for (auto attachedShape : initialState->GetAttachedShapes()) {
        material->AttachToShape(attachedShape);
    }
    for (auto attachedCurve : initialState->GetAttachedCurves()) {
        material->AttachToCurve(attachedCurve);
    }

    return material;
}

HdRprApiMaterial::CompiledState::~CompiledState() {
    if (!m_materialImages.empty()) {
        m_imageCache->RequireGarbageCollection();
    }

    for (auto shape : m_attachedShapes) {
        shape->DetachOnReleaseAction(HdRprApiObjectActionTokens->detachMaterial, false);
        rprShapeSetMaterial(shape->GetHandle(), nullptr);
        rprShapeSetDisplacementMaterial(shape->GetHandle(), nullptr);
    }
    if (m_rootMaterial) {
        rprObjectDelete(m_rootMaterial);
    }
    if (m_displacementMaterial) {
        rprObjectDelete(m_displacementMaterial);
    }
    for (auto node : m_materialNodes) {
        rprObjectDelete(node);
    }
}

void HdRprApiMaterial::CompiledState::AttachToShape(RprApiObject* shape) {
    shape->DetachOnReleaseAction(HdRprApiObjectActionTokens->detachMaterial);

    m_attachedShapes.push_back(shape);
    RPR_ERROR_CHECK(rprShapeSetMaterial(shape->GetHandle(), m_rootMaterial), "Failed to set shape material");
    if (m_displacementMaterial) {
        RPR_ERROR_CHECK(rprShapeSetDisplacementMaterial(shape->GetHandle(), m_displacementMaterial), "Failed to set shape material");
    }
    shape->AttachOnReleaseAction(HdRprApiObjectActionTokens->detachMaterial, [this, shape](void* meshHandle) {
        rprShapeSetMaterial(meshHandle, nullptr);
        rprShapeSetDisplacementMaterial(meshHandle, nullptr);
        auto shapeIter = std::find(m_attachedShapes.begin(), m_attachedShapes.end(), shape);
        if (TF_VERIFY(shapeIter != m_attachedShapes.end())) {
            m_attachedShapes.erase(shapeIter);
        }
    });
}

void HdRprApiMaterial::CompiledState::AttachToCurve(RprApiObject* shape) {
    // no - op
}

HdRprApiMaterial::HdRprApiMaterial(std::unique_ptr<MaterialAdapter>&& adapter)
    : m_state(std::unique_ptr<InitialState>(new InitialState(std::move(adapter)))) {

}

void HdRprApiMaterial::AttachToShape(RprApiObject* shape) {
    if (m_state) {
        m_state->AttachToShape(shape);
    }
}

void HdRprApiMaterial::AttachToCurve(RprApiObject* shape) {
    if (m_state) {
        m_state->AttachToCurve(shape);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
