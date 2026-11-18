// Owner: Jaeku Kang
#include "app/pipeline.h"
#include <stdexcept>
#include "quality/quality.h"
#include "scoring/scoring.h"
#include "structure/segment.h"

namespace la::app {
PipelineResult scoreNormalized(const NormalizedCup &ref, const NormalizedCup &user,
                               const structure::LayerParams &params, std::string *stage) {
    auto at = [stage](const char *s) {
        if (stage)
            *stage = s;
    };
    PipelineResult r;
    r.ref = ref;
    r.user = user;
    if (!ref.ok || !user.ok)
        throw std::runtime_error("컵 후보를 찾지 못했습니다.");
    at("segment pattern");
    r.refMask = structure::segmentPattern(r.ref);
    r.userMask = structure::segmentPattern(r.user);
    at("multi-line structure and rotation");
    r.structure = structure::compareStructure(r.ref, r.refMask, r.user, r.userMask, params);
    at("pattern size validation");
    structure::requireMatchingSize(r.structure);
    // Quality gets a rotation only: original coverage and cup-relative offset are preserved.
    // Structural anisotropic scale/translation never leaks into the quality inputs.
    at("align quality frame");
    r.user = normalize::rotateCup(user, r.structure.angleDeg);
    r.userMask = structure::segmentPattern(r.user);
    at("quality metrics");
    r.quality = quality::evaluateQuality(r.user, r.userMask, r.ref, r.refMask);
    at("multi-line scoring");
    r.score = scoring::computeScore(r.structure, r.quality.Q);
    // SHARED/report/batch still expect one count. Do not invent one from scanlines.
    // Consumers must migrate to PipelineResult.structure; debug layers already uses it.
    r.refLayers.count = r.userLayers.count = -1;
    r.refLayers.fftCount = r.userLayers.fftCount = -1;
    r.refLayers.U = r.userLayers.U = r.score.U;
    return r;
}
PipelineResult runPipeline(const cv::Mat &refBgr, const cv::Mat &userBgr, const PatternSpec &spec,
                           const PipelineParams &params, std::string *stage) {
    if (spec.targetLayers <= 0)
        throw std::invalid_argument("positive targetLayers required");
    if (stage)
        *stage = "normalize reference";
    auto ref = normalize::normalizeCup(refBgr, params.normalize);
    if (stage)
        *stage = "normalize user";
    auto user = normalize::normalizeCup(userBgr, params.normalize);
    return scoreNormalized(ref, user, params.layers, stage);
}
PipelineResult runPipeline(const cv::Mat &ref, const cv::Mat &user, const PatternSpec &spec,
                           std::string *stage) {
    return runPipeline(ref, user, spec, PipelineParams(), stage);
}
} // namespace la::app
