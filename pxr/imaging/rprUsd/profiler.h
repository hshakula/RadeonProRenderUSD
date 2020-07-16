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

#ifndef RPRUSD_PROFILER_H
#define RPRUSD_PROFILER_H

#include "pxr/imaging/rprUsd/api.h"
#include "pxr/base/tf/singleton.h"
#include "pxr/base/tf/stringUtils.h"

#include <chrono>

PXR_NAMESPACE_OPEN_SCOPE

class RprUsdProfiler {
public:
    using clock = std::chrono::high_resolution_clock;

    enum TimePoint {
        kDelegateCreationEnd = 0,
        kContextCreationStart,
        kContextCreationEnd,
        kTextureCommitStart,
        kTextureCommitEnd,
        kTexturesLoadingStart,
        kTexturesLoadingEnd,
        kMTResourcesCommitStart,
        kMTResourcesCommitEnd,
        kTimePointsCount
    };

    RPRUSD_API
    static void RecordTimePoint(TimePoint timePoint);

    enum IntervalType {
        kMaterialSyncInterval = 0,
        kMeshSyncInterval,
        kMeshCommitInterval,
        kIntervalsCount
    };

    RPRUSD_API
    static void SampleInterval(IntervalType interval, clock::duration intervalDuration);

    RPRUSD_API
    static void DumpSyncStats();

private:
    friend class TfSingleton<RprUsdProfiler>;
    RprUsdProfiler() = default;

    static RprUsdProfiler& GetInstance() {
        return TfSingleton<RprUsdProfiler>::GetInstance();
    }

    static std::string FormatDuration(clock::duration duration);

private:
    bool m_firstSync = true;
    clock::time_point m_timePoints[kTimePointsCount];

    struct Interval {
        clock::duration duration = {};
        int numSamples = 0;
    };
    Interval m_intervals[kIntervalsCount];

    static constexpr const char* kIntervalNames[kIntervalsCount] = {
        "material sync",
        "mesh sync",
        "mesh commit",
    };
};

/// By default profiler 
#if !defined(RPR_USD_PROFILER_ENABLE)
# define RPR_USD_PROFILER_FUNCTION_BEGIN return
#else
# define RPR_USD_PROFILER_FUNCTION_BEGIN
#endif

inline void RprUsdProfiler::RecordTimePoint(TimePoint timePoint) {
    RPR_USD_PROFILER_FUNCTION_BEGIN;
    auto& profiler = GetInstance();
    profiler.m_timePoints[timePoint] = clock::now();

    switch (timePoint) {
        case kDelegateCreationEnd:
            profiler.m_firstSync = true;
            break;
        case kContextCreationEnd: {
            auto contextCreationDuration = profiler.m_timePoints[kContextCreationEnd] - profiler.m_timePoints[kContextCreationStart];
            printf("Context creation time: %s\n", FormatDuration(contextCreationDuration).c_str());
            break;
        }
        default:
            break;
    }
}

inline void RprUsdProfiler::SampleInterval(IntervalType intervalType, clock::duration intervalDuration) {
    RPR_USD_PROFILER_FUNCTION_BEGIN;
    auto& interval = GetInstance().m_intervals[intervalType];

    interval.duration += intervalDuration;
    interval.numSamples++;
}

inline void RprUsdProfiler::DumpSyncStats() {
    RPR_USD_PROFILER_FUNCTION_BEGIN;
    auto& profiler = GetInstance();

    if (profiler.m_firstSync) {
        profiler.m_firstSync = false;

        printf("Time from delegate creation: %s\n", FormatDuration(clock::now() - profiler.m_timePoints[kDelegateCreationEnd]).c_str());
    }

    auto mtCommitDuration = profiler.m_timePoints[kMTResourcesCommitEnd] - profiler.m_timePoints[kMTResourcesCommitStart];
    if (mtCommitDuration.count() > 0) {
        printf("MT commit: %s\n", FormatDuration(mtCommitDuration).c_str());

        profiler.m_timePoints[kMTResourcesCommitStart] = {};
        profiler.m_timePoints[kMTResourcesCommitEnd] = {};
    }

    auto textureCommitDuration = profiler.m_timePoints[kTextureCommitEnd] - profiler.m_timePoints[kTextureCommitStart];
    if (textureCommitDuration.count() > 0) {
        auto texturesLoadingDuration = profiler.m_timePoints[kTexturesLoadingEnd] - profiler.m_timePoints[kTexturesLoadingStart];
        auto rprImagesCreationDuration = textureCommitDuration - texturesLoadingDuration;

        printf("Texture commit: %s\n", FormatDuration(textureCommitDuration).c_str());
        printf("Textures loading from disk: %s\n", FormatDuration(texturesLoadingDuration).c_str());
        printf("rpr::Image's creation: %s\n", FormatDuration(rprImagesCreationDuration).c_str());

        profiler.m_timePoints[kTextureCommitStart] = {};
        profiler.m_timePoints[kTextureCommitEnd] = {};
        profiler.m_timePoints[kTexturesLoadingStart] = {};
        profiler.m_timePoints[kTexturesLoadingEnd] = {};
    }

    for (int i = 0; i < kIntervalsCount; ++i) {
        auto& interval = profiler.m_intervals[i];
        if (!interval.numSamples) continue;

        printf("Total duration of %s: %s\n", kIntervalNames[i], FormatDuration(interval.duration).c_str());

        interval.numSamples = 0;
        interval.duration = {0};
    }
}

inline std::string RprUsdProfiler::FormatDuration(clock::duration duration) {
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
    duration -= minutes;

    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    duration -= seconds;

    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    duration -= milliseconds;

    return TfStringPrintf("%02d:%02d.%03d", int(minutes.count()), int(seconds.count()), int(milliseconds.count()));
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif // RPRUSD_PROFILER_H
