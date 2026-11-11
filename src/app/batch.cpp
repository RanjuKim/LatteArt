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
    (void)path;
    LA_TODO("readLabels");
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
    (void)row; (void)photoDir;
    LA_TODO("scoreRow");
}

// ---------------------------------------------------------------------------
// Step 3: write results.csv with std::ofstream.
// Header (one line):
//   file,pattern,target_layers,true_layers,detected_layers,fft_layers,
//   uncertain,layer_correct,U,Q,contrast,shape,texture,position,size,
//   final_score,error
// Then one line per result. layer_correct = 1 if detectedLayers == trueLayers.
void writeResults(const std::string& path, const std::vector<BatchResult>& results) {
    (void)path; (void)results;
    LA_TODO("writeResults");
}

// ---------------------------------------------------------------------------
// Step 4: print a short summary to the console, e.g.
//   rows: 10, scored: 8, errors: 2
//   layer count correct: 6 / 8 (75.0%)
// (count only rows without an error for the accuracy)
void printSummary(const std::vector<BatchResult>& results) {
    (void)results;
    LA_TODO("printSummary");
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
