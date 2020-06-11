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

#ifndef HD_RPR_IPC_LAYER_H
#define HD_RPR_IPC_LAYER_H

#include "pxr/usd/usd/stage.h"

PXR_NAMESPACE_OPEN_SCOPE

class RprIpcServer;

class HdRprIpcLayer {
public:
    std::string const& GetEncodedStage();
    uint64_t GetTimestamp() { return m_timestamp; }

protected:
    void OnEdit();

protected:
    UsdStageRefPtr m_stage;

private:
    uint64_t m_timestamp;

    std::string m_encodedStage;
    bool m_isEncodedStageDirty = true;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HD_RPR_IPC_LAYER_H
