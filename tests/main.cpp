#include "TestHarness.hpp"

#include <cstring>
#include <exception>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string> filters;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--update-golden") == 0) {
            mpltest::updateGoldenMode() = true;
        } else if (std::strcmp(argv[index], "--filter") == 0 && index + 1 < argc) {
            filters.push_back(argv[++index]);
        }
    }

    const std::vector<mpltest::TestCase>& tests = mpltest::registry();
    int failed = 0;
    int selected = 0;

    for (const mpltest::TestCase& test : tests) {
        if (!filters.empty()) {
            bool matched = false;
            for (const std::string& filter : filters) {
                matched = matched || test.name.find(filter) != std::string::npos;
            }
            if (!matched) {
                continue;
            }
        }
        ++selected;
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

    std::cout << "\n" << (selected - failed) << "/" << selected << " tests passed\n";
    return failed == 0 ? 0 : 1;
}
