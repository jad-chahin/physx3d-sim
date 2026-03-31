#include <exception>
#include <iostream>

#include "TestRegistry.h"

using test_registry::TestList;

void appendPhysicsCoreTests(TestList& tests);
void appendUiPauseMenuTests(TestList& tests);
void appendMinimapTests(TestList& tests);
void appendRenderMaterialTests(TestList& tests);
void appendSceneLightingTests(TestList& tests);
void appendFocusCameraTests(TestList& tests);

int main()
{
    TestList tests;
    appendPhysicsCoreTests(tests);
    appendUiPauseMenuTests(tests);
    appendMinimapTests(tests);
    appendRenderMaterialTests(tests);
    appendSceneLightingTests(tests);
    appendFocusCameraTests(tests);

    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << "\n";
        } catch (const std::exception& ex) {
            ++failures;
            std::cout << "[FAIL] " << name << ": " << ex.what() << "\n";
        }
    }

    if (failures != 0) {
        std::cout << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << tests.size() << " test(s) passed\n";
    return 0;
}
