#pragma once

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace clipforge::tests {

struct TestFailure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline void require_true(bool cond, std::string_view expr, const char* file, int line) {
    if (!cond) {
        std::ostringstream oss;
        oss << file << ":" << line << ": REQUIRE(" << expr << ") failed.";
        throw TestFailure(oss.str());
    }
}

inline void require_near(double a, double b, double tol, const char* file, int line) {
    if (std::fabs(a - b) > tol) {
        std::ostringstream oss;
        oss << file << ":" << line << ": REQUIRE_NEAR(" << a << " vs " << b
            << ", tol=" << tol << ") failed.";
        throw TestFailure(oss.str());
    }
}

#define CF_REQUIRE(expr) \
    ::clipforge::tests::require_true(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

#define CF_REQUIRE_NEAR(a, b, tol) \
    ::clipforge::tests::require_near(static_cast<double>(a), static_cast<double>(b), static_cast<double>(tol), __FILE__, __LINE__)

void run_wav_io_tests();
void run_processing_tests();
void run_dsp_differential_tests();

} // namespace clipforge::tests
