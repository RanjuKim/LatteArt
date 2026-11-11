// Owner: Junkyu Kim
// Batch evaluation for validation:
//   LatteArt.exe --batch <args...>
// Planned: read data/labels.csv, run runPipeline() on every row, write a CSV
// with detected layers, U, Q, item scores and final score next to the human
// ratings for correlation analysis.
#pragma once

namespace la::app {

// argc/argv are the arguments AFTER "--batch".
int runBatch(int argc, char** argv);

}  // namespace la::app
