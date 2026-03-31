#ifndef PHYSICS3D_TESTS_TESTREGISTRY_H
#define PHYSICS3D_TESTS_TESTREGISTRY_H

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace test_registry {

using TestCase = std::pair<std::string, std::function<void()>>;
using TestList = std::vector<TestCase>;

} // namespace test_registry

#endif // PHYSICS3D_TESTS_TESTREGISTRY_H
