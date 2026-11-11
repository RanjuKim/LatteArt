// Owner: Junkyu Kim
#include "basic/clahe.h"

#include "core/todo.h"

namespace la::basic {

cv::Mat clahe(const cv::Mat& src, int tilesX, int tilesY, float clipLimit) {
    (void)src; (void)tilesX; (void)tilesY; (void)clipLimit;
    LA_TODO("clahe: per-tile clipped histogram + bilinear blending between tiles");
}

}  // namespace la::basic
