// Owner: Jaeku Kang
// The whole scoring pipeline as one function, used by main (single run) and
// by batch.cpp (many runs).
#pragma once
#include <string>

#include "core/types.h"
#include "structure/layers.h"
#include "normalize/normalize.h"

namespace la::app {

struct PipelineResult {
    NormalizedCup ref, user;
    PatternMask refMask, userMask;
    QualityResult quality;
    ScoreResult score;
    StructureComparison structure; // authoritative shared multi-line result
};

struct PipelineParams {
    normalize::NormalizeParams normalize;
    structure::LayerParams layers;
};

// For --canon inputs: bypass cup detection, but keep all structure/size/quality checks.
PipelineResult scoreNormalized(const NormalizedCup &ref, const NormalizedCup &user,
                               const structure::LayerParams &params = structure::LayerParams(),
                               std::string *stage = nullptr);
PipelineResult runPipeline(const cv::Mat &refBgr, const cv::Mat &userBgr, const PatternSpec &spec,
                           const PipelineParams &params, std::string *stage = nullptr);

// `stage` (optional) is updated before each step so callers can report where
// an exception came from.
// PatternSpec is catalog metadata only; targetLayers does not affect scoring.
PipelineResult runPipeline(const cv::Mat &refBgr, const cv::Mat &userBgr, const PatternSpec &spec,
                           std::string *stage = nullptr);

} // namespace la::app
