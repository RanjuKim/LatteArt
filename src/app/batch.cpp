// Owner: Cecelia Hein
// Multi-line batch export: one image summary, one row per compared line,
// and one row per matched/missing/extra peak. No photo-wide layer count.
#include "app/batch.h"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "app/pipeline.h"
#include "io/io.h"
#include "scoring/scoring.h"

namespace la::app {
namespace {
namespace fs = std::filesystem;
using Row = std::vector<std::string>;

// CSV quoting is needed for file names and feedback containing commas, quotes
// or newlines. A malformed record fails explicitly instead of silently shifting columns.
bool readRecord(std::istream& in, Row& row) {
    row.clear();
    std::string field;
    bool quoted = false, closed = false, started = false;
    char c;
    while (in.get(c)) {
        started = true;
        if (quoted) {
            if (c == '"') {
                if (in.peek() == '"') { in.get(c); field += '"'; }
                else { quoted = false; closed = true; }
            } else field += c;
        } else if (c == ',' || c == '\n' || c == '\r') {
            row.push_back(field);
            field.clear();
            closed = false;
            if (c != ',') {
                if (c == '\r' && in.peek() == '\n') in.get(c);
                return true;
            }
        } else if (c == '"') {
            if (!field.empty() || closed) throw std::runtime_error("invalid quote in CSV field");
            quoted = true;
        } else {
            if (closed) throw std::runtime_error("unexpected text after quoted CSV field");
            field += c;
        }
    }
    if (quoted) throw std::runtime_error("unterminated quoted CSV field");
    if (started) row.push_back(field);
    return started;
}

void writeRecord(std::ostream& out, const Row& row) {
    for (size_t i = 0; i < row.size(); ++i) {
        if (i) out << ',';
        const auto& field = row[i];
        if (field.find_first_of(",\"\r\n") == std::string::npos) out << field;
        else {
            out << '"';
            for (char c : field) { if (c == '"') out << '"'; out << c; }
            out << '"';
        }
    }
    out << '\n';
}

template<class T> std::string number(T value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(9) << value;
    return out.str();
}

struct LabelRow {
    size_t id = 0;
    std::string file, pattern, notes, referencePath;
    int catalogLayers = 0;
};

std::vector<LabelRow> readLabels(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read labels: " + path.string());
    // Skip a UTF-8 BOM, including when the first header is quoted.
    if (input.peek() == 0xEF) {
        char bom[3];
        input.read(bom, 3);
        if (input.gcount() != 3 || static_cast<unsigned char>(bom[1]) != 0xBB ||
            static_cast<unsigned char>(bom[2]) != 0xBF)
            throw std::runtime_error("labels must be UTF-8 CSV");
    }
    Row header;
    if (!readRecord(input, header)) throw std::runtime_error("labels CSV has no header");
    for (size_t i = 0; i < header.size(); ++i)
        for (size_t j = 0; j < i; ++j)
            if (header[i] == header[j]) throw std::runtime_error("duplicate CSV column: " + header[i]);
    auto column = [&](const std::string& name) {
        for (size_t i = 0; i < header.size(); ++i) if (header[i] == name) return int(i);
        return -1;
    };
    const int fileCol = column("file"), patternCol = column("pattern"), refCol = column("reference"),
              catalogCol = column("target_layers"), notesCol = column("notes");
    if (fileCol < 0 || patternCol < 0 || (refCol < 0 && catalogCol < 0))
        throw std::runtime_error("labels require file,pattern and reference (or legacy target_layers)");
    Row fields;
    std::vector<LabelRow> labels;
    while (readRecord(input, fields)) {
        if (fields.size() == 1 && fields[0].empty()) continue;
        if (fields.size() != header.size()) throw std::runtime_error("CSV record has incorrect column count");
        auto get = [&](int col) { return col < 0 ? std::string() : fields[col]; };
        LabelRow row;
        row.id = labels.size() + 1;
        row.file = get(fileCol);
        row.pattern = get(patternCol);
        row.notes = get(notesCol);
        if (row.file.empty() || row.pattern.empty()) throw std::runtime_error("file/pattern must not be empty");
        const auto catalog = get(catalogCol);
        if (!catalog.empty()) {
            size_t used = 0;
            row.catalogLayers = std::stoi(catalog, &used);
            if (used != catalog.size() || row.catalogLayers <= 0)
                throw std::runtime_error("target_layers metadata must be a positive integer or empty");
        }
        auto reference = get(refCol);
        if (reference.empty()) {
            if (row.catalogLayers <= 0) throw std::runtime_error("each row needs a reference image");
            row.referencePath = (fs::path("data/reference") /
                                (row.pattern + std::to_string(row.catalogLayers) + ".jpg")).string();
        } else {
            fs::path refPath(reference);
            if (refPath.is_relative()) refPath = path.parent_path() / refPath;
            row.referencePath = refPath.lexically_normal().string();
        }
        // true_layers, if present in old label files, is deliberately not used.
        labels.push_back(std::move(row));
    }
    return labels;
}

struct BatchResult {
    LabelRow label;
    StructureComparison structure;
    QualityResult quality;
    ScoreResult score;
    std::string stage, error;
};

BatchResult scoreRow(const LabelRow& row, const fs::path& photoDir) {
    BatchResult out;
    out.label = row;
    out.stage = "load images";
    try {
        const auto ref = io::loadColor(row.referencePath);
        const auto user = io::loadColor((photoDir / fs::path(row.file)).string());
        PatternSpec spec{row.pattern, row.catalogLayers};
        auto result = runPipeline(ref, user, spec, &out.stage);
        out.structure = std::move(result.structure);
        out.quality = std::move(result.quality);
        out.score = result.score;
        out.stage.clear();
    } catch (const std::exception& e) {
        out.error = e.what();
    }
    return out;
}

void writeResults(const fs::path& summaryPath, const fs::path& linesPath, const fs::path& matchesPath,
                  const std::vector<BatchResult>& results) {
    // Binary mode preserves embedded newlines inside quoted notes on Windows.
    std::ofstream summary(summaryPath, std::ios::binary), lines(linesPath, std::ios::binary),
                  matches(matchesPath, std::ios::binary);
    if (!summary || !lines || !matches) throw std::runtime_error("cannot open batch output CSV files");
    writeRecord(summary, {"row_id","file","pattern","reference","catalog_layers","notes","status",
                         "line_count","fully_matched_lines","missing_occurrences","extra_occurrences",
                         "rotation_deg","width_difference","height_difference","U","Q","contrast",
                         "shape","texture","position","size","final_score","vertical_lines","horizontal_lines",
                         "vertical_score","horizontal_score","vertical_structure","horizontal_structure","error_stage","error"});
    writeRecord(lines, {"row_id","file","line_index","direction","coordinate","reference_count","user_count","matched_count",
                       "missing","extra","spacing","thickness","U","band_low","band_high","Q","final_score"});
    writeRecord(matches, {"row_id","file","line_index","direction","coordinate","reference_index","user_index",
                         "reference_position","user_position","reference_width","user_width","status"});
    for (const auto& r : results) {
        const bool ok = r.error.empty();
        const std::string status = ok ? "scored" :
                                  (r.stage == "pattern size validation" ? "rejected_size" : "error");
        Row row{number(r.label.id), r.label.file, r.label.pattern, r.label.referencePath,
                r.label.catalogLayers ? number(r.label.catalogLayers) : "", r.label.notes, status};
        if (ok) {
            size_t full = 0, missing = 0, extra = 0;
            for (size_t i = 0; i < r.structure.lines.size(); ++i) {
                const auto& line = r.structure.lines[i];
                if (!line.reference.empty() && line.missing == 0 && line.extra == 0) ++full;
                missing += line.missing;
                extra += line.extra;
                writeRecord(lines, {number(r.label.id), r.label.file, number(i), directionName(line.direction), number(line.coordinate),
                    number(line.reference.size()), number(line.user.size()), number(line.matches.size()),
                    number(line.missing), number(line.extra), number(line.spacingScore),
                    number(line.thicknessScore), number(line.U), number(line.bandLow), number(line.bandHigh),
                    number(r.score.Q), number(scoring::computeLineScore(line, r.score.Q))});
                std::vector<bool> refMatched(line.reference.size()), userMatched(line.user.size());
                auto emit = [&](int ri, int ui, const char* kind) {
                    const Peak* ref = ri < 0 ? nullptr : &line.reference.at(ri);
                    const Peak* user = ui < 0 ? nullptr : &line.user.at(ui);
                    writeRecord(matches, {number(r.label.id), r.label.file, number(i), directionName(line.direction), number(line.coordinate),
                        ref ? number(ri) : "", user ? number(ui) : "",
                        ref ? number(ref->position) : "", user ? number(user->position) : "",
                        ref ? number(ref->width) : "", user ? number(user->width) : "", kind});
                };
                for (const auto& pair : line.matches) {
                    emit(pair.reference, pair.user, "matched");
                    refMatched.at(pair.reference) = userMatched.at(pair.user) = true;
                }
                for (size_t j = 0; j < refMatched.size(); ++j)
                    if (!refMatched[j]) emit(int(j), -1, "missing");
                for (size_t j = 0; j < userMatched.size(); ++j)
                    if (!userMatched[j]) emit(-1, int(j), "extra");
            }
            const Row metrics{number(r.structure.lines.size()), number(full), number(missing), number(extra),
                number(r.structure.angleDeg), number(r.structure.widthDifference), number(r.structure.heightDifference),
                number(r.score.U), number(r.score.Q), number(r.quality.contrast.score), number(r.quality.shape.score),
                number(r.quality.texture.score), number(r.quality.position.score), number(r.quality.size.score),
                number(r.score.finalScore), number(r.score.vertical.lineCount), number(r.score.horizontal.lineCount),
                number(r.score.vertical.finalScore), number(r.score.horizontal.finalScore),
                number(r.score.vertical.alignmentScore), number(r.score.horizontal.alignmentScore)};
            row.insert(row.end(), metrics.begin(), metrics.end());
        } else row.resize(28); // Failed/rejected images have no numeric score, not a fake zero.
        row.push_back(r.stage);
        row.push_back(r.error);
        writeRecord(summary, row);
    }
    summary.flush(); lines.flush(); matches.flush();
    if (!summary || !lines || !matches) throw std::runtime_error("failed to write complete batch output");
}
} // namespace

int runBatch(int argc, char** argv) {
    if (argc != 3) {
        std::printf("usage: --batch <labels.csv> <photo_dir> <out.csv>\n");
        return 1;
    }
    const fs::path labelPath(argv[0]), photoDir(argv[1]), summaryPath(argv[2]);
    const fs::path linesPath = summaryPath.parent_path() / (summaryPath.stem().string() + "_lines.csv");
    const fs::path matchesPath = summaryPath.parent_path() / (summaryPath.stem().string() + "_matches.csv");
    const auto labels = readLabels(labelPath);
    // Do not overwrite labels or source photos even if the caller reuses an input path.
    for (const auto& output : {summaryPath, linesPath, matchesPath}) {
        const auto target = fs::weakly_canonical(output);
        if (target == fs::weakly_canonical(labelPath)) throw std::invalid_argument("output overlaps labels CSV");
        for (const auto& label : labels)
            if (target == fs::weakly_canonical(label.referencePath) ||
                target == fs::weakly_canonical(photoDir / fs::path(label.file)))
                throw std::invalid_argument("output overlaps an input photo");
    }
    std::vector<BatchResult> results;
    size_t scored = 0, errors = 0, rejected = 0;
    double scoreSum = 0;
    for (const auto& label : labels) {
        results.push_back(scoreRow(label, photoDir));
        const auto& r = results.back();
        if (r.error.empty()) { ++scored; scoreSum += r.score.finalScore; }
        else if (r.stage == "pattern size validation") ++rejected;
        else ++errors;
    }
    writeResults(summaryPath, linesPath, matchesPath, results);
    std::printf("rows: %zu, scored: %zu, size rejected: %zu, errors: %zu\n", results.size(), scored, rejected, errors);
    if (scored) std::printf("mean final score: %.3f / 100\n", scoreSum / scored);
    std::printf("saved %s\n      %s\n      %s\n", summaryPath.string().c_str(),
                linesPath.string().c_str(), matchesPath.string().c_str());
    // Row-level failures are recorded and do not prevent remaining rows being scored.
    return 0;
}
} // namespace la::app
