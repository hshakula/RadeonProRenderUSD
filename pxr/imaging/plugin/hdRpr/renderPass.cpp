#include "renderPass.h"
#include "renderDelegate.h"
#include "config.h"
#include "rprApi.h"
#include "renderBuffer.h"
#include "renderParam.h"

#include "pxr/imaging/hd/renderPassState.h"
#include "pxr/imaging/hd/renderIndex.h"
#include "pxr/base/trace/reporter.h"

#include <GL/glew.h>
#include <fstream>

PXR_NAMESPACE_OPEN_SCOPE

HdRprRenderPass::HdRprRenderPass(HdRenderIndex* index,
                                 HdRprimCollection const& collection,
                                 HdRprRenderParam* renderParam)
    : HdRenderPass(index, collection)
    , m_renderParam(renderParam) {

}

void HdRprRenderPass::_Execute(HdRenderPassStateSharedPtr const& renderPassState, TfTokenVector const& renderTags) {
    auto& config = HdRprConfig::GetInstance();
    config.Sync(GetRenderIndex()->GetRenderDelegate());
    if (config.IsDirty(HdRprConfig::DirtyAll)) {
        m_renderParam->GetRenderThread()->StopRender();
    }

    auto rprApiConst = m_renderParam->GetRprApi();

    auto& vp = renderPassState->GetViewport();
    GfVec2i newViewportSize(static_cast<int>(vp[2]), static_cast<int>(vp[3]));
    auto oldViewportSize = rprApiConst->GetViewportSize();
    if (oldViewportSize != newViewportSize) {
        m_renderParam->AcquireRprApiForEdit()->SetViewportSize(newViewportSize);
    }

    if (rprApiConst->GetAovBindings() != renderPassState->GetAovBindings()) {
        m_renderParam->AcquireRprApiForEdit()->SetAovBindings(renderPassState->GetAovBindings());
    }

    auto& cameraViewMatrix = rprApiConst->GetCameraViewMatrix();
    auto& wvm = renderPassState->GetWorldToViewMatrix();
    if (cameraViewMatrix != wvm) {
        m_renderParam->AcquireRprApiForEdit()->SetCameraViewMatrix(wvm);
    }

    auto& cameraProjMatrix = rprApiConst->GetCameraProjectionMatrix();
    auto proj = renderPassState->GetProjectionMatrix();
    if (cameraProjMatrix != proj) {
        m_renderParam->AcquireRprApiForEdit()->SetCameraProjectionMatrix(proj);
    }

    if (rprApiConst->IsChanged()) {
        for (auto& aovBinding : renderPassState->GetAovBindings()) {
            if (aovBinding.renderBuffer) {
                auto rprRenderBuffer = static_cast<HdRprRenderBuffer*>(aovBinding.renderBuffer);
                rprRenderBuffer->SetConverged(false);
            }
        }
        m_renderParam->GetRenderThread()->StartRender();

        static int traceCount = 0;
        std::ofstream reportFile("trace" + std::to_string(traceCount++) + ".txt");
        auto reporter = TraceReporter::GetGlobalReporter();
        reporter->Report(reportFile);
        reporter->ClearTree();
    }
}

bool HdRprRenderPass::IsConverged() const {
    for (auto& aovBinding : m_renderParam->GetRprApi()->GetAovBindings()) {
        if (aovBinding.renderBuffer &&
            !aovBinding.renderBuffer->IsConverged()) {
            return false;
        }
    }
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
