// Owner: Cecelia Hein
//
// How to work on this file
// ------------------------
// Write runBatch() step by step (Step 1 -> 4 below). After each step, build
// (Ctrl+Shift+B) and run:
//   bin\Release\LatteArt.exe --batch data/labels.csv data/user results.csv
//
// Until the rest of the pipeline is finished, every row will fail with a
// "TODO: ..." message. That is expected: your job is that the CSV is read
// correctly and every row ends up in results.csv (with the error message).
// Real numbers will appear automatically once the other parts are done.
//
// Only standard C++ is needed here (std::ifstream, std::ofstream,
// std::getline, std::stoi). Load images ONLY with io::loadColor().
#include "app/batch.h"

#include <cstdio>
#include <exception>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "app/pipeline.h"
#include "core/todo.h"
#include "io/io.h"

namespace la::app {

namespace {

// One line of labels.csv.
struct LabelRow {
    std::string file;
    std::string pattern;
    int targetLayers = 0;
    int trueLayers = 0;
    std::string notes;
};

// ---------------------------------------------------------------------------
// Step 1: read labels.csv.
//   - open the file with std::ifstream; if it fails, throw std::runtime_error
//   - read line by line with std::getline(file, line)
//   - skip the first line (header) and empty lines
//   - split each line at ',' : put the line in a std::stringstream and call
//     std::getline(ss, field, ',') repeatedly
//   - convert target_layers / true_layers with std::stoi
//   - return all rows
std::vector<LabelRow> readLabels(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("readLabels: could not open " + path);

    std::vector<LabelRow> rows;
    std::string line;
    bool header = true;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (header) {
            header = false;
            continue;
        }
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string fileName, pattern, targetText, trueText, notes;
        if (!std::getline(ss, fileName, ',')) continue;
        if (!std::getline(ss, pattern, ',')) continue;
        if (!std::getline(ss, targetText, ',')) continue;
        if (!std::getline(ss, trueText, ',')) continue;
        std::getline(ss, notes);

        try {
            LabelRow row;
            row.file = fileName;
            row.pattern = pattern;
            row.targetLayers = std::stoi(targetText);
            row.trueLayers = std::stoi(trueText);
            row.notes = notes;
            rows.push_back(row);
        } catch (const std::exception& e) {
            throw std::runtime_error("readLabels: invalid numeric field in " + path +
                                     " (" + e.what() + ")");
        }
    }
    return rows;
}

// ---------------------------------------------------------------------------
// Step 2: score one row.
//   - reference photo: "data/reference/" + row.pattern + std::to_string(row.targetLayers) + ".jpg"
//   - user photo:      photoDir + "/" + row.file
//   - load both with io::loadColor(path)
//   - PatternSpec spec; spec.name = row.pattern; spec.targetLayers = row.targetLayers;
//   - std::string stage;  PipelineResult r = runPipeline(refImg, userImg, spec, &stage);
//   - fill a BatchResult from r (see the fields below)
//   - if anything throws, catch std::exception& e and put
//     "stage: " + e.what() into result.error (replace any ',' with ';'
//     so the CSV is not broken)
struct BatchResult {
    LabelRow label;
    int detectedLayers = -1;   // r.userLayers.count
    int fftLayers = -1;        // r.userLayers.fftCount
    bool uncertain = false;    // r.userLayers.uncertain
    float U = 0.f, Q = 0.f;    // r.score.U, r.score.Q
    float contrast = 0.f, shape = 0.f, texture = 0.f, position = 0.f, size = 0.f;  // r.quality.<item>.score
    float finalScore = 0.f;    // r.score.finalScore
    std::string error;         // empty when the row was scored
};

BatchResult scoreRow(const LabelRow& row, const std::string& photoDir) {
    BatchResult result;
    result.label = row;
    std::string stage = "load images";

    try {
        const std::string referencePath =
            "data/reference/" + row.pattern + std::to_string(row.targetLayers) + ".jpg";
        const std::string userPath = photoDir + "/" + row.file;

        stage = "load images";
        const cv::Mat refImg = io::loadColor(referencePath);
        const cv::Mat userImg = io::loadColor(userPath);

        PatternSpec spec;
        spec.name = row.pattern;
        spec.targetLayers = row.targetLayers;
        PipelineResult r = runPipeline(refImg, userImg, spec, &stage);

        result.detectedLayers = r.userLayers.count;
        result.fftLayers = r.userLayers.fftCount;
        result.uncertain = r.userLayers.uncertain;
        result.U = r.score.U;
        result.Q = r.score.Q;
        result.contrast = r.quality.contrast.score;
        result.shape = r.quality.shape.score;
        result.texture = r.quality.texture.score;
        result.position = r.quality.position.score;
        result.size = r.quality.size.score;
        result.finalScore = r.score.finalScore;
    } catch (const std::exception& e) {
        std::string message = stage + ": " + e.what();
        for (char& ch : message) {
            if (ch == ',') ch = ';';
        }
        result.error = message;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Step 3: write results.csv with std::ofstream.
// Header (one line):
//   file,pattern,target_layers,true_layers,detected_layers,fft_layers,
//   uncertain,layer_correct,U,Q,contrast,shape,texture,position,size,
//   final_score,error
// Then one line per result. layer_correct = 1 if detectedLayers == trueLayers.
void writeResults(const std::string& path, const std::vector<BatchResult>& results) {
    std::ofstream file(path);
    if (!file) throw std::runtime_error("writeResults: could not open " + path);

    file << "file,pattern,target_layers,true_layers,detected_layers,fft_layers,"
         << "uncertain,layer_correct,U,Q,contrast,shape,texture,position,size,"
         << "final_score,error\n";

    for (const BatchResult& r : results) {
        const int layerCorrect =
            r.error.empty() && r.detectedLayers == r.label.trueLayers ? 1 : 0;
        file << r.label.file << ',' << r.label.pattern << ','
             << r.label.targetLayers << ',' << r.label.trueLayers << ','
             << r.detectedLayers << ',' << r.fftLayers << ','
             << (r.uncertain ? 1 : 0) << ',' << layerCorrect << ','
             << r.U << ',' << r.Q << ','
             << r.contrast << ',' << r.shape << ',' << r.texture << ','
             << r.position << ',' << r.size << ','
             << r.finalScore << ',' << r.error << '\n';
    }
}

// ---------------------------------------------------------------------------
// Step 4: print a short summary to the console, e.g.
//   rows: 10, scored: 8, errors: 2
//   layer count correct: 6 / 8 (75.0%)
// (count only rows without an error for the accuracy)
void printSummary(const std::vector<BatchResult>& results) {
    int scored = 0;
    int correct = 0;
    int errors = 0;

    for (const BatchResult& r : results) {
        if (!r.error.empty()) {
            ++errors;
            continue;
        }
        ++scored;
        if (r.detectedLayers == r.label.trueLayers) ++correct;
    }

    const double percent = scored == 0 ? 0.0 : 100.0 * correct / scored;
    std::printf("rows: %zu, scored: %d, errors: %d\n",
                results.size(), scored, errors);
    std::printf("layer count correct: %d / %d (%.1f%%)\n",
                correct, scored, percent);
}

}  // namespace

int runBatch(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: --batch <labels.csv> <photo_dir> <out.csv>\n");
        return 1;
    }
    const std::vector<LabelRow> rows = readLabels(argv[0]);
    std::vector<BatchResult> results;
    for (const LabelRow& row : rows) results.push_back(scoreRow(row, argv[1]));
    writeResults(argv[2], results);
    printSummary(results);
    return 0;
}

}  // namespace la::app
