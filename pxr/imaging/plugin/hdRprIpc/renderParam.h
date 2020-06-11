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

#ifndef HDRPR_RENDER_PARAM_H
#define HDRPR_RENDER_PARAM_H

#include "pxr/imaging/hd/renderDelegate.h"
#include "rpr/imaging/ipc/server.h"
#include "pxr/usd/sdf/path.h"

#include <map>

PXR_NAMESPACE_OPEN_SCOPE

class RprIpcServer;
class HdRprIpcLayer;
class HdRprRenderThread;

class HdRprRenderParam final : public HdRenderParam {
public:
    HdRprRenderParam(RprIpcServer* ipcServer, HdRprRenderThread* renderThread)
        : ipcServer(ipcServer)
        , renderThread(renderThread)
        , m_fullStageResync{true} {

    }
    ~HdRprRenderParam() override = default;

    RprIpcServer* ipcServer;
    HdRprRenderThread* renderThread;
    std::map<SdfPath, HdRprIpcLayer*> activeLayers;

    void RestartRender() { m_restartRender.store(true); }
    bool IsRenderShouldBeRestarted() { return m_restartRender.exchange(false); }

    void RequestFullStageResync() { m_fullStageResync.store(true); }
    bool IsFullStageResyncRequested() { return m_fullStageResync.load(); }
    bool ResetFullStageResyncRequest() { return m_fullStageResync.exchange(false); }

private:
    std::atomic<bool> m_restartRender;

    /// Enqueued IPR requests
    std::atomic<bool> m_fullStageResync;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_RENDER_PARAM_H
