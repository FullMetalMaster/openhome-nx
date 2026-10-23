// Test helper: stub definitions for every save-family opener you are NOT linking for real, so a test binary
// can link save/savefile.cpp plus only the reader(s) under test.
//
// Before including this header, define the macro of each family whose real .cpp you ARE linking:
//   OH_REAL_GEN3  OH_REAL_GEN12  OH_REAL_GEN45  OH_REAL_GEN67  OH_REAL_BDSP  OH_REAL_SCBLOCK
// Example (testing Gen 1-2):   #define OH_REAL_GEN12   then link source/save/gen12.cpp.
#pragma once
#include "save/readers.hpp"

namespace save {
#ifndef OH_REAL_GEN3
SavePtr openGen3(const std::vector<uint8_t>&, const std::string&, const Game*, std::string&) { return nullptr; }
#endif
#ifndef OH_REAL_GEN12
SavePtr openGen12(const std::vector<uint8_t>&, const std::string&, const Game*, std::string&) { return nullptr; }
#endif
#ifndef OH_REAL_GEN45
SavePtr openGen45(const std::vector<uint8_t>&, const std::string&, const Game*, std::string&) { return nullptr; }
#endif
#ifndef OH_REAL_GEN67
SavePtr openGen67(const std::vector<uint8_t>&, const std::string&, const Game*, std::string&) { return nullptr; }
#endif
#ifndef OH_REAL_BDSP
SavePtr openBDSP(const std::vector<uint8_t>&, const std::string&, const Game*, std::string&) { return nullptr; }
#endif
#ifndef OH_REAL_SCBLOCK
SavePtr openSCBlock(const std::vector<uint8_t>&, const std::string&, const Game*, std::string&) { return nullptr; }
#endif
}  // namespace save
