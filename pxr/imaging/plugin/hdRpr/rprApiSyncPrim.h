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

#ifndef HDRPR_API_SYNC_PRIM_H
#define HDRPR_API_SYNC_PRIM_H

#include "pxr/imaging/rprUsd/contextMetadata.h"

namespace rpr { class Context; class Scene; }

PXR_NAMESPACE_OPEN_SCOPE

class HdRprApiSyncPrim {
public:
    virtual void Commit(rpr::Context* rprContext, rpr::Scene* rprScene,
                        RprUsdContextMetadata const& rprContextMetadata) = 0;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_API_SYNC_PRIM_H
