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

#ifndef HDRPR_MULTITHREAD_RPR_API_CONTEXT_H
#define HDRPR_MULTITHREAD_RPR_API_CONTEXT_H

#include "pxr/base/vt/array.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/tf/token.h"

#include <mutex>
#include <vector>
#include <unordered_set>

namespace rpr { class Context; }

PXR_NAMESPACE_OPEN_SCOPE

namespace multithread_rpr_api {

class Resource;
class Mesh;

class Context {
public:
    Context() = default;

    bool CommitResources(rpr::Context* rprContext);

    void EnqueueResourceForCommit(Resource* resource);

    template <typename Iter>
    void EnqueueResourceForCommit(Iter begin, Iter end);

private:
    void _EnqueueResourceForCommit(Resource* resource);

private:
    std::mutex m_resourcesToCommitMutex;
    std::vector<Resource*> m_resourcesToCommit;
    std::unordered_set<Resource*> m_resourcesLookup;
};

class Resource {
public:
    virtual bool Commit(rpr::Context* rprContext) = 0;
};

template <typename Iter>
void Context::EnqueueResourceForCommit(Iter it, Iter end) {
    if (it == end) return;

    std::lock_guard<std::mutex> lock(m_resourcesToCommitMutex);
    for (; it != end; ++it) {
        _EnqueueResourceForCommit(*it);
    }
}

} // namespace multithread_rpr_api

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_MULTITHREAD_RPR_API_CONTEXT_H
