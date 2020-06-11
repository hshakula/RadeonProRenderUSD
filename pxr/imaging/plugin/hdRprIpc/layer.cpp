#include "layer.h"

PXR_NAMESPACE_OPEN_SCOPE

std::string const& HdRprIpcLayer::GetEncodedStage() {
    if (m_stage && m_isEncodedStageDirty) {
        m_isEncodedStageDirty = false;

        if (!m_stage->ExportToString(&m_encodedStage)) {
            TF_RUNTIME_ERROR("Failed to export stage");
        }
    }

    return m_encodedStage;
}

void HdRprIpcLayer::OnEdit() {
    m_timestamp = std::chrono::steady_clock::now().time_since_epoch().count() / 1000;
    m_isEncodedStageDirty = true;
}

PXR_NAMESPACE_CLOSE_SCOPE
