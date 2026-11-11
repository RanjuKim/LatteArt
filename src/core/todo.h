// Owner: SHARED - change only with team agreement (PR approved by all)
// Placeholder for functions that are not implemented yet.
// Calling one throws, and main() prints which step is missing.
#pragma once

#include <stdexcept>
#include <string>

#define LA_TODO(what) \
    throw std::logic_error(std::string("TODO: ") + (what) + "  [" + __FILE__ + "]")
