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

#include "motion.h"

PXR_NAMESPACE_OPEN_SCOPE

void RprUsdDecomposeTransform(
    GfMatrix4f const& transform,
    GfVec3f& scale,
    GfQuatf& orient,
    GfVec3f& translate) {

    translate = GfVec3f(transform.ExtractTranslation());

    GfVec3f col[3], skew;

    // Now get scale and shear.
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            col[i][j] = transform[i][j];
        }
    }

    scale[0] = col[0].GetLength();
    col[0] /= scale[0];

    skew[2] = GfDot(col[0], col[1]);
    // Make Y col orthogonal to X col
    col[1] = col[1] - skew[2] * col[0];

    scale[1] = col[1].GetLength();
    col[1] /= scale[1];
    skew[2] /= scale[1];

    // Compute XZ and YZ shears, orthogonalize Z col
    skew[1] = GfDot(col[0], col[2]);
    col[2] = col[2] - skew[1] * col[0];
    skew[0] = GfDot(col[1], col[2]);
    col[2] = col[2] - skew[0] * col[1];

    scale[2] = col[2].GetLength();
    col[2] /= scale[2];
    skew[1] /= scale[2];
    skew[0] /= scale[2];

    // At this point, the matrix is orthonormal.
    // Check for a coordinate system flip. If the determinant
    // is -1, then negate the matrix and the scaling factors.
    if (GfDot(col[0], GfCross(col[1], col[2])) < 0.0f) {
        for (int i = 0; i < 3; i++) {
            scale[i] *= -1.0f;
            col[i] *= -1.0f;
        }
    }

    float trace = col[0][0] + col[1][1] + col[2][2];
    if (trace > 0.0f) {
        float root = std::sqrt(trace + 1.0f);
        orient.SetReal(0.5f * root);
        root = 0.5f / root;
        orient.SetImaginary(
            root * (col[1][2] - col[2][1]),
            root * (col[2][0] - col[0][2]),
            root * (col[0][1] - col[1][0]));
    } else {
        static int next[3] = {1, 2, 0};
        int i, j, k = 0;
        i = 0;
        if (col[1][1] > col[0][0]) i = 1;
        if (col[2][2] > col[i][i]) i = 2;
        j = next[i];
        k = next[j];

        float root = std::sqrt(col[i][i] - col[j][j] - col[k][k] + 1.0f);

        GfVec3f im;
        im[i] = 0.5f * root;
        root = 0.5f / root;
        im[j] = root * (col[i][j] + col[j][i]);
        im[k] = root * (col[i][k] + col[k][i]);
        orient.SetImaginary(im);
        orient.SetReal(root * (col[j][k] - col[k][j]));
    }
}

void RprUsdGetMotion(
    GfMatrix4f const& startTransform,
    GfMatrix4f const& endTransform,
    GfVec3f* linearMotion,
    GfVec3f* scaleMotion,
    GfVec3f* rotateAxis,
    float* rotateAngle) {

    GfVec3f startScale, startTranslate; GfQuatf startRotation;
    GfVec3f endScale, endTranslate; GfQuatf endRotation;
    RprUsdDecomposeTransform(startTransform, startScale, startRotation, startTranslate);
    RprUsdDecomposeTransform(endTransform, endScale, endRotation, endTranslate);

    *linearMotion = endTranslate - startTranslate;
    *scaleMotion = endScale - startScale;
    *rotateAxis = GfVec3f(1, 0, 0);
    *rotateAngle = 0.0f;

    auto rotateMotion = endRotation * startRotation.GetInverse();
    auto imLen = rotateMotion.GetImaginary().GetLength();
    if (imLen > std::numeric_limits<float>::epsilon()) {
        *rotateAxis = rotateMotion.GetImaginary() / imLen;
        *rotateAngle = 2.0f * std::atan2(imLen, rotateMotion.GetReal());
    }

    if (*rotateAngle > M_PI) {
        *rotateAngle -= 2 * M_PI;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
