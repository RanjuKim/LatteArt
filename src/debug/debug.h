// Owner: SHARED - change only with team agreement (PR approved by all)
// Debug mode: runs ONE pipeline stage and saves its intermediate results as
// debug_<stage>_*.png, so each stage can be checked before the whole
// pipeline works (and shown in the report / demo afterwards).
//
//   LatteArt.exe --debug preprocess <photo.jpg>                    (Junkyu Kim)
//   LatteArt.exe --debug normalize  <photo.jpg>                    (Jaeku Kang)
//   LatteArt.exe --debug segment    <canon.png>                    (Ranju Kim)
//   LatteArt.exe --debug layers     <canon.png>                    (Jaeku Kang)
//   LatteArt.exe --debug quality    <ref_canon.png> <user_canon.png> (Ranju Kim)
//   LatteArt.exe --debug viz        <canon.png>                    (Cecelia Hein)
//
// Each stage lives in its own file (src/debug/<stage>.cpp) owned by the
// owner of that stage, so nobody needs to edit main.cpp to test their code.
#pragma once
#include <string>
#include <vector>

namespace la::debug {

using Args = std::vector<std::string>;

int runPreprocess(const Args& args);  // preprocess.cpp - Junkyu Kim
int runNormalize(const Args& args);   // normalize.cpp  - Jaeku Kang
int runSegment(const Args& args);     // segment.cpp    - Ranju Kim
int runLayers(const Args& args);      // layers.cpp     - Jaeku Kang
int runQuality(const Args& args);     // quality.cpp    - Ranju Kim
int runViz(const Args& args);         // viz.cpp        - Cecelia Hein

}  // namespace la::debug
