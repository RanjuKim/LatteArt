// Owner: Jaeku Kang
#include "normalize/normalize.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "basic/color.h"
#include "basic/edge.h"
#include "basic/geometry.h"
#include "normalize/manual.h"

namespace la::normalize {
namespace {
constexpr double pi = 3.14159265358979323846;
bool solve5(double a[5][6], double x[5]) {
    for (int c = 0; c < 5; ++c) {
        int best = c;
        for (int r = c + 1; r < 5; ++r)
            if (std::abs(a[r][c]) > std::abs(a[best][c]))
                best = r;
        if (std::abs(a[best][c]) < 1e-10)
            return false;
        for (int j = c; j < 6; ++j)
            std::swap(a[c][j], a[best][j]);
        double d = a[c][c];
        for (int j = c; j < 6; ++j)
            a[c][j] /= d;
        for (int r = 0; r < 5; ++r)
            if (r != c) {
                double f = a[r][c];
                for (int j = c; j < 6; ++j)
                    a[r][j] -= f * a[c][j];
            }
    }
    for (int r = 0; r < 5; ++r)
        x[r] = a[r][5];
    return true;
}
float sample(const cv::Mat &g, float x, float y) {
    return g.ptr<float>(std::clamp(int(std::lround(y)), 0,
                                   g.rows - 1))[std::clamp(int(std::lround(x)), 0, g.cols - 1)];
}
cv::Point2f point(const Ellipse &e, double t, double s = 1) {
    double a = e.angleDeg * pi / 180, u = e.a * std::cos(t) * s, v = e.b * std::sin(t) * s;
    return {float(e.center.x + std::cos(a) * u - std::sin(a) * v),
            float(e.center.y + std::sin(a) * u + std::cos(a) * v)};
}
struct Texture {
    double mean = 0, contrast = 0, homogeneity = 0;
};
// Symmetric horizontal/vertical GLCM; identical fixed quantization in both patches.
Texture texture(const cv::Mat &g, cv::Point2f p, const NormalizeParams &cfg) {
    int n = cfg.glcmLevels, r = cfg.glcmPatchRadius, pixels = 0, pairs = 0;
    std::vector<double> hist(size_t(n) * n, 0);
    Texture t;
    auto level = [n](float v) { return std::clamp(int(v * n), 0, n - 1); };
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x) {
            float v = sample(g, p.x + x, p.y + y);
            t.mean += v;
            ++pixels;
            int a = level(v);
            for (int axis = 0; axis < 2; ++axis) {
                if ((axis == 0 && x == r) || (axis == 1 && y == r))
                    continue;
                int b = level(sample(g, p.x + x + (axis == 0), p.y + y + (axis == 1)));
                hist[a * n + b]++;
                hist[b * n + a]++;
                pairs += 2;
            }
        }
    t.mean /= pixels;
    for (int a = 0; a < n; ++a)
        for (int b = 0; b < n; ++b) {
            double prob = hist[a * n + b] / pairs, d = double(a - b) / (n - 1);
            t.contrast += prob * d * d;
            t.homogeneity += prob / (1.0 + (a - b) * (a - b));
        }
    return t;
}
struct Candidate {
    Ellipse e;
    std::vector<cv::Point2f> pts;
    std::vector<int> components;
    std::string method = "component";
    float coverage = 0, error = 0, score = 0, support = 1;
};
float residual(const Ellipse &e, cv::Point2f p) {
    const double a = e.angleDeg * pi / 180, cs = std::cos(a), sn = std::sin(a);
    const double dx = p.x - e.center.x, dy = p.y - e.center.y;
    return float(std::abs(std::hypot((cs*dx + sn*dy)/e.a, (-sn*dx + cs*dy)/e.b) - 1));
}
std::string geometryRejection(const Ellipse &e, int w, int h, const NormalizeParams &cfg) {
    if (!(e.a > 0) || !(e.b > 0) || !std::isfinite(e.center.x) || !std::isfinite(e.center.y))
        return "invalid geometry";
    if (e.b < cfg.minRadiusFraction * std::min(w,h)) return "too small";
    if (e.b/e.a < cfg.minAxisRatio) return "axis ratio";
    const double a=e.angleDeg*pi/180, cs=std::cos(a), sn=std::sin(a);
    const double rx=std::hypot(e.a*cs,e.b*sn), ry=std::hypot(e.a*sn,e.b*cs);
    if(e.center.x-rx<1 || e.center.y-ry<1 || e.center.x+rx>w-2 || e.center.y+ry>h-2)
        return "outside image";
    return {};
}
void measure(Candidate &c, const NormalizeParams &cfg, int imageShortSide) {
    std::array<bool,36> sectors{};
    double error=0;
    const double a=c.e.angleDeg*pi/180,cs=std::cos(a),sn=std::sin(a);
    for(auto p:c.pts) {
        const double dx=p.x-c.e.center.x,dy=p.y-c.e.center.y;
        const double u=(cs*dx+sn*dy)/c.e.a,v=(-sn*dx+cs*dy)/c.e.b;
        const double d=std::abs(std::hypot(u,v)-1);
        error+=d;
        if(d<cfg.maxRelativeFitError*2) {
            const int sector=int((std::atan2(v,u)+pi)*36/(2*pi));
            sectors[std::clamp(sector,0,35)]=true;
        }
    }
    c.error=float(error/c.pts.size());
    c.coverage=float(std::count(sectors.begin(),sectors.end(),true))/36;
    c.score=c.coverage-4*c.error+0.1f*c.e.b/imageShortSide-0.1f*(1-c.support);
}
std::string rejection(const Candidate &c, int w, int h, const NormalizeParams &cfg) {
    auto reason=geometryRejection(c.e,w,h,cfg);
    if(!reason.empty()) return reason;
    if(c.coverage<cfg.minAngularCoverage) return "angular coverage";
    if(c.error>cfg.maxRelativeFitError) return "fit error";
    if(c.support<cfg.minInlierFraction) return "insufficient inlier fraction";
    return {};
}
// Reproducible bounded RANSAC, local to one connected component. Refit consensus
// using the unchanged least-squares solver; require support in the original pool.
bool robustFit(const Candidate &source, int w, int h, const NormalizeParams &cfg, Candidate &best) {
    unsigned state=0x9e3779b9u+unsigned(source.pts.size());
    auto random=[&](){state^=state<<13;state^=state>>17;state^=state<<5;return state;};
    float bestRank=-1;
    for(int iteration=0;iteration<cfg.ransacIterations;++iteration) {
        std::vector<cv::Point2f> samplePoints;
        std::array<size_t,6> indices{};
        for(size_t k=0;k<indices.size();++k) {
            size_t index;
            bool duplicate;
            do {index=random()%source.pts.size(); duplicate=false;
                for(size_t j=0;j<k;++j) duplicate=duplicate || indices[j]==index;
            } while(duplicate);
            indices[k]=index; samplePoints.push_back(source.pts[index]);
        }
        Ellipse e;
        try {e=fitEllipse(samplePoints);} catch(const std::invalid_argument&){continue;}
        if(!geometryRejection(e,w,h,cfg).empty()) continue;
        Candidate c;
        c.components=source.components;
        c.method="RANSAC";
        for(int refit=0;refit<3;++refit) {
            c.pts.clear();
            for(auto p:source.pts) if(residual(e,p)<=cfg.inlierTolerance) c.pts.push_back(p);
            if(c.pts.size()<60 || c.pts.size()<cfg.minInlierFraction*source.pts.size()) break;
            try {e=fitEllipse(c.pts);} catch(const std::invalid_argument&){c.pts.clear();break;}
        }
        if(c.pts.size()<60) continue;
        c.e=e;
        c.pts.clear();
        for(auto p:source.pts) if(residual(e,p)<=cfg.inlierTolerance) c.pts.push_back(p);
        if(c.pts.size()<60) continue;
        c.support=float(c.pts.size())/source.pts.size();
        measure(c,cfg,std::min(w,h));
        if(!rejection(c,w,h,cfg).empty()) continue;
        const float rank=c.score+0.1f*c.support;
        if(rank>bestRank) {bestRank=rank;best=std::move(c);}
    }
    return bestRank>=0;
}
bool compatible(const Candidate &a,const Candidate &b,const NormalizeParams &cfg) {
    return std::hypot(a.e.center.x-b.e.center.x,a.e.center.y-b.e.center.y)
               <=cfg.arcCenterTolerance*std::max(a.e.a,b.e.a) &&
           std::abs(a.e.a-b.e.a)<=cfg.arcAxisTolerance*std::max(a.e.a,b.e.a) &&
           std::abs(a.e.b-b.e.b)<=cfg.arcAxisTolerance*std::max(a.e.b,b.e.b);
}
std::vector<Candidate> candidates(const cv::Mat &gray, const NormalizeParams &cfg,
                                  CupDiagnostics *diagnostics=nullptr) {
    auto edges=basic::canny(gray,cfg.cannyLow,cfg.cannyHigh);
    if(diagnostics) {diagnostics->gray=gray;diagnostics->edges=edges;}
    const int w=gray.cols,h=gray.rows;
    std::vector<unsigned char> seen(size_t(w)*h,0);
    std::vector<int> queue;
    std::vector<Candidate> out,parts,robustPools;
    auto record=[&](const Candidate &c,const std::string &method,const std::string &reason,
                    const std::vector<cv::Point2f> &pool,bool fitted=true) {
        if(!diagnostics) return;
        RimCandidateDiagnostic d;
        d.ellipse=c.e;d.points=pool;d.inliers=c.pts;d.components=c.components;
        d.method=method;d.rejection=reason;d.coverage=c.coverage;d.fitError=c.error;
        d.inlierFraction=c.support;d.score=c.score;d.accepted=reason.empty();d.fitted=fitted;
        diagnostics->candidates.push_back(std::move(d));
    };
    int component=0;
    for(int y=1;y<h-1;++y) for(int x=1;x<w-1;++x) {
        const int start=y*w+x;
        if(seen[start] || !edges.ptr<uchar>(y)[x]) continue;
        queue={start};seen[start]=1;
        for(size_t k=0;k<queue.size();++k) {
            const int px=queue[k]%w,py=queue[k]/w;
            for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                const int xx=px+dx,yy=py+dy;
                if(xx<0||yy<0||xx>=w||yy>=h) continue;
                const int q=yy*w+xx;
                if(!seen[q] && edges.ptr<uchar>(yy)[xx]) {seen[q]=1;queue.push_back(q);}
            }
        }
        if(queue.size()<60) continue;
        Candidate c;
        c.components={++component};
        const size_t stride=std::max(size_t(1),queue.size()/2500);
        for(size_t k=0;k<queue.size();k+=stride) c.pts.emplace_back(float(queue[k]%w),float(queue[k]/w));
        try {c.e=fitEllipse(c.pts);} catch(const std::invalid_argument&) {
            record(c,"component","fit failed",c.pts,false);continue;
        }
        measure(c,cfg,std::min(w,h));
        const auto reason=rejection(c,w,h,cfg);
        record(c,"component",reason,c.pts);
        if(reason.empty()) out.push_back(c);
        if(!geometryRejection(c.e,w,h,cfg).empty()) continue;
        if(c.coverage>=.15f && c.error<=cfg.maxRelativeFitError) parts.push_back(c);
        if(c.error>cfg.maxRelativeFitError && c.pts.size()>=120) robustPools.push_back(std::move(c));
    }
    // Recovery must not displace an already valid original candidate. A more
    // complete ceramic edge can outrank the liquid edge without being a better
    // crop. Preserve the original successful path; recover only candidate absence.
    if(!out.empty()) {
        std::sort(out.begin(),out.end(),[](const Candidate&a,const Candidate&b){return a.score>b.score;});
        if(out.size()>8) out.resize(8);
        return out;
    }
    auto bySize=[](const Candidate&a,const Candidate&b){return a.pts.size()>b.pts.size();};
    std::stable_sort(parts.begin(),parts.end(),bySize);
    if(parts.size()>size_t(cfg.maxArcCandidates)) parts.resize(cfg.maxArcCandidates);
    for(size_t i=0;i<parts.size();++i) {
        Candidate group=parts[i];
        for(size_t j=0;j<parts.size();++j) {
            if(i==j || !compatible(parts[i],parts[j],cfg)) continue;
            Candidate merged=group;
            merged.method="merged arcs";
            merged.pts.insert(merged.pts.end(),parts[j].pts.begin(),parts[j].pts.end());
            merged.components.push_back(parts[j].components.front());
            try {merged.e=fitEllipse(merged.pts);} catch(const std::invalid_argument&){continue;}
            measure(merged,cfg,std::min(w,h));
            if(!geometryRejection(merged.e,w,h,cfg).empty() || merged.error>cfg.maxRelativeFitError ||
               merged.coverage<group.coverage+1.f/36-1e-6f) continue;
            group=std::move(merged);
            const auto reason=rejection(group,w,h,cfg);
            record(group,"merged arcs",reason,group.pts);
            if(reason.empty()) out.push_back(group);
        }
    }
    std::stable_sort(robustPools.begin(),robustPools.end(),bySize);
    if(robustPools.size()>16) robustPools.resize(16);
    for(const auto &pool:robustPools) {
        Candidate robust;
        if(robustFit(pool,w,h,cfg,robust)) {
            record(robust,"RANSAC",{},pool.pts);out.push_back(std::move(robust));
        } else record(pool,"RANSAC","no supported ellipse",pool.pts);
    }
    std::stable_sort(out.begin(),out.end(),[](const Candidate&a,const Candidate&b){return a.score>b.score;});
    std::vector<Candidate> unique;
    for(auto &c:out) {
        bool duplicate=false;
        for(const auto &kept:unique) {
            if(std::hypot(c.e.center.x-kept.e.center.x,c.e.center.y-kept.e.center.y)<.03f*kept.e.b &&
               std::abs(c.e.a-kept.e.a)<.03f*kept.e.a && std::abs(c.e.b-kept.e.b)<.03f*kept.e.b)
                duplicate=true;
        }
        if(!duplicate) unique.push_back(std::move(c));
        if(unique.size()==8) break;
    }
    return unique;
}

std::pair<Ellipse, float> liquidBoundary(const cv::Mat &gray, const Ellipse &rim,
                                         const NormalizeParams &cfg) {
    float best = -std::numeric_limits<float>::infinity(), bestScale = 1;
    std::vector<std::pair<float, float>> scales;
    double margin = (cfg.glcmPatchRadius + 2.0) / rim.b;
    for (double s = cfg.liquidSearchMin; s <= cfg.liquidSearchMax + 1e-6; s += 0.01) {
        std::vector<float> evidence;
        for (int k = 0; k < 48; ++k) {
            double t = 2 * pi * k / 48;
            auto in = texture(gray, point(rim, t, s - margin), cfg),
                 out = texture(gray, point(rim, t, s + margin), cfg);
            double intensity =
                std::abs(out.mean - in.mean) + 0.25 * std::max(0.0, out.mean - in.mean);
            double change = std::abs(in.contrast - out.contrast) +
                            0.25 * std::abs(in.homogeneity - out.homogeneity);
            evidence.push_back(float(intensity + cfg.textureWeight * change));
        }
        std::sort(evidence.begin(), evidence.end());
        float score = 0;
        for (int k = 8; k < 40; ++k)
            score += evidence[k] / 32;
        scales.push_back({float(s), score});
        if (score > best) {
            best = score;
            bestScale = float(s);
        }
    }
    // Ceramic/background can have a stronger edge than coffee/ceramic. Select the
    // innermost *local peak* with comparable circumferential evidence, not the
    // innermost edge pixel or the strongest edge alone.
    for (size_t i = 1; i + 1 < scales.size(); ++i) {
        if (scales[i].second >= best * cfg.innerBoundarySupport &&
            scales[i].second >= scales[i - 1].second && scales[i].second >= scales[i + 1].second) {
            bestScale = scales[i].first;
            best = scales[i].second;
            break;
        }
    }
    Ellipse e = rim;
    e.a *= bestScale;
    e.b *= bestScale;
    return {e, best};
}
void validate(const NormalizeParams &p) {
    if (p.detectionSize < 64 || p.glcmLevels < 2 || p.glcmLevels > 64 || p.glcmPatchRadius < 1 ||
        p.glcmPatchRadius > 16 || !(p.minAxisRatio > 0 && p.minAxisRatio <= 1) ||
        !(p.minRadiusFraction > 0 && p.minRadiusFraction < 0.5f) ||
        !(p.minAngularCoverage > 0 && p.minAngularCoverage <= 1) || !(p.maxRelativeFitError > 0) ||
        !(p.liquidSearchMin > 0 && p.liquidSearchMax >= p.liquidSearchMin &&
          p.liquidSearchMax <= 1.2f) ||
        !(p.innerBoundarySupport > 0 && p.innerBoundarySupport <= 1) ||
        !(p.arcCenterTolerance > 0 && p.arcCenterTolerance <= .5f) ||
        !(p.arcAxisTolerance > 0 && p.arcAxisTolerance <= .5f) ||
        p.maxArcCandidates < 2 || p.maxArcCandidates > 100 ||
        p.ransacIterations < 0 || p.ransacIterations > 2000 ||
        !(p.inlierTolerance > 0 && p.inlierTolerance <= p.maxRelativeFitError) ||
        !(p.minInlierFraction >= .5f && p.minInlierFraction <= 1) ||
        !std::isfinite(p.textureWeight) || p.textureWeight < 0)
        throw std::invalid_argument("invalid NormalizeParams");
}
} // namespace
Ellipse fitEllipse(const std::vector<cv::Point2f> &pts) {
    if (pts.size() < 6)
        throw std::invalid_argument("fitEllipse: insufficient points");
    double mx = 0, my = 0, scale = 0;
    for (auto p : pts) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
            throw std::invalid_argument("fitEllipse: nonfinite point");
        mx += p.x;
        my += p.y;
    }
    mx /= pts.size();
    my /= pts.size();
    for (auto p : pts)
        scale += (p.x - mx) * (p.x - mx) + (p.y - my) * (p.y - my);
    scale = std::sqrt(scale / pts.size());
    if (scale < 1e-6)
        throw std::invalid_argument("fitEllipse: degenerate points");
    double normal[5][6]{};
    for (auto p : pts) {
        double x = (p.x - mx) / scale, y = (p.y - my) / scale, v[5] = {x * x, x * y, y * y, x, y};
        for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 5; ++j)
                normal[i][j] += v[i] * v[j];
            normal[i][5] += v[i];
        }
    }
    double v[5];
    if (!solve5(normal, v))
        throw std::invalid_argument("fitEllipse: singular fit");
    double A = v[0], B = v[1], C = v[2], D = v[3], E = v[4], det = 4 * A * C - B * B;
    if (det <= 1e-10)
        throw std::invalid_argument("fitEllipse: not an ellipse");
    double cx = (B * E - 2 * C * D) / det, cy = (B * D - 2 * A * E) / det;
    double k = 1 - (A * cx * cx + B * cx * cy + C * cy * cy + D * cx + E * cy);
    double disc = std::hypot(A - C, B), l1 = (A + C - disc) / 2, l2 = (A + C + disc) / 2;
    if (l1 <= 0 || l2 <= 0 || k <= 0)
        throw std::invalid_argument("fitEllipse: invalid axes");
    Ellipse e;
    e.center = {float(mx + cx * scale), float(my + cy * scale)};
    e.a = float(scale * std::sqrt(k / l1));
    e.b = float(scale * std::sqrt(k / l2));
    e.angleDeg = float(0.5 * std::atan2(B, A - C) * 180 / pi + 90);
    if (!std::isfinite(e.a) || !std::isfinite(e.b))
        throw std::invalid_argument("fitEllipse: invalid result");
    return e;
}
std::vector<cv::Point2f> findRimPoints(const cv::Mat &gray) {
    auto c = candidates(gray, NormalizeParams());
    if (c.empty())
        throw std::runtime_error("컵 후보를 찾지 못했습니다.");
    return c.front().pts;
}
CupDetection detectCup(const cv::Mat &bgr, const NormalizeParams &cfg, CupDiagnostics *diagnostics) {
    if (diagnostics) *diagnostics = CupDiagnostics();
    validate(cfg);
    if (bgr.empty() || bgr.type() != CV_8UC3)
        throw std::invalid_argument("detectCup: expected non-empty BGR image");
    double s = std::min(1.0, double(cfg.detectionSize) / std::max(bgr.cols, bgr.rows));
    auto gray = basic::bgrToGray(bgr);
    if (s < 1)
        gray = basic::warpPerspective(
            gray, {1 / s, 0, 0, 0, 1 / s, 0, 0, 0, 1},
            {std::max(2, int(bgr.cols * s)), std::max(2, int(bgr.rows * s))});
    if (diagnostics) diagnostics->detectionScale = float(s);
    auto list = candidates(gray, cfg, diagnostics);
    if (list.empty())
        throw std::runtime_error("컵 후보를 찾지 못했습니다.");
    CupDetection result;
    float best = -1e9f;
    for (const auto &c : list) {
        auto liquid = liquidBoundary(gray, c.e, cfg);
        float score = c.score + liquid.second;
        if (score > best) {
            best = score;
            result.rim = c.e;
            result.liquid = liquid.first;
            result.coverage = c.coverage;
            result.fitError = c.error;
            result.rimMethod = c.method;
            result.rimComponents = c.components;
        }
    }
    for (Ellipse *e : {&result.rim, &result.liquid}) {
        e->center.x /= float(s);
        e->center.y /= float(s);
        e->a /= float(s);
        e->b /= float(s);
    }
    return result;
}
float patternAxisAngleDeg(const cv::Mat &mask) {
    if (mask.empty() || mask.type() != CV_8UC1)
        throw std::invalid_argument("patternAxisAngleDeg: expected mask");
    double n = 0, mx = 0, my = 0;
    for (int y = 0; y < mask.rows; ++y)
        for (int x = 0; x < mask.cols; ++x)
            if (mask.ptr<uchar>(y)[x]) {
                ++n;
                mx += x;
                my += y;
            }
    if (n < 2)
        return 0;
    mx /= n;
    my /= n;
    double xx = 0, xy = 0, yy = 0;
    for (int y = 0; y < mask.rows; ++y)
        for (int x = 0; x < mask.cols; ++x)
            if (mask.ptr<uchar>(y)[x]) {
                xx += (x - mx) * (x - mx);
                xy += (x - mx) * (y - my);
                yy += (y - my) * (y - my);
            }
    return float(-90 - 0.5 * std::atan2(2 * xy, xx - yy) * 180 / pi);
}
NormalizedCup normalizeCup(const cv::Mat &bgr) { return normalizeCup(bgr, NormalizeParams()); }
NormalizedCup normalizeCup(const cv::Mat &bgr, const NormalizeParams &cfg) {
    const auto e = detectCup(bgr, cfg).liquid;
    // Symmetric affine correction keeps in-plane camera orientation until reference comparison.
    double a = e.angleDeg * pi / 180, cs = std::cos(a), sn = std::sin(a),
           sx = e.a / kCanonicalRadius, sy = e.b / kCanonicalRadius;
    double m00 = sx * cs * cs + sy * sn * sn, m01 = (sx - sy) * cs * sn,
           m11 = sx * sn * sn + sy * cs * cs, c = kCanonicalSize / 2.0;
    basic::Mat3 map{
        m00, m01, e.center.x - c * (m00 + m01), m01, m11, e.center.y - c * (m01 + m11), 0, 0, 1};
    auto cup =
        fromCanonicalImage(basic::warpPerspective(bgr, map, {kCanonicalSize, kCanonicalSize}));
    cup.tiltRatio = e.b / e.a;
    return cup;
}
NormalizedCup rotateCup(const NormalizedCup &cup, float angleDeg) {
    if (!std::isfinite(angleDeg))
        throw std::invalid_argument("rotateCup: invalid angle");
    auto out = fromCanonicalImage(basic::warpPerspective(
        cup.bgr, basic::rotationAbout(cup.center, -angleDeg), {kCanonicalSize, kCanonicalSize}));
    out.tiltRatio = cup.tiltRatio;
    out.axisAngleDeg = cup.axisAngleDeg + angleDeg;
    return out;
}
} // namespace la::normalize
