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

#include ".//tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

RprUsdTokensType::RprUsdTokensType() :
    full("Full", TfToken::Immortal),
    full2("Full2", TfToken::Immortal),
    fullLegacy("FullLegacy", TfToken::Immortal),
    high("High", TfToken::Immortal),
    id("rpr:id", TfToken::Immortal),
    low("Low", TfToken::Immortal),
    medium("Medium", TfToken::Immortal),
    rpr("rpr", TfToken::Immortal),
    rprGlobalQuality("rpr:global:quality", TfToken::Immortal),
    allTokens({
        full,
        full2,
        fullLegacy,
        high,
        id,
        low,
        medium,
        rpr,
        rprGlobalQuality
    })
{
}

TfStaticData<RprUsdTokensType> RprUsdTokens;

PXR_NAMESPACE_CLOSE_SCOPE
