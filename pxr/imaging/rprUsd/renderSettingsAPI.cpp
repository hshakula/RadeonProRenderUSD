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

#include ".//renderSettingsAPI.h"
#include "pxr/usd/usd/schemaRegistry.h"
#include "pxr/usd/usd/typed.h"
#include "pxr/usd/usd/tokens.h"

#include "pxr/usd/sdf/types.h"
#include "pxr/usd/sdf/assetPath.h"

PXR_NAMESPACE_OPEN_SCOPE

// Register the schema with the TfType system.
TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<RprUsdRenderSettingsAPI,
        TfType::Bases< UsdAPISchemaBase > >();
    
}

TF_DEFINE_PRIVATE_TOKENS(
    _schemaTokens,
    (RenderSettingsAPI)
);

/* virtual */
RprUsdRenderSettingsAPI::~RprUsdRenderSettingsAPI()
{
}

/* static */
RprUsdRenderSettingsAPI
RprUsdRenderSettingsAPI::Get(const UsdStagePtr &stage, const SdfPath &path)
{
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return RprUsdRenderSettingsAPI();
    }
    return RprUsdRenderSettingsAPI(stage->GetPrimAtPath(path));
}


/* virtual */
UsdSchemaType RprUsdRenderSettingsAPI::_GetSchemaType() const {
    return RprUsdRenderSettingsAPI::schemaType;
}

/* static */
RprUsdRenderSettingsAPI
RprUsdRenderSettingsAPI::Apply(const UsdPrim &prim)
{
    if (prim.ApplyAPI<RprUsdRenderSettingsAPI>()) {
        return RprUsdRenderSettingsAPI(prim);
    }
    return RprUsdRenderSettingsAPI();
}

/* static */
const TfType &
RprUsdRenderSettingsAPI::_GetStaticTfType()
{
    static TfType tfType = TfType::Find<RprUsdRenderSettingsAPI>();
    return tfType;
}

/* static */
bool 
RprUsdRenderSettingsAPI::_IsTypedSchema()
{
    static bool isTyped = _GetStaticTfType().IsA<UsdTyped>();
    return isTyped;
}

/* virtual */
const TfType &
RprUsdRenderSettingsAPI::_GetTfType() const
{
    return _GetStaticTfType();
}

UsdAttribute
RprUsdRenderSettingsAPI::GetQualityAttr() const
{
    return GetPrim().GetAttribute(RprUsdTokens->rprGlobalQuality);
}

UsdAttribute
RprUsdRenderSettingsAPI::CreateQualityAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(RprUsdTokens->rprGlobalQuality,
                       SdfValueTypeNames->Token,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

namespace {
static inline TfTokenVector
_ConcatenateAttributeNames(const TfTokenVector& left,const TfTokenVector& right)
{
    TfTokenVector result;
    result.reserve(left.size() + right.size());
    result.insert(result.end(), left.begin(), left.end());
    result.insert(result.end(), right.begin(), right.end());
    return result;
}
}

/*static*/
const TfTokenVector&
RprUsdRenderSettingsAPI::GetSchemaAttributeNames(bool includeInherited)
{
    static TfTokenVector localNames = {
        RprUsdTokens->rprGlobalQuality,
    };
    static TfTokenVector allNames =
        _ConcatenateAttributeNames(
            UsdAPISchemaBase::GetSchemaAttributeNames(true),
            localNames);

    if (includeInherited)
        return allNames;
    else
        return localNames;
}

PXR_NAMESPACE_CLOSE_SCOPE

// ===================================================================== //
// Feel free to add custom code below this line. It will be preserved by
// the code generator.
//
// Just remember to wrap code in the appropriate delimiters:
// 'PXR_NAMESPACE_OPEN_SCOPE', 'PXR_NAMESPACE_CLOSE_SCOPE'.
// ===================================================================== //
// --(BEGIN CUSTOM CODE)--
