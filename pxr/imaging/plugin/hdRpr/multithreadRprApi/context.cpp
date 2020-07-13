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

#include "context.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace multithread_rpr_api {

bool Context::CommitResources(rpr::Context* rprContext) {
    bool isDirty = false;
    for (auto resource : m_resourcesToCommit) {
        isDirty |= resource->Commit(rprContext);
    }
    return isDirty;
}

void Context::EnqueueResourceForCommit(Resource* resource) {
    std::lock_guard<std::mutex> lock(m_resourcesToCommitMutex);
    _EnqueueResourceForCommit(resource);
}

void Context::_EnqueueResourceForCommit(Resource* resource) {
    // Do not insert twice the same resource
    if (m_resourcesLookup.count(resource)) return;

    // we do not want to lose the order in which resources are added
    // because the subsequent resource may require previous one to be
    // committed first that's why we insert them in the vector
    m_resourcesToCommit.push_back(resource);
    m_resourcesLookup.insert(resource);
}

} // namespace multithread_rpr_api

PXR_NAMESPACE_CLOSE_SCOPE
