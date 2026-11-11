// Owner: Jaeku Kang
// The whole scoring pipeline as one function, used by main (single run) and
// by batch.cpp (many runs).
#pragma once
#include <string>

#include "core/types.h"

namespace la::app {

struct PipelineResult {
    NormalizedCup ref, user;
    PatternMask refMask, userMask;
    LayerResult refLayers, userLayers;
    QualityResult quality;
    ScoreResult score;
};

// `stage` (optional) is updated before each step so callers can report where
// an exception came from.
PipelineResult runPipeline(const cv::Mat& refBgr, const cv::Mat& userBgr, const PatternSpec& spec,
                           std::string* stage = nullptr);

}  // namespace la::app
