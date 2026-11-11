// Owner: SHARED - change only with team agreement (PR approved by all)
// Personal sandboxes. Each developer experiments ONLY in their own file, so
// nobody needs to touch main.cpp:
//   LatteArt.exe --selftest jaeku  [args...]
//   LatteArt.exe --selftest ranju  [args...]
//   LatteArt.exe --selftest junkyu [args...]
#pragma once
#include <string>
#include <vector>

namespace la::selftest {

int jaeku(const std::vector<std::string>& args);   // src/selftest/jaeku.cpp
int ranju(const std::vector<std::string>& args);   // src/selftest/ranju.cpp
int junkyu(const std::vector<std::string>& args);  // src/selftest/junkyu.cpp

}  // namespace la::selftest
