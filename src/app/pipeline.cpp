// Owner: Jaeku Kang
#include "app/pipeline.h"

#include "normalize/normalize.h"
#include "quality/quality.h"
#include "scoring/scoring.h"
#include "structure/layers.h"
#include "structure/segment.h"

namespace la::app {

PipelineResult runPipeline(const cv::Mat& refBgr, const cv::Mat& userBgr, const PatternSpec& spec,
                           std::string* stage) {
    auto at = [stage](const char* s) {
        if (stage) *stage = s;
    };
    PipelineResult r;

    at("normalize");
    r.ref = normalize::normalizeCup(refBgr);
    r.user = normalize::normalizeCup(userBgr);

    at("segment pattern");
    r.refMask = structure::segmentPattern(r.ref);
    r.userMask = structure::segmentPattern(r.user);

    at("count layers");
    r.refLayers = structure::analyzeLayers(r.ref, r.refMask);
    r.userLayers = structure::analyzeLayers(r.user, r.userMask);

    at("quality metrics");
    r.quality = quality::evaluateQuality(r.user, r.userMask, r.ref, r.refMask);

    at("scoring");
    r.score = scoring::computeScore(r.userLayers, spec.targetLayers, r.quality.Q);
    return r;
}

}  // namespace la::app
