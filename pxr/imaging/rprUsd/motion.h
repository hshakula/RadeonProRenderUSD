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

#ifndef RPRUSD_MOTION_H
#define RPRUSD_MOTION_H

#include "pxr/imaging/rprUsd/api.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/matrix4f.h"

PXR_NAMESPACE_OPEN_SCOPE

RPRUSD_API
void RprUsdDecomposeTransform(GfMatrix4f const& transform,
                              GfVec3f& scale,
                              GfQuatf& orient,
                              GfVec3f& translate);

RPRUSD_API
void RprUsdGetMotion(GfMatrix4f const& startTransform,
                     GfMatrix4f const& endTransform,
                     GfVec3f* linearMotion,
                     GfVec3f* scaleMotion,
                     GfVec3f* rotateAxis,
                     float* rotateAngle);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // RPRUSD_MOTION_H
