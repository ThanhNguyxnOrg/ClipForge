#include "test_harness.hpp"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    const std::string_view filter = (argc > 1) ? std::string_view(argv[1]) : "all";

    try {
        if (filter == "all" || filter == "wav") {
            clipforge::tests::run_wav_io_tests();
            std::cout << "[PASS] WAV I/O & RIFF validation tests\n";
        }
        if (filter == "all" || filter == "processing") {
            clipforge::tests::run_processing_tests();
            std::cout << "[PASS] Audio processing & batch safety tests\n";
        }
        if (filter == "all" || filter == "dsp") {
            clipforge::tests::run_dsp_differential_tests();
            std::cout << "[PASS] DSP C++ vs NASM SSE2 differential tests\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
