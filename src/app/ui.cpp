// Owner: Ranju Kim
// Interactive viewer (see ui.h). Screens are plain images built with our own
// drawing code and shown with io::show; keys come from io::waitKey.
#include "app/ui.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/pipeline.h"
#include "io/io.h"
#include "viz/draw.h"
#include "viz/report.h"

// Windows standard "Open" dialog for choosing your own photos (GUI only,
// not part of the image processing).
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

namespace fs = std::filesystem;

namespace la::app {

namespace {

// ---------------------------------------------------------------------------
// Look and layout. 1180 x 690 fits a 1366 x 768 laptop screen with the
// window title bar and the taskbar.
constexpr int W = 1180, H = 690;
constexpr int kFooterY = H - 34;
const char* kWindow = "LatteArt";

const viz::Color kBg{28, 26, 24}, kPanel{44, 40, 37}, kSelect{60, 105, 150}, kLine{80, 74, 68};
const viz::Color kText{235, 235, 235}, kDim{150, 150, 150}, kAccent{90, 170, 235};  // latte orange
const viz::Color kGood{80, 200, 90}, kWarn{40, 200, 235}, kBad{70, 70, 230};

// ---------------------------------------------------------------------------
// Demo cases: the same list as demo_samples/run_demo.ps1.
struct Case {
    std::string group;    // folder name, e.g. 1_high_score ("own" for chosen photos)
    fs::path ref, user;   // fs::path so folders with Korean letters work
    std::string pattern;
};

std::vector<Case> demoCases(const fs::path& demo) {
    const struct { const char *group, *ref, *user, *pattern; } list[] = {
        {"1_high_score", "Pro_Tulip_3.png", "Pro_Tulip_3__scale90.png", "tulip"},
        {"1_high_score", "Pro_Heart_3.png", "Pro_Heart_3__rot10.png", "heart"},
        {"1_high_score", "Pro_Tulip_4.png", "Pro_Tulip_4__scale90.png", "tulip"},
        {"1_high_score", "Pro_Heart_3.png", "Pro_Heart_3__dark.png", "heart"},
        {"1_high_score", "Pro_Tulip_3.png", "Pro_Tulip_3__rot10.png", "tulip"},
        {"2_layers_merged", "Pro_Tulip_4.png", "Pro_Tulip_4__thick.png", "tulip"},
        {"3_practice_good", "Pro_Tulip_3.png", "Tulip_Practice_11.png", "tulip"},
        {"3_practice_good", "Pro_Tulip_3.png", "Tulip_Practice_18.png", "tulip"},
        {"3_practice_good", "Pro_Heart_3.png", "Heart_Practice_18.png", "heart"},
        {"3_practice_good", "Pro_Tulip_3.png", "Tulip_Practice_9.png", "tulip"},
        {"3_practice_good", "Pro_Heart_3.png", "Heart_Practice_14.png", "heart"},
        {"4_practice_bad", "Pro_Tulip_1.png", "Tulip_Practice_6.png", "tulip"},
        {"4_practice_bad", "Pro_Tulip_1.png", "Tulip_Practice_8.png", "tulip"},
        {"4_practice_bad", "Pro_Heart_3.png", "Heart_Practice_6.png", "heart"},
        {"5_should_fail", "Pro_Heart_3.png", "no_cup_gray.png", "heart"},
        {"5_should_fail", "Pro_Tulip_3.png", "Tulip_Practice_1.png", "tulip"},
        {"5_should_fail", "Pro_Heart_3.png", "Heart_Practice_21.png", "heart"},
        {"5_should_fail", "Pro_Heart_3.png", "Heart_Practice_5.png", "heart"},
    };
    std::vector<Case> cases;
    for (const auto& c : list)
        cases.push_back({c.group, demo / "reference" / c.ref, demo / c.group / c.user, c.pattern});
    return cases;
}

// demo_samples next to the working directory, the executable, or one of
// their parents (bin\Release is two levels below the repository).
fs::path findDemo(const char* exePath) {
    std::vector<fs::path> starts;
    std::error_code ec;
    starts.push_back(fs::current_path(ec));
    if (exePath && *exePath) starts.push_back(fs::absolute(fs::path(exePath), ec).parent_path());
    for (fs::path p : starts) {
        for (int up = 0; up < 5 && !p.empty(); ++up, p = p.parent_path()) {
            if (fs::exists(p / "demo_samples" / "reference", ec)) return p / "demo_samples";
            if (p == p.parent_path()) break;
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// Pixel helpers (our own: no OpenCV drawing or resizing).
cv::Mat blank(viz::Color c = kBg) {
    cv::Mat img(H, W, CV_8UC3);
    for (int y = 0; y < H; ++y) {
        uchar* p = img.ptr<uchar>(y);
        for (int x = 0; x < W; ++x) { p[3 * x] = c[0]; p[3 * x + 1] = c[1]; p[3 * x + 2] = c[2]; }
    }
    return img;
}

void fillRect(cv::Mat& img, int x0, int y0, int x1, int y1, viz::Color c) {
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, img.cols); y1 = std::min(y1, img.rows);
    for (int y = y0; y < y1; ++y) {
        uchar* p = img.ptr<uchar>(y);
        for (int x = x0; x < x1; ++x) { p[3 * x] = c[0]; p[3 * x + 1] = c[1]; p[3 * x + 2] = c[2]; }
    }
}

// Area-average downscale so the image fits in maxW x maxH (never enlarges).
cv::Mat fitInto(const cv::Mat& src, int maxW, int maxH) {
    if (src.empty() || src.type() != CV_8UC3) return cv::Mat();
    const double s = std::min({1.0, double(maxW) / src.cols, double(maxH) / src.rows});
    const int w = std::max(1, int(src.cols * s)), h = std::max(1, int(src.rows * s));
    cv::Mat out(h, w, CV_8UC3);
    for (int y = 0; y < h; ++y) {
        const int sy0 = std::min(int(y / s), src.rows - 1);
        const int sy1 = std::clamp(int((y + 1) / s), sy0 + 1, src.rows);
        uchar* d = out.ptr<uchar>(y);
        for (int x = 0; x < w; ++x) {
            const int sx0 = std::min(int(x / s), src.cols - 1);
            const int sx1 = std::clamp(int((x + 1) / s), sx0 + 1, src.cols);
            unsigned sum[3] = {0, 0, 0}, n = 0;
            for (int yy = sy0; yy < sy1; ++yy) {
                const uchar* p = src.ptr<uchar>(yy);
                for (int xx = sx0; xx < sx1; ++xx, ++n)
                    for (int ch = 0; ch < 3; ++ch) sum[ch] += p[3 * xx + ch];
            }
            for (int ch = 0; ch < 3; ++ch) d[3 * x + ch] = static_cast<uchar>(sum[ch] / n);
        }
    }
    return out;
}

void blit(cv::Mat& dst, const cv::Mat& src, int x0, int y0) {
    if (src.empty()) return;
    for (int y = 0; y < src.rows; ++y) {
        const int dy = y0 + y;
        if (dy < 0 || dy >= dst.rows) continue;
        const uchar* s = src.ptr<uchar>(y);
        uchar* d = dst.ptr<uchar>(dy);
        for (int x = 0; x < src.cols; ++x) {
            const int dx = x0 + x;
            if (dx < 0 || dx >= dst.cols) continue;
            d[3 * dx] = s[3 * x]; d[3 * dx + 1] = s[3 * x + 1]; d[3 * dx + 2] = s[3 * x + 2];
        }
    }
}

cv::Mat loadOrEmpty(const fs::path& path) {
    try {
        return io::loadColorFile(path);
    } catch (const std::exception&) {
        return cv::Mat();
    }
}

// Image centered in a box, with a thin frame around the box.
void picture(cv::Mat& img, const cv::Mat& src, int x, int y, int w, int h) {
    fillRect(img, x, y, x + w, y + h, kPanel);
    const cv::Mat fit = fitInto(src, w - 8, h - 8);
    if (!fit.empty()) blit(img, fit, x + (w - fit.cols) / 2, y + (h - fit.rows) / 2);
    viz::drawRect(img, {x, y}, {x + w - 1, y + h - 1}, kLine, 1);
}

// Our font has digits, A-Z (lower case drawn as upper case), space and . : / % -
// Anything else (commas, brackets, quotes, Korean...) becomes a space.
std::string clean(const std::string& s) {
    std::string out;
    for (unsigned char ch : s) {
        if (std::isalnum(ch) && ch < 128) out += char(std::toupper(ch));
        else if (ch == '.' || ch == ':' || ch == '/' || ch == '%' || ch == '-' || ch == ' ') out += char(ch);
        else if (ch == ')') continue;  // "(0.011)." -> "0.011." rather than "0.011 ."
        else if (ch < 128 || (ch & 0xC0) == 0xC0) out += ' ';  // one space per (multi-byte) character
    }
    return out;
}

int textWidth(const std::string& s, int scale) { return int(clean(s).size()) * 6 * scale; }

void text(cv::Mat& img, int x, int y, const std::string& s, viz::Color c, int scale = 2) {
    viz::drawText(img, {x, y}, clean(s), c, scale);
}

void textCentered(cv::Mat& img, int cx, int y, const std::string& s, viz::Color c, int scale = 2) {
    text(img, cx - textWidth(s, scale) / 2, y, s, c, scale);
}

std::vector<std::string> wrap(const std::string& s, size_t maxChars) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flush = [&] {
        if (word.empty()) return;
        if (!line.empty() && line.size() + 1 + word.size() > maxChars) { lines.push_back(line); line.clear(); }
        line += (line.empty() ? "" : " ") + word;
        word.clear();
    };
    for (char ch : clean(s)) {
        if (ch == ' ') flush();
        else word += ch;
    }
    flush();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

std::string num(double v, int decimals) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

viz::Color scoreColor(double frac) { return frac >= 0.8 ? kGood : frac >= 0.5 ? kWarn : kBad; }

void bar(cv::Mat& img, int x, int y, int w, int h, double frac, viz::Color c) {
    fillRect(img, x, y, x + w, y + h, kLine);
    fillRect(img, x, y, x + int(w * std::clamp(frac, 0.0, 1.0)), y + h, c);
}

void header(cv::Mat& img, const std::string& title, const std::string& subtitle) {
    fillRect(img, 0, 0, W, 78, kPanel);
    text(img, 24, 16, title, kAccent, 3);
    text(img, 24, 50, subtitle, kDim, 2);
    textCentered(img, W - 150, 26, "TEAM 22", kDim, 2);
}

void footer(cv::Mat& img, const std::string& keys, const std::string& status) {
    fillRect(img, 0, kFooterY, W, H, kPanel);
    text(img, 24, kFooterY + 10, keys, kText, 2);
    if (!status.empty()) text(img, W - 24 - textWidth(status, 2), kFooterY + 10, status, kAccent, 2);
}

// ---------------------------------------------------------------------------
// Screens.
struct Outcome {
    bool ok = false;
    PipelineResult r;
    cv::Mat render;  // Junkyu's renderResult image
    std::string stage, error;
    double seconds = 0;
};

std::string fileName(const fs::path& p) { return p.filename().u8string(); }

cv::Mat menuScreen(const std::vector<Case>& cases, int sel, const cv::Mat& refThumb,
                   const cv::Mat& userThumb, const std::string& status) {
    cv::Mat img = blank();
    header(img, "LATTE ART SCORING", "PICK A CASE AND PRESS ENTER TO SCORE IT");

    constexpr int listX = 24, listW = 600, rowH = 30, top = 92;
    fillRect(img, listX, top - 6, listX + listW, top + rowH * 18 + 6, kPanel);
    for (int i = 0; i < int(cases.size()) && i < 18; ++i) {
        const int y = top + i * rowH;
        if (i == sel) fillRect(img, listX, y, listX + listW, y + rowH, kSelect);
        const viz::Color c = i == sel ? kText : kDim;
        text(img, listX + 12, y + 8, (i < 9 ? " " : "") + std::to_string(i + 1), c);
        text(img, listX + 70, y + 8, fileName(cases[i].user), c);
    }
    if (cases.empty())
        text(img, listX + 12, top + 8, "DEMO SAMPLES NOT FOUND - PRESS O FOR YOUR OWN PHOTOS", kWarn);

    constexpr int px = 650, pw = 250, ph = 250;
    if (sel >= 0 && sel < int(cases.size())) {
        const Case& c = cases[sel];
        text(img, px, top, "REFERENCE", kDim);
        text(img, px + pw + 26, top, "PHOTO TO SCORE", kDim);
        picture(img, refThumb, px, top + 24, pw, ph);
        picture(img, userThumb, px + pw + 26, top + 24, pw, ph);
        int y = top + 24 + ph + 24;
        text(img, px, y, "PATTERN   " + c.pattern, kText); y += 36;
        text(img, px, y, "REF   " + fileName(c.ref), kDim); y += 26;
        text(img, px, y, "USER  " + fileName(c.user), kDim);
    }
    footer(img, "W/S MOVE   ENTER SCORE   O OWN PHOTOS   Q QUIT", status);
    return img;
}

void tabs(cv::Mat& img, int tab) {
    const char* names[] = {"1 OVERVIEW", "2 STRUCTURE", "3 QUALITY"};
    int x = 24;
    for (int i = 0; i < 3; ++i) {
        const int w = textWidth(names[i], 2) + 32;
        fillRect(img, x, 88, x + w, 120, i == tab ? kSelect : kPanel);
        text(img, x + 16, 97, names[i], i == tab ? kText : kDim);
        x += w + 8;
    }
}

std::string verdict(double score) {
    if (score >= 90) return "EXCELLENT - SAME STRUCTURE AS THE REFERENCE";
    if (score >= 60) return "GOOD - THE LAYERS MATCH THE REFERENCE";
    if (score >= 30) return "SOME LAYERS DO NOT MATCH THE REFERENCE";
    return "THE STRUCTURE DIFFERS FROM THE REFERENCE";
}

struct ItemRow {
    const char* name;
    int weight;
    const ItemScore* item;
};

std::vector<ItemRow> items(const QualityResult& q) {
    return {{"CONTRAST", 30, &q.contrast}, {"SHAPE", 25, &q.shape}, {"TEXTURE", 20, &q.texture},
            {"POSITION", 15, &q.position}, {"SIZE", 10, &q.size}};
}

void overview(cv::Mat& img, const Outcome& o) {
    const ScoreResult& s = o.r.score;
    constexpr int x0 = 24, top = 136;

    // Big score.
    text(img, x0, top, "FINAL SCORE", kDim);
    const std::string big = num(s.finalScore, 1);
    text(img, x0, top + 28, big, scoreColor(s.finalScore / 100.0), 9);
    text(img, x0 + textWidth(big, 9) + 12, top + 72, "/ 100", kDim, 3);
    int y = top + 108;
    for (const std::string& line : wrap(verdict(s.finalScore), 46)) { text(img, x0, y, line, kText); y += 24; }

    // Structure numbers.
    int lines = 0, full = 0, missing = 0, extra = 0;
    for (const LineComparison& l : o.r.structure.lines) {
        ++lines; missing += l.missing; extra += l.extra;
        full += (l.missing == 0 && l.extra == 0);
    }
    y += 14;
    const std::pair<std::string, std::string> rows[] = {
        {"STRUCTURE U", num(s.U, 2)},
        {"QUALITY Q", num(s.Q, 2)},
        {"VERTICAL", num(s.vertical.finalScore, 1)},
        {"HORIZONTAL", num(s.horizontal.finalScore, 1)},
        {"ROTATION", num(o.r.structure.angleDeg, 1) + " DEG"},
        {"SCAN LINES", std::to_string(full) + " / " + std::to_string(lines) + " FULLY MATCHED"},
        {"LAYERS", std::to_string(missing) + " MISSING   " + std::to_string(extra) + " EXTRA"},
    };
    for (const auto& [k, v] : rows) {
        text(img, x0, y, k, kDim);
        text(img, x0 + 170, y, v, kText);
        y += 26;
    }

    // Quality bars.
    const std::vector<ItemRow> quality = items(o.r.quality);
    y += 10;
    for (const ItemRow& it : quality) {
        text(img, x0, y, it.name, kDim);
        bar(img, x0 + 130, y, 250, 14, it.item->score, scoreColor(it.item->score));
        text(img, x0 + 392, y, num(100.0 * it.item->score, 0), kText);
        y += 22;
    }

    // Aligned cups.
    constexpr int cx = 590, cw = 280, ch = 280;
    text(img, cx, top, "REFERENCE", kDim);
    text(img, cx + cw + 16, top, "YOURS - ALIGNED", kDim);
    picture(img, o.r.ref.bgr, cx, top + 24, cw, ch);
    picture(img, o.r.user.bgr, cx + cw + 16, top + 24, cw, ch);
    int fy = top + 24 + ch + 20;
    text(img, cx, fy, "TOP ADVICE", kAccent);
    fy += 26;
    // Feedback of the weakest quality item (`quality` outlives this pointer).
    const ItemRow* worst = nullptr;
    for (const ItemRow& it : quality)
        if (!worst || it.item->score < worst->item->score) worst = &it;
    if (worst && worst->item->score >= 0.9f) {
        text(img, cx, fy, "ALL QUALITY ITEMS ARE CLOSE TO", kText); fy += 24;
        text(img, cx, fy, "THE REFERENCE.", kText);
    } else if (worst) {
        text(img, cx + 150, fy - 26, worst->name, kDim);
        for (const std::string& line : wrap(worst->item->feedback, 46)) { text(img, cx, fy, line, kText); fy += 24; }
    }
    text(img, cx, H - 70, "SCORED IN " + num(o.seconds, 1) + " S   F: FULL-SIZE RESULT IMAGE", kDim);
}

void structureTab(cv::Mat& img, const Outcome& o) {
    constexpr int top = 132;
    const int boxH = kFooterY - top - 10;
    const cv::Mat fit = fitInto(o.render, 640, boxH);
    if (!fit.empty()) blit(img, fit, 24, top);
    const int x = 24 + std::max(fit.cols, 1) + 30;
    int y = top + 4;
    text(img, x, y, "HOW TO READ", kAccent); y += 30;
    const std::pair<viz::Color, std::string> legend[] = {
        {kGood, "GREEN   LAYER MATCHED"},
        {viz::kRed, "RED     LAYER MISSING IN YOUR CUP"},
        {viz::kYellow, "YELLOW  EXTRA LAYER IN YOUR CUP"},
    };
    for (const auto& [c, s] : legend) {
        fillRect(img, x, y, x + 16, y + 14, c);
        text(img, x + 26, y, s, kText);
        y += 26;
    }
    y += 12;
    for (const std::string& line : wrap("EACH BAR IS ONE SCAN LINE ACROSS THE PATTERN. "
                                        "LAYERS ARE COMPARED WITH THE REFERENCE LINE BY LINE, "
                                        "VERTICALLY AND HORIZONTALLY.", 36)) {
        text(img, x, y, line, kDim); y += 24;
    }
    y += 16;
    text(img, x, y, "VERTICAL   " + num(o.r.score.vertical.finalScore, 1), kText); y += 26;
    text(img, x, y, "HORIZONTAL " + num(o.r.score.horizontal.finalScore, 1), kText); y += 26;
    text(img, x, y, "SIZE CHECK " + std::string(o.r.structure.sizeOk ? "OK" : "FAILED"), kText); y += 40;
    text(img, x, y, "PRESS F FOR THE FULL-SIZE IMAGE", kAccent);
}

void qualityTab(cv::Mat& img, const Outcome& o) {
    int y = 136;
    for (const ItemRow& it : items(o.r.quality)) {
        text(img, 24, y, it.name, kText, 2);
        text(img, 160, y, std::to_string(it.weight) + "%", kDim);
        bar(img, 230, y, 360, 16, it.item->score, scoreColor(it.item->score));
        text(img, 604, y, num(100.0 * it.item->score, 0) + " / 100", kText);
        text(img, 760, y, "YOU " + num(it.item->raw, 3) + "   REF " + num(it.item->ref, 3), kDim);
        y += 28;
        for (const std::string& line : wrap(it.item->feedback, 90)) { text(img, 40, y, line, kAccent); y += 22; }
        y += 18;
    }
    text(img, 24, y + 4, "QUALITY Q = WEIGHTED SUM = " + num(o.r.score.Q, 2), kText);
}

std::string friendlyError(const std::string& stage) {
    if (stage == "load images") return "THE PHOTO FILE COULD NOT BE READ.";
    if (stage == "normalize reference") return "NO CUP WAS FOUND IN THE REFERENCE PHOTO.";
    if (stage == "normalize user") return "NO CUP WAS FOUND IN YOUR PHOTO.";
    if (stage == "pattern size validation") return "THE PATTERN SIZE DOES NOT MATCH THE REFERENCE.";
    if (stage.find("structure") != std::string::npos || stage.find("segment") != std::string::npos)
        return "NO LATTE ART PATTERN WAS FOUND.";
    return "SCORING STOPPED AT STEP: " + stage;
}

std::string errorHint(const std::string& stage) {
    if (stage.rfind("normalize", 0) == 0)
        return "SHOOT STRAIGHT FROM ABOVE WITH THE WHOLE CUP VISIBLE AND A PLAIN BACKGROUND.";
    if (stage == "pattern size validation")
        return "THE FOAM COVERS TOO MUCH OR TOO LITTLE OF THE CUP COMPARED WITH THE REFERENCE.";
    if (stage == "load images") return "CHECK THAT THE FILE IS A PNG / JPG / BMP PHOTO AND STILL EXISTS.";
    return "SEE THE CONSOLE WINDOW FOR DETAILS.";
}

cv::Mat resultScreen(const Case& c, int index, int count, const Outcome& o, int tab, const std::string& status) {
    cv::Mat img = blank();
    const std::string where = index >= 0 ? "CASE " + std::to_string(index + 1) + " / " + std::to_string(count) + "   " : "";
    header(img, o.ok ? "RESULT" : "COULD NOT SCORE", where + fileName(c.user) + "   VS   " + fileName(c.ref));
    if (o.ok) {
        tabs(img, tab);
        if (tab == 0) overview(img, o);
        else if (tab == 1) structureTab(img, o);
        else qualityTab(img, o);
        footer(img, "1/2/3 TAB VIEW   A/D PREV/NEXT   S SAVE   B BACK   Q QUIT", status);
    } else {
        // Text column ends before the photo at x = 760 (12 px per character).
        int y = 150;
        for (const std::string& line : wrap(friendlyError(o.stage), 38)) { text(img, 40, y, line, kBad, 3); y += 34; }
        y += 16;
        for (const std::string& line : wrap(errorHint(o.stage), 56)) { text(img, 40, y, line, kText); y += 28; }
        y += 20;
        text(img, 40, y, "STEP: " + o.stage, kDim);
        picture(img, loadOrEmpty(c.user), 760, 140, 380, 380);
        footer(img, "A/D PREV/NEXT   B BACK   Q QUIT", status);
    }
    return img;
}

// ---------------------------------------------------------------------------
Outcome score(const Case& c) {
    Outcome o;
    const auto t0 = std::chrono::steady_clock::now();
    o.stage = "load images";
    try {
        PatternSpec spec;
        spec.name = c.pattern;
        const cv::Mat ref = io::loadColorFile(c.ref), user = io::loadColorFile(c.user);
        o.r = runPipeline(ref, user, spec, &o.stage);
        viz::printReport(spec, o.r.structure, o.r.quality, o.r.score);
        o.stage = "render result";
        o.render = viz::renderResult(o.r.ref, o.r.user, o.r.structure, o.r.score);
        o.ok = true;
    } catch (const std::exception& e) {
        o.error = e.what();
        std::fprintf(stderr, "[error] stage '%s': %s\n", o.stage.c_str(), e.what());
    }
    o.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return o;
}

// Windows standard "Open" dialog. Returns an empty path when cancelled.
// OFN_NOCHANGEDIR keeps the working directory (where S saves screens).
fs::path pickFile(const wchar_t* title, const fs::path& startDir) {
    wchar_t file[4096] = L"";
    const std::wstring dir = startDir.wstring();
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = FindWindowA(nullptr, kWindow);
    ofn.lpstrFilter = L"Photos (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = static_cast<DWORD>(std::size(file));
    ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str();
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    return GetOpenFileNameW(&ofn) ? fs::path(file) : fs::path();
}

// Display name only: "heart" / "tulip" when the file names say so.
std::string guessPattern(const fs::path& user, const fs::path& ref) {
    std::string names = user.filename().u8string() + " " + ref.filename().u8string();
    for (char& ch : names) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    if (names.find("heart") != std::string::npos) return "heart";
    if (names.find("tulip") != std::string::npos) return "tulip";
    return "custom";
}

bool chooseOwnPhotos(fs::path& lastDir, Case& c) {
    const fs::path ref = pickFile(L"1 / 2   Choose the REFERENCE photo (expert latte art)", lastDir);
    if (ref.empty()) return false;
    lastDir = ref.parent_path();
    const fs::path user = pickFile(L"2 / 2   Choose the photo to SCORE", lastDir);
    if (user.empty()) return false;
    lastDir = user.parent_path();
    c = {"own", ref, user, guessPattern(user, ref)};
    return true;
}

cv::Mat busy(cv::Mat base, const std::string& message) {
    fillRect(base, W / 2 - 300, H / 2 - 50, W / 2 + 300, H / 2 + 50, kSelect);
    textCentered(base, W / 2, H / 2 - 12, message, kText, 3);
    return base;
}

int snapshot(const std::vector<Case>& cases, const fs::path& dir) {
    fs::create_directories(dir);
    auto save = [&](const std::string& name, const cv::Mat& img) {
        io::save((dir / name).string(), img);
        std::printf("saved %s\n", (dir / name).string().c_str());
    };
    if (cases.empty()) throw std::runtime_error("demo_samples not found");
    const int ok = 1, fail = std::min(14, int(cases.size()) - 1);
    save("ui_menu.png", menuScreen(cases, ok, loadOrEmpty(cases[ok].ref), loadOrEmpty(cases[ok].user), ""));
    const Outcome o = score(cases[ok]);
    for (int tab = 0; tab < 3; ++tab)
        save("ui_result_" + std::to_string(tab + 1) + ".png", resultScreen(cases[ok], ok, int(cases.size()), o, tab, ""));
    save("ui_error.png", resultScreen(cases[fail], fail, int(cases.size()), score(cases[fail]), 0, ""));
    const int poor = std::min(11, int(cases.size()) - 1);  // a poorly made practice cup
    save("ui_result_poor.png", resultScreen(cases[poor], poor, int(cases.size()), score(cases[poor]), 0, ""));
    return o.ok ? 0 : 2;
}

}  // namespace

int runUi(const char* exePath, int argc, char** argv) {
    const fs::path demo = findDemo(exePath);
    const std::vector<Case> cases = demo.empty() ? std::vector<Case>{} : demoCases(demo);
    if (argc >= 2 && std::string(argv[0]) == "--snapshot") return snapshot(cases, argv[1]);

    std::printf("LatteArt viewer: use the keys shown at the bottom of the window.\n");
    enum class Screen { Menu, Result } screen = Screen::Menu;
    int sel = 0, tab = 0, current = -1, saved = 0;
    Case active;
    Outcome outcome;
    std::string status;
    // The "Open" dialog starts in pictures/ (team photos), then remembers the
    // last folder used.
    std::error_code ec;
    fs::path lastDir = demo.empty() ? fs::current_path(ec) : demo.parent_path() / "pictures";
    if (!fs::exists(lastDir, ec)) lastDir = demo;
    std::map<fs::path, cv::Mat> thumbs;  // raw photos are large: load each once
    auto thumb = [&](const fs::path& path) -> const cv::Mat& {
        auto it = thumbs.find(path);
        if (it == thumbs.end()) it = thumbs.emplace(path, fitInto(loadOrEmpty(path), 250, 250)).first;
        return it->second;
    };
    auto run = [&](int index, const Case& c) {
        cv::Mat base = screen == Screen::Menu && !cases.empty()
                           ? menuScreen(cases, sel, thumb(cases[sel].ref), thumb(cases[sel].user), "")
                           : blank();
        io::show(kWindow, busy(base, "SCORING... PLEASE WAIT"));
        io::waitKey(1);
        active = c;
        current = index;
        outcome = score(c);
        tab = 0;
        screen = Screen::Result;
    };

    for (;;) {
        cv::Mat frame;
        if (screen == Screen::Menu) {
            const bool has = sel >= 0 && sel < int(cases.size());
            frame = menuScreen(cases, sel, has ? thumb(cases[sel].ref) : cv::Mat(),
                               has ? thumb(cases[sel].user) : cv::Mat(), status);
        } else {
            frame = resultScreen(active, current, int(cases.size()), outcome, tab, status);
        }
        status.clear();
        io::show(kWindow, frame);
        const int raw = io::waitKey(0);
        if (raw == -1) return 0;  // window closed: waitKey returns at once with no window
        const int key = std::tolower(raw & 0xFF);
        if (key == 'q' || (key == 27 && screen == Screen::Menu)) return 0;

        if (screen == Screen::Menu) {
            const int n = int(cases.size());
            if ((key == 'w' || key == 'k') && n) sel = (sel + n - 1) % n;
            else if ((key == 's' || key == 'j') && n) sel = (sel + 1) % n;
            else if ((key == 13 || key == 10 || key == ' ') && n) run(sel, cases[sel]);
            else if (key == 'o') {
                io::show(kWindow, busy(frame, "CHOOSE THE PHOTOS"));
                io::waitKey(1);
                Case own;
                if (chooseOwnPhotos(lastDir, own)) run(-1, own);
                else status = "CANCELLED";
            }
        } else {
            const int n = int(cases.size());
            if (key == 'b' || key == 8 || key == 27) { screen = Screen::Menu; if (current >= 0) sel = current; }
            else if (key >= '1' && key <= '3' && outcome.ok) tab = key - '1';
            else if (key == 9 && outcome.ok) tab = (tab + 1) % 3;
            else if (key == 'd' && current >= 0 && n) { sel = (current + 1) % n; run(sel, cases[sel]); }
            else if (key == 'a' && current >= 0 && n) { sel = (current + n - 1) % n; run(sel, cases[sel]); }
            else if (key == 'f' && outcome.ok) io::show("LatteArt - full result", outcome.render);
            else if (key == 's') {
                const std::string name = "latteart_screen_" + std::to_string(++saved) + ".png";
                io::save(name, frame);
                if (outcome.ok) io::save("latteart_result_" + std::to_string(saved) + ".png", outcome.render);
                status = "SAVED " + name;
                std::printf("saved %s in %s\n", name.c_str(), fs::current_path().string().c_str());
            }
        }
    }
}

}  // namespace la::app
