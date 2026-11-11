// Owner: Cecelia Hein
// Batch scoring: scores every photo listed in labels.csv and writes one CSV
// row per photo, so we can measure how often the detected layer count
// matches the true layer count.
//
//   LatteArt.exe --batch <labels.csv> <photo_dir> <out.csv>
//   e.g. LatteArt.exe --batch data/labels.csv data/user results.csv
//
// labels.csv columns:  file,pattern,target_layers,true_layers,notes
//   file          photo file name inside <photo_dir>, e.g. tulip3_01.jpg
//   pattern       heart / tulip
//   target_layers layer count the barista was trying to make
//   true_layers   layer count actually visible in the photo (counted by eye)
//   notes         free text (must not contain commas)
//
// Reference photo for each row: data/reference/<pattern><target_layers>.jpg
//   e.g. pattern=tulip, target_layers=3 -> data/reference/tulip3.jpg
#pragma once

namespace la::app {

// argc/argv are the arguments AFTER "--batch":
//   argv[0] = labels.csv, argv[1] = photo_dir, argv[2] = out.csv
int runBatch(int argc, char** argv);

}  // namespace la::app
