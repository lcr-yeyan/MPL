#include "TestHarness.hpp"

#include <cstring>
#include <exception>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--update-golden") == 0) {
            mpltest::updateGoldenMode() = true;
        }
    }

    const std::vector<mpltest::TestCase>& tests = mpltest::registry();
    int failed = 0;

    for (const mpltest::TestCase& test : tests) {
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << "\n";
        } catch (const std::exception& error) {
            ++failed;
            std::cout << "[FAIL] " << test.name << "\n       " << error.what() << "\n";
        } catch (...) {
            ++failed;
            std::cout << "[FAIL] " << test.name << "\n       unknown exception\n";
        }
    }

    std::cout << "\n" << (tests.size() - static_cast<std::size_t>(failed)) << "/"
              << tests.size() << " tests passed\n";
    return failed == 0 ? 0 : 1;
}
