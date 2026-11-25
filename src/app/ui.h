// Owner: Ranju Kim
// Interactive viewer: keyboard-driven screens on top of the scoring pipeline.
//
//   LatteArt.exe            (no arguments, e.g. double-click or Ctrl+F5)
//   LatteArt.exe --ui
//   LatteArt.exe --ui --snapshot <dir>   saves every screen as PNG, no window
//
// Only OpenCV input/output is used (imshow / waitKey / imread / imwrite through
// io::). Everything on screen is drawn with our own pixel functions (viz/draw),
// and the only interaction is the keyboard via waitKey, so the assignment's
// "OpenCV I/O functions only" rule holds.
#pragma once

namespace la::app {

// exePath = argv[0] of the program (used to find demo_samples next to the
// repository). argc/argv are the arguments AFTER "--ui" (none when started
// without arguments). Returns the process exit code.
int runUi(const char* exePath, int argc, char** argv);

}  // namespace la::app
