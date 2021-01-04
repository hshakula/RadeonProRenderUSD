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

#ifndef PXR_IMAGING_RPRUSD_TOKENS_H
#define PXR_IMAGING_RPRUSD_TOKENS_H

/// \file rprUsd/tokens.h

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// 
// This is an automatically generated file (by usdGenSchema.py).
// Do not hand-edit!
// 
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

#include "pxr/pxr.h"
#include ".//api.h"
#include "pxr/base/tf/staticData.h"
#include "pxr/base/tf/token.h"
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


/// \class RprUsdTokensType
///
/// \link RprUsdTokens \endlink provides static, efficient
/// \link TfToken TfTokens\endlink for use in all public USD API.
///
/// These tokens are auto-generated from the module's schema, representing
/// property names, for when you need to fetch an attribute or relationship
/// directly by name, e.g. UsdPrim::GetAttribute(), in the most efficient
/// manner, and allow the compiler to verify that you spelled the name
/// correctly.
///
/// RprUsdTokens also contains all of the \em allowedTokens values
/// declared for schema builtin attributes of 'token' scene description type.
/// Use RprUsdTokens like so:
///
/// \code
///     gprim.GetMyTokenValuedAttr().Set(RprUsdTokens->full);
/// \endcode
struct RprUsdTokensType {
    RPRUSD_API RprUsdTokensType();
    /// \brief "Full"
    /// 
    /// Possible value for RprUsdRenderSettingsAPI::GetRprGlobalQualityAttr()
    const TfToken full;
    /// \brief "Full2"
    /// 
    /// Default value for RprUsdRenderSettingsAPI::GetRprGlobalQualityAttr()
    const TfToken full2;
    /// \brief "FullLegacy"
    /// 
    /// Possible value for RprUsdRenderSettingsAPI::GetRprGlobalQualityAttr()
    const TfToken fullLegacy;
    /// \brief "High"
    /// 
    /// Possible value for RprUsdRenderSettingsAPI::GetRprGlobalQualityAttr()
    const TfToken high;
    /// \brief "rpr:id"
    /// 
    /// TODO: move to RprMaterialAPI
    const TfToken id;
    /// \brief "Low"
    /// 
    /// Possible value for RprUsdRenderSettingsAPI::GetRprGlobalQualityAttr()
    const TfToken low;
    /// \brief "Medium"
    /// 
    /// Possible value for RprUsdRenderSettingsAPI::GetRprGlobalQualityAttr()
    const TfToken medium;
    /// \brief "rpr"
    /// 
    /// Special token for the rprUsd library.
    const TfToken rpr;
    /// \brief "rpr:global:quality"
    /// 
    /// RprUsdRenderSettingsAPI
    const TfToken rprGlobalQuality;
    /// A vector of all of the tokens listed above.
    const std::vector<TfToken> allTokens;
};

/// \var RprUsdTokens
///
/// A global variable with static, efficient \link TfToken TfTokens\endlink
/// for use in all public USD API.  \sa RprUsdTokensType
extern RPRUSD_API TfStaticData<RprUsdTokensType> RprUsdTokens;

PXR_NAMESPACE_CLOSE_SCOPE

#endif
